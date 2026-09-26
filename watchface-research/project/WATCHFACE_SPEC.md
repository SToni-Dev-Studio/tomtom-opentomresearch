# Watch-face design notes and compatibility review

This document records the watch-face implementation now staged in this
research repository and distinguishes implemented source behavior from
physical-device validation.

## Target constraints

- TomTom ONE v6, model ID 19; live `/proc/barcelona/cputype` reports `4`.
- The OpenTom source maps that CPU value to S3C2412; use this source/device
  evidence rather than the earlier S3C2443 identification.
- 320x240 display, Nano-X/Microwindows, C compatible with the bundled
  `arm-linux-gcc` 3.3.4 toolchain.
- Keep implementation native and small. Do not add web assets, HTML, CSS,
  JavaScript, or desktop-only dependencies.

## Implemented source behavior

`applications/src/tools/watchface.c` contains five selectable visual modes:

| Face | Direction |
|---|---|
| Frost | Cyan outline numerals on a black background |
| Aqua | Cyan/blue accents with a restrained wave motif |
| Lavender | Soft lavender solid digits |
| Sunset | Tangerine and rose accents |
| Telemetry | Blue accents and only real device data |

The default is **12-hour, horizontal `HH:MM`** with an AM/PM badge; on the
hour, the hour digits expand and the minutes are omitted. Configuration can
select horizontal or stacked layout, 12/24-hour time, AM/PM visibility, and
the initial face. Command-line switches override layout and time-format
settings. The startup template launches the configured default face after
Nano-X starts. A touchscreen button-down cycles faces; `watchface_next`
sends `SIGUSR1` to the running app, or starts it if it is not running. The
menu invokes this helper so it will not open a duplicate instance. Face
transitions and exposure events redraw the face, while ordinary updates
redraw changed digit/text regions; the aqua accent animates once per second.

Telemetry reads uptime, load averages, memory, local/UTC time, and network
state from Linux interfaces. It distinguishes the `usb0` gadget link and
`eth0` wired link from a default route; a route is not proof of internet
access. Initial network state is read once, then link, address, and route
changes are received through a `NETLINK_ROUTE` socket. There are no ICMP
pings or periodic network probes. If the event monitor cannot be opened, the
page shows a warning. Failed metric reads display `unavailable`, not zero.
Network-state redraws are limited to the status rows. The one-second event
wait remains for clock and telemetry updates.

The network-enabled source has been cross-compiled warning-free with the
bundled ARM GCC 3.3.4 and staged to the device's USB storage along with the
12-hour horizontal configuration. The user reports the app is now loaded and
connected; link reporting and face geometry have not been independently
verified. Boot autostart and the revised menu helper are in the repository
but have not been installed on the running device. A prior device measurement
of the older running app was 696 kB RSS with no CPU tick increase over one
10-second idle sample; it does not validate this version's runtime usage.

## Future work and limits

There is no settings page and no live config reload yet; edits to
`watchface.cfg` take effect when the app is restarted. The overlay callback is
the extension point for future local weather or media providers; do not
fabricate values or add background polling. Keep future extensions native C
and avoid JSON/runtime settings until the UI behavior is designed. A
transactional live updater is not included; preserve the prior executable
before swapping app files and keep all kernel/`ttsystem` work separate.

The incoming spec's hard RAM/CPU ceilings and sub-millisecond rendering claim
are not verified and are intentionally not repeated as guarantees. Build
using the bundled ARM compiler and inspect the output; only the physical
screen can verify final geometry, touch behavior, and visual quality.

## Compatibility findings

- The app uses the supplied Nano-X API and C89-style constructs compatible
  with the bundled ARM GCC 3.3.4.
- The startup template only adds watchface autostart; it does not change the
  device's power-button behavior. The separate 10-second GPIO/power-button
  draft is not built for the verified `2.6.13-tt190943` kernel, and the
  available kernel build reports `2.6.13-LeddaZ`.
- The upstream repository's `TomTom 1 Project/` vendor backup remains
  excluded from this research repository.
