# TomTom BusyBox upgrade candidate

## Status

This folder contains a **test candidate**, not a replacement image and not
something installed on the TomTom:

- `busybox-1.24.2-arm`: BusyBox 1.24.2 built for 32-bit little-endian ARM
  using the OpenTom GCC 3.3.4 / glibc 2.3.2 toolchain.
- `busybox-1.24.2.config`: the migrated OpenTom BusyBox configuration used
  for the build.
- `busybox-1.24.2.tar.bz2`: the matching upstream source archive.
- `SHA256SUMS`: checksums for the candidate, config, and source archive.

The live TomTom reports BusyBox 1.22.1. Version 1.24.2 is a conservative,
buildable upgrade candidate, but it is from 2016 and should **not** be
described as a modern or security-current BusyBox. Newer upstream releases
were tested against the supplied legacy toolchain but did not build cleanly:
1.30.1 hit old Linux/glibc header conflicts, and 1.37.0 requires libc/kernel
timestamp APIs (`utimensat`, `futimens`, `AT_FDCWD`) absent from this target
environment. The TomTom kernel is 2.6.13, so a newer source compiling against
different headers would not by itself establish runtime compatibility.

## Build inputs and result

The build used:

- OpenTom source tree: `/home/sepisotoni/projects/LeddaZ-OpenTom`
- `gcc-3.3.4_glibc-2.3.2`, target `arm-linux`
- Existing `configs/busybox_config.arm-linux`, migrated with BusyBox 1.24.2
  `oldconfig`
- `CONFIG_EXTRA_CFLAGS="-DMNT_DETACH=2"` because the bundled target headers
  omit the `MNT_DETACH` constant; value `2` is the Linux `umount2` detach flag
  supported by the target kernel.

The build completed and emitted an ARM EABI, dynamically linked executable
using `/lib/ld-linux.so.2`; its libc symbol requirements are no newer than
GLIBC 2.3. The build produced eight compiler warnings. `make install` was
run only into a temporary staging directory. The staged applet links include
the current device's observed shell, Telnet, mount, networking, module, and
core utility commands. No device-side installation or execution has been
performed; runtime compatibility remains unverified.

To reproduce from the full OpenTom checkout:

```sh
tar -xf busybox-1.24.2.tar.bz2
cd busybox-1.24.2
cp /path/to/busybox-1.24.2.config .config
cd /path/to/LeddaZ-OpenTom
source get_cross_env.sh
cd /path/to/busybox-1.24.2
make ARCH=arm CROSS_COMPILE=arm-linux- CC=arm-linux-gcc oldconfig
make -j2 ARCH=arm CROSS_COMPILE=arm-linux- CC=arm-linux-gcc busybox
```

The candidate's `.config` already contains the compatibility define. Use the
matching BusyBox source archive; do not substitute a different version with
this config and assume equivalent applets or kernel requirements.

## Safe eventual device test

When the owner explicitly switches the TomTom to USB file mode, copy only
`busybox-1.24.2-arm` to a separate path on its storage, for example
`opentom/busybox-test/busybox`. Do **not** overwrite `/bin/busybox`, replace
system symlinks, change the initramfs, or edit `ttsystem`.

First verify the copied file checksum and run the candidate explicitly from
the existing root shell (`busybox --list`, then basic read-only commands).
Only after that passes should an isolated shell/Telnet test be considered.
Keep the original shell and startup paths available for rollback. Replacing
the shell used by the normal Telnet login or at boot requires a separately
reviewed root-filesystem/`ttsystem` update and recovery plan; copying this
single executable is not sufficient to make it the default shell.

The TomTom currently exposes Telnet without SSH. Keep any later test on the
direct trusted USB connection; Telnet is unencrypted. No BusyBox candidate
has been copied to or run on the device.
