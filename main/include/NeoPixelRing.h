#ifndef __NEO_PIXEL_RING_H__
#define __NEO_PIXEL_RING_H__

#include "config.h"
#include <stdint.h>
#include <led_strip.h>
#include "led.h"

class NeoPixelRing
{
private:
    led_strip_handle_t strip;
    // led_strip can't read pixels back, so keep track of what was written
    RGB_t pixels[NEO_PIXEL_COUNT];

    void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void setPixelColor(uint16_t index, uint32_t color);
    void show(void);

public:
    // Constructor
    NeoPixelRing(void);

    // Destructor
    //~NeoPixelRing();

    // Methods
    void begin(void);
    void fadeColor(uint8_t r, uint8_t g, uint8_t b, uint16_t fadeTime);
    void fadeColor(RGB_t *endColor, uint16_t fadeTime);
    void getColor(RGB_t *color);
    void off(void);
    void rainbow(uint8_t wait);
    void rainbowCycle(uint8_t wait);
    void setColor(uint8_t r, uint8_t g, uint8_t b);
    void setColor(RGB_t *color);
    uint32_t wheel(uint8_t wheelPos);
    void wipeColor(uint8_t r, uint8_t g, uint8_t b);
    void wipeColor(RGB_t *color);
};

#endif
