#!/usr/bin/env bash
# Builds and runs the tests on this computer, no board needed.
#
# This works because LightState, the parsers, and the discovery message have no hardware
# or FreeRTOS code in them. They're ordinary C++, so a normal compiler can build them.

# Stop at the first error, including errors in the middle of a pipeline
set -Eeo pipefail

# The project root, worked out from where this script is, so it runs from any directory
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# ArduinoJson comes from the same copy that the firmware uses
json="$root/managed_components/bblanchon__arduinojson/src"

if [ ! -d "$json" ]; then
    echo "ArduinoJson hasn't been downloaded yet. Run: idf.py reconfigure" >&2
    exit 1
fi

# Build in a temporary directory, and delete it when this script ends, however it ends
output="$(mktemp -d)"
trap 'rm -rf "$output"' EXIT

# Copy the code being tested, without your real config.h, so the test one gets used instead.
# The .cpp files have to come too: #include "config.h" looks in the .cpp file's own folder
# first, and in src/ that would find your real config.h.
mkdir "$output/src"
cp "$root"/src/*.h "$root/src/commands.cpp" "$root/src/discovery.cpp" "$root/src/LightState.cpp" "$output/src/"
rm -f "$output/src/config.h"

# -fsanitize adds run time checks that fail the test on memory bugs (like reading past the
# end of a buffer) and undefined behaviour, which might otherwise go unnoticed.
# -isystem hides warnings that come from inside ArduinoJson.
c++ -std=c++17 -Wall -Wextra -g \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$root/test/host" -I"$output/src" -isystem "$json" \
    "$root/test/host/tests.cpp" \
    "$output/src/commands.cpp" \
    "$output/src/discovery.cpp" \
    "$output/src/LightState.cpp" \
    -o "$output/tests"

"$output/tests"
