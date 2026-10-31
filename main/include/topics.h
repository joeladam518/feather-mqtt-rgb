// topics.h: every MQTT topic the device uses, in one place.

#pragma once

#include <stddef.h>
#include <string.h>
#include "config.h"
#include "commands.h"

// Two strings written next to each other get joined by the compiler, so with a TOPIC_BASE
// of "rgb_tree", TOPIC_LIGHT_SET becomes "rgb_tree/light/set". All of this happens
// at compile time, so there's no string building while the firmware runs.
#define TOPIC_AVAILABILITY TOPIC_BASE "/availability"
#define TOPIC_LIGHT_SET    TOPIC_BASE "/light/set"
#define TOPIC_LIGHT_STATE  TOPIC_BASE "/light/state"

// These two are decided by Home Assistant, so they don't use TOPIC_BASE
#define HA_DISCOVERY_TOPIC "homeassistant/device/" DEVICE_ID "/config"
#define HA_STATUS_TOPIC    "homeassistant/status"

// Arrays, so that code can go from a twinkle's index straight to its topic.
// Index 0 is tw1, index 1 is tw2, and index 2 is tw3.
static const char *const TOPIC_TWINKLE_SET[TWINKLE_COUNT] = {
    TOPIC_BASE "/tw1/set",
    TOPIC_BASE "/tw2/set",
    TOPIC_BASE "/tw3/set",
};

static const char *const TOPIC_TWINKLE_STATE[TWINKLE_COUNT] = {
    TOPIC_BASE "/tw1/state",
    TOPIC_BASE "/tw2/state",
    TOPIC_BASE "/tw3/state",
};

// Incoming topics are not NUL terminated, and comparing the lengths is what
// stops a topic matching one that it's only the start of.
//
// The old code only compared as many characters as the incoming topic had, so "tree/co"
// would have matched "tree/color". A length of zero would have matched everything.
//
// "inline" is here because this function is defined in a header, which gets compiled into
// several files. size_t is the type C++ uses for sizes and lengths.
static inline bool topicEquals(const char *expected, const char *topic, size_t length)
{
    return strlen(expected) == length && strncmp(expected, topic, length) == 0;
}
