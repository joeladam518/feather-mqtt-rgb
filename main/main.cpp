#include "config.h"
#include "globals.h"
// Library Headers
#include <driver/gpio.h>
#include <esp_log.h>
#include <mqtt_client.h>
// Custom Headers
#include "MqttEventProcessing.h"
#include "NeoPixelRing.h"
#include "wifi.h"

static const char *MAIN_TAG = "RGB_TREE";

//==============================================================================
// Globals

// Mqtt client
esp_mqtt_client_handle_t mqttClient;

// NeoPixels
NeoPixelRing ring;

// Task handles
TaskHandle_t processShortTaskHandle = NULL;
TaskHandle_t processLongTaskHandle = NULL;

// Queues
QueueHandle_t shortActionQueue = NULL;
QueueHandle_t longActionQueue = NULL;

// Mutexes
SemaphoreHandle_t ringMutex = NULL;

//==============================================================================
// Main

static void stop(const char *message = NULL)
{
    if (message) {
        ESP_LOGE(MAIN_TAG, "%s", message);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// NOTE: app_main is excuted on core #1 (CONFIG_ESP_MAIN_TASK_AFFINITY_CPU1)
extern "C" void app_main(void)
{
    // Connect to Wifi
    wifiConnect();

    // Set the default values for the twinkle lights
    gpio_set_level((gpio_num_t)TW1_PIN, 0);
    gpio_set_level((gpio_num_t)TW2_PIN, 0);
    gpio_set_level((gpio_num_t)TW3_PIN, 0);

    // The input side stays enabled so gpio_get_level() can report the pin's state
    gpio_config_t twinkle_config = {};
    twinkle_config.pin_bit_mask = (1ULL << TW1_PIN) | (1ULL << TW2_PIN) | (1ULL << TW3_PIN);
    twinkle_config.mode = GPIO_MODE_INPUT_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&twinkle_config));

    // Configure RTOS
    shortActionQueue = xQueueCreate(3, sizeof(SubscriptionAction_t));
    if (!shortActionQueue) {
        stop("Failed to ceate shortActionQueue");
    }
    longActionQueue = xQueueCreate(5, sizeof(SubscriptionAction_t));
    if (!longActionQueue) {
        stop("Failed to ceate longActionQueue");
    }
    ringMutex = xSemaphoreCreateMutex();
    if (!ringMutex) {
        stop("Failed to ceate ringMutex");
    }

    // Initialize neopixel ring
    if (xSemaphoreTake(ringMutex, 0) == pdTRUE) {
        ring.begin();
        xSemaphoreGive(ringMutex);
    }

    // Create the tasks that process the incomming mqtt data
    xTaskCreatePinnedToCore(
        processShortTask,        // Function to be called
        "Process Short Tasks",   // Name of task
        4096,                    // Stack size (bytes in ESP32, words in FreeRTOS)
        NULL,                    // Parameter to pass to function
        2,                       // Task priority (0 to configMAX_PRIORITIES - 1)
        &processShortTaskHandle, // Task handle
        APP_CPU_NUM              // Run on core
    );
    xTaskCreatePinnedToCore(
        processLongTask,         // Function to be called
        "Process Long Actions",  // Name of task
        4096,                    // Stack size (bytes in ESP32, words in FreeRTOS)
        NULL,                    // Parameter to pass to function
        1,                       // Task priority (0 to configMAX_PRIORITIES - 1)
        &processLongTaskHandle,  // Task handle
        APP_CPU_NUM              // Run on core
    );

    // Start the mqtt task
    esp_mqtt_client_config_t mqtt_config = {};
    mqtt_config.broker.address.hostname = MQTT_BROKER;
    mqtt_config.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_config.broker.address.port = MQTT_PORT;
    mqtt_config.credentials.client_id = "RGB_MQTT_TREE";
    mqtt_config.task.priority = 5;
    mqtt_config.task.stack_size = 6144;
    mqtt_config.buffer.size = 2048;

    mqttClient = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqttClient);

    ESP_LOGI(MAIN_TAG, "Finished Setup");
}
