// Tests the logic that doesn't need a board. Run with scripts/test-host.sh

#include <stdio.h>
#include <string.h>

#include <ArduinoJson.h>

#include "commands.h"
#include "config.h"
#include "discovery.h"
#include "LightState.h"
#include "topics.h"

static int failures = 0;
static int checks = 0;

#define CHECK(condition) do { \
    checks++; \
    if (!(condition)) { \
        failures++; \
        printf("FAILED %s:%d: %s\n", __func__, __LINE__, #condition); \
    } \
} while (0)

//==============================================================================
// Helpers

static bool same(RGB_t a, RGB_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static LightCommand_t parse(const char *json)
{
    LightCommand_t command = {};
    bool ok = parseLightCommand(json, strlen(json), &command);
    CHECK(ok);
    return command;
}

static bool rejects(const char *json)
{
    LightCommand_t command = {};
    return !parseLightCommand(json, strlen(json), &command);
}

static void send(LightState &state, const char *json, uint32_t nowMs)
{
    state.apply(parse(json), nowMs);
}

static const RGB_t RED = {255, 0, 0};
static const RGB_t BLUE = {0, 0, 255};
static const RGB_t BLACK = {0, 0, 0};

static const char *SET_RED = "{\"color\":{\"r\":255,\"g\":0,\"b\":0}}";
static const char *SET_BLUE = "{\"color\":{\"r\":0,\"g\":0,\"b\":255}}";
static const char *START_LOOP = "{\"effect\":\"colorloop\"}";

//==============================================================================
// The three things the device has to do

static void test_starts_off(void)
{
    LightState state;
    CHECK(same(state.render(0), BLACK));
    CHECK(!state.status().on);
    CHECK(!state.isAnimating());
}

static void test_color_shows_immediately_without_transition(void)
{
    LightState state;
    send(state, SET_RED, 100);
    CHECK(same(state.render(100), RED));
    CHECK(state.status().on);
    CHECK(!state.isAnimating());
}

static void test_loop_keeps_changing_color(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    CHECK(state.isAnimating());

    // Past the blend in, a third of the way around the wheel is a different color
    RGB_t first = state.render(2000);
    RGB_t second = state.render(12000);
    CHECK(!same(first, second));

    // And it never stops on its own
    state.render(10 * LOOP_CYCLE_MS);
    CHECK(state.isAnimating());
}

static void test_loop_comes_back_around(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    CHECK(same(state.render(2000), state.render(2000 + LOOP_CYCLE_MS)));
}

static void test_color_breaks_the_loop(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    state.render(5000);

    send(state, SET_BLUE, 5020);
    CHECK(same(state.render(5020), BLUE));
    CHECK(!state.isAnimating());
    CHECK(state.status().effect == EFFECT_SOLID);

    // It stays that way
    CHECK(same(state.render(60000), BLUE));
}

static void test_color_with_transition_breaks_the_loop_smoothly(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    RGB_t before = state.render(5000);

    send(state, "{\"color\":{\"r\":0,\"g\":0,\"b\":255},\"transition\":1}", 5000);
    CHECK(same(state.render(5000), before));
    CHECK(state.status().effect == EFFECT_SOLID);
    CHECK(same(state.render(6000), BLUE));
    CHECK(!state.isAnimating());
}

static void test_twinkles_leave_the_loop_alone(void)
{
    LightState withTwinkles;
    LightState without;
    send(withTwinkles, START_LOOP, 0);
    send(without, START_LOOP, 0);

    for (uint32_t now = 0; now < 60000; now += FRAME_MS) {
        if (now % 1000 == 0) {
            withTwinkles.setTwinkle((now / 1000) % TWINKLE_COUNT, (now / 1000) % 2 == 0);
        }
        CHECK(same(withTwinkles.render(now), without.render(now)));
    }

    CHECK(withTwinkles.isAnimating());
    CHECK(withTwinkles.status().effect == EFFECT_COLORLOOP);
}

static void test_twinkles_are_independent(void)
{
    LightState state;
    state.setTwinkle(1, true);
    CHECK(!state.status().twinkles[0]);
    CHECK(state.status().twinkles[1]);
    CHECK(!state.status().twinkles[2]);

    send(state, SET_RED, 0);
    send(state, "{\"state\":\"OFF\"}", 10);
    CHECK(state.status().twinkles[1]);

    state.setTwinkle(1, false);
    CHECK(!state.status().twinkles[1]);
}

//==============================================================================
// Fades

static void test_fade_reaches_the_halfway_point(void)
{
    LightState state;
    send(state, "{\"color\":{\"r\":200,\"g\":0,\"b\":0},\"transition\":2}", 0);
    CHECK(state.isAnimating());
    CHECK(state.render(1000).r == 100);
    CHECK(state.render(2000).r == 200);
    CHECK(!state.isAnimating());
}

static void test_fade_length_survives_late_frames(void)
{
    LightState state;
    send(state, "{\"color\":{\"r\":255,\"g\":0,\"b\":0},\"transition\":1}", 0);

    // One frame arrives very late, and the fade still ends on time
    state.render(20);
    CHECK(same(state.render(1500), RED));
    CHECK(!state.isAnimating());
}

static void test_new_color_during_a_fade_starts_from_what_is_showing(void)
{
    LightState state;
    send(state, "{\"color\":{\"r\":200,\"g\":0,\"b\":0},\"transition\":2}", 0);

    send(state, "{\"color\":{\"r\":0,\"g\":0,\"b\":200},\"transition\":2}", 1000);
    RGB_t start = state.render(1000);
    CHECK(start.r == 100);
    CHECK(start.b == 0);
    CHECK(same(state.render(3000), RGB_t{0, 0, 200}));
}

static void test_starting_the_loop_does_not_jump(void)
{
    LightState state;
    send(state, SET_BLUE, 0);
    send(state, START_LOOP, 100);
    CHECK(same(state.render(100), BLUE));
}

//==============================================================================
// On, off, and brightness

static void test_off_then_on_restores_color(void)
{
    LightState state;
    send(state, SET_RED, 0);
    send(state, "{\"state\":\"OFF\"}", 100);
    CHECK(same(state.render(100), BLACK));
    CHECK(same(state.status().color, RED));

    send(state, "{\"state\":\"ON\"}", 200);
    CHECK(same(state.render(200), RED));
}

static void test_off_then_on_restores_the_loop(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    send(state, "{\"state\":\"OFF\"}", 5000);
    CHECK(same(state.render(5000), BLACK));
    CHECK(!state.isAnimating());
    CHECK(state.status().effect == EFFECT_COLORLOOP);

    send(state, "{\"state\":\"ON\"}", 6000);
    CHECK(state.isAnimating());
    CHECK(state.status().on);
}

static void test_effect_turns_the_ring_on(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    CHECK(state.status().on);
}

static void test_solid_stops_the_loop_and_keeps_the_color(void)
{
    LightState state;
    send(state, SET_RED, 0);
    send(state, START_LOOP, 100);
    send(state, "{\"effect\":\"solid\"}", 5000);
    CHECK(!state.isAnimating());
    CHECK(same(state.render(5000), RED));
}

static void test_brightness_keeps_the_loop_running(void)
{
    LightState state;
    send(state, START_LOOP, 0);
    send(state, "{\"brightness\":100}", 5000);
    CHECK(state.isAnimating());
    CHECK(state.status().effect == EFFECT_COLORLOOP);

    RGB_t frame = state.render(10000);
    CHECK(frame.r <= 100 && frame.g <= 100 && frame.b <= 100);
}

static void test_brightness_scales_color(void)
{
    LightState state;
    send(state, SET_RED, 0);
    send(state, "{\"brightness\":128}", 100);
    CHECK(same(state.render(100), RGB_t{128, 0, 0}));
    CHECK(same(state.status().color, RED));
}

static void test_brightness_does_not_turn_the_ring_on(void)
{
    LightState state;
    send(state, "{\"brightness\":128}", 0);
    CHECK(!state.status().on);
}

static void test_clock_rollover(void)
{
    LightState state;
    uint32_t start = 0xFFFFFF00;
    send(state, "{\"color\":{\"r\":200,\"g\":0,\"b\":0},\"transition\":1}", start);
    CHECK(state.render(start + 500).r == 100);
    CHECK(state.render(start + 1000).r == 200);
}

//==============================================================================
// Parsing

static void test_parses_a_full_home_assistant_command(void)
{
    LightCommand_t command = parse(
        "{\"state\":\"ON\",\"color\":{\"r\":1,\"g\":2,\"b\":3},"
        "\"brightness\":4,\"transition\":2.5,\"effect\":\"colorloop\"}"
    );

    CHECK(command.hasState && command.on);
    CHECK(command.hasColor && same(command.color, RGB_t{1, 2, 3}));
    CHECK(command.hasBrightness && command.brightness == 4);
    CHECK(command.hasTransition && command.transitionMs == 2500);
    CHECK(command.hasEffect && command.effect == EFFECT_COLORLOOP);
}

static void test_missing_keys_are_not_set(void)
{
    LightCommand_t command = parse("{}");
    CHECK(!command.hasState);
    CHECK(!command.hasColor);
    CHECK(!command.hasBrightness);
    CHECK(!command.hasTransition);
    CHECK(!command.hasEffect);
}

static void test_rejects_bad_payloads(void)
{
    CHECK(rejects(""));
    CHECK(rejects("not json"));
    CHECK(rejects("{\"state\":\"ON\""));
    CHECK(rejects("[1,2,3]"));
    CHECK(rejects("{\"state\":\"MAYBE\"}"));
    CHECK(rejects("{\"state\":1}"));
    CHECK(rejects("{\"color\":{\"r\":300,\"g\":0,\"b\":0}}"));
    CHECK(rejects("{\"color\":{\"r\":-1,\"g\":0,\"b\":0}}"));
    CHECK(rejects("{\"color\":{\"r\":1,\"g\":2}}"));
    CHECK(rejects("{\"color\":\"red\"}"));
    CHECK(rejects("{\"brightness\":256}"));
    CHECK(rejects("{\"brightness\":\"high\"}"));
    CHECK(rejects("{\"transition\":-1}"));
    CHECK(rejects("{\"transition\":true}"));
    CHECK(rejects("{\"transition\":4000}"));
    CHECK(rejects("{\"effect\":\"strobe\"}"));
    CHECK(rejects("{\"effect\":1}"));
}

static void test_payload_size_limit(void)
{
    char payload[COMMAND_PAYLOAD_MAX + 2];
    memset(payload, ' ', sizeof(payload));

    // Exactly at the limit, with no NUL terminator anywhere in the given length
    payload[0] = '{';
    payload[COMMAND_PAYLOAD_MAX - 1] = '}';
    LightCommand_t command = {};
    CHECK(parseLightCommand(payload, COMMAND_PAYLOAD_MAX, &command));

    // Stray bytes after the given length aren't read
    payload[COMMAND_PAYLOAD_MAX] = 'x';
    CHECK(parseLightCommand(payload, COMMAND_PAYLOAD_MAX, &command));

    // One over
    CHECK(!parseLightCommand(payload, COMMAND_PAYLOAD_MAX + 1, &command));
}

static void test_on_off(void)
{
    bool on = false;
    CHECK(parseOnOff("ON", 2, &on) && on);
    CHECK(parseOnOff("OFF", 3, &on) && !on);

    // Length is what counts, the payload isn't NUL terminated
    CHECK(parseOnOff("ONxxx", 2, &on) && on);
    CHECK(!parseOnOff("ONE", 3, &on));
    CHECK(!parseOnOff("on", 2, &on));
    CHECK(!parseOnOff("", 0, &on));
}

//==============================================================================
// Topics

static void test_topics_match_exactly(void)
{
    const char *topic = TOPIC_LIGHT_SET;
    size_t length = strlen(topic);

    CHECK(topicEquals(TOPIC_LIGHT_SET, topic, length));
    CHECK(!topicEquals(TOPIC_LIGHT_SET, topic, length - 1));
    CHECK(!topicEquals(TOPIC_LIGHT_SET, topic, 0));
    CHECK(!topicEquals("a/b", "a/b/c", 5));
    CHECK(!topicEquals("a/b/c", "a/b", 3));

    // Incoming topics aren't NUL terminated
    CHECK(topicEquals("a/b", "a/bXXXX", 3));
}

//==============================================================================
// What we send out

static void test_status_is_what_home_assistant_expects(void)
{
    LightStatus_t status = {};
    status.on = true;
    status.color = {1, 2, 3};
    status.brightness = 200;
    status.effect = EFFECT_COLORLOOP;

    char output[256];
    CHECK(serializeLightStatus(&status, output, sizeof(output)) > 0);

    JsonDocument doc;
    CHECK(!deserializeJson(doc, output));
    CHECK(strcmp(doc["state"], "ON") == 0);
    CHECK(strcmp(doc["color_mode"], "rgb") == 0);
    CHECK(strcmp(doc["effect"], "colorloop") == 0);
    CHECK(doc["brightness"] == 200);
    CHECK(doc["color"]["r"] == 1);
    CHECK(doc["color"]["g"] == 2);
    CHECK(doc["color"]["b"] == 3);
}

static void test_status_round_trips_through_the_parser(void)
{
    LightStatus_t status = {};
    status.on = false;
    status.color = {9, 8, 7};
    status.brightness = 42;

    char output[256];
    size_t length = serializeLightStatus(&status, output, sizeof(output));

    LightCommand_t command = {};
    CHECK(parseLightCommand(output, length, &command));
    CHECK(command.hasState && !command.on);
    CHECK(same(command.color, status.color));
    CHECK(command.brightness == 42);
}

static void test_status_refuses_a_small_buffer(void)
{
    LightStatus_t status = {};
    char output[8];
    CHECK(serializeLightStatus(&status, output, sizeof(output)) == 0);
}

static void test_discovery(void)
{
    static char output[DISCOVERY_PAYLOAD_MAX];
    size_t length = serializeDiscovery(output, sizeof(output));
    printf("discovery payload: %zu of %d bytes\n", length, DISCOVERY_PAYLOAD_MAX);
    CHECK(length > 0);

    JsonDocument doc;
    CHECK(!deserializeJson(doc, output));

    // The required blocks for device based discovery
    CHECK(doc["dev"]["ids"].is<const char *>());
    CHECK(doc["o"]["name"].is<const char *>());

    JsonObject ring = doc["cmps"]["ring"];
    CHECK(strcmp(ring["p"], "light") == 0);
    CHECK(strcmp(ring["schema"], "json") == 0);
    CHECK(strcmp(ring["command_topic"], TOPIC_LIGHT_SET) == 0);
    CHECK(strcmp(ring["state_topic"], TOPIC_LIGHT_STATE) == 0);
    CHECK(strcmp(ring["supported_color_modes"][0], "rgb") == 0);
    CHECK(ring["brightness"] == true);
    CHECK(ring["effect"] == true);
    CHECK(strcmp(ring["effect_list"][0], "colorloop") == 0);
    CHECK(strcmp(ring["effect_list"][1], "solid") == 0);

    const char *keys[TWINKLE_COUNT] = {"tw1", "tw2", "tw3"};
    for (int i = 0; i < TWINKLE_COUNT; i++) {
        JsonObject twinkle = doc["cmps"][keys[i]];
        CHECK(strcmp(twinkle["p"], "switch") == 0);
        CHECK(strcmp(twinkle["command_topic"], TOPIC_TWINKLE_SET[i]) == 0);
        CHECK(strcmp(twinkle["state_topic"], TOPIC_TWINKLE_STATE[i]) == 0);
        CHECK(twinkle["unique_id"].is<const char *>());
    }

    // Every unique_id really is unique
    CHECK(strcmp(doc["cmps"]["tw1"]["unique_id"], doc["cmps"]["tw2"]["unique_id"]) != 0);
    CHECK(strcmp(doc["cmps"]["tw2"]["unique_id"], doc["cmps"]["tw3"]["unique_id"]) != 0);
    CHECK(strcmp(doc["cmps"]["ring"]["unique_id"], doc["cmps"]["tw1"]["unique_id"]) != 0);
}

//==============================================================================

int main(void)
{
    test_starts_off();
    test_color_shows_immediately_without_transition();
    test_loop_keeps_changing_color();
    test_loop_comes_back_around();
    test_color_breaks_the_loop();
    test_color_with_transition_breaks_the_loop_smoothly();
    test_twinkles_leave_the_loop_alone();
    test_twinkles_are_independent();
    test_fade_reaches_the_halfway_point();
    test_fade_length_survives_late_frames();
    test_new_color_during_a_fade_starts_from_what_is_showing();
    test_starting_the_loop_does_not_jump();
    test_off_then_on_restores_color();
    test_off_then_on_restores_the_loop();
    test_effect_turns_the_ring_on();
    test_solid_stops_the_loop_and_keeps_the_color();
    test_brightness_keeps_the_loop_running();
    test_brightness_scales_color();
    test_brightness_does_not_turn_the_ring_on();
    test_clock_rollover();
    test_parses_a_full_home_assistant_command();
    test_missing_keys_are_not_set();
    test_rejects_bad_payloads();
    test_payload_size_limit();
    test_on_off();
    test_topics_match_exactly();
    test_status_is_what_home_assistant_expects();
    test_status_round_trips_through_the_parser();
    test_status_refuses_a_small_buffer();
    test_discovery();

    printf("%d checks, %d failed\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
