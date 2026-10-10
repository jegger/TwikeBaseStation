# Twike charger – ESPHome firmware

## Setup

```sh
cd esphome
cp secrets.example.yaml secrets.yaml   # fill in broker host + passwords
uvx esphome config twike-charger.yaml  # validate
```

## First flash (USB)

**Do the first boot with the charger/contactors disconnected** and check with
a multimeter that DO1–DO3 stay off.

```sh
uvx esphome run twike-charger.yaml --device /dev/cu.usbmodem*
```

If the board doesn't show up, hold BOOT while plugging in USB.

Then, with the Ethernet cable plugged in, watch the log for:
- `ethernet` getting an IP via DHCP (if not: check the W5500 pins)
- `mqtt` connecting over TLS (if TLS fails: check the broker host and the clock)

## Check the topics

Subscribe to `twike-charger/#` (EMQX web console or `mosquitto_sub`):

| Topic | Payload |
|---|---|
| `twike-charger/status` | `online` / `offline` (retained) |
| `twike-charger/switch/charging/command` | publish `ON` / `OFF` |
| `twike-charger/switch/charging/state` | `ON` / `OFF` |
| `twike-charger/switch/heating/command` | publish `ON` / `OFF` |
| `twike-charger/switch/heating/state` | `ON` / `OFF` |
| `twike-charger/binary_sensor/twike_plugged_in/state` | `ON` / `OFF` |
| `twike-charger/text_sensor/charger_state/state` | `Not plugged in` / `Idle` / `Starting / stopping` / `Charging` |
| `twike-charger/sensor/power/state` | live power in W (from the Shelly, every 30 s) |
| `twike-charger/sensor/current/state` | live current in A (from the Shelly, every 30 s) |
| `twike-charger/sensor/energy_today/state` | Wh used today so far |
| `twike-charger/sensor/energy_total/state` | Shelly lifetime counter in Wh |
| `twike-charger/energy/day/YYYY-MM-DD` | Wh used on that day (retained, one per finished day) |
| `twike-charger/energy/current_history` | `{"end":<unix ts>,"step":300,"v":[...]}`: 5 min average current in A for the last 24 h (retained, updated every 5 min) |

**In IoT MQTT Panel, turn retain OFF for the command topics.** The ESP also
clears retained commands and ignores commands for 5 s after each connect,
so a stale retained `ON` can't restart charging after a reboot.

## Later updates (OTA over the LAN)

```sh
uvx esphome run twike-charger.yaml --device twike-charger.local   # or the IP
```

## Behaviour

- Charging ON works only when plugged in: DO1 on → 5 s → re-check → DO2 on
- Charging OFF: DO2 off → 2 s → DO1 off
- Unplugging stops charging and turns heating off right away
- Hard caps on the ESP: charging stops after 10 h, heating after 2 h
  (the `max_*_time` substitutions)
- Reboot or power loss → all outputs off; charging does **not** resume

## Tests

The current history logic lives in [sample_history.h](sample_history.h) and is
tested off-device:

```sh
cd tests && c++ -std=c++17 -I.. test_sample_history.cpp -o /tmp/t && /tmp/t
```
