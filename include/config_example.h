#ifndef __RGB_TREE_CONFIG_H__
#define __RGB_TREE_CONFIG_H__

#include <soc/soc.h>

// Debug
#define DEBUG          false // This will trigger other library's debug functionality
#define APP_DEBUG      false
#define APP_MQTT_DEBUG false

// Wifi
#define WLAN_SSID ""
#define WLAN_PASS ""

// MQTT
#define MQTT_URI  "mqtt://0.0.0.0:1883" // Use 8883 for SSL
#define MQTT_USER ""
#define MQTT_PASS ""

// Subscription Topics
#define SUB_GET_COLOR     ""
#define SUB_SET_COLOR     ""
#define SUB_GET_TW_LIGHTS ""
#define SUB_SET_TW_LIGHTS ""

// Publish Topics
#define PUB_GET_COLOR     ""
#define PUB_GET_TW_LIGHTS ""

// pins
#define TW1_PIN 33
#define TW2_PIN 15
#define TW3_PIN 32
#define NEO_PIXEL_PIN 14
#define NEO_PIXEL_COUNT 16

// Mqtt overrides
// #define MQTT_CORE_SELECTION_ENABLED 0
// #define MQTT_TASK_CORE PRO_CPU_NUM

#endif
