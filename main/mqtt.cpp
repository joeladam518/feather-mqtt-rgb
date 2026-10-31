#include "config.h"
#include "mqtt.h"

#include <errno.h>
#include <string.h>

#include <esp_log.h>
#include <mqtt_client.h>

#include "commands.h"
#include "discovery.h"
#include "lights.h"
#include "topics.h"

#define QOS_AT_MOST_ONCE 0
#define QOS_AT_LEAST_ONCE 1

#define RETAIN 1
#define NO_RETAIN 0

// How quickly the broker notices we're gone and announces it for us
#define KEEPALIVE_SECONDS 30

#define INCOMING_BUFFER_SIZE 2048
#define OUTGOING_BUFFER_SIZE (DISCOVERY_PAYLOAD_MAX + 512)

#define STATUS_PAYLOAD_MAX 192

static const char *TAG = "MQTT";

static const char *const PAYLOAD_ONLINE = "online";
static const char *const PAYLOAD_OFFLINE = "offline";

static esp_mqtt_client_handle_t client = NULL;

// Only the status task publishes this. It's too big to want on a task's stack.
static char discoveryPayload[DISCOVERY_PAYLOAD_MAX];

//==============================================================================
// Incoming messages

static void handleLightCommand(const char *payload, size_t length)
{
    Command_t command = {};
    command.type = COMMAND_LIGHT;

    if (!parseLightCommand(payload, length, &command.light)) {
        ESP_LOGW(TAG, "Ignoring invalid light command: %.*s", (int)length, payload);
        return;
    }

    lightsSend(&command);
}

static void handleTwinkleCommand(uint8_t index, const char *payload, size_t length)
{
    Command_t command = {};
    command.type = COMMAND_TWINKLE;
    command.twinkle.index = index;

    if (!parseOnOff(payload, length, &command.twinkle.on)) {
        ESP_LOGW(TAG, "Ignoring invalid twinkle command: %.*s", (int)length, payload);
        return;
    }

    lightsSend(&command);
}

static void handleHomeAssistantStatus(const char *payload, size_t length)
{
    if (topicEquals(PAYLOAD_ONLINE, payload, length)) {
        lightsRequestAnnouncement(true);
    }
}

static void handleData(esp_mqtt_event_handle_t event)
{
    // A message too big for the buffer arrives in pieces. Nothing we accept is that big.
    if (event->current_data_offset != 0 || event->data_len != event->total_data_len) {
        ESP_LOGW(TAG, "Ignoring a fragmented message");
        return;
    }

    ESP_LOGD(TAG, "%.*s: %.*s", event->topic_len, event->topic, event->data_len, event->data);

    if (topicEquals(TOPIC_LIGHT_SET, event->topic, event->topic_len)) {
        handleLightCommand(event->data, event->data_len);
        return;
    }

    for (uint8_t i = 0; i < TWINKLE_COUNT; i++) {
        if (topicEquals(TOPIC_TWINKLE_SET[i], event->topic, event->topic_len)) {
            handleTwinkleCommand(i, event->data, event->data_len);
            return;
        }
    }

    if (topicEquals(HA_STATUS_TOPIC, event->topic, event->topic_len)) {
        handleHomeAssistantStatus(event->data, event->data_len);
        return;
    }

    ESP_LOGW(TAG, "Unhandled topic: %.*s", event->topic_len, event->topic);
}

static void subscribeAll(void)
{
    esp_mqtt_client_subscribe(client, TOPIC_LIGHT_SET, QOS_AT_LEAST_ONCE);
    esp_mqtt_client_subscribe(client, HA_STATUS_TOPIC, QOS_AT_LEAST_ONCE);

    for (uint8_t i = 0; i < TWINKLE_COUNT; i++) {
        esp_mqtt_client_subscribe(client, TOPIC_TWINKLE_SET[i], QOS_AT_LEAST_ONCE);
    }
}

// This runs on the mqtt task, so nothing in here is allowed to wait. If it blocks,
// keepalives stop and the broker disconnects us.
static void eventHandler(void *handlerArgs, esp_event_base_t base, int32_t eventId, void *eventData)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;

    switch ((esp_mqtt_event_id_t)eventId) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected");
            subscribeAll();
            lightsRequestAnnouncement(false);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected");
            break;
        case MQTT_EVENT_DATA:
            handleData(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
        default:
            ESP_LOGD(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

//==============================================================================
// Public

void mqttStart(void)
{
    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = MQTT_BROKER;
    config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    config.broker.address.port = MQTT_PORT;
    config.credentials.client_id = DEVICE_ID;
    config.session.keepalive = KEEPALIVE_SECONDS;
    config.session.last_will.topic = TOPIC_AVAILABILITY;
    config.session.last_will.msg = PAYLOAD_OFFLINE;
    config.session.last_will.qos = QOS_AT_LEAST_ONCE;
    config.session.last_will.retain = RETAIN;
    config.task.priority = 5;
    config.task.stack_size = 6144;
    config.buffer.size = INCOMING_BUFFER_SIZE;
    config.buffer.out_size = OUTGOING_BUFFER_SIZE;

    // Leave these empty in config.h for a broker without a login
    if (strlen(MQTT_USER) > 0) {
        config.credentials.username = MQTT_USER;
        config.credentials.authentication.password = MQTT_PASS;
    }

    client = esp_mqtt_client_init(&config);
    configASSERT(client);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, eventHandler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));
}

void mqttPublishStatus(const LightStatus_t *status)
{
    char payload[STATUS_PAYLOAD_MAX];

    if (serializeLightStatus(status, payload, sizeof(payload)) > 0) {
        esp_mqtt_client_publish(client, TOPIC_LIGHT_STATE, payload, 0, QOS_AT_MOST_ONCE, RETAIN);
    } else {
        ESP_LOGE(TAG, "Status didn't fit in its buffer");
    }

    for (uint8_t i = 0; i < TWINKLE_COUNT; i++) {
        const char *state = status->twinkles[i] ? "ON" : "OFF";
        esp_mqtt_client_publish(client, TOPIC_TWINKLE_STATE[i], state, 0, QOS_AT_MOST_ONCE, RETAIN);
    }
}

void mqttPublishAnnouncement(void)
{
    esp_mqtt_client_publish(client, TOPIC_AVAILABILITY, PAYLOAD_ONLINE, 0, QOS_AT_LEAST_ONCE, RETAIN);

    if (serializeDiscovery(discoveryPayload, sizeof(discoveryPayload)) > 0) {
        esp_mqtt_client_publish(client, HA_DISCOVERY_TOPIC, discoveryPayload, 0, QOS_AT_LEAST_ONCE, NO_RETAIN);
    } else {
        ESP_LOGE(TAG, "Discovery didn't fit in its buffer");
    }
}
