# About this fork

This is a personal fork of [aly-fly/EleksTubeHAX](https://github.com/aly-fly/EleksTubeHAX),
maintained for a single IPSTube clock (`IPSTubeA_III` board, ESP32-S3) driven by Home Assistant
over MQTT.

## AI-assisted code

**The changes in this fork are written with an AI coding assistant (Claude Code).** They are
tested on one real clock, not on the other supported models.

If you do not want AI-generated code, do not use this fork: use the upstream project instead.

## What differs from upstream

Work happens on the `nacolada` branch, based on upstream `v1.3.15`.

| Change | Files | Status |
|---|---|---|
| Hostname set before `WiFi.mode()`, so the DHCP name no longer alternates | `src/WiFi_WPS.cpp` | done |
| Root CA of the maintainer's own MQTT broker | `data/mqtt-ca-root.pem` | done, useless to anyone else |
| One LED colour per tube, driven over MQTT | to come | planned |
| Show an arbitrary number or a short message, then return to the time | to come | planned |
| Countdown / stopwatch | to come | considered |

## Building the TLS firmware used here

`include/_USER_DEFINES.h` is ignored by git, as upstream intends. To reproduce the build:

1. Copy `include/_USER_DEFINES - empty.h` to `include/_USER_DEFINES.h`.
2. In the Home Assistant section, uncomment `#define MQTT_USE_TLS`.
3. Replace `data/mqtt-ca-root.pem` with the root CA of your own broker.
4. Build the `IPSTube_S3` environment, with `WEB_SETUP_PORTAL` left enabled in `platformio.ini`.

Two things learned the hard way:

- The broker port lives in the NVS, written by the setup portal. Flash the application at
  `0x10000` and the LittleFS image at `0x140000`. The full `FW_*.bin` image at `0x0` overwrites
  the NVS, and the WiFi and MQTT settings with it.
- The SDK ships mbedTLS 2.28, which only matches the server name against the `DNS:` entries of
  the certificate, never the `IP:` ones. If the clock connects by IP address, the address must
  also be present as a `DNS:` entry in the broker certificate.
