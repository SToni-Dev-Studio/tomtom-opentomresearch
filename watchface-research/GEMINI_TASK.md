# Gemini task: improve OpenTom watch display and time persistence

Treat `README.md` as the project facts and constraints. Review the reference
image at `reference/apple-watch-reference.png` and the source/context files in
this folder. Work only on the relevant app/startup source files included here;
return proposed file changes or a patch with the same paths.

## Goals

1. Make an attractive, readable watch display for the attached 320x240 TomTom
   screen. Use the reference as inspiration. Keep it colorful and clean; do
   not add an enclosing box/frame.
2. Keep display logic modular so time, status, and future navigation/info pages
   can be added or replaced independently.
3. Allow a tap anywhere on the screen to advance to the next face.
4. Prevent flicker and waste: avoid full-screen repaint on each clock tick;
   update only changed display regions, and keep idle CPU use low.
5. Investigate how to retain correct time across reboot. Do not assume an RTC
   exists. Use repository/source evidence and, if hardware facts are absent,
   state what must be checked on the physical unit. Only use a detected RTC,
   or an explicitly available network/GPS time source. Network synchronization
   must not block boot indefinitely. Clearly explain the no-RTC/no-network
   limitation.
6. Respect the existing Europe/Paris summer-time context rather than treating
   a fixed UTC offset as a general timezone solution.

## Constraints

- Target: TomTom ONE v6, model ID 19, S3C2412-reported hardware.
- UI: Nano-X/Microwindows with the supplied header/library context.
- Keep code compatible with the repository's old ARM GCC 3.3.4 toolchain and C
  dialect. Build/test in the complete checkout if available.
- Do not modify kernel/modules, replace `ttsystem`, reboot or make device-side
  changes, or claim hardware validation.
- Do not change power-button behavior. The existing power-button program
  suspends the device; leave it untouched.
- Do not restore automatic touchscreen calibration at startup.
- Do not claim a router USB socket will provide networking. Verify and explain
  the actual USB/Ethernet/routing requirements before suggesting remote NTP or
  update behavior.
- Report changed paths, exact build/test commands and results, remaining
  hardware questions, and any limitations. Avoid broad refactors or unrelated
  changes.
