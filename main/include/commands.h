// commands.h: the data that gets passed around the firmware.
//
// This file has nothing to do with MQTT, on purpose. A network message gets turned into
// one of these structs as soon as it arrives, and the rest of the code only ever sees the
// struct. So adding another way to control the device (like CoAP) only means writing code
// that fills in the same struct.

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "led.h"

// Longest command payload we accept
#define COMMAND_PAYLOAD_MAX 256

// typedef gives a type a shorter name. The _t ending is a C naming habit that means
// "this is a type". An enum is a set of named numbers, so the code can say EFFECT_SOLID
// instead of a mystery 0.
typedef enum LightEffect {
    EFFECT_SOLID = 0,
    EFFECT_COLORLOOP = 1,
} LightEffect_t;

// Every field is optional, a message only changes what it mentions.
//
// That's what the has... flags are for. Without them there'd be no way to tell "brightness
// wasn't in the message" from "brightness was 0". The value next to each flag only means
// something when its flag is true.
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

// Switches the twinkle lights on or off
typedef struct TwinkleCommand {
    bool on;
} TwinkleCommand_t;

typedef enum CommandType {
    COMMAND_LIGHT = 0,
    COMMAND_TWINKLE = 1,
} CommandType_t;

// What gets sent to the lights task, whichever protocol it arrived on
typedef struct Command {
    // Says which part of the union below is the real one
    CommandType_t type;

    // A union is one piece of memory that can be used as any one of these types, but only
    // one at a time. A command is either a light command or a twinkle command, never both,
    // so there's no need to reserve room for both. Reading the one that wasn't written
    // gives garbage, which is why the type field above must always be checked first.
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
    bool twinkle;
} LightStatus_t;

// The payloads are not NUL terminated, so these always take a length.
// They return false, and leave the command unusable, if the payload is invalid.
//
// C strings normally end with a zero byte (the NUL terminator), and functions like strlen
// look for it to find the end. MQTT payloads don't have one, so reading them without
// the length would run off the end into whatever is next in memory.
//
// *command is a pointer. It's how these functions hand back more than one thing: the
// return value says whether it worked, and the result gets written to where it points.
bool parseLightCommand(const char *payload, size_t length, LightCommand_t *command);
bool parseOnOff(const char *payload, size_t length, bool *on);

// Returns the number of bytes written, or 0 if it didn't fit
size_t serializeLightStatus(const LightStatus_t *status, char *output, size_t size);
