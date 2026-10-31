#include "config.h"
#include "globals.h"

#include <esp_check.h>
#include <esp_log.h>
#include "led.h"
#include "NeoPixelRing.h"

#define RMT_RESOLUTION_HZ (10 * 1000 * 1000)

static const char *RING_TAG = "RING_RGB_TREE";

//-------------------------------
// Helpers
//-------------------------------
static int32_t mapRange(int32_t x, int32_t inMin, int32_t inMax, int32_t outMin, int32_t outMax)
{
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

static uint32_t packColor(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

//-------------------------------
// Constructor
//-------------------------------
NeoPixelRing::NeoPixelRing(void):
    strip(NULL),
    pixels{}
{}

//-------------------------------
// Destructor
//-------------------------------
// TODO: if you ever allocate memory for anything in this class free it with this destructor
// NeoPixelRing::~NeoPixelRing()
// {}

//-------------------------------
// Private methods
//-------------------------------
void NeoPixelRing::setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    pixels[index] = {r, g, b};
    ESP_RETURN_VOID_ON_ERROR(led_strip_set_pixel(strip, index, r, g, b), RING_TAG, "Failed to set pixel");
}

void NeoPixelRing::setPixelColor(uint16_t index, uint32_t color)
{
    setPixelColor(index, (uint8_t)(color >> 16), (uint8_t)(color >> 8), (uint8_t)color);
}

void NeoPixelRing::show(void)
{
    ESP_RETURN_VOID_ON_ERROR(led_strip_refresh(strip), RING_TAG, "Failed to refresh strip");
}

//-------------------------------
// methods
//-------------------------------
void NeoPixelRing::begin(void)
{
    led_strip_config_t stripConfig = {};
    stripConfig.strip_gpio_num = NEO_PIXEL_PIN;
    stripConfig.max_leds = NEO_PIXEL_COUNT;
    stripConfig.led_model = LED_MODEL_WS2812;
    stripConfig.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;

    led_strip_rmt_config_t rmtConfig = {};
    rmtConfig.clk_src = RMT_CLK_SRC_DEFAULT;
    rmtConfig.resolution_hz = RMT_RESOLUTION_HZ;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&stripConfig, &rmtConfig, &strip));
    off();
}

void NeoPixelRing::fadeColor(uint8_t r, uint8_t g, uint8_t b, uint16_t fadeTime)
{
    RGB_t startColor = {0, 0, 0};
    getColor(&startColor);

    RGB_t color = {0, 0, 0};
    for (int i = 0; i < fadeTime; i++) {
        color.r = mapRange(i, 0, fadeTime, startColor.r, r);
        color.g = mapRange(i, 0, fadeTime, startColor.g, g);
        color.b = mapRange(i, 0, fadeTime, startColor.b, b);

        setColor(&color);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    setColor(r, g, b);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void NeoPixelRing::fadeColor(RGB_t *endColor, uint16_t fadeTime)
{
    fadeColor(endColor->r, endColor->g, endColor->b, fadeTime);
}

void NeoPixelRing::getColor(RGB_t *color)
{
    // TODO: Whatever color the greatest number of pixel is, return that.
    *color = pixels[0];
}

void NeoPixelRing::off(void)
{
    setColor(0,0,0);
}

void NeoPixelRing::setColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t i;
    for (i = 0; i < NEO_PIXEL_COUNT; i++) {
        setPixelColor(i, r, g, b);
    }

    show();
}

void NeoPixelRing::setColor(RGB_t *color)
{
    setColor(color->r, color->g, color->b);
}

void NeoPixelRing::rainbow(uint8_t wait)
{
    uint16_t i, j;
    for (j = 0; j < 256; j++) {
        for(i = 0; i < NEO_PIXEL_COUNT; i++) {
            setPixelColor(i, wheel((i + j) & 255));
        }

        show();
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

void NeoPixelRing::rainbowCycle(uint8_t wait)
{
    uint16_t i, j;
    uint16_t cycles = (256 * 5); // 5 cycles of all colors on wheel

    for (j = 0; j < cycles; j++) {
        for (i = 0; i < NEO_PIXEL_COUNT; i++) {
            setPixelColor(i, wheel(((i * 256 / NEO_PIXEL_COUNT) + j) & 255));
        }

        show();
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

uint32_t NeoPixelRing::wheel(uint8_t wheelPos)
{
    wheelPos = 255 - wheelPos;

    if (wheelPos < 85) {
        return packColor(255 - wheelPos * 3, 0, wheelPos * 3);
    }

    if (wheelPos < 170) {
        wheelPos -= 85;
        return packColor(0, wheelPos * 3, 255 - wheelPos * 3);
    }

    wheelPos -= 170;
    return packColor(wheelPos * 3, 255 - wheelPos * 3, 0);
}

void NeoPixelRing::wipeColor(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t i;
    for (i = 0; i < NEO_PIXEL_COUNT; i++) {
        setPixelColor(i, r, g, b);
        show();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void NeoPixelRing::wipeColor(RGB_t *color)
{
    wipeColor(color->r, color->g, color->b);
}
