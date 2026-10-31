// LightState.h: the class that decides what the lights should be doing.
// See LightState.cpp for how it works and why it's built this way.

// #pragma once stops this file being included twice in the same compile, which would
// define everything twice and fail. It does the same job as the #ifndef guards in the
// old headers, with less typing.
#pragma once

#include <stdint.h>
#include "commands.h"
#include "led.h"

// How far apart animation frames are. 20 ms is 50 frames per second.
#define FRAME_MS 20

// Decides what the lights should be doing. It never waits and never touches
// hardware: you tell it the time, and it tells you the color to show right then.
// That's what lets a command interrupt anything, because nothing here is a loop
// that has to finish first.
//
// Keeping hardware and FreeRTOS out of this class is also what lets the tests in
// test/host run it on a computer, passing in made up times.
class LightState
{
// Private members can only be used by this class's own functions
private:
    // What's been asked for. These are the settings, not what's on the ring right now.
    bool on;
    RGB_t color;
    uint8_t brightness;
    LightEffect_t effect;
    bool twinkles[TWINKLE_COUNT];

    // What's on the ring right now, which is where the next fade starts from
    RGB_t displayed;

    // The current fade, if there is one. A fade is nothing more than these four values.
    bool fading;
    RGB_t fadeFrom;
    uint32_t fadeStartMs;
    uint32_t fadeDurationMs;

    // When the loop started, which is how it knows where it should be in its cycle
    uint32_t loopStartMs;

    bool isLoopShowing(void) const;
    RGB_t target(uint32_t nowMs) const;

// Public members can be used by anything
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
