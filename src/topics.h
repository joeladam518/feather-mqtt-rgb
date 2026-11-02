// topics.h: every MQTT topic the device uses, in one place.

#pragma once

#include <stddef.h>
#include <string.h>
#include "config.h"

// Two strings written next to each other get joined by the compiler, so with a TOPIC_BASE
// of "rgb_tree", TOPIC_LIGHT_SET becomes "rgb_tree/light/set". All of this happens
// at compile time, so there's no string building while the firmware runs.
#define TOPIC_AVAILABILITY  TOPIC_BASE "/availability"
#define TOPIC_LIGHT_SET     TOPIC_BASE "/light/set"
#define TOPIC_LIGHT_STATE   TOPIC_BASE "/light/state"
#define TOPIC_TWINKLE_SET   TOPIC_BASE "/twinkle/set"
#define TOPIC_TWINKLE_STATE TOPIC_BASE "/twinkle/state"

// These two are decided by Home Assistant, so they don't use TOPIC_BASE
#define HA_DISCOVERY_TOPIC "homeassistant/device/" DEVICE_ID "/config"
#define HA_STATUS_TOPIC    "homeassistant/status"

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
