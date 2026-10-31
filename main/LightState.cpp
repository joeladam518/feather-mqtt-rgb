// LightState.cpp: decides what the lights should be doing.
//
// This is the most important file to understand. The trick that makes everything
// interruptible is here: nothing in this file ever waits or loops over time.
//
// The old fadeColor() worked like a recipe with waiting in it: "set the color, sleep
// 10 ms, set the next color, sleep...". While it was sleeping, nothing could stop it.
//
// This code works like a formula instead: "given the time right now, what color should
// the ring be?". Every call returns instantly. So a fade or the loop isn't something
// that's running. It's just a few numbers that say how to work out the color. A new
// command changes those numbers, and the very next frame shows the new behaviour.

#include "config.h"
#include "LightState.h"

// Starting the loop without a transition eases into it instead of jumping to red
#define LOOP_BLEND_MS 1000

// There are 255 steps between each of the 6 corners of the color wheel
#define HUE_STEPS (6 * 255)

//-------------------------------
// Helpers
//-------------------------------
//
// "static" on a function that's outside a class means it's private to this file.
// Nothing else in the program can see or call these.

// Turns a position on the color wheel (0 to 1529) into a fully bright color.
//
// The wheel has 6 corners: red, yellow, green, cyan, blue, magenta, then back to red.
// Going from one corner to the next only ever changes one channel:
//
//   red -> yellow    green rises    case 0
//   yellow -> green  red falls      case 1
//   green -> cyan    blue rises     case 2
//   cyan -> blue     green falls    case 3
//   blue -> magenta  red rises      case 4
//   magenta -> red   blue falls     default (case 5)
static RGB_t hueToRgb(uint32_t hue)
{
    // How far we are between two corners, as a number from 0 to 254
    uint8_t rising = hue % 255;
    uint8_t falling = 255 - rising;

    // Which pair of corners we're between. Dividing whole numbers throws away the
    // remainder, so 0-254 gives 0, 255-509 gives 1, and so on.
    switch (hue / 255) {
        case 0:
            // {255, rising, 0} builds an RGB_t, and the fields are in r, g, b order
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

// Dims one color channel. A brightness of 255 leaves it alone, 128 is about half.
//
// Whole number division always rounds down, so adding half of 255 first (127) makes
// it round to the nearest number instead. Multiplying first keeps the accuracy.
// C++ does this math as an int, which is why value * brightness doesn't overflow
// even though both inputs are 8 bits.
static uint8_t scale(uint8_t value, uint8_t brightness)
{
    return (value * brightness + 127) / 255;
}

// Finds the value part of the way from one number to another. It's the same idea as
// the Arduino map() function that the old code used.
//
//   elapsed = 0         gives from
//   elapsed = duration  gives to
//   halfway             gives the number in the middle
//
// The (int32_t) casts matter. (to - from) is negative when fading down, and unsigned
// numbers can't be negative. commands.cpp limits a transition to one hour, which keeps
// 255 * elapsed small enough to fit in 32 bits.
static uint8_t blend(uint8_t from, uint8_t to, uint32_t elapsed, uint32_t duration)
{
    return from + ((int32_t)to - from) * (int32_t)elapsed / (int32_t)duration;
}

//-------------------------------
// Constructor
//-------------------------------
//
// A constructor runs when a LightState is created. The list after the colon is a
// member initializer list: it gives each member its starting value before the body
// of the constructor runs.
//
// The color starts as white, so that a plain "turn on" shows something even if no
// color has ever been set. The ring itself starts off because on is false.
LightState::LightState(void):
    on(false),
    color{255, 255, 255},
    brightness(255),
    effect(EFFECT_SOLID),
    twinkle(false),
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
//
// LightState:: says these functions belong to the LightState class. "const" at the end
// is a promise that the function won't change any member variables, and the compiler
// enforces that.

// The loop can be selected while the ring is off, in which case it isn't really showing.
// That's what lets "off" remember the loop, and "on" bring it back.
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
        // The color comes straight from how long the loop has been running. There's no
        // "current hue" being stepped along, so a late frame can't slow the loop down.
        // % wraps it around, so the position is always somewhere inside one cycle.
        uint32_t position = (nowMs - loopStartMs) % LOOP_CYCLE_MS;

        // Converts "how far through the cycle" into "how far around the wheel".
        // The multiply happens first to keep accuracy, and (uint64_t) makes the math
        // 64 bit so that the big number in the middle can't overflow.
        base = hueToRgb((uint64_t)position * HUE_STEPS / LOOP_CYCLE_MS);
    }

    // Brightness is applied last, so it works the same for a color and for the loop.
    // The stored color is never changed, which is why turning brightness back up works.
    return {scale(base.r, brightness), scale(base.g, brightness), scale(base.b, brightness)};
}

//-------------------------------
// methods
//-------------------------------

// Applies a command. This is the function that makes a color break the loop.
//
// "const LightCommand_t &command" is a reference. It lets this function look at the
// caller's command without copying it, and const means it can't change it.
void LightState::apply(const LightCommand_t &command, uint32_t nowMs)
{
    // Whatever happens next starts from what's showing right now.
    // This has to come first, before anything below changes what target() returns.
    RGB_t from = render(nowMs);
    bool loopWasShowing = isLoopShowing();

    // Setting a color is what stops the loop. There's nothing to cancel, target() just
    // stops looking at the clock now that the effect is solid.
    if (command.hasColor) {
        color = command.color;
        effect = EFFECT_SOLID;
        on = true;
    }

    // Brightness doesn't touch the effect, so the loop keeps running. It doesn't set on
    // either, so moving a brightness slider won't turn on a ring that's off.
    if (command.hasBrightness) {
        brightness = command.brightness;
    }

    if (command.hasEffect) {
        effect = command.effect;
        if (effect == EFFECT_COLORLOOP) {
            on = true;
        }
    }

    // This is checked last so that an explicit state always wins. For example
    // {"state":"OFF","color":...} sets the color for later, and the ring stays off.
    // Turning off leaves color and effect alone, which is how "on" restores them.
    if (command.hasState) {
        on = command.on;
    }

    // Only restart the loop's clock if it's just starting. A brightness change during
    // the loop has loopWasShowing true, so the loop carries on from where it was.
    bool loopStarting = !loopWasShowing && isLoopShowing();
    if (loopStarting) {
        loopStartMs = nowMs;
    }

    // A ? b : c means "if a then b, otherwise c"
    if (command.hasTransition) {
        fadeDurationMs = command.transitionMs;
    } else {
        fadeDurationMs = loopStarting ? LOOP_BLEND_MS : 0;
    }

    // Every command starts a new fade from what's on the ring, replacing any old one.
    // That's all it takes for a color during a fade to carry on smoothly. A duration of
    // 0 means no fade at all, and render() goes straight to the target.
    fading = fadeDurationMs > 0;
    fadeFrom = from;
    fadeStartMs = nowMs;
}

// Twinkle lights are kept completely separate from the ring. Nothing here touches any
// of the ring's variables, which is why they can't disturb the loop. This only records
// the state. lights.cpp is what really switches the pin.
void LightState::setTwinkle(bool twinkleOn)
{
    twinkle = twinkleOn;
}

// Works out the color to show at this moment. The lights task calls this every frame.
RGB_t LightState::render(uint32_t nowMs)
{
    RGB_t to = target(nowMs);

    // Using elapsed time means a late frame can't stretch a fade. If a frame shows up
    // late, it just draws further along. Counting frames instead would slow the fade.
    //
    // Subtracting works even when the clock wraps past its maximum (every 49.7 days),
    // because unsigned math wraps around too. test_clock_rollover checks this.
    uint32_t elapsed = nowMs - fadeStartMs;
    if (fading && elapsed >= fadeDurationMs) {
        fading = false;
    }

    if (!fading) {
        displayed = to;
        return displayed;
    }

    // Mid fade. "to" is recalculated every frame, so fading into the loop follows the
    // loop's changing color instead of aiming at a color that's already gone.
    displayed.r = blend(fadeFrom.r, to.r, elapsed, fadeDurationMs);
    displayed.g = blend(fadeFrom.g, to.g, elapsed, fadeDurationMs);
    displayed.b = blend(fadeFrom.b, to.b, elapsed, fadeDurationMs);

    return displayed;
}

// True if the color could be different on the next frame. When this is false the lights
// task can sleep until a command arrives, using no CPU at all.
bool LightState::isAnimating(void) const
{
    return fading || isLoopShowing();
}

// Copies out everything we report to the outside world.
LightStatus_t LightState::status(void) const
{
    // = {} sets every field to zero, so nothing is left with a random value
    LightStatus_t current = {};
    current.on = on;
    current.color = color;
    current.brightness = brightness;
    current.effect = effect;
    current.twinkle = twinkle;

    return current;
}
