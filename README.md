# Praefectus Native

Praefectus is a small Qt desktop application for backing up selected files and
folders to Proton Drive. It runs as the logged-in user, uses Proton's official
`proton-drive` CLI, and keeps the backup worker running after the window closes.
It is not a full system image tool: boot files, filesystem snapshots, and
consistent live-database backups are outside the scope of this version.

## What It Does

- Creates independent named backup sets with separate sources, exclusions, schedules, and retention limits.
- Backs up ordinary files and hidden paths such as `~/.config`.
- Skips symbolic links and reports missing or unreadable items instead of silently treating them as backed up.
- Writes a verified manifest for every copy, including incomplete copies and their failed items.
- Stores copies below `remote-root/<computer>/<set>/<copy-id>/`, so every run is independently restorable.
- Restores selected verified files or folders without downloading unrelated files.
- Discovers copies from a fresh installation using remote manifests; old local settings are not required and old schedules are never reactivated.
- Retains three successful copies by default and removes only positively identified Praefectus copies for the correct set.

## Install On Omarchy

The intended Omarchy distribution is an Arch package, just like OmaWrite. Once
the package is published in the Omarchy package repository, install it with:

```bash
omarchy pkg add praefectus-native
```

For the AUR package:

```bash
omarchy pkg aur add praefectus-native-git
```

If the AUR package is not available yet, build the included AUR package from
this checkout:

```bash
sudo pacman -S --needed base-devel git
git clone https://github.com/jcergolj/praefectus-castri-posterioris.git
cd praefectus-castri-posterioris/pkgbuild
makepkg -Csi
```

The package installs `praefectus-native`, `praefectus-native-worker`, an
application launcher, an icon, and user systemd units. It does not package
Proton's CLI. Install `proton-drive` using the supported package or release
source for your system, then check it:

```bash
command -v proton-drive
proton-drive auth login
proton-drive filesystem info /my-files
```

The app never asks for or stores the Proton password. Authentication belongs to
the CLI's supported credential flow.

The installed CLI was checked against its command help and JSON output. Uploads
use a parent folder plus replace conflict strategy; downloads use remove
conflict strategy; discovery uses `filesystem list`; cleanup uses per-item
`trash` followed by `delete`. The CLI reports remote byte sizes and an optional
SHA-256 field is used when available. Current Proton metadata normally exposes
size and a non-verified SHA-1 digest instead, so backups verify the remote size
and the manifest records the local SHA-256 for restore-time verification. The
app never uses `empty-trash`.

Start the application from the desktop menu or run:

```bash
praefectus-native
```

Create and save at least one backup set in the app, then enable scheduled work:

```bash
systemctl --user daemon-reload
systemctl --user enable --now praefectus-native.timer
systemctl --user status praefectus-native.timer
```

The package install hook prints this command after installation. The timer is
not enabled automatically because it must not start before the Proton CLI is
authenticated and a backup set exists.

The timer runs after login and catches up missed work. It does not require user
lingering. To stop scheduled work without removing the package:

```bash
systemctl --user disable --now praefectus-native.timer
```

## Build From Source

For development or non-Arch systems:

```bash
cmake -S native -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The main binaries are `build/praefectus-native` and
`build/praefectus-native-worker`. The source installer is also available when
the worker is built outside the package:

```bash
build/praefectus-native-install build/praefectus-native-worker
```

## First Backup

1. Open Praefectus and create a backup set.
2. Enter one source file or folder per line. Hidden folders such as `~/.config` are valid.
3. Add exclusions if needed and press **Preview**. Review included, excluded, skipped, and missing paths.
4. Set the remote root, for example `/my-files/backups`, then save the set.
5. Press **Back up** for a manual run, or configure a daily, weekly, or monthly schedule.

Each set has its own schedule and retention value. Monthly schedules use the
last day of the month when the configured day does not exist. An external drive
can be required, and a set can be limited to AC power; those sets wait instead
of producing a misleading partial run. The queue and run state are stored at:

```text
~/.config/praefectus/native-backup.json
~/.config/praefectus/native-backup-runs.json
```

## Retention And Cleanup

Retention defaults to three verified successful copies and can be changed per
set. A failed or incomplete run never deletes an older successful copy.

After a verified success, Praefectus calculates old successful copies and
eligible incomplete copies for that set. Before the first cleanup it stores the
exact proposed remote paths and shows them in the UI. Nothing is removed until
**Confirm proposed cleanup** is pressed. Leaving the decision pending retains
all proposed copies. Later cleanups are automatic after confirmation.

For Proton Drive, cleanup calls `filesystem trash` for each exact copy and then
`filesystem delete` for that same item. Praefectus never calls
`filesystem empty-trash`. If permanent deletion fails after trashing, the
target and cleanup phase are persisted and the next attempt resumes without
expanding the deletion scope.

Cleanup state is stored at:

```text
~/.config/praefectus/native-backup-cleanup.json
```

## Restore After Reinstall

1. Install Praefectus and authenticate the Proton CLI for the current user.
2. Open the application on the fresh installation. Do not recreate a local set just to discover old copies.
3. Enter the old remote root, such as `/my-files/backups`, and press **Discover remote backups**.
4. Search by computer, set, copy, or status. Select the intended computer and set, then choose a verified file and destination folder.
5. For an incomplete copy, restore only the listed verified entries. Failed, missing, or unavailable items remain visibly unavailable and are not offered as successful restores.

Remote manifests include computer, set, copy, timestamp, completion status,
expected items, failed items, file paths, sizes, and checksums. Copies from two
computers with similarly named sets remain distinguishable by their provenance.
Malformed or unsupported manifests and files that fail remote verification are
reported as limitations; they are never presented as successful restores.

The default restore destination is a separate folder. Restored files are
checked against manifest size and SHA-256 data, and destination traversal or
symbolic-link escapes are rejected. No recovered metadata is written into the
local schedule or backup queue.

## Files And Services

Configuration and state live under `~/.config/praefectus`. User units are
installed under `/usr/lib/systemd/user` by the package:

```text
praefectus-native.service
praefectus-native.timer
```

The worker uses `Nice=19`, idle I/O scheduling, and `CPUQuota=10%` so backups
remain unobtrusive. Inspect logs with:

```bash
journalctl --user -u praefectus-native.service
```

Remove the package with `sudo pacman -Rns praefectus-native-git` or the package
name provided by the Omarchy repository. User configuration and remote backups
are deliberately left in place for recovery.
