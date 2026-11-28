# esp-libs

ESP-IDF components shared by the ESP32 firmwares. Each folder is one component.

| Component | What it holds | Depends on |
|---|---|---|
| `esp-support` | The base: `Mutex`, `Queue`, `NVSProperty`, `Bounce2`, and a CMake helper for raw partition images | — |
| `esp-network-support` | Wi-Fi with `NetworkConnection`, and HTTP helpers | `esp-support` |
| `esp-mqtt-support` | `MQTTConnection`, and Home Assistant's MQTT discovery | `esp-support` |
| `esp-mdm` | Device management through IoT Support: provisioning, device configuration, OTA, coredump upload and the log sink | `esp-mqtt-support`, `esp-network-support`, `esp-support` |
| `esp-audio-support` | A UDP stream of 16-bit mono PCM, buffered against jitter and played on I2S | `esp-support` |
| `esp-sensor-support` | Sensor drivers: SGP40 with Sensirion's VOC index, ACS725, an LDR, SEN0626 and DFRobot's gesture and face sensor | `esp-support` |
| `esp-light-algorithms` | Light and LED channel control | `esp-support` |
| `esp-status-led-support` | A status LED, plain or WS2812 | `esp-support`, `led_strip` |
| `esp-zha-support` | ZigBee Home Automation devices | `esp-support`, Espressif's ZigBee libraries |

A component with its own README has the details there.

## Using it from a firmware

- **The checkout sits beside the firmware's repository**, in a KubeCoder environment and in
  Jenkins alike. The firmware's `main/idf_component.yml` takes each component as a path
  dependency, `path: ../../esp-libs/<component>`.
- **The sdkconfig defaults come along.** A component with a `sdkconfig.defaults` expects the
  firmware's top-level `CMakeLists.txt` to list that file in `SDKCONFIG_DEFAULTS`, before the
  firmware's own.
- **A firmware takes a change here at its own next build.** Its Jenkinsfile clones this repository
  with `changelog: false, poll: false`, so a push here starts no firmware build. It also stays
  out of the firmware builds' change sets.
