#pragma once

#include <stddef.h>
#include <stdint.h>
#include "led.h"

// Longest command payload we accept
#define COMMAND_PAYLOAD_MAX 256

#define TWINKLE_COUNT 3

typedef enum LightEffect {
    EFFECT_SOLID = 0,
    EFFECT_COLORLOOP = 1,
} LightEffect_t;

// Every field is optional, a message only changes what it mentions
typedef struct LightCommand {
    bool hasState;
    bool on;
    bool hasColor;
    RGB_t color;
    bool hasBrightness;
    uint8_t brightness;
    bool hasTransition;
    uint32_t transitionMs;
    bool hasEffect;
    LightEffect_t effect;
} LightCommand_t;

typedef struct TwinkleCommand {
    uint8_t index;
    bool on;
} TwinkleCommand_t;

typedef enum CommandType {
    COMMAND_LIGHT = 0,
    COMMAND_TWINKLE = 1,
} CommandType_t;

// What gets sent to the lights task, whichever protocol it arrived on
typedef struct Command {
    CommandType_t type;
    union {
        LightCommand_t light;
        TwinkleCommand_t twinkle;
    };
} Command_t;

// What the device reports back about itself
typedef struct LightStatus {
    bool on;
    RGB_t color;
    uint8_t brightness;
    LightEffect_t effect;
    bool twinkles[TWINKLE_COUNT];
} LightStatus_t;

// The payloads are not NUL terminated, so these always take a length.
// They return false, and leave the command unusable, if the payload is invalid.
bool parseLightCommand(const char *payload, size_t length, LightCommand_t *command);
bool parseOnOff(const char *payload, size_t length, bool *on);

// Returns the number of bytes written, or 0 if it didn't fit
size_t serializeLightStatus(const LightStatus_t *status, char *output, size_t size);
