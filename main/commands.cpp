#include "commands.h"

#include <string.h>

#include <ArduinoJson.h>

// One hour, which also keeps the fade math in LightState inside 32 bits
#define TRANSITION_MAX_SECONDS 3600.0f

static bool parseState(JsonVariantConst value, bool *on)
{
    const char *state = value.as<const char *>();
    if (state == NULL) {
        return false;
    }

    return parseOnOff(state, strlen(state), on);
}

static bool parseColor(JsonVariantConst value, RGB_t *color)
{
    JsonVariantConst r = value["r"];
    JsonVariantConst g = value["g"];
    JsonVariantConst b = value["b"];

    if (!r.is<uint8_t>() || !g.is<uint8_t>() || !b.is<uint8_t>()) {
        return false;
    }

    color->r = r.as<uint8_t>();
    color->g = g.as<uint8_t>();
    color->b = b.as<uint8_t>();

    return true;
}

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

static bool parseEffect(JsonVariantConst value, LightEffect_t *effect)
{
    const char *name = value.as<const char *>();
    if (name == NULL) {
        return false;
    }

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

    JsonDocument doc;
    if (deserializeJson(doc, payload, length) || !doc.is<JsonObjectConst>()) {
        return false;
    }

    memset(command, 0, sizeof(*command));
    JsonObjectConst root = doc.as<JsonObjectConst>();

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

size_t serializeLightStatus(const LightStatus_t *status, char *output, size_t size)
{
    JsonDocument doc;

    doc["state"] = status->on ? "ON" : "OFF";
    doc["color_mode"] = "rgb";
    doc["brightness"] = status->brightness;
    doc["effect"] = status->effect == EFFECT_COLORLOOP ? "colorloop" : "solid";

    JsonObject color = doc["color"].to<JsonObject>();
    color["r"] = status->color.r;
    color["g"] = status->color.g;
    color["b"] = status->color.b;

    if (measureJson(doc) >= size) {
        return 0;
    }

    return serializeJson(doc, output, size);
}
