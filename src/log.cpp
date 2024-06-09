#include "config.h"
#include <Arduino.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include "log.h"
#include "mqttEventProcessing.h"

#if defined(APP_DEBUG) && APP_DEBUG
void appLog() {
    Serial.println();
}

void appLog(char *message) {
    Serial.println(message);
}

void appLog(const char *message) {
    Serial.println(message);
}

void appLog(const __FlashStringHelper *message) {
    Serial.println(message);
}

void appLog(DeserializationError *error)
{
    Serial.println(F("[json deserialization error]"));
    Serial.println(error->c_str());
}

void appLog(esp_mqtt_event_handle_t event)
{
    Serial.println(F("[mqtt event]"));
    Serial.printf("  topic: %.*s\n", event->topic_len, event->topic);
    Serial.printf("  topic len: %i\n", event->topic_len);
    Serial.printf("  data: %.*s\n", event->data_len, event->data);
    Serial.printf("  data len: %i\n", event->data_len);
}

void appLog(SubscriptionAction_t *action)
{
    Serial.println(F("[action]"));
    Serial.printf("  type: %i\n", action->type);
    Serial.printf("  data: %.*s\n", action->dataLength, action->data);
    Serial.printf("  data len: %i\n", action->dataLength);
}
#endif
