# On-device watchface update package

This package is applied by the native C `watchface_update` utility on the
TomTom. Copy this directory's `SHA256SUMS` and `payload/` into
`/mnt/sdcard/opentom/update/`, and install the ARM updater as
`/mnt/sdcard/opentom/bin/watchface_update`.

The menu item **Apply Watchface Update** runs a checksum-only preflight and
then applies the package. It accepts exactly these files:

- `payload/bin/watchface`
- `payload/etc/watchface.cfg`

Before replacement, it copies both installed files to a dated directory under
`opentom/rollback/`. It stages replacement files beside the destinations and
renames them into place, restoring backups if a replacement fails. On
success, it stops and restarts the watchface process. It does not replace
`start.sh`, the menu file, power-button code, kernel modules, or `ttsystem`.

SHA-256 is an integrity check, not a digital signature: someone able to alter
both payload and manifest can substitute an update. The package is intended
for local, owner-controlled staging only, not remote network delivery.
