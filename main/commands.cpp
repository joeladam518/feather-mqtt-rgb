// commands.cpp: turns message payloads into commands, and status into JSON.
//
// The rule in here is to check everything and trust nothing. A payload comes from the
// network, so it could be anything at all. Parsing happens in one place, and everything
// past this point can rely on a command being valid. A message with even one bad value
// gets rejected as a whole, so a half applied command can't happen.
//
// The JSON format matches Home Assistant's MQTT JSON light.

#include "commands.h"

#include <string.h>

// ArduinoJson only has "Arduino" in its name. It's a plain C++ library, and this
// firmware doesn't use the Arduino framework.
#include <ArduinoJson.h>

// One hour, which also keeps the fade math in LightState inside 32 bits
#define TRANSITION_MAX_SECONDS 3600.0f

// JsonVariantConst is ArduinoJson's type for "a JSON value that I'm only reading",
// which could turn out to be a string, a number, an object, or anything else.

// Reads "ON" or "OFF" out of a JSON value
static bool parseState(JsonVariantConst value, bool *on)
{
    // Gives NULL if the value isn't a string, like {"state":1}
    const char *state = value.as<const char *>();
    if (state == NULL) {
        return false;
    }

    return parseOnOff(state, strlen(state), on);
}

// Reads {"r":1,"g":2,"b":3}. All three are required, because a color with a channel
// missing is more likely to be a mistake than something that was meant.
static bool parseColor(JsonVariantConst value, RGB_t *color)
{
    JsonVariantConst r = value["r"];
    JsonVariantConst g = value["g"];
    JsonVariantConst b = value["b"];

    // is<uint8_t>() is only true for a number that fits in 0 to 255, so this
    // also rejects 300, -1, strings, and missing keys
    if (!r.is<uint8_t>() || !g.is<uint8_t>() || !b.is<uint8_t>()) {
        return false;
    }

    color->r = r.as<uint8_t>();
    color->g = g.as<uint8_t>();
    color->b = b.as<uint8_t>();

    return true;
}

// Home Assistant sends transition in seconds, but everything inside the firmware works
// in milliseconds, so it's converted here at the edge.
static bool parseTransition(JsonVariantConst value, uint32_t *transitionMs)
{
    if (!value.is<float>()) {
        return false;
    }

    float seconds = value.as<float>();
    if (seconds < 0.0f || seconds > TRANSITION_MAX_SECONDS) {
        return false;
    }

    *transitionMs = (uint32_t)(seconds * 1000.0f);

    return true;
}

// The names here have to match effect_list in discovery.cpp
static bool parseEffect(JsonVariantConst value, LightEffect_t *effect)
{
    const char *name = value.as<const char *>();
    if (name == NULL) {
        return false;
    }

    // strcmp returns 0 when two strings are the same
    if (strcmp(name, "colorloop") == 0) {
        *effect = EFFECT_COLORLOOP;
        return true;
    }

    if (strcmp(name, "solid") == 0) {
        *effect = EFFECT_SOLID;
        return true;
    }

    return false;
}

bool parseLightCommand(const char *payload, size_t length, LightCommand_t *command)
{
    if (length > COMMAND_PAYLOAD_MAX) {
        return false;
    }

    // Passing the length means the parser can't read past the end of the payload.
    // deserializeJson returns an error that counts as true if something went wrong.
    // The second check rejects valid JSON that isn't an object, like [1,2,3].
    JsonDocument doc;
    if (deserializeJson(doc, payload, length) || !doc.is<JsonObjectConst>()) {
        return false;
    }

    // Start with every has... flag false, then turn them on as keys are found
    memset(command, 0, sizeof(*command));
    JsonObjectConst root = doc.as<JsonObjectConst>();

    // is<JsonVariantConst>() is how you ask ArduinoJson "is this key in the message at
    // all?". So a key that's there with a bad value (like "state":null) still counts as
    // present, and gets rejected, instead of being quietly treated as missing.
    if (root["state"].is<JsonVariantConst>()) {
        command->hasState = true;
        if (!parseState(root["state"], &command->on)) {
            return false;
        }
    }

    if (root["color"].is<JsonVariantConst>()) {
        command->hasColor = true;
        if (!parseColor(root["color"], &command->color)) {
            return false;
        }
    }

    if (root["brightness"].is<JsonVariantConst>()) {
        if (!root["brightness"].is<uint8_t>()) {
            return false;
        }
        command->hasBrightness = true;
        command->brightness = root["brightness"].as<uint8_t>();
    }

    if (root["transition"].is<JsonVariantConst>()) {
        command->hasTransition = true;
        if (!parseTransition(root["transition"], &command->transitionMs)) {
            return false;
        }
    }

    if (root["effect"].is<JsonVariantConst>()) {
        command->hasEffect = true;
        if (!parseEffect(root["effect"], &command->effect)) {
            return false;
        }
    }

    return true;
}

// Reads the plain ON or OFF payload that the LED outputs use.
//
// strncmp only compares the first n bytes, which is safe here because the payload has no
// NUL terminator. Checking the length first is what makes "ONE" fail instead of matching.
bool parseOnOff(const char *payload, size_t length, bool *on)
{
    if (length == 2 && strncmp(payload, "ON", 2) == 0) {
        *on = true;
        return true;
    }

    if (length == 3 && strncmp(payload, "OFF", 3) == 0) {
        *on = false;
        return true;
    }

    return false;
}

// Builds what gets published to the light's state topic
size_t serializeLightStatus(const LightStatus_t *status, char *output, size_t size)
{
    JsonDocument doc;

    // -> reads a field through a pointer. It's the same as (*status).on.
    doc["state"] = status->on ? "ON" : "OFF";
    doc["color_mode"] = "rgb";
    doc["brightness"] = status->brightness;
    doc["effect"] = status->effect == EFFECT_COLORLOOP ? "colorloop" : "solid";

    // Creates the nested {"color":{...}} object
    JsonObject color = doc["color"].to<JsonObject>();
    color["r"] = status->color.r;
    color["g"] = status->color.g;
    color["b"] = status->color.b;

    // >= instead of > because the text also needs one extra byte for its NUL terminator.
    // Returning 0 is better than sending JSON that got cut off halfway through.
    if (measureJson(doc) >= size) {
        return 0;
    }

    return serializeJson(doc, output, size);
}
