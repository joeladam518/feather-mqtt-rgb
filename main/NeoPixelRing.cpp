#include "config.h"
#include "NeoPixelRing.h"

#include <esp_check.h>

#define RMT_RESOLUTION_HZ (10 * 1000 * 1000)

static const char *TAG = "RING";

//-------------------------------
// Constructor
//-------------------------------
NeoPixelRing::NeoPixelRing(void):
    strip(NULL)
{}

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
    show({0, 0, 0});
}

// Takes about half a millisecond for 16 pixels
void NeoPixelRing::show(RGB_t color)
{
    for (uint16_t i = 0; i < NEO_PIXEL_COUNT; i++) {
        ESP_RETURN_VOID_ON_ERROR(led_strip_set_pixel(strip, i, color.r, color.g, color.b), TAG, "Failed to set pixel");
    }

    ESP_RETURN_VOID_ON_ERROR(led_strip_refresh(strip), TAG, "Failed to refresh strip");
}
