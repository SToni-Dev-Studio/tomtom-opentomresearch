# TomTom OpenTom watch-face handoff

This directory is the focused work area for Gemini. It contains the current
watch-face source, Nano-X API/build context, device/kernel evidence, and the
visual reference. Keep new work under `watchface-research/project/` using the
existing project-relative paths.

## Target and build

- Device: TomTom ONE v6, model ID 19.
- Device metadata reports `cputype=4`. In this OpenTom source, model ID 19 is
  Casablanca and CPU value 4 means Samsung S3C2412. This conflicts with the
  earlier S3C2443 identification; use the device-reported/source-mapped value
  unless new hardware evidence resolves it.
- OpenTom source: LeddaZ/OpenTom at commit
  `ad2646cfd48dd49f2ecfe785077a245ecb7457de`.
- Toolchain: bundled `gcc-3.3.4_glibc-2.3.2`, compiler `arm-linux-gcc`.
- Visual reference: `reference/apple-watch-reference.png`.
- Read `GEMINI_TASK.md` for scope, constraints, power-button requirement, and
  requested response.

Only native embedded-compatible C, shell, configuration, and concise
documentation belong in the implementation. Do not create HTML/CSS/JavaScript
or browser-only mockups.

## Contents

- `project/applications/src/tools/watchface.c`: five native watch-face modes,
  12-hour horizontal default, config options, event-driven USB/Ethernet and
  route status on the telemetry page, and partial updates. See
  `project/WATCHFACE_SPEC.md` for behavior/validation status.
- `project/src/opentom_skel/etc/watchface.cfg`: default 12-hour horizontal
  layout, AM/PM indicator, and initial face.
- `project/src/opentom_skel/bin/watchface_next`: signal-based face switch.
- `project/src/opentom_skel/etc/nxmenu.cfg`: watch-face menu entry.
- `project/applications/src/tools/Makefile`: app-specific build rules.
- `project/src/opentom_skel/start.sh`: persistent startup template.
- `project/kernel/drivers/barcelona/gpio/gpio.c`: relevant GPIO/power-button
  source from the OpenTom checkout.
- `project/applications/src/tools/power_button.c`: current power-button helper.
- `project/kernel/drivers/char/s3c2410-rtc.c`: relevant RTC driver source.
- `project/kernel/.config`: build configuration excerpt's full source config.
- `context/HARDWARE_FACTS.md`: observed device values and config facts.
- `microwindows/`: Nano-X public headers, config, and client library.
- `baseline/watchface-current-arm`: ARM executable staged before the latest
  local source refactor; treat as baseline only, not as the current build.
- `opentom-license.txt`: project license notice.

## Findings to keep in mind

The kernel configuration has `CONFIG_RTC=m`, `CONFIG_S3C2410_RTC=y`, and
`CONFIG_S3C2410_RTC_SETTIMEOFDAY=y`, but
`CONFIG_S3C2410_RTC_GETTIMEOFDAY` is disabled. `/dev/rtc` and
`/proc/driver/rtc` exist on the live device, but the RTC read
`2000-01-03 00:23` after system time was set to `2026-09-26 16:37 UTC`.
The RTC has not been written or tested across shutdown; correct time
persistence and boot synchronization remain unverified.

The unmodified power GPIO source reports an event after about 400–600 ms and
has a separate 10-second pre-PIC reset path. The source snapshot now contains
a **draft**, not device-validated, 10-second-button change: the GPIO event
threshold is 50 polls (about 10 seconds), startup requests the GPIO driver's
power-off ioctl for that event, and low-battery events still run
`bin/suspend`. Short presses no longer generate the normal button event.
Casablanca model 19 has a `PWR_RST` pin in the board source, but the ioctl can
refuse shutdown based on hardware/PIC/USB-host-detect state, and the driver
warns against some charger/PIC cases. The device is still running its original
`power_button -b bin/suspend bin/suspend` command. The user-space helper now
cross-compiles cleanly with the bundled ARM GCC and `-Wall -Werror`; the
kernel driver has not been built. Do not install or test this draft with
external power connected. A target-kernel rebuild and controlled hardware
verification are still required.

The staged watchface executable's ARM ELF sections are 15,070 bytes text, 456
bytes data, and 28 bytes BSS. It dynamically links `libnano-X.so`, `libm.so.6`,
and `libc.so.6`, so these section sizes are not process RSS. The event loop
waits up to one second between updates and the face renderer uses partial
updates. In a live device sample, the running watchface had 696 kB RSS,
2,260 kB VSZ, and 20 kB data; its CPU tick counters did not increase over
one 10-second idle sample. This supports the 2 MiB resident-memory goal for
that sample only; update/transition load was not measured. The device
reported 30,016 kB total RAM and 20,228 kB free. The live RTC still reported
`2000-01-03 00:23` after system time was set to `2026-09-26 16:37 UTC`.
`/dev/rtc` and `/proc/driver/rtc` exist, but the RTC has not been written or
tested across shutdown, so correct time persistence is not established.

Device metadata reported a GPS UART (`ttySAC1`, `gpstype=128`), but that is not
proof that a valid GPS time/fix is available to user space. Investigate the
actual GPS stream and timing before relying on it.

USB Ethernet at `192.168.101.115` was observed while connected to a Linux host.
The host USB interface now uses `192.168.101.114/32` and reaches the device
with ping and Telnet; this host-side route may need reapplying after a
reconnection. A router USB connector is not necessarily a USB Ethernet host;
there is no verified router internet, NTP, DNS, or remote-update setup yet.

The project startup template uses a POSIX CET/CEST rule with daylight-saving
transitions for Paris, so it does not depend on installed zoneinfo. This
template is not installed on the device.

The live shell uses BusyBox 1.22.1. A separate BusyBox 1.24.2 ARM build
candidate, matching source archive, migrated config, and checksums are staged
under `busybox-upgrade/`. It was built with the supplied ARM GCC/glibc
toolchain and includes the live device's observed applet set. It is a
test-only candidate, not a modern/security-current replacement; newer tested
versions did not build against the legacy headers/runtime. It has not been
copied to or run on the TomTom. See `busybox-upgrade/README.md` before any
device-side test.

## Build check without replacing the full checkout's source

Use the bundled `LeddaZ-OpenTom` toolchain, compiling this staged source
directly to `/tmp` so the full checkout's existing local watchface work is not
overwritten:

```sh
HANDOFF=/path/to/tomtom-opentomresearch/watchface-research
cd /path/to/LeddaZ-OpenTom
source get_cross_env.sh
"$CC" -O2 -Wall -Werror \
  -I"$ROOT/kernel/include" \
  -I"$HANDOFF/microwindows" \
  -o /tmp/watchface \
  "$HANDOFF/project/applications/src/tools/watchface.c" \
  -L"$HANDOFF/microwindows" -lnano-X -lm
file /tmp/watchface
```

The result should be a warning-clean 32-bit ARM ELF linked against the
existing Nano-X library. The USB-volume snapshot is stored outside the repo
at `/home/sepisotoni/tomtom-one-v6-backup-20260926/`; the watchface and its
configuration have been staged there after a verified backup. The live
screen has not been inspected since staging.

Do not replace `ttsystem` or reboot the device using the available
`2.6.13-LeddaZ` build; it does not match the running `2.6.13-tt190943` kernel.
The power-button startup remains unchanged. There is no verified
`tt190943`-matching kernel source/config/toolchain yet.
