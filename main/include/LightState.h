#pragma once

#include <stdint.h>
#include "commands.h"
#include "led.h"

// How far apart animation frames are
#define FRAME_MS 20

// Decides what the lights should be doing. It never waits and never touches
// hardware: you tell it the time, and it tells you the color to show right then.
// That's what lets a command interrupt anything, because nothing here is a loop
// that has to finish first.
class LightState
{
private:
    bool on;
    RGB_t color;
    uint8_t brightness;
    LightEffect_t effect;
    bool twinkles[TWINKLE_COUNT];

    RGB_t displayed;
    bool fading;
    RGB_t fadeFrom;
    uint32_t fadeStartMs;
    uint32_t fadeDurationMs;
    uint32_t loopStartMs;

    bool isLoopShowing(void) const;
    RGB_t target(uint32_t nowMs) const;

public:
    // Constructor
    LightState(void);

    // Methods
    void apply(const LightCommand_t &command, uint32_t nowMs);
    void setTwinkle(uint8_t index, bool twinkleOn);
    RGB_t render(uint32_t nowMs);
    bool isAnimating(void) const;
    LightStatus_t status(void) const;
};
