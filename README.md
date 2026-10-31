# feather-mqtt-rgb-tree

An MQTT powered RGB LED tree. It does three things:

1. Switches the twinkle lights on and off.
2. Sets the color of a NeoPixel ring.
3. Runs a color loop, where the whole ring slowly drifts through the rainbow.

A new color breaks the loop immediately. The twinkle lights can be switched at any time without disturbing the loop.

Firmware for the [Adafruit ESP32 Feather V2](https://learn.adafruit.com/adafruit-esp32-feather-v2) (ESP32-PICO-MINI-02,
8MB flash), built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/) v6.1 or newer.

- [Hardware](#hardware)
- [Setup](#setup)
- [ESP-IDF commands](#esp-idf-commands)
- [Debugging](#debugging)
- [Tests](#tests)
- [How it works](#how-it-works)
- [MQTT reference](#mqtt-reference)
- [Updating an app from the old format](#updating-an-app-from-the-old-format)
- [Home Assistant](#home-assistant)

## Hardware

| What | GPIO | Setting in `config.h` |
|---|---|---|
| NeoPixel ring data (16 pixels, GRB) | 14 | `NEO_PIXEL_PIN`, `NEO_PIXEL_COUNT` |
| Twinkle lights | 33 | `TWINKLE_PIN` |
| Power for the NeoPixel on the board | 2 | `BOARD_PIXEL_POWER_PIN` |

GPIO 14 may output a PWM signal during boot, so the ring can flicker briefly before the firmware clears it.

The NeoPixel on the board is kept dark by holding GPIO 2 low, which cuts its power. Its data line is GPIO 0, which is
unused for now. GPIO 2 also powers the STEMMA QT connector, so anything plugged in there has no power either.

Everything starts off after a power cycle or reset.

## Setup

Install ESP-IDF. You only need to do this once:

```shell
brew install cmake ninja
git clone -b v6.1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32
```

Then load it into each new terminal before using `idf.py`:

```shell
. ~/esp/esp-idf/export.sh
```

If that complains about a missing Python environment, your Python version changed. Run `install.sh` again.

Create your config, then fill in your wifi, MQTT, and device settings. It's gitignored, so your passwords stay local.

```shell
cp main/include/config_example.h main/include/config.h
```

Build and flash:

```shell
idf.py build
idf.py -p <serial port> flash monitor
```

If the board last ran the old PlatformIO/Arduino firmware, run `idf.py -p <serial port> erase-flash` once first,
because the partition table has changed.

## ESP-IDF commands

**Finding the serial port.** Plug in the board and run `ls /dev/cu.*`. Look for something like `/dev/cu.usbserial-*`
or `/dev/cu.SLAB_USBtoUART`. Set `export ESPPORT=/dev/cu.usbserial-xxxx` and you can leave out `-p` from then on.

| Command | What it does |
|---|---|
| `idf.py build` | Compiles everything, only rebuilding what changed |
| `idf.py -p PORT flash` | Builds if needed, then writes to the board |
| `idf.py -p PORT app-flash` | Writes only the app, which is faster when the bootloader and partitions haven't changed |
| `idf.py -p PORT monitor` | Shows the board's serial output. Quit with `Ctrl+]` |
| `idf.py -p PORT flash monitor` | Flashes, then starts the monitor. This is the everyday one |
| `idf.py -p PORT erase-flash` | Wipes the whole flash |
| `idf.py menuconfig` | Opens the settings menu |
| `idf.py size` | Shows how much flash and RAM the app uses |
| `idf.py size-components` | The same, broken down by component |
| `idf.py reconfigure` | Re-runs CMake and downloads components, for example after editing `idf_component.yml` |
| `idf.py update-dependencies` | Updates components within the versions in `idf_component.yml` |
| `idf.py fullclean` | Deletes the build output. Try this when a build is confused |

**Settings.** `sdkconfig.defaults` holds the settings for this board and is committed. `sdkconfig` is generated from
it the first time you build, and is not committed. Changes in `menuconfig` go to `sdkconfig`, so they're lost if it's
deleted. To change a setting for good, edit `sdkconfig.defaults`, delete `sdkconfig`, and build. `sdkconfig.defaults`
is not read again while `sdkconfig` exists.

**VS Code.** Espressif has an [ESP-IDF extension](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension)
that wraps these commands in buttons, and it's recommended in `.vscode/extensions.json`.

## Debugging

**Logs.** Each file logs with a tag: `LIGHTS`, `MQTT`, `RING`, and `WIFI_RGB_TREE`. The default level is Info, so
`ESP_LOGD` lines, including every incoming MQTT message, are hidden. To see them, add this to `sdkconfig.defaults`,
delete `sdkconfig`, and rebuild:

```
CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y
```

To see them from one part of the code only, set the maximum level to Debug in `menuconfig` (Component config > Log)
and leave the default at Info. Then turn on the tag you care about, for example in `app_main`:

```cpp
esp_log_level_set("MQTT", ESP_LOG_DEBUG);
```

**Watching MQTT.** Often the quickest way to find a problem is to see what's on the broker:

```shell
mosquitto_sub -h <broker> -u <user> -P <password> -v -t 'rgb_tree/#'
```

**Crashes.** When the firmware crashes it prints a `Guru Meditation Error` and a backtrace, then reboots. If you're
running `idf.py monitor`, it translates the backtrace into function names and line numbers for you. Keep the firmware
you flashed in `build/`, because that's how it finds them.

**Core dumps.** A core dump also captures every task's state at the moment of the crash. The simplest setup needs no
extra partition: in `menuconfig`, go to Component config > Core dump and set the destination to UART. The monitor
decodes the dump when a crash happens. To keep dumps across reboots, choose Flash instead, add a `coredump` partition
with a custom partition table, and read a dump with `idf.py coredump-info` or `idf.py coredump-debug`. See
[Espressif's core dump guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/core_dump.html).

**Stack overflows.** Each task has a fixed stack size, set in `lights.cpp` and `mqtt.cpp`. An overflow crashes with a
`stack overflow` message that names the task. To see how close a task is getting, call this from inside it:

```cpp
ESP_LOGI(TAG, "Stack left: %u", uxTaskGetStackHighWaterMark(NULL));
```

**JTAG (`idf.py openocd`, `idf.py gdb`).** You'll rarely need this here. This board has no built-in JTAG, so it needs
an external probe such as an ESP-Prog, and JTAG uses GPIO 12 to 15. That includes the NeoPixel data pin (14), so the
ring has to be disconnected while debugging this way. Use logs and core dumps first.

## Tests

The light logic and the message parsing have no hardware in them, so they can run on your computer:

```shell
idf.py reconfigure   # Only needed once, downloads ArduinoJson
scripts/test-host.sh
```

The tests are in `test/host/tests.cpp`. They can't test the real hardware, wifi, or the broker.

## How it works

```
Core 0                                   Core 1
WiFi driver
MQTT task  --parse--> command queue ---> lights task  (sole owner of ring and GPIO)
status task <-------- latest state <---- wakes on a command or the next frame
   | publishes state and discovery
```

**The main idea: animations aren't loops.** A function like this can't be interrupted, because nothing else can run
in that task until it returns:

```cpp
for (int i = 0; i < steps; i++) {
    drawFrame(i);
    vTaskDelay(10);
}
```

Instead, the lights task (`lights.cpp`) waits on its queue with a timeout of one frame. Either a command arrives and
is applied straight away, or the timeout runs out and it's time to draw. Either way it draws one frame and goes back
to waiting. Since it's never stuck in the middle of an animation, a color can break the loop right away, and twinkle
commands are just a GPIO write in between frames.

**`LightState` decides what to show.** You give it the time, and it returns the color for that moment. Working from
the clock means a late frame can't stretch a fade. It has no hardware or FreeRTOS code, which is what makes it
testable on a computer.

**Only the lights task touches the lights.** With a single owner there's nothing to lock, so there's no mutex.

**Only the status task publishes.** The MQTT client holds a lock while it reconnects, for up to 10 seconds, and every
publish waits on that lock. If the lights task published, the animation would freeze whenever the broker dropped. So
the lights task leaves its latest state in a one-slot queue, and the status task does the waiting.

**The MQTT event handler never waits.** It runs on the MQTT task, so if it blocks, keepalives stop and the broker
disconnects the device. It turns each message into a `Command_t` and uses `xQueueSend` with a timeout of 0.

**Adding another protocol.** `commands.cpp` and `lights.h` know nothing about MQTT. Another protocol such as CoAP
would turn its messages into a `Command_t`, call `lightsSend()`, and get a line in `reportStatus()` in `lights.cpp`.

## MQTT reference

`<base>` is `TOPIC_BASE` from `config.h`.

| Topic | Direction | Payload |
|---|---|---|
| `<base>/light/set` | To device | JSON, see below |
| `<base>/light/state` | From device, retained | JSON, see below |
| `<base>/twinkle/set` | To device | `ON` or `OFF` |
| `<base>/twinkle/state` | From device, retained | `ON` or `OFF` |
| `<base>/availability` | From device, retained | `online` or `offline` |
| `homeassistant/device/<DEVICE_ID>/config` | From device | Home Assistant discovery |
| `homeassistant/status` | To device | `online` makes the device send discovery and its state again |

State is retained, so a client that subscribes gets the current state immediately, and there's no need to ask for it.
`availability` goes to `offline` automatically if the device disappears, within about 45 seconds.

### Light command

Send to `<base>/light/set`. Every key is optional, and a message only changes what it mentions. The limit is 256
bytes. A message with an invalid value is ignored completely, and a warning is logged.

```json
{"state": "ON", "color": {"r": 255, "g": 0, "b": 0}, "brightness": 255, "transition": 2, "effect": "colorloop"}
```

| Key | Values | Notes |
|---|---|---|
| `state` | `"ON"`, `"OFF"` | |
| `color` | `{"r", "g", "b"}`, each 0 to 255 | All three are required |
| `brightness` | 0 to 255 | Applied on top of the color or loop |
| `transition` | 0 to 3600 | Fade time in seconds, decimals are fine |
| `effect` | `"colorloop"`, `"solid"` | |

| You send | What happens |
|---|---|
| A color | Stops the loop and goes to that color, turning the ring on if it was off |
| A color during a fade | Starts from whatever is showing right now |
| `effect: colorloop` | Starts the loop, turning the ring on if it was off |
| `effect: solid` | Stops the loop and goes back to the last color |
| `brightness` alone | Changes brightness, and the color or loop keeps running |
| `state: OFF` | Turns the ring off, remembering the color and whether the loop was running |
| `state: ON` alone | Brings back the last color or loop |

Without a `transition`, changes are instant, except that the loop eases in over 1 second. The loop's speed is
`LOOP_CYCLE_MS` in `config.h`, 30 seconds per full cycle by default.

### Light state

Published to `<base>/light/state` after every change, and on connecting.

```json
{"state": "ON", "color_mode": "rgb", "brightness": 255, "effect": "solid", "color": {"r": 255, "g": 0, "b": 0}}
```

`color` is the last color that was set, not what's showing during the loop. `brightness` is reported separately, so
`color` isn't dimmed by it.

### Examples

These assume `TOPIC_BASE` is `rgb_tree`. Add `-u <user> -P <password>` if your broker has a login.

```shell
# Set a color
mosquitto_pub -h <broker> -t rgb_tree/light/set -m '{"color":{"r":255,"g":0,"b":0}}'

# Fade to blue over 2 seconds
mosquitto_pub -h <broker> -t rgb_tree/light/set -m '{"color":{"r":0,"g":0,"b":255},"transition":2}'

# Start the color loop
mosquitto_pub -h <broker> -t rgb_tree/light/set -m '{"effect":"colorloop"}'

# Switch the twinkle lights while the loop keeps running
mosquitto_pub -h <broker> -t rgb_tree/twinkle/set -m ON

# Turn the ring off, then bring it back
mosquitto_pub -h <broker> -t rgb_tree/light/set -m '{"state":"OFF"}'
mosquitto_pub -h <broker> -t rgb_tree/light/set -m '{"state":"ON"}'

# Watch everything the device says
mosquitto_sub -h <broker> -v -t 'rgb_tree/#'
```

## Updating an app from the old format

The old topics and payloads are gone. An app using them will stop working until it's updated.

| Old | New |
|---|---|
| `SUB_SET_COLOR` with `{"r":255,"g":0,"b":0}` | `<base>/light/set` with `{"color":{"r":255,"g":0,"b":0}}` |
| `"time": 100` (counted 10 ms steps, so 1 second) | `"transition": 1` (seconds) |
| `SUB_GET_COLOR`, then read `PUB_GET_COLOR` | Subscribe to `<base>/light/state`, it's retained and updates itself |
| `SUB_SET_TW_LIGHTS` with `{"tw1":1}` | `ON` or `OFF` to `<base>/twinkle/set` |
| `SUB_GET_TW_LIGHTS`, then read `PUB_GET_TW_LIGHTS` | Subscribe to `<base>/twinkle/state` |
| Nothing | Start the loop with `{"effect":"colorloop"}` |
| Nothing | Subscribe to `<base>/availability` to see if the device is online |

Your app should subscribe to the `state` topics and update its display from them, not assume a command worked.

## Home Assistant

The device announces itself using
[MQTT discovery](https://www.home-assistant.io/integrations/mqtt/), so there's no YAML to write.

1. Set up the [MQTT integration](https://www.home-assistant.io/integrations/mqtt/) in Home Assistant, if you haven't.
2. Point the device at the same broker. Home Assistant's Mosquitto add-on needs a login, so set `MQTT_USER` and
   `MQTT_PASS` in `config.h`.
3. Flash the device and let it connect.
4. Look under Settings > Devices & services > MQTT for a device called `DEVICE_NAME`.

It shows up with these entities:

| Entity | Type | Controls |
|---|---|---|
| Ring | Light | On/off, color, brightness, and transition. The Effect list has `colorloop` and `solid` |
| Twinkle | Switch | The twinkle lights |

Picking a color while `colorloop` is running breaks the loop. Both show as unavailable when the device goes
offline. The device re-announces itself whenever Home Assistant restarts.

To remove it, delete the device in Home Assistant. It'll be added again the next time the device connects unless you
change `DEVICE_ID` or stop the device from running.

### Manual setup

If you've turned off MQTT discovery in Home Assistant, you can add it in `configuration.yaml` instead. Don't do this
with discovery on, or you'll get a duplicate of everything. Replace `rgb_tree` with your `TOPIC_BASE`.

```yaml
mqtt:
  - light:
      schema: json
      name: Tree ring
      unique_id: rgb_tree_ring
      command_topic: rgb_tree/light/set
      state_topic: rgb_tree/light/state
      availability_topic: rgb_tree/availability
      brightness: true
      flash: false
      supported_color_modes: ["rgb"]
      effect: true
      effect_list: ["colorloop", "solid"]
  - switch:
      name: Tree twinkle
      unique_id: rgb_tree_twinkle
      command_topic: rgb_tree/twinkle/set
      state_topic: rgb_tree/twinkle/state
      availability_topic: rgb_tree/availability
```
