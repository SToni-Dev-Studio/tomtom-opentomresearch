# Collected hardware and source facts

These facts come from the LeddaZ/OpenTom checkout and prior read-only checks of
the attached TomTom ONE v6. They are evidence to guide investigation, not
proof that the current device image has every source-tree feature enabled.

## Device-reported facts

Observed in `/proc/barcelona` on the device:

- `modelid`: `19`
- `modelname`: `TomTom ONE`
- `usbname`: `ONE (v6)`
- `familyname`: `TomTom ONE`
- `cputype`: `4`
- `gpsdev`: `ttySAC1`
- `gpstype`: `128`
- `bluetooth`: `0`
- `btchip`: `0`
- `btdev`: empty

Earlier, `/dev/bt` was absent. Bluetooth protocol modules existed in the kernel,
but the TomTom reported no Bluetooth chip/device.

## RTC source/config evidence

The copied `project/kernel/.config` contains:

```text
CONFIG_RTC=m
CONFIG_S3C2410_RTC=y
CONFIG_S3C2410_RTC_SETTIMEOFDAY=y
# CONFIG_S3C2410_RTC_GETTIMEOFDAY is not set
```

The kernel source includes `project/kernel/drivers/char/s3c2410-rtc.c`. The
SoC RTC driver supports register access, but the GETTIMEOFDAY option that
reads RTC time into system time is disabled in this config; SETTIMEOFDAY
enables setting the RTC from system time. In a live read-only check, both
`/dev/rtc` and `/proc/driver/rtc` existed. After setting system time from the
host, the device reported `2026-09-26 16:37 UTC`, while the RTC still reported
`2000-01-03 00:23` (`rtc_epoch: 1900`). `/sbin/hwclock` is a BusyBox symlink,
but it was not run. The RTC has not been written or tested across power-off;
retention and startup synchronization remain unverified.

A device battery may keep the RTC backup domain alive, but merely having an
internal battery or RTC hardware does not establish correct time retention.
Use a controlled test on the physical unit: set known correct time, fully
shut down (not just suspend), wait, start, compare RTC and system time, then
repeat after a longer interval. Protect user data and do not perform this test
without owner approval.

## Power-button source facts

From `project/kernel/drivers/barcelona/gpio/gpio.c`:

```c
#define GPIO_POLL_DELAY (HZ / 5)     /* 5 polls / sec */
#define GPIO_PREPIC_TIMEOUT (10 * 5) /* 10 seconds */
#define GPIO_SHUTDOWN_TIMEOUT (2)    /* original: about 400-600 ms */
```

The driver independently handles the normal ON/OFF status event and the
10-second pre-PIC reset/suicide path when supported by the platform. The
research copy now has an uncommitted draft changing the normal event threshold
to 50 polls (about 10 seconds), adding `power_button -p` to request the guarded
power-pin ioctl on a button event, and wiring that option into the copied
startup template. Low-battery handling still invokes `bin/suspend`.

This draft is not installed on the device. The running device still reports
the original `power_button -b bin/suspend bin/suspend` invocation. The guarded
ioctl can decline to shut down depending on real-shutdown pin, PIC, and
USB-host-detect state; the kernel source warns against suicide shutdown in
some charger/PIC configurations. Model 19's source pin table has a `PWR_RST`
pin, but successful and safe shutdown on this exact unit has not been tested.
Do not install or exercise the draft while external power is connected.

The user-space handler draft initially had missing `wait()` declaration and
an unused variable; those were fixed, and it now cross-compiles with the
bundled ARM GCC using `-Wall -Werror`. The kernel module/driver has not been
built or hardware-tested.

## Live resource sample

While the watch face was running on the connected TomTom, `/proc/678/status`
reported `VmRSS: 696 kB`, `VmSize: 2260 kB`, `VmData: 20 kB`, and one thread.
The process remained sleeping with `SleepAVG: 98%`. Its `/proc/678/stat` user
and system CPU tick counters did not change during one 10-second idle sample.
These are point-in-time/idle observations, not a sustained or face-transition
benchmark; they do not prove the resource ceiling under every update pattern.
The whole device reported 30,016 kB total RAM and 20,228 kB free at that time.

## GPS/time facts

The model reports a GPS UART at `ttySAC1` with `gpstype=128`. This does not
prove a GPS receiver currently has a fix or that user space can read a valid
UTC timestamp. Inspect existing GPS startup/listener utilities and NMEA
permissions/protocol before proposing GPS as a clock source. GPS time must be
validated for fix quality and UTC/date handling.

## Network facts

USB gadget Ethernet at `192.168.101.115` was observed while tethered to a
Linux host. A host assigns/routs networking in that arrangement. A generic
router USB connector does not necessarily implement USB Ethernet host mode,
DHCP, forwarding, DNS, internet access, or NTP. No always-on router connection
or automatic update channel has been validated.

The currently attached host interface `enxaa5f8bb8bb14` is up and can ping
the device; Telnet port 23 is reachable and SSH port 22 is refused. The host
USB interface now has `192.168.101.114/32`, with a host route to
`192.168.101.115/32` using source `.114`; ping and Telnet were verified after
the change. The device-side `.115` address was already responding, so no
device IP reassignment was needed. This is a live host network configuration
and may need to be reapplied after USB reconnection or host reboot.

## Timezone

The revised research startup template uses the POSIX
`CET-1CEST,M3.5.0/2,M10.5.0/3` rule, which applies Paris daylight-saving
transitions without relying on a zoneinfo database. This source template has
not been installed or verified on the device.
