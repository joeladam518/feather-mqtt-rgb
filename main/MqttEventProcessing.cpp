#include "config.h"
#include "globals.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include <string.h>

#include <ArduinoJson.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <mqtt_client.h>

#include "led.h"
#include "MqttEventProcessing.h"
#include "NeoPixelRing.h"

#if defined(APP_DEBUG) && APP_DEBUG
#include "debug.h"
#endif

//==============================================================================
// Helpers

// Shorter topics must come before longer topics
static SubsctiptionCallbackType_t getCallbackType(esp_mqtt_event_handle_t event)
{
    if (strncmp(SUB_SET_COLOR, event->topic, event->topic_len) == 0) {
        return SET_COLOR;
    }

    if (strncmp(SUB_GET_COLOR, event->topic, event->topic_len) == 0) {
        return GET_COLOR;
    }

    if (strncmp(SUB_SET_TW_LIGHTS, event->topic, event->topic_len) == 0) {
        return SET_TW;
    }

    if (strncmp(SUB_GET_TW_LIGHTS, event->topic, event->topic_len) == 0) {
        return GET_TW;
    }

    return UNKNOWN;
}

static bool isShortTask(SubsctiptionCallbackType_t type) {
    if (type == GET_COLOR || type == GET_TW || type == SET_TW) {
        return true;
    }

    return false;
}

static bool isLongTask(SubsctiptionCallbackType_t type)
{
    if (type == SET_COLOR) {
        return true;
    }

    return false;
}

static void clearAction(SubscriptionAction_t *action)
{
    action->callbackType = UNKNOWN;
    action->client = NULL;
    memset(action->data, '\0', SUBSCRIPTIONDATALEN);
    action->dataLength = 0;
}

static void setAction(
    SubscriptionAction_t *action,
    SubsctiptionCallbackType_t type,
    esp_mqtt_event_handle_t event
) {
    clearAction(action);

    if (SUBSCRIPTIONDATALEN < event->data_len) {
        ESP_LOGW(MQTT_TAG, "Data length was too long");
        return;
    }

    action->callbackType = type;
    action->client = event->client;
    strncpy(action->data, (char *)event->data, event->data_len);
    action->dataLength = event->data_len;
}

//==============================================================================
// Mqtt publish functions

void publishRgbStatus(void)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "publishRgbStatus()");
    #endif

    char output[SUBSCRIPTIONDATALEN];
    JsonDocument doc;
    RGB_t color = {0, 0, 0};

    if (xSemaphoreTake(ringMutex, 0) == pdTRUE) {
        ring.getColor(&color);
        xSemaphoreGive(ringMutex);
    } else {
        #if defined(APP_DEBUG) && APP_DEBUG
            ESP_LOGI(MQTT_TAG, "The ring is already taken.");
        #endif

        return;
    }

    doc["r"] = color.r;
    doc["g"] = color.g;
    doc["b"] = color.b;

    serializeJson(doc, output, sizeof(output));
    esp_mqtt_client_publish(mqttClient, PUB_GET_COLOR, output, 0, 0, 0);
}

void publishTwinkleLightStatus(void)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "publishTwinkleLightStatus()");
    #endif

    char output[SUBSCRIPTIONDATALEN];
    JsonDocument doc;

    doc["tw1"] = gpio_get_level((gpio_num_t)TW1_PIN);
    doc["tw2"] = gpio_get_level((gpio_num_t)TW2_PIN);
    doc["tw3"] = gpio_get_level((gpio_num_t)TW3_PIN);

    serializeJson(doc, output, sizeof(output));
    esp_mqtt_client_publish(mqttClient, PUB_GET_TW_LIGHTS, output, 0, 0, 0);
}

//==============================================================================
// Mqtt subscribe callback functions

void getColor(SubscriptionAction_t *action)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "getColor()");
    #endif

    if (SUBSCRIPTIONDATALEN < action->dataLength) {
        #if defined(APP_DEBUG) && APP_DEBUG
            ESP_LOGI(MQTT_TAG, "data is larger than max legnth. Can not parse.");
        #endif

        return;
    }

    publishRgbStatus();
}

void setColor(SubscriptionAction_t *action)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "setColor()");
    #endif

    if (SUBSCRIPTIONDATALEN < action->dataLength) {
        #if defined(APP_DEBUG) && APP_DEBUG
            ESP_LOGI(MQTT_TAG, "data is larger than max legnth. Can not parse.");
        #endif

        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, action->data);

    if (error) {
        #if defined(APP_DEBUG) && APP_DEBUG
            printDeserializeError(&error);
        #endif

        return;
    }

    uint8_t r = doc["r"].as<uint8_t>();
    uint8_t g = doc["g"].as<uint8_t>();
    uint8_t b = doc["b"].as<uint8_t>();
    uint16_t time = doc["time"].as<uint16_t>();

    if (xSemaphoreTake(ringMutex, 0) == pdTRUE) {
        if (time) {
            ring.fadeColor(r, g, b, time);
        } else {
            ring.setColor(r, g, b);
        }
        xSemaphoreGive(ringMutex);
    } else {
        #if defined(APP_DEBUG) && APP_DEBUG
            ESP_LOGI(MQTT_TAG, "The ring is already taken. setColor()");
        #endif
    }

    publishRgbStatus();
}

void getTwinkleLights(SubscriptionAction_t *action)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "getTwinkleLights()");
    #endif

    if (SUBSCRIPTIONDATALEN < action->dataLength) {
        #if defined(APP_DEBUG) && APP_DEBUG
            ESP_LOGI(MQTT_TAG, "data is larger than max legnth. Can not parse.");
        #endif

        return;
    }

    publishTwinkleLightStatus();
}

void setTwinkleLights(SubscriptionAction_t *action)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "setTwinkleLights()");
    #endif

    if (SUBSCRIPTIONDATALEN < action->dataLength) {
        #if defined(APP_DEBUG) && APP_DEBUG
            ESP_LOGI(MQTT_TAG, "data is larger than max legnth. Can not parse.");
        #endif

        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, action->data);

    if (error) {
        #if defined(APP_DEBUG) && APP_DEBUG
            printDeserializeError(&error);
        #endif

        return;
    }

    if (doc["tw1"].is<JsonVariantConst>()) {
        uint8_t tw1 = doc["tw1"].as<uint8_t>();

        if (tw1 > 0) {
            gpio_set_level((gpio_num_t)TW1_PIN, 1);
        } else {
            gpio_set_level((gpio_num_t)TW1_PIN, 0);
        }
    }

    if (doc["tw2"].is<JsonVariantConst>()) {
        uint8_t tw2 = doc["tw2"].as<uint8_t>();

        if (tw2 > 0) {
            gpio_set_level((gpio_num_t)TW2_PIN, 1);
        } else {
            gpio_set_level((gpio_num_t)TW2_PIN, 0);
        }
    }

    if (doc["tw3"].is<JsonVariantConst>()) {
        uint8_t tw3 = doc["tw3"].as<uint8_t>();

        if (tw3 > 0) {
            gpio_set_level((gpio_num_t)TW3_PIN, 1);
        } else {
            gpio_set_level((gpio_num_t)TW3_PIN, 0);
        }
    }

    publishTwinkleLightStatus();
}

//==============================================================================
// Process Tasks

void processShortTask(void *parameter)
{
    SubscriptionAction_t action;

    while (1) {
        if (xQueueReceive(shortActionQueue, &action, 0) == pdTRUE) {
            #if defined(APP_DEBUG) && APP_DEBUG
                ESP_LOGI(MQTT_TAG, "processShortTask()");
                printSubscriptionAction(&action);
            #endif

            switch(action.callbackType) {
                case GET_COLOR:
                    getColor(&action);
                    break;
                case GET_TW:
                    getTwinkleLights(&action);
                    break;
                case SET_TW:
                    setTwinkleLights(&action);
                    break;
                default:
                    ESP_LOGW(MQTT_TAG, "processShortTask() did not call any callback");
            }

            clearAction(&action);

        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}

void processLongTask(void *parameter)
{
    SubscriptionAction_t action;

    while (1) {
        if (xQueueReceive(longActionQueue, &action, 0) == pdTRUE) {
            #if defined(APP_DEBUG) && APP_DEBUG
                ESP_LOGI(MQTT_TAG, "processLongTask()");
                printSubscriptionAction(&action);
            #endif

            switch(action.callbackType) {
                case SET_COLOR:
                    setColor(&action);
                    break;
                default:
                    ESP_LOGW(MQTT_TAG, "processLongTask() did not call any callback");
            }

            clearAction(&action);

        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

//==============================================================================
// Mqtt functions

static void mqtt_unsubscribe_all(esp_mqtt_client_handle_t client)
{
    esp_mqtt_client_unsubscribe(client, SUB_GET_COLOR);
    esp_mqtt_client_unsubscribe(client, SUB_SET_COLOR);
    esp_mqtt_client_unsubscribe(client, SUB_GET_TW_LIGHTS);
    esp_mqtt_client_unsubscribe(client, SUB_SET_TW_LIGHTS);
}

static void mqtt_subsribe_all(esp_mqtt_client_handle_t client)
{
    mqtt_unsubscribe_all(client);
    esp_mqtt_client_subscribe(client, SUB_GET_COLOR, QOS_AT_MOST_ONCE);
    esp_mqtt_client_subscribe(client, SUB_SET_COLOR, QOS_AT_MOST_ONCE);
    esp_mqtt_client_subscribe(client, SUB_GET_TW_LIGHTS, QOS_AT_MOST_ONCE);
    esp_mqtt_client_subscribe(client, SUB_SET_TW_LIGHTS, QOS_AT_MOST_ONCE);
}

static void mqtt_handle_data_event(esp_mqtt_event_handle_t event)
{
    #if defined(APP_DEBUG) && APP_DEBUG
        ESP_LOGI(MQTT_TAG, "mqtt_handle_data_event()");
        printMqttMessageData(event->topic, event->topic_len, event->data, event->data_len);
    #endif

    SubscriptionAction_t action;
    SubsctiptionCallbackType_t callbackType = getCallbackType(event);

    if (isShortTask(callbackType)) {
        setAction(&action, callbackType, event);
        if (action.callbackType != UNKNOWN && action.client != NULL) {
            xQueueSend(shortActionQueue, &action, portMAX_DELAY);
        }
    } else if (isLongTask(callbackType)) {
        setAction(&action, callbackType, event);
        if (action.callbackType != UNKNOWN && action.client != NULL) {
            xQueueSend(longActionQueue, &action, portMAX_DELAY);
        }
    } else {
        ESP_LOGI(MQTT_TAG, "Topic was unhandled");
    }
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_subsribe_all(client);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            mqtt_handle_data_event(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(MQTT_TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGI(MQTT_TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGI(MQTT_TAG, "Other event id:%d", event->event_id);
            break;
    }
}
