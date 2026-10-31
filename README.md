# feather-mqtt-rgb-tree
An mqtt powered rgb led tree

Firmware for the Adafruit HUZZAH32 ESP32 Feather, built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/) v6.1 or newer.

## Setup

Install ESP-IDF and load it into your shell:

```shell
brew install cmake ninja
git clone -b v6.1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32
. ~/esp/esp-idf/export.sh
```

Create your config, then fill in your wifi, mqtt, and topic settings:

```shell
cp main/include/config_example.h main/include/config.h
```

## Build and flash

```shell
idf.py build
idf.py -p <serial port> flash monitor
```

Board settings live in `sdkconfig.defaults`. `sdkconfig` is generated from it, so delete `sdkconfig` after changing the
defaults. If the board last ran the PlatformIO/Arduino firmware, run `idf.py -p <serial port> erase-flash` once first,
because the partition table has changed.
