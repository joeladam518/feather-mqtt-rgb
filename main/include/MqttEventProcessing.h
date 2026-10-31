#ifndef __RGB_TREE_MQTT_H__
#define __RGB_TREE_MQTT_H__

#include "config.h"
#include <mqtt_client.h>
#include <esp_log.h>

#define SUBSCRIPTIONDATALEN 100
#define READ_SUBSCRIPTION_TIMEOUT 2000

static const char *const MQTT_TAG = "MQTT_RGB_TREE";

typedef void (*SubscribeCallbackBufferType)(char *str, uint16_t len);

typedef enum MqttQos {
    QOS_AT_MOST_ONCE = 0,
    QOS_AT_LEAST_ONCE = 1,
    QOS_EXACTLY_ONCE = 2,
} MqttQos_t;

typedef enum SubsctiptionCallbackType {
    UNKNOWN = 0,
    GET_COLOR = 1,
    SET_COLOR = 2,
    GET_TW = 3,
    SET_TW = 4
} SubsctiptionCallbackType_t;

typedef struct SubscriptionAction {
    SubsctiptionCallbackType_t callbackType;
    esp_mqtt_client_handle_t client;
    char data[SUBSCRIPTIONDATALEN];
    uint16_t dataLength;
} SubscriptionAction_t;

// Subscribe callbacks
void getColor(SubscriptionAction_t *action);
void setColor(SubscriptionAction_t *action);
void getTwinkleLights(SubscriptionAction_t *action);
void setTwinkleLights(SubscriptionAction_t *action);

// Publish functions
void publishRgbStatus(void);
void publishTwinkleLightStatus(void);

// Task functions
void processShortTask(void *parameter);
void processLongTask(void *parameter);

// Mqtt functions
void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

#endif
