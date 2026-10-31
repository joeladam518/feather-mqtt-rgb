// config_example.h: a template for your settings.
//
// Copy this to main/include/config.h and fill it in. config.h is gitignored, so your
// passwords never get committed, and this file stays safe to share. These are #defines
// because they're fixed when the firmware is built. Changing one means building and
// flashing again.

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
#define TWINKLE_PIN 33
#define NEO_PIXEL_PIN 14
#define NEO_PIXEL_COUNT 16

// Switches the power to the NeoPixel on the board itself. Its data line is GPIO 0.
// This also powers the STEMMA QT connector, so the two can only go on and off together.
#define BOARD_PIXEL_POWER_PIN 2

// How long the color loop takes to go all the way around
#define LOOP_CYCLE_MS 30000
