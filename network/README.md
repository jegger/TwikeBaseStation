# Network setup

## Overview

| Device | IP | How it gets it | Access |
|---|---|---|---|
| Swisscom Internet-Box (main router) | `192.168.1.1` | – | `https://internetbox.swisscom.ch` |
| TP-Link TL-WR902AC (Wi-Fi range extender) | `192.168.1.115` | DHCP | – |
| Teltonika RUT140 (Wi-Fi client + bridge) | `192.168.1.131` | DHCP from the Internet-Box | `https://192.168.1.131` (home Wi-Fi) |
| RUT140 rescue access | `192.168.2.1` | static (RUT's own LAN) | `https://192.168.2.1`, Mac with manual IP `192.168.2.10` |
| ESP32-S3 Twike controller (RUT **WAN** port) | `192.168.1.210` | **static**, set in [twike-charger.yaml](../esphome/twike-charger.yaml) | ESPHome OTA port 3232 |
| Shelly Pro EM-50 (RUT **LAN** port) | `192.168.1.211` | **static**, set in the Shelly UI | `http://192.168.1.211` |

- Home network: `192.168.1.0/24`, gateway and DNS `192.168.1.1`, Internet-Box DHCP range approx. `.100–.199`
- Static IPs live **outside** the DHCP range (`.200+`)
- Passwords (RUT admin, RUT Wi-Fi, home Wi-Fi) are in the password manager, not in this repo

```
Internet-Box ── Wi-Fi ── TP-Link extender ── Wi-Fi ── RUT140 ──┬─ WAN port → ESP    (192.168.1.210)
 192.168.1.1                .115                   .131  │     └─ LAN port → Shelly (192.168.1.211)
                                                          └─ own Wi-Fi "RUT140_xxxx" (bridged)
```

### Why static IPs

The RUT140 bridges its Ethernet ports into the home network with **Relayd**.
Normal traffic passes fine, but DHCP doesn't make it through Relayd + the
TP-Link extender (both rewrite MAC addresses). So the ESP and the Shelly use
static IPs, and everything behind the RUT shows up in the Internet-Box with
the same MAC (`9a:25:4a:c3:cd:ce`), which also mixes up the host names shown
there.

## RUT140 setup from scratch (Wi-Fi only)

Firmware at the time of writing: `RUT14X_R_00.07.24.7`. Menu names may differ
slightly on other versions.

### 1. Factory reset

- Hold the reset button ~6 s until the LEDs signal a reset, wait 2–3 min
- Unplug ESP and Shelly for now

### 2. First login

1. On the Mac, disable **Auto-Join** for the home Wi-Fi
2. Join `RUT140_xxxx` (password on the label)
3. Open `https://192.168.1.1`, log in as `admin` with the label password
4. Wizard: set a new strong admin password, time zone Europe/Zurich, skip the rest

### 3. Move the RUT's own LAN

1. **Network → LAN → lan → Edit**: IPv4 `192.168.2.1`, netmask `255.255.255.0` → Save & Apply
2. Reconnect to `RUT140_xxxx`, open `https://192.168.2.1`

### 4. WAN port as LAN

1. **Network → LAN** (edit `lan`): enable **Use WAN port as LAN** → Save & Apply

### 5. Join the home Wi-Fi as client

1. **Network → Wireless → SSIDs → Scan** (2.4 GHz) → pick the home Wi-Fi
   (currently the TP-Link extender `…_Ext`) → **Join network**, enter password
2. Mode `Client`, Network: WAN side (creates interface `wan1` / `wifi1`) → Save & Apply
3. Reconnect to `RUT140_xxxx` (the radio may switch channel)
4. **Status → Network**: note the client's `192.168.1.x` address (→ admin address)

### 6. Install Relayd

1. **System → Package Manager**: install **Relay configuration**

### 7. Enable remote HTTPS

1. **System → Administration → Access Control**: HTTPS **Remote access: on** (port 443) → Save & Apply

### 8. Fixed IP on the Mac (DHCP goes off next)

1. System Settings → Wi-Fi → `RUT140_xxxx` → Details → TCP/IP:
   Manually, `192.168.2.10`, mask `255.255.255.0`, router `192.168.2.1`
2. Check `https://192.168.2.1` still opens

### 9. Disable the RUT's DHCP server

Relayd refuses to start while it is on.

1. **Network → LAN → lan → Edit**: disable **DHCPv4** and **DHCPv6** → Save & Apply

### 10. Enable Relayd

1. **Network → Wireless → Relayd**: Interface `lan`, Wireless interface
   `wifi1 (<home Wi-Fi>)`, **Enabled** → Save & Apply

This moves `wan1` into a new firewall zone **`relayd`** (input: reject).

### 11. Allow the admin console from the home Wi-Fi

The Access Control rule from step 7 only covers zone `wan`, so it doesn't
apply to `relayd`.

1. **Network → Firewall → Traffic Rules → Add new instance**:
   type **Open ports on router**, name `Allow-HTTPS-relayd`, **TCP**, **HTTPS (443)** → **Add**
2. Edit the new rule: **Source zone `relayd`**, destination **Device**, action **Accept**
3. Make sure the rule is switched **on** (it can be added disabled) → **Save & Apply**
4. Check: the row reads *From relayd → To Device port 443*, and
   `https://192.168.1.131` opens from the home Wi-Fi

Alternative: **Network → Firewall → Zones → relayd → Input: Accept**
(opens all RUT services to the home network).

### 12. Devices

1. ESP → **WAN** port, Shelly → **LAN** port
2. Shelly: in its web UI (network settings → Ethernet) set static
   `192.168.1.211`, mask `255.255.255.0`, gateway `192.168.1.1`, DNS `192.168.1.1`
   → **reboot** the Shelly (the new IP only applies after a restart)
3. ESP: static IP is in the firmware (`manual_ip` in
   [twike-charger.yaml](../esphome/twike-charger.yaml)), flash once over USB
4. Mac: switch `RUT140_xxxx` back to **Using DHCP**, re-enable Auto-Join for the home Wi-Fi

### 13. Verify (from the home Wi-Fi)

```sh
curl -sk -o /dev/null -w "%{http_code}\n" https://192.168.1.131/                # RUT: 200
curl -s http://192.168.1.211/rpc/Shelly.GetDeviceInfo | jq -r .id                # Shelly
nc -z -G 3 192.168.1.210 3232 && echo "ESP OTA reachable"                        # ESP
```

## Rescue access

If the home Wi-Fi is down (or the RUT's client IP changed), join
`RUT140_xxxx`, set the Mac's IP manually to `192.168.2.10` / `255.255.255.0` /
router `192.168.2.1`, and open `https://192.168.2.1`.

To find the RUT on the home network instead:

```sh
for i in $(seq 1 254); do ping -c1 -W300 192.168.1.$i >/dev/null 2>&1 & done; wait
arp -an | grep 9a:25:4a:c3:cd:ce     # all devices behind the RUT share this MAC
```

## Notes

- The RUT's `192.168.1.131` comes from DHCP and may change. A reservation in
  the Internet-Box may not stick because of the rewritten MAC; if it moves,
  find it with the scan above or via rescue access.
- If the RUT ever connects to the Internet-Box directly (without the TP-Link
  extender), DHCP through Relayd might work again, but the static IPs keep
  working either way.
- Teltonika docs: [Relayd / Wi-Fi bridge](https://wiki.teltonika-networks.com/view/RUTOS_relay_/_Wi-Fi_extender_/_repeater_/_bridge_configuration),
  [RUT140 LAN](https://wiki.teltonika-networks.com/view/RUT140_LAN)
