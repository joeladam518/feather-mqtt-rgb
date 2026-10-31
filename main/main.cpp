#include "lights.h"
#include "mqtt.h"
#include "wifi.h"

extern "C" void app_main(void)
{
    lightsStart();
    wifiConnect();
    mqttStart();
}
