# Watch-face design notes and compatibility review

This document carries forward the useful watch-face ideas from
`sepisotoni/tomtom-opentom-research`, while distinguishing proposals from
features already present in the OpenTom checkout.

## Target constraints

- TomTom ONE v6, model ID 19; live `/proc/barcelona/cputype` reports `4`.
- The OpenTom source maps that CPU value to S3C2412; use this source/device
  evidence rather than the earlier S3C2443 identification.
- 320x240 display, Nano-X/Microwindows, C compatible with the bundled
  `arm-linux-gcc` 3.3.4 toolchain.
- Keep implementation native and small. Do not add web assets, HTML, CSS,
  JavaScript, or desktop-only dependencies.

## Current implementation versus incoming proposal

The existing `applications/src/tools/watchface.c` in the full OpenTom
checkout has four face modes and changes faces on touchscreen taps. The
incoming repository adds **no watch-face implementation code**: its changes
are an expanded design/specification document plus ignore rules and a
separate TomTom device backup. The five named faces, layout modes, 12/24-hour
options, configuration file, and minute animation below are future design
proposals, not features verified in the running app.

The display should remain modular and repaint only changed regions where
practical. Face changes and Nano-X expose/resize events may redraw the full
face. Keep idle processing event-driven with a bounded wait. Avoid claims
about CPU or memory budgets until measured on the target device: the most
recent live sample showed 696 kB RSS and no process CPU tick change during
one 10-second idle interval, not a full-load or face-transition benchmark.

## Proposed face palette

These visual directions from the incoming design may guide future face work:

| Face | Direction |
|---|---|
| Frost | Cyan outline numerals on a black background |
| Aqua | Cyan/blue accents with a restrained wave motif |
| Lavender | Soft lavender solid digits |
| Sunset | Tangerine and rose accents |
| Telemetry | Blue accents and only real device data |

The proposal's “zero mock data” constraint is retained: report a value as
unavailable rather than inventing telemetry.

## Deferred options

Horizontal/stacked layouts, 12/24-hour display, AM/PM badge, and a persisted
default face are possible later additions. If implemented, prefer a tiny
plain-text `key=value` configuration over JSON and validate values with
defaults for missing/invalid entries. Do not add a settings page or claim
runtime-editable configuration until its user interaction and reload behavior
are designed. These options are not a requirement for the current watch-face
work.

The proposed exact pixel coordinates, typography claims, vector glyph
storage, sub-millisecond rendering time, and hard RAM/CPU figures from the
incoming document have not been verified against the C source, Nano-X
renderer, or physical display; treat them as mockup targets, not guarantees.

## Compatibility findings

- The incoming spec correctly targets an old ARM C/Nano-X environment, but
  makes stronger performance and feature claims than the currently available
  evidence supports. This document qualifies those claims.
- The existing watch application uses four faces and supports tap-to-cycle;
  do not change its behavior to five faces, add configuration, or rework
  layouts without an explicit implementation request.
- The incoming repository also contains a large `TomTom 1 Project/` device
  backup with vendor firmware, application assets, and other proprietary
  files. Those files are not OpenTom source and are intentionally not copied
  into this research repository. The spec and relevant design ideas are the
  only imported content.
