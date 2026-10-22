# Two Renogy Pro Bluetooth batteries

## Setup

1. Flash this firmware and close the Renogy app so its battery connections are
   released. Keep Camper Sentinel within Bluetooth range of both batteries.
2. Open **Settings → Bluetooth Batteries** (`/settings/batteries`) using your
   router Wi-Fi or Camper Sentinel's setup access point.
3. Wait for discovery, then refresh the page. It lists nearby named Bluetooth
   devices and their addresses; the list can include devices other than batteries.
4. Copy the first battery's address into Battery 1 and the second into Battery 2.
   Device ID defaults to 255, the direct-device address used by the reference
   reader. Leave it at 255 unless your battery requires a different address.
5. Save. Assignments persist across reboot and apply without restarting.
6. Wait for the next polling cycle and check the dashboard. The setup page
   shows a connection, model, or protocol error if a battery cannot be read.

An unused slot can be left blank. Both slots must have different addresses.
Bluetooth monitoring can be disabled from this page.

## OLED pages

Press the display button to cycle through Ambient, Renogy Battery 1, Renogy
Battery 2, Connectivity, Network and System. Pressing it while the display is
asleep wakes the current page; the next press advances it.

Each battery page shows charge percentage, sample age, voltage, signed current,
remaining/capacity Ah and temperature in Fahrenheit. `CACHED` means the latest
poll failed but the last successful sample is still within the 12-minute limit;
its displayed age keeps increasing. Expired or missing readings show
`Data unavailable`, and an unassigned slot shows `Not assigned`.

The Network page shows Wi-Fi signal, cellular data status, the MQTT transport,
and the Wi-Fi IP when connected (otherwise setup AP or cellular IP). The System
page's `RAM free` value uses internal byte-addressable RAM. The Ambient page's
`BACKUP` voltage is the board's existing power input reading, separate from the
two Renogy battery pages.

## Readings and MQTT

Each battery has independent voltage, signed current, remaining Ah, capacity Ah,
charge percentage calculated from remaining/capacity, and the first reported
temperature sensor in Celsius. Heater status and control are not implemented.

Bluetooth scanning and sequential battery reads run in a background task.
The first scan/poll starts after a 30-second network startup grace period. The task then waits five
minutes after completing each cycle; connection delays and
read timeouts can make the actual interval longer. Reads use Modbus function 3;
the firmware does not change BMS settings or heater operation.

## NimBLE transport

The firmware uses pinned NimBLE-Arduino 2.3.9, with only central and observer
roles enabled and one connection at a time. The Arduino Bluedroid BLE library
is excluded from dependency selection. Saved battery assignments, protocol
decoding, settings pages, MQTT topics and HA entity names remain the same.

Scanning uses NimBLE's asynchronous scanner, bounded to five seconds with a
seven-second application deadline. Up to 32 scan results are retained until
the polling cycle ends; the settings page lists up to 12 named candidates.
Connections, discovery, CCCD writes and Modbus reads use asynchronous native
NimBLE APIs. No per-battery wrapper service/characteristic trees are retained.
Discovery stores at most 16 service ranges and only the needed GATT handles.
Notification fragments are copied from mbuf chains into a fixed eight-packet
queue and validated with the existing Modbus CRC checks. An overflow rejects
the read rather than accepting truncated data.

Connection attempts have a 10-second controller timeout and 11-second outer
deadline. Complete service/characteristic/descriptor discovery has one shared
15-second deadline. CCCD/write acknowledgments and each register response wait
at most five seconds; disconnect waits at most three seconds. All waits stay
in the Bluetooth worker and never force-delete a task inside the host stack.
Operation tokens and connection handles reject late GATT callbacks.
GAP callbacks also carry a unique connection-attempt token. A disconnect for
the current peer clears a pending opening even if no successful CONNECT callback
arrived first. Retired-attempt callbacks cannot clear a newer opening; a late
successful retired connection is terminated. GAP connect/disconnect status and
cancel return codes are logged to diagnose radio/establishment failures.

On timeout or memory pressure, the transport requests cancellation of a pending
connection or termination of an active link. Retry requires confirmed idle
connection state, no host connection attempt or scan, five seconds of settling,
and sufficient internal RAM. Backoff is 60, 120, 240, then 300 seconds. Three
fully successful cycles reset the backoff. NimBLE does not use the former
Bluedroid application-unregister/host-restart worker. If native cancellation or
disconnection never completes, polling stays paused; a reboot may be required.
Neither Wi-Fi nor MQTT is explicitly stopped by Bluetooth recovery.

The previous internal-RAM guards remain: at least 20,000 bytes free and a 6 KB
contiguous block before connecting, and at least 8 KB free plus a 2 KB block
during operations. The figures use internal byte-addressable memory, not the
general ESP heap. These cooperative checks cannot reserve memory against a
concurrent SDK allocation. Low-memory reads are cancelled and cleaned up.

Battery order alternates and connections are separated by ten seconds for
networking to settle. A completed-cycle status reports how many assigned
batteries actually updated, for example `NimBLE poll: 2/2 batteries updated`;
`0/2` does not indicate successful battery reads. Five-minute polling and
12-minute data freshness remain in effect. Replacing the stack is intended to
reduce memory pressure. The hardware report below records the current baseline;
additional failure-mode testing is still required.

### Reported working baseline (2026-10-22)

The owner reported solid operation after 143,743 seconds (almost 40 hours):
both batteries updated, MQTT connected over Wi-Fi, zero Wi-Fi disconnects or
driver recoveries, 90,964 bytes of internal free heap and a minimum of 38,008
bytes since boot. The largest internal free block was 86,004 bytes. This is an
observed endurance result, not a controlled test of every failure/recovery path.

Missed polls retain the last successful sample for up to 12 minutes, allowing
one missed five-minute poll with margin for scanning and recovery. Its age
continues increasing; missed polls never refresh its timestamp. After 12 minutes
the readings become unavailable. The dashboard and MQTT `last_poll_error` field
show the last polling failure even while a recent cached sample is available.
A battery must report model
`RBT12200LFP-BT...` before telemetry is accepted; confirm the physical model label
if a different model appears.

MQTT topics (retained, refreshed every 30 seconds when MQTT is connected):

- `rvmonitor/camper01/battery/1/state`
- `rvmonitor/camper01/battery/1/availability`
- `rvmonitor/camper01/battery/2/state`
- `rvmonitor/camper01/battery/2/availability`

State payloads contain `address`, `model`, and `valid`. Valid readings add
`voltage`, `current`, `remaining_ah`, `capacity_ah`, `charge_percent`,
`reading_age_seconds`, and, when available, `temperature_c`. Unavailable payloads
omit measurements and include an error. Home Assistant discovery creates five
entities per slot and combines battery availability with Camper Sentinel's
MQTT availability; entities expire after 12 minutes without MQTT updates.
The cached battery state is still published every 30 seconds; this does not
initiate another Bluetooth poll or make an old sample fresh. MQTT device
availability still applies independently. The slower Bluetooth rate reduces
connection attempts but does not repair the underlying SDK failure. Current
and charge readings can now lag physical changes by about five minutes.

## Protocol references

This is an independent C++ implementation of the register layout and GATT IDs
documented by the open-source reader, not an imported Python library:

- https://github.com/cyrils/renogy-bt/blob/main/renogybt/BatteryClient.py
- https://github.com/cyrils/renogy-bt/blob/main/renogybt/BaseClient.py
- https://github.com/cyrils/renogy-bt/blob/main/config.ini

Read requests use write service FFD0 / characteristic FFD1 and notifications
from FFF1. Model is read at register 5122 (8 words), battery telemetry at 5042
(6 words), and temperature sensors at 5017 (17 words). Packets are reassembled
across notifications and checked for device ID, function, byte count, and Modbus
CRC16 before decoding.

## Hardware verification still required

- Flash the new firmware and confirm startup identifies NimBLE-Arduino 2.3.9.
- Close the phone app. Confirm both saved battery addresses remain assigned.
- After the 30-second grace period, verify both model/telemetry reads, comparing
  voltage, current, charge and temperature against the Renogy app in separate
  sessions. Confirm each physical battery's slot assignment.
- Collect `/health` after the first poll and after several five-minute cycles.
  Confirm `bluetooth_stack` is `NimBLE-Arduino 2.3.9`. Compare
  `internal_free_heap`, `internal_largest_block` and `internal_minimum_heap`;
  the minimum is the SDK's low-water figure since boot, not just the latest poll.
  Compare internal byte-addressable free RAM while Wi-Fi MQTT and the dashboard
  are in use. Watch discovery memory logs for each battery's active connection.
- Verify repeated page refreshes, ambient MQTT, cellular fallback and setup AP
  remain functional during polling. The endurance report above does not verify
  every network/failure scenario.
- Move one battery out of range. Check that a recent cached sample retains its
  original timestamp, expires after 12 minutes, and recovers on later successful
  reads without disrupting the other slot or Wi-Fi. Disable monitoring during a
  retry countdown and verify no new attempt starts until re-enabled.
- Check timeout cleanup logs and recovery while the device runs overnight.
  Persistent connection/cancellation failure may require reboot; the firmware
  does not claim automatic recovery from every host/controller fault.

Packet checks run at startup before Bluetooth starts. The serial log should
report `Renogy packet self-check passed`. They cover a known request CRC,
negative current and 32-bit capacity, truncated frames, wrong IDs, corrupted CRC
and zero capacity. The portable entry point is `test/renogy_protocol_checks.cpp`.
These checks have been compiled here; hardware coverage is limited to the owner
report above and the manual scenarios actually exercised.
