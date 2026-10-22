# Local Wi-Fi setup

When Wi-Fi is disabled or no SSID is configured, Camper Sentinel starts a setup
access point immediately. With configured Wi-Fi, it starts after 30 seconds
without a connection, including after an established connection is lost.
Cellular connectivity does not prevent the setup access point from starting.

1. Join `CamperSentinel-XXXXXX` from your phone or laptop.
2. Use password `Camper-XXXXXX`, replacing `XXXXXX` with the same six uppercase
   characters shown at the end of the network name. The serial log also shows
   the network name, password, and settings URL.
3. Stay connected if your phone warns that the network has no internet.
4. Open `http://192.168.4.1/settings/connectivity` in your browser.
5. Enable Wi-Fi, enter your router's SSID and password, and save.

The firmware tries the saved credentials automatically. The setup access point
stays available until reboot, even after the router connection succeeds. After
reboot, it starts again only when Wi-Fi is unavailable. Diagnostics and the
connectivity settings page show whether the setup access point is active.

The hotspot provides local settings and dashboard access, not internet sharing.
The password is derived from the device's MAC address for convenient recovery;
anyone who knows the network name can derive it, so it is not a private secret.

## MQTT transport recovery

MQTT normally prefers router Wi-Fi. After repeated Wi-Fi MQTT connection
failures, it prefers available cellular data for five minutes. Before ending a
working cellular MQTT session to retry Wi-Fi, it now resolves the broker name
through Wi-Fi. A failed DNS probe leaves cellular connected and starts another
five-minute delay. The log reports `WiFi broker DNS probe failed; keeping cellular
MQTT connected and retrying WiFi in five minutes`.

DNS success permits the normal switch back to Wi-Fi; it does not prove TCP/TLS
will succeed. This preflight prevents the observed DNS-failure reconnect cycle,
not every possible switch-back interruption. The SDK DNS lookup is synchronous
and may briefly delay the main loop while cellular remains connected.

Two consecutive failed Wi-Fi DNS probes while cellular MQTT remains connected
request a Wi-Fi driver restart/rejoin, even when the station still reports
`WL_CONNECTED`. Probes are five minutes apart, so this normally requires around
ten minutes of continued failure after fallback. A successful Wi-Fi MQTT
connection or a new join clears this count. Restarts are limited to once per
five minutes and deferred while a phone/laptop is connected to the setup hotspot.
The same recovery path still handles five minutes of reported disconnection.

This is a recovery heuristic: DNS failure does not itself prove the LAN is
unreachable, but repeated failures are used to recover the observed condition
where ping and HTTP also stop responding. A DNS-only outage can therefore also
cause a Wi-Fi rejoin. Local Wi-Fi sessions briefly disconnect during recovery;
the code does not stop the active cellular MQTT transport or reboot the device.
Driver recovery can still fail in a faulty SDK/radio and needs hardware testing.
The serial log prints the station IP, gateway, DNS, RSSI and recovery reason.
Check the reported IP after rejoining because DHCP may assign a different address.
`/health` exposes `wifi_driver_recoveries`, `wifi_network_failures`, and
`wifi_recovery_pending` when reachable.

HTML streaming sends each chunk's framing and payload together and explicitly
clears the library's chunked-response flag when finishing or aborting a send.
This prevents a second terminator and the library's separate footer write from
bypassing the HTTP timeout. This framing correction does not establish the cause
of Bluetooth failures or all socket stalls. `/health` also reports
`internal_free_heap` and `internal_largest_block` for byte-addressable internal
RAM, which can be lower than the general ESP32 heap figures.

## Hardware verification

- Boot with no saved SSID: confirm the hotspot appears and settings load.
- Boot with an unavailable SSID: confirm the hotspot appears after 30 seconds
  of firmware Wi-Fi processing.
- Save valid credentials through the hotspot: confirm router Wi-Fi and MQTT
  connect, and the hotspot settings remain reachable.
- Turn off the router: confirm local access remains available and cellular MQTT
  still works when cellular data is available.
- Reboot with the router available: confirm station connection succeeds and
  the setup hotspot stays off.
- Disable Wi-Fi in settings: confirm the setup hotspot remains available.
