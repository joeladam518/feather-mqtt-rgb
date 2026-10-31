#pragma once

#include "commands.h"

// Sets the lights to off and starts the tasks. Call this before anything that
// uses the network, so the outputs are in a known state even if wifi never connects.
void lightsStart(void);

// Hands a command to the lights task. This never blocks, so it's safe to call
// from a network event handler. Returns false if the command had to be dropped.
bool lightsSend(const Command_t *command);

// Asks for the current state to be reported again, for example after reconnecting.
// Set delayed when Home Assistant has just started, to give it a moment to get ready.
void lightsRequestAnnouncement(bool delayed);
