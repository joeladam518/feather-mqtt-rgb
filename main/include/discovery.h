#pragma once

#include <stddef.h>

// Largest Home Assistant discovery message we expect to send
#define DISCOVERY_PAYLOAD_MAX 3072

// Returns the number of bytes written, or 0 if it didn't fit
size_t serializeDiscovery(char *output, size_t size);
