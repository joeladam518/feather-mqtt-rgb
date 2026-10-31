#ifndef __RGB_TREE_DEBUG_H__
#define __RGB_TREE_DEBUG_H__

#include "config.h"
#include "globals.h"
#include <ArduinoJson.h>
#include "MqttEventProcessing.h"

void printMqttMessageData(char *topic, uint16_t topic_length, char *data, uint16_t data_length);
void printSubscriptionAction(SubscriptionAction_t *action);
void printSubscriptionCallbackData(char *data, uint16_t length);
void printDeserializeError(DeserializationError *error);

#endif
