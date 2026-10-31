#include "config.h"
#include "lights.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <soc/soc.h>

#include "LightState.h"
#include "mqtt.h"
#include "NeoPixelRing.h"

#define COMMAND_QUEUE_LENGTH 16

// Notification bits for the status task
#define STATUS_CHANGED     (1 << 0)
#define STATUS_ANNOUNCE    (1 << 1)
#define STATUS_ANNOUNCE_HA (1 << 2)

// Home Assistant asks for a small random delay after it starts
#define ANNOUNCE_DELAY_MS 1000
#define ANNOUNCE_JITTER_MS 1000

static const char *TAG = "LIGHTS";

static const gpio_num_t TWINKLE_PINS[TWINKLE_COUNT] = {
    (gpio_num_t)TW1_PIN,
    (gpio_num_t)TW2_PIN,
    (gpio_num_t)TW3_PIN,
};

static QueueHandle_t commandQueue = NULL;
static QueueHandle_t statusSlot = NULL;
static TaskHandle_t statusTaskHandle = NULL;

//==============================================================================
// Helpers

static uint32_t nowMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool sameColor(RGB_t a, RGB_t b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static void applyCommand(LightState &state, const Command_t &command)
{
    if (command.type == COMMAND_LIGHT) {
        state.apply(command.light, nowMs());
    } else if (command.twinkle.index < TWINKLE_COUNT) {
        gpio_set_level(TWINKLE_PINS[command.twinkle.index], command.twinkle.on ? 1 : 0);
        state.setTwinkle(command.twinkle.index, command.twinkle.on);
    }
}

// The only place that reports state, a second protocol would be added here
static void reportStatus(const LightStatus_t *status)
{
    mqttPublishStatus(status);
}

static void reportAnnouncement(void)
{
    mqttPublishAnnouncement();
}

//==============================================================================
// Tasks

// Owns the ring and the twinkle lights. Nothing else touches them, so there's
// nothing to lock.
//
// This wakes up for a new command, or when it's time for the next frame, then
// goes back to waiting. Nothing in here takes longer than a frame, which is why
// a command never has to wait for an animation to finish.
//
// It must never call into mqtt, see the status task below.
static void lightsTask(void *parameter)
{
    NeoPixelRing ring;
    LightState state;
    Command_t command;
    RGB_t shown = {0, 0, 0};

    // Started here so the ring's interrupt stays on this core, away from wifi
    ring.begin();

    while (1) {
        TickType_t wait = state.isAnimating() ? pdMS_TO_TICKS(FRAME_MS) : portMAX_DELAY;

        if (xQueueReceive(commandQueue, &command, wait) == pdTRUE) {
            // Take everything that's waiting, so a burst only draws one frame
            do {
                applyCommand(state, command);
            } while (xQueueReceive(commandQueue, &command, 0) == pdTRUE);

            LightStatus_t status = state.status();
            xQueueOverwrite(statusSlot, &status);
            xTaskNotify(statusTaskHandle, STATUS_CHANGED, eSetBits);
        }

        RGB_t next = state.render(nowMs());
        if (!sameColor(next, shown)) {
            ring.show(next);
            shown = next;
        }
    }
}

// Sends our state out, and it's fine for this to get stuck. The mqtt client holds
// its lock while it's reconnecting, for up to its 10 second network timeout, and
// every publish waits on that lock. That's why the lights task only leaves its
// latest state here and carries on animating.
static void statusTask(void *parameter)
{
    uint32_t bits = 0;
    LightStatus_t status;

    while (1) {
        xTaskNotifyWait(0, UINT32_MAX, &bits, portMAX_DELAY);

        if (bits & STATUS_ANNOUNCE_HA) {
            vTaskDelay(pdMS_TO_TICKS(ANNOUNCE_DELAY_MS + esp_random() % ANNOUNCE_JITTER_MS));
        }

        if (bits & (STATUS_ANNOUNCE | STATUS_ANNOUNCE_HA)) {
            reportAnnouncement();
        }

        if (xQueuePeek(statusSlot, &status, 0) == pdTRUE) {
            reportStatus(&status);
        }
    }
}

//==============================================================================
// Public

void lightsStart(void)
{
    gpio_config_t twinkleConfig = {};
    twinkleConfig.mode = GPIO_MODE_OUTPUT;

    for (uint8_t i = 0; i < TWINKLE_COUNT; i++) {
        gpio_set_level(TWINKLE_PINS[i], 0);
        twinkleConfig.pin_bit_mask |= 1ULL << TWINKLE_PINS[i];
    }

    ESP_ERROR_CHECK(gpio_config(&twinkleConfig));

    commandQueue = xQueueCreate(COMMAND_QUEUE_LENGTH, sizeof(Command_t));
    statusSlot = xQueueCreate(1, sizeof(LightStatus_t));
    configASSERT(commandQueue && statusSlot);

    LightState initial;
    LightStatus_t status = initial.status();
    xQueueOverwrite(statusSlot, &status);

    BaseType_t created = xTaskCreatePinnedToCore(
        statusTask,        // Function to be called
        "status",          // Name of task
        4096,              // Stack size (bytes in ESP32, words in FreeRTOS)
        NULL,              // Parameter to pass to function
        3,                 // Task priority (0 to configMAX_PRIORITIES - 1)
        &statusTaskHandle, // Task handle
        PRO_CPU_NUM        // Run on core
    );
    configASSERT(created == pdPASS);

    created = xTaskCreatePinnedToCore(
        lightsTask,        // Function to be called
        "lights",          // Name of task
        4096,              // Stack size (bytes in ESP32, words in FreeRTOS)
        NULL,              // Parameter to pass to function
        5,                 // Task priority (0 to configMAX_PRIORITIES - 1)
        NULL,              // Task handle
        APP_CPU_NUM        // Run on core
    );
    configASSERT(created == pdPASS);
}

bool lightsSend(const Command_t *command)
{
    if (xQueueSend(commandQueue, command, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Command queue is full, dropping a command");
        return false;
    }

    return true;
}

void lightsRequestAnnouncement(bool delayed)
{
    xTaskNotify(statusTaskHandle, delayed ? STATUS_ANNOUNCE_HA : STATUS_ANNOUNCE, eSetBits);
}
