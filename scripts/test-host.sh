#!/usr/bin/env bash
# Builds and runs the tests on this computer, no board needed.

set -Eeo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
json="$root/managed_components/bblanchon__arduinojson/src"

if [ ! -d "$json" ]; then
    echo "ArduinoJson hasn't been downloaded yet. Run: idf.py reconfigure" >&2
    exit 1
fi

output="$(mktemp -d)"
trap 'rm -rf "$output"' EXIT

# Copy the headers so your real config.h can't be picked up instead of the test one
mkdir "$output/include"
cp "$root"/main/include/*.h "$output/include/"
rm -f "$output/include/config.h"

c++ -std=c++17 -Wall -Wextra -g \
    -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -I"$root/test/host" -I"$output/include" -isystem "$json" \
    "$root/test/host/tests.cpp" \
    "$root/main/commands.cpp" \
    "$root/main/discovery.cpp" \
    "$root/main/LightState.cpp" \
    -o "$output/tests"

"$output/tests"
