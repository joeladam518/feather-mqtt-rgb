#pragma once

#include <led_strip.h>
#include "led.h"

class NeoPixelRing
{
private:
    led_strip_handle_t strip;

public:
    // Constructor
    NeoPixelRing(void);

    // Methods
    void begin(void);
    void show(RGB_t color);
};
