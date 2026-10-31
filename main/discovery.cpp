#include "config.h"
#include "discovery.h"

#include <ArduinoJson.h>

#include "commands.h"
#include "topics.h"

static const char *const TWINKLE_KEYS[TWINKLE_COUNT] = {"tw1", "tw2", "tw3"};
static const char *const TWINKLE_NAMES[TWINKLE_COUNT] = {"Twinkle 1", "Twinkle 2", "Twinkle 3"};
static const char *const TWINKLE_IDS[TWINKLE_COUNT] = {DEVICE_ID "_tw1", DEVICE_ID "_tw2", DEVICE_ID "_tw3"};

// See https://www.home-assistant.io/integrations/mqtt/ (device discovery)
size_t serializeDiscovery(char *output, size_t size)
{
    JsonDocument doc;

    JsonObject device = doc["dev"].to<JsonObject>();
    device["ids"] = DEVICE_ID;
    device["name"] = DEVICE_NAME;
    device["mf"] = "Adafruit";
    device["mdl"] = "HUZZAH32 ESP32 Feather";

    JsonObject origin = doc["o"].to<JsonObject>();
    origin["name"] = "feather-mqtt-rgb-tree";
    origin["url"] = "https://github.com/joeladam518/feather-mqtt-rgb-tree";

    JsonObject components = doc["cmps"].to<JsonObject>();

    JsonObject light = components["ring"].to<JsonObject>();
    light["p"] = "light";
    light["name"] = "Ring";
    light["unique_id"] = DEVICE_ID "_ring";
    light["schema"] = "json";
    light["command_topic"] = TOPIC_LIGHT_SET;
    light["state_topic"] = TOPIC_LIGHT_STATE;
    light["availability_topic"] = TOPIC_AVAILABILITY;
    light["brightness"] = true;
    light["flash"] = false;
    light["effect"] = true;
    light["supported_color_modes"].to<JsonArray>().add("rgb");

    JsonArray effects = light["effect_list"].to<JsonArray>();
    effects.add("colorloop");
    effects.add("solid");

    for (uint8_t i = 0; i < TWINKLE_COUNT; i++) {
        JsonObject twinkle = components[TWINKLE_KEYS[i]].to<JsonObject>();
        twinkle["p"] = "switch";
        twinkle["name"] = TWINKLE_NAMES[i];
        twinkle["unique_id"] = TWINKLE_IDS[i];
        twinkle["command_topic"] = TOPIC_TWINKLE_SET[i];
        twinkle["state_topic"] = TOPIC_TWINKLE_STATE[i];
        twinkle["availability_topic"] = TOPIC_AVAILABILITY;
    }

    if (measureJson(doc) >= size) {
        return 0;
    }

    return serializeJson(doc, output, size);
}
