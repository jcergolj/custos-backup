# Praefectus Backup

Praefectus Backup is a small Qt desktop app for backing up selected files and
folders to Proton Drive. It runs as your user and uses Proton's official
`proton-drive` CLI. It is intended for personal files and folders, not system
images, boot files, filesystem snapshots, or consistent live-database backups.

## Quick Start

### 1. Install

The package name is `praefectus-native-git`. On Omarchy, install it from the
AUR when it is available:

```bash
omarchy pkg aur add praefectus-native-git
```

Until the AUR entry is available, build the public recipe directly:

```bash
sudo pacman -S --needed base-devel git
git clone https://github.com/jcergolj/praefectus-castri-posterioris.git
cd praefectus-castri-posterioris/pkgbuild
makepkg -Csi
```

The package installs the `praefectus-native` app and worker, the **Praefectus
Backup** launcher entry, and user systemd units. Start it from the Omarchy
launcher with `Super+Space`, or run:

```bash
praefectus-native
```

### 2. Authenticate Proton Drive

Install the `proton-drive` CLI using the package or release source supported by
your system. Authenticate it as the same user who will run Praefectus:

```bash
command -v proton-drive
proton-drive auth login
proton-drive filesystem info /my-files
```

Praefectus never asks for or stores your Proton password.

### 3. Create and run a backup

1. Open **Praefectus Backup** and create a backup set.
2. Enter one source file or folder per line. Hidden paths such as `~/.config` are valid.
3. Add exclusions if needed and press **Preview**.
4. Review included, excluded, skipped, and missing paths.
5. Set a remote root, for example `/my-files/backups`, and save the set.
6. Press **Back up** for the first run.

Each backup set has its own sources, exclusions, schedule, retention value, and
optional external-drive or AC-power requirements.

Schedules can be daily, weekly, or monthly. If a monthly day does not exist in
the current month, the last day of that month is used.

### 4. Enable scheduling

Scheduling is not enabled during installation. First save at least one backup
set and confirm that Proton Drive authentication works:

```bash
systemctl --user daemon-reload
systemctl --user enable --now praefectus-native.timer
systemctl --user status praefectus-native.timer
```

The timer runs as your user after login and catches up missed work. It does not
require user lingering.

## Restore

To restore after reinstalling or moving to another computer:

1. Install Praefectus Backup and authenticate `proton-drive`.
2. Open the app and enter the old remote root.
3. Press **Discover remote backups**. Do not recreate a local set first.
4. Select the computer, set, and verified files to restore.
5. Choose a separate destination folder and start the restore.

Incomplete copies expose only verified entries. Missing, failed, malformed, or
unverifiable items are not presented as successful restores.

## Daily Operation

- Use **Preview** before a first backup or after changing sources and exclusions.
- Retention keeps three verified successful copies by default.
- Failed or incomplete runs do not remove older successful copies.
- The first cleanup proposal requires confirmation; later cleanups use the saved decision.
- To stop scheduling without uninstalling:

```bash
systemctl --user disable --now praefectus-native.timer
```

## Troubleshooting

Check the worker log:

```bash
journalctl --user -u praefectus-native.service
```

Check the timer and Proton CLI:

```bash
systemctl --user status praefectus-native.timer
command -v proton-drive
proton-drive filesystem info /my-files
```

An external-drive or AC-power requirement makes a set wait instead of creating
a misleading partial backup. Missing or unreadable source paths are reported
in the preview and manifest.

## Uninstall

Stop scheduling first if it is enabled, then remove the package:

```bash
systemctl --user disable --now praefectus-native.timer
sudo pacman -Rns praefectus-native-git
```

Package removal does not delete `~/.config/praefectus` or remote backups. Keep
them if you may need to restore later.

## Technical Documentation

See [Technical Notes](TECHNICAL.md) for remote layout, manifests, verification,
cleanup behavior, and package/service details.

## Development

Build and test the native app from the repository root:

```bash
cmake -S native -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The main binaries are `build/praefectus-native` and
`build/praefectus-native-worker`. The Arch package recipe is in `pkgbuild/`.
