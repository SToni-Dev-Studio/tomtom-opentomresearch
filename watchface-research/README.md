# TomTom OpenTom watch-face handoff

This folder is a small, portable working set for Gemini to review and update the
watch-face app. It contains the current relevant project files and API/build
context, not a full copy of the 1.3 GB OpenTom checkout.

## Target and project

- Device: TomTom ONE v6, model ID 19; reports S3C2412-class hardware.
- Display: 320x240 framebuffer, 16 bpp on the running image; Nano-X/Microwindows.
- Project: `https://github.com/LeddaZ/OpenTom`
- Checked-out commit when this handoff was prepared:
  `ad2646cfd48dd49f2ecfe785077a245ecb7457de`
- Local build uses the bundled `gcc-3.3.4_glibc-2.3.2` ARM cross-toolchain.
- The reference image is `reference/apple-watch-reference.png`.

## Contents

- `project/applications/src/tools/watchface.c`: current app source.
- `project/applications/src/tools/Makefile`: relevant app build rules.
- `project/src/opentom_skel/start.sh`: persistent startup template.
- `project/src/opentom_skel/etc/nxmenu.cfg`: persistent menu template.
- `project/src/opentom_skel/bin/watchface_next`: signal-based page cycler.
- `microwindows/`: Nano-X headers, build configuration, and client library.
- `baseline/watchface-current-arm`: stripped ARM executable last staged for
  the device before this source handoff.
- `opentom-license.txt`: project license notice.

Generated binaries and local/device backup archives are not source inputs;
do not commit or upload backup archives.

## Current behavior and known caveats

The watch-face app has stacked digital, Flow, Flux, and device-info pages.
Touch input advances pages; the helper script can also advance them by sending
`SIGUSR1` to the running app. The drawing code tries to update only changed
regions. Review it for display correctness and CPU usage on the actual
Microwindows/Nano-X build.

The startup template sets `TZ=CEST-2`, assigns USB gadget IP
`192.168.101.115`, and no longer runs touchscreen calibration automatically.
The exact time itself is not known to survive power loss. Determine whether
this device/image has a usable RTC before proposing persistence; if it does
not, correct time requires an available source such as network time or GPS.
Do not claim time persistence without demonstrating the hardware/source path.

USB Ethernet currently works when connected to the Linux host. A router's USB
port is not automatically a USB host network connection; remote updates and
NTP require actual network connectivity, routing, and a time/update service.

The power-button handler currently launches suspend behavior. Power-button
remapping is deliberately out of scope and must not be applied.

## Build in the full OpenTom checkout

This handoff omits the large cross-toolchain and sysroot. In the full project
checkout, with its dependencies present:

```sh
cd /path/to/LeddaZ-OpenTom
source get_cross_env.sh
make -B -C applications/src/tools watchface
```

The expected compiler is `arm-linux-gcc` from the bundled GCC 3.3.4 toolchain;
the app links Nano-X and `libm`. Validate a proposed source change with the
repository's exact cross-compiler and check the resulting ELF is 32-bit ARM.

The files under `project/` preserve their paths relative to the repository
root, so Gemini's proposed changes can be copied back to matching paths in the
full checkout for a real build.
