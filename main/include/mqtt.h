#pragma once

#include "commands.h"

// Connects to the broker, and keeps reconnecting if the connection drops
void mqttStart(void);

// These can block for several seconds if the connection is down
void mqttPublishStatus(const LightStatus_t *status);
void mqttPublishAnnouncement(void);
