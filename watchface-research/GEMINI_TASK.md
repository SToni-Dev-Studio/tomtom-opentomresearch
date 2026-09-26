# Gemini task: native OpenTom watch faces, time, and long-press shutdown

You have the uploaded `tomtom-opentomresearch` repository. Work inside
`watchface-research/project/`, following the file paths already laid out
there. Read `watchface-research/README.md` and
`watchface-research/context/HARDWARE_FACTS.md` first; inspect the supplied C
sources and `reference/apple-watch-reference.png`.

## Output must fit this device

This is an embedded TomTom ONE v6 running Linux, not a web browser or a modern
desktop. Implement the display as native C using the supplied Nano-X/
Microwindows API and old ARM GCC toolchain conventions. Do not generate HTML,
CSS, JavaScript, React, web pages, browser mockups, or other files unsuitable
for this framebuffer device. Keep changes limited to relevant C, shell,
configuration, and concise documentation files.

## Watch display

The native app is staged at
`project/applications/src/tools/watchface.c`. It provides five face styles,
horizontal or stacked layout, 12/24-hour display, optional AM/PM, a config
file, tap-to-cycle, and partial updates. The default is 12-hour horizontal.
Review or extend this code only when asked, and do not claim the staged source
has been installed or visually validated on the physical LCD.

Build with the repository's ARM GCC 3.3.4 / Nano-X environment. Check C
compatibility, warning output, and 32-bit ARM output. Keep the UI native; do
not create HTML/CSS/JavaScript or browser mockups. Do not claim unsupported
CPU/RAM guarantees; measure the actual binary and active faces on-device.

## Correct time across reboot

The owner believes the device's battery should keep time, but verify instead
of assuming. The supplied running-build config excerpt shows:

- `CONFIG_RTC=m`
- `CONFIG_S3C2410_RTC=y`
- `CONFIG_S3C2410_RTC_SETTIMEOFDAY=y`
- `CONFIG_S3C2410_RTC_GETTIMEOFDAY` is disabled.

Investigate the supplied RTC driver and startup behavior. Live read-only
inspection already found `/dev/rtc` and `/proc/driver/rtc`, but the RTC read
`2000-01-03 00:23` after system time had been set to `2026-09-26 16:37 UTC`.
The RTC has not been written or tested across shutdown. Determine the
remaining software/build questions and propose a safe retention test; do not
write the RTC, reboot, or otherwise operate the physical device.

The hardware metadata observed on this model reports GPS at `ttySAC1` and
`gpstype=128`, but do not assume a valid GPS fix or that its NMEA stream is
available to user space. Check existing GPS utilities/protocol. If no working
RTC read or GPS time source is verified, explain that after power loss the
correct time cannot be recovered without a network/GPS source. Any network
time sync must be optional, bounded, non-blocking during boot, and only run
when a genuine routed network is available. Use the `Europe/Paris` timezone
(including daylight-saving rules), not a permanently hardcoded UTC offset.

## Power button: ten-second hold to turn off

The owner specifically wants the device to power off only after the power
button is held continuously for 10 seconds. The research copy already
contains an uncommitted draft in `kernel/drivers/barcelona/gpio/gpio.c`,
`applications/src/tools/power_button.c`, and `src/opentom_skel/start.sh`.
Review those files and the original behavior carefully. The original GPIO
source has a 400–600 ms `GPIO_SHUTDOWN_TIMEOUT`, while
`GPIO_PREPIC_TIMEOUT` is 10 seconds; do not confuse these independent paths.
The live device still runs the original
`power_button -b bin/suspend bin/suspend` command.

Assess whether the existing draft actually implements the requirement and
identify any correctness/safety issues. Account for low-battery, suspend,
hardware-PIC reset, USB-host, and charger-connected behavior. Do not expand the
patch, change the shipped image, install anything, or power off/reboot the
physical TomTom. State the target-kernel rebuild/brick risk and give a safe
test plan. Any source change must remain in this research copy.

## Network and update limits

The currently observed `192.168.101.115` interface is the TomTom's USB gadget
Ethernet address while tethered to a Linux host. A router USB socket is not
automatically a USB Ethernet host, and the TomTom does not yet have verified
router/WAN routing, DNS, NTP, or remote update service. Do not promise
always-on internet or automatic updates until the required network hardware,
routing, and secure update mechanism are proven.

## Report

Return:

1. Changed file paths and a focused patch.
2. Build/check commands and actual results, distinguishing the already
   successful user-space cross-compile from the unbuilt kernel driver.
3. Evidence-backed RTC/GPS findings, including the live RTC reading, and
   remaining physical tests.
4. Whether a 10-second-only power-off can be achieved safely, which source
   path must change, and risks.
5. Explicitly say no device-side installation/reboot was performed.

Do not change `ttsystem`, USB kernel modules, RTC state, or the physical
device. Keep the power-button source draft uncommitted and uninstalled until
it can be tested safely.
