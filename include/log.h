#ifndef __RGB_TREE_LOG_H__
#define __RGB_TREE_LOG_H__

#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <mqtt_client.h>
#include "mqttEventProcessing.h"

#if defined(APP_DEBUG) && APP_DEBUG
void appLog();
void appLog(char *message);
void appLog(const char *message);
void appLog(const __FlashStringHelper *message);
void appLog(DeserializationError *error);
void appLog(esp_mqtt_event_handle_t event);
void appLog(SubscriptionAction_t *action);

#define APP_LOG(data) appLog(data)
#define APP_LOGF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
#define APP_LOG(data)
#define APP_LOGF(format, ...)
#endif

#if defined(APP_DEBUG) && APP_DEBUG && defined(APP_MQTT_DEBUG) && APP_MQTT_DEBUG
#define MQTT_EVENT_LOG(data) appLog(data)
#define MQTT_EVENT_LOGF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
#define MQTT_EVENT_LOG(data)
#define MQTT_EVENT_LOGF(format, ...)
#endif

#endif
