#pragma once

// Wifi
#define WLAN_SSID ""
#define WLAN_PASS ""

// MQTT
#define MQTT_BROKER "0.0.0.0"
#define MQTT_PORT   1883
#define MQTT_USER   "" // Leave empty if your broker doesn't need a login
#define MQTT_PASS   ""

// Device
#define DEVICE_ID   "rgb_tree"   // Letters, numbers, and underscores. Must be unique on your broker.
#define DEVICE_NAME "RGB Tree"   // What Home Assistant calls it
#define TOPIC_BASE  "rgb_tree"   // Every topic starts with this

// pins
#define TW1_PIN 33
#define TW2_PIN 15
#define TW3_PIN 32
#define NEO_PIXEL_PIN 14
#define NEO_PIXEL_COUNT 16

// How long the color loop takes to go all the way around
#define LOOP_CYCLE_MS 30000
