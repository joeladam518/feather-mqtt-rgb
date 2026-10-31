#pragma once

#include <stddef.h>
#include <string.h>
#include "config.h"
#include "commands.h"

#define TOPIC_AVAILABILITY TOPIC_BASE "/availability"
#define TOPIC_LIGHT_SET    TOPIC_BASE "/light/set"
#define TOPIC_LIGHT_STATE  TOPIC_BASE "/light/state"

#define HA_DISCOVERY_TOPIC "homeassistant/device/" DEVICE_ID "/config"
#define HA_STATUS_TOPIC    "homeassistant/status"

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
static inline bool topicEquals(const char *expected, const char *topic, size_t length)
{
    return strlen(expected) == length && strncmp(expected, topic, length) == 0;
}
