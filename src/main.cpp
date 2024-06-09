#include "config.h"
#include "globals.h"
// FreeRtos
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
// Library Headers
#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h> // ESP32 Wifi client library
#include <mqtt_client.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <esp_log.h>
// Custom Headers
#include "mqttEventProcessing.h"
#include "neoPixelRing.h"

//==============================================================================
// Globals

/**
 *  Adafruit NeoPixels
 *
 *  Argument 1 = Number of pixels in NeoPixel strip
 *  Argument 2 = Arduino pin number (most are valid)
 *  Argument 3 = Pixel type flags, add together as needed:
 *      NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
 *      NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
 *      NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
 *      NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
 *      NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
 */
Adafruit_NeoPixel neoPixel(NEO_PIXEL_COUNT, NEO_PIXEL_PIN, NEO_GRB + NEO_KHZ800);
NeoPixelRing ring(&neoPixel);

// Task handles
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t processShortTaskHandle = NULL;
TaskHandle_t processLongTaskHandle = NULL;

// Queues
QueueHandle_t shortActionQueue = NULL;
QueueHandle_t longActionQueue = NULL;

// Mutexes
SemaphoreHandle_t ringMutex = NULL;

// Mqtt client
esp_mqtt_client_handle_t mqttClient;
esp_mqtt_client_config_t mqtt_config = {
    .event_loop_handle = &mqttTaskHandle,
    .uri = MQTT_URI,
    .client_id = "RGB_MQTT_TREE",
    .keepalive = 360,
    .task_prio = 5,
    .task_stack = 4096, // 6144
    .buffer_size = 2048,
};

//==============================================================================
// Main

// NOTE: setup is excuted on core #1
void setup()
{
    Serial.begin(115200);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Connect to Wifi
    Serial.println("");
    Serial.print(F("Connecting to "));
    Serial.print(WLAN_SSID);

    WiFi.begin(WLAN_SSID, WLAN_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }

    Serial.println(F("Success!"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());

    // Set the default values for the twinkle lights
    pinMode(TW1_PIN, OUTPUT);
    digitalWrite(TW1_PIN, LOW);
    pinMode(TW2_PIN, OUTPUT);
    digitalWrite(TW2_PIN, LOW);
    pinMode(TW3_PIN, OUTPUT);
    digitalWrite(TW3_PIN, LOW);

    // Configure RTOS
    shortActionQueue = xQueueCreate(3, sizeof(SubscriptionAction_t));
    APP_FAIL_IF(!shortActionQueue, F("Failed to ceate shortActionQueue"));
    longActionQueue = xQueueCreate(3, sizeof(SubscriptionAction_t));
    APP_FAIL_IF(!longActionQueue, F("Failed to ceate longActionQueue"))
    ringMutex = xSemaphoreCreateMutex();
    APP_FAIL_IF(!ringMutex, F("Failed to ceate ringMutex"));

    // Initialize neopixel ring
    if (xSemaphoreTake(ringMutex, 0) == pdTRUE) {
        ring.begin();
        xSemaphoreGive(ringMutex);
    } else {
        Serial.println(F("Didn't start ring"));
        while(1);
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
    mqttClient = esp_mqtt_client_init(&mqtt_config);
    APP_FAIL_IF(!mqttClient, F("mqtt client failed to initialize..."));
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqttClient);

    Serial.println(F("Finished Setup"));
    Serial.println("");

    // Remove the setup() and loop() task
    vTaskDelete(NULL);
}

// NOTE: loop is excuted on core #1
void loop() {}
