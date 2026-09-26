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
settings. A touchscreen button-down cycles faces; `watchface_next` sends
`SIGUSR1` to the running app to do the same. Face transitions and exposure
events redraw the face, while ordinary updates redraw changed digit/text
regions; the aqua accent animates once per second.

Telemetry reads uptime, load averages, memory, and local/UTC time from system
interfaces. Failed metric reads now display `unavailable`, not zero. The
implementation uses a one-second event wait. These are source-level
observations only: the face variants, config parsing, and animation have not
been installed or visually tested on the device. A prior device measurement
of the older running app was 696 kB RSS with no CPU tick increase over one
10-second idle sample; it does not validate this version's runtime usage.

## Future work and limits

There is no settings page and no live config reload yet; edits to
`watchface.cfg` take effect when the app is restarted. Keep future extensions
native C and avoid JSON/runtime settings until the UI behavior is designed.

The incoming spec's hard RAM/CPU ceilings and sub-millisecond rendering claim
are not verified and are intentionally not repeated as guarantees. Build
using the bundled ARM compiler and inspect the output; only the physical
screen can verify final geometry, touch behavior, and visual quality.

## Compatibility findings

- The app uses the supplied Nano-X API and C89-style constructs compatible
  with the bundled ARM GCC 3.3.4.
- The imported upstream startup script and power helper were not copied:
  startup must preserve the separate 10-second GPIO/power-button draft, and
  the watchface feature does not require a power-handler change.
- The upstream repository's `TomTom 1 Project/` vendor backup remains
  excluded from this research repository.
