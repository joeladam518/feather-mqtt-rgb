#ifndef __RGB_TREE_NEO_PIXEL_RING_H__
#define __RGB_TREE_NEO_PIXEL_RING_H__

#include <Adafruit_NeoPixel.h>
#include "led.h"

class NeoPixelRing
{
private:
    Adafruit_NeoPixel *neoPixel;

public:
    // Constructor
    NeoPixelRing(Adafruit_NeoPixel *neoPixel);

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
