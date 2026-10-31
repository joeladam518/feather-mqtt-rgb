#include "config.h"
#include "LightState.h"

// Starting the loop without a transition eases into it instead of jumping to red
#define LOOP_BLEND_MS 1000

// There are 255 steps between each of the 6 corners of the color wheel
#define HUE_STEPS (6 * 255)

//-------------------------------
// Helpers
//-------------------------------
static RGB_t hueToRgb(uint32_t hue)
{
    uint8_t rising = hue % 255;
    uint8_t falling = 255 - rising;

    switch (hue / 255) {
        case 0:
            return {255, rising, 0};
        case 1:
            return {falling, 255, 0};
        case 2:
            return {0, 255, rising};
        case 3:
            return {0, falling, 255};
        case 4:
            return {rising, 0, 255};
        default:
            return {255, 0, falling};
    }
}

static uint8_t scale(uint8_t value, uint8_t brightness)
{
    return (value * brightness + 127) / 255;
}

static uint8_t blend(uint8_t from, uint8_t to, uint32_t elapsed, uint32_t duration)
{
    return from + ((int32_t)to - from) * (int32_t)elapsed / (int32_t)duration;
}

//-------------------------------
// Constructor
//-------------------------------
LightState::LightState(void):
    on(false),
    color{255, 255, 255},
    brightness(255),
    effect(EFFECT_SOLID),
    twinkles{},
    displayed{0, 0, 0},
    fading(false),
    fadeFrom{0, 0, 0},
    fadeStartMs(0),
    fadeDurationMs(0),
    loopStartMs(0)
{}

//-------------------------------
// Private methods
//-------------------------------
bool LightState::isLoopShowing(void) const
{
    return on && effect == EFFECT_COLORLOOP;
}

// Where the ring should end up at this moment. During the loop that's a moving target.
RGB_t LightState::target(uint32_t nowMs) const
{
    if (!on) {
        return {0, 0, 0};
    }

    RGB_t base = color;
    if (effect == EFFECT_COLORLOOP) {
        uint32_t position = (nowMs - loopStartMs) % LOOP_CYCLE_MS;
        base = hueToRgb((uint64_t)position * HUE_STEPS / LOOP_CYCLE_MS);
    }

    return {scale(base.r, brightness), scale(base.g, brightness), scale(base.b, brightness)};
}

//-------------------------------
// methods
//-------------------------------
void LightState::apply(const LightCommand_t &command, uint32_t nowMs)
{
    // Whatever happens next starts from what's showing right now
    RGB_t from = render(nowMs);
    bool loopWasShowing = isLoopShowing();

    if (command.hasColor) {
        color = command.color;
        effect = EFFECT_SOLID;
        on = true;
    }

    if (command.hasBrightness) {
        brightness = command.brightness;
    }

    if (command.hasEffect) {
        effect = command.effect;
        if (effect == EFFECT_COLORLOOP) {
            on = true;
        }
    }

    if (command.hasState) {
        on = command.on;
    }

    bool loopStarting = !loopWasShowing && isLoopShowing();
    if (loopStarting) {
        loopStartMs = nowMs;
    }

    if (command.hasTransition) {
        fadeDurationMs = command.transitionMs;
    } else {
        fadeDurationMs = loopStarting ? LOOP_BLEND_MS : 0;
    }

    fading = fadeDurationMs > 0;
    fadeFrom = from;
    fadeStartMs = nowMs;
}

void LightState::setTwinkle(uint8_t index, bool twinkleOn)
{
    twinkles[index] = twinkleOn;
}

RGB_t LightState::render(uint32_t nowMs)
{
    RGB_t to = target(nowMs);

    // Using elapsed time means a late frame can't stretch a fade
    uint32_t elapsed = nowMs - fadeStartMs;
    if (fading && elapsed >= fadeDurationMs) {
        fading = false;
    }

    if (!fading) {
        displayed = to;
        return displayed;
    }

    displayed.r = blend(fadeFrom.r, to.r, elapsed, fadeDurationMs);
    displayed.g = blend(fadeFrom.g, to.g, elapsed, fadeDurationMs);
    displayed.b = blend(fadeFrom.b, to.b, elapsed, fadeDurationMs);

    return displayed;
}

bool LightState::isAnimating(void) const
{
    return fading || isLoopShowing();
}

LightStatus_t LightState::status(void) const
{
    LightStatus_t current = {};
    current.on = on;
    current.color = color;
    current.brightness = brightness;
    current.effect = effect;
    for (uint8_t i = 0; i < TWINKLE_COUNT; i++) {
        current.twinkles[i] = twinkles[i];
    }

    return current;
}
