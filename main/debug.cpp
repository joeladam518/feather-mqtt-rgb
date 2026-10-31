#include "config.h"
#include "debug.h"
#include <ArduinoJson.h>
#include <esp_log.h>
#include "MqttEventProcessing.h"

void printDeserializeError(DeserializationError *error)
{
    ESP_LOGE(MQTT_TAG, "Arduino Json eserialization error: %s", error->c_str());
}

void printMqttMessageData(char *topic, uint16_t topic_length, char *data, uint16_t data_length)
{
    ESP_LOGI(MQTT_TAG, "Topic: %.*s", topic_length, topic);
    ESP_LOGI(MQTT_TAG, "Topic Length: %u", topic_length);
    ESP_LOGI(MQTT_TAG, "Data: %.*s", data_length, data);
    ESP_LOGI(MQTT_TAG, "Data Length: %u", data_length);
}

void printSubscriptionAction(SubscriptionAction_t *action)
{
    ESP_LOGI(MQTT_TAG, "Callback Type: %d", action->callbackType);
    ESP_LOGI(MQTT_TAG, "Data: %.*s", action->dataLength, action->data);
    ESP_LOGI(MQTT_TAG, "Length: %u", action->dataLength);
}

void printSubscriptionCallbackData(char *data, uint16_t length)
{
    ESP_LOGI(MQTT_TAG, "Data: %.*s", length, data);
    ESP_LOGI(MQTT_TAG, "Length: %u", length);
}
