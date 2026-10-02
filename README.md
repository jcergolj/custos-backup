# Custos Backup

Custos Backup is a small Omarchy desktop app for backing up selected files and
folders to Proton Drive. It runs as your user and uses Proton's official
`proton-drive` CLI. It is intended for personal files and folders, not system
images, boot files, filesystem snapshots, or consistent live-database backups.

Create a named backup set for documents, photos, projects, or personal
configuration folders, then run it manually or on a schedule. Each run creates
a separate copy you can restore later.

## Get started

### 1. Install Custos

On Arch Linux or Omarchy, build and install the package:

```bash
sudo pacman -S --needed base-devel git
git clone https://github.com/jcergolj/custos-backup.git
cd custos-backup/pkgbuild
makepkg -Csi
```

Open **Custos Backup** from your app launcher, or run `custos`.

### 2. Authenticate Proton Drive

Install the `proton-drive` CLI using the package or release source supported by
your system. Authenticate it as the same user who will run Custos.

When Custos cannot connect to Proton Drive, it shows **Sign in to Proton**.
Press it to open the CLI login in your terminal, which launches your browser.
Keep the terminal open until authentication completes. Custos checks the
connection again automatically; use **↻** to check immediately. The button's
tooltip shows the last connection error.

You can also sign in from a terminal:

```bash
command -v proton-drive
proton-drive auth login
proton-drive filesystem info /my-files
```

Custos uses this login session. It never asks for or stores your Proton password.
The sign-in button uses `xdg-terminal-exec`, available on Omarchy. If it cannot
open a terminal, use the commands above.

### 3. Create and run a backup

1. Open **Custos Backup** and press **+** beside **Backup sets**.
2. Name the set, such as **Documents** or **Projects**.
3. Use **+** under **Source files and folders** to choose what to back up.
4. Select exclusions with **+** under **Exclusions**, or enter names or paths,
   one per line.
5. Choose a schedule, or leave scheduling disabled for manual backups.
6. Press **Save**, then open the set's **⋯** menu and choose **Back up now**.

Hidden paths such as `~/.config` are valid sources; select the folder or enter its
full path in a source field. Use **Preview** to review included, excluded,
skipped, and missing paths before running.

Each named backup set has its own files, exclusions, and schedule. Every run
creates a separate copy. By default, copies are stored under `/my-files/backups`
in Proton Drive, and the latest three verified successful copies are kept.

**Advanced settings** lets you change the remote folder and number of copies
to keep, or run only on AC power.

It also offers a **Resource usage (all backups)** setting:

| Preset | CPU quota | CPU priority (`nice`) | I/O priority |
| --- | ---: | ---: | --- |
| Very low (default) | 10% | 19 | Idle |
| Low | 25% | 15 | Idle |
| Medium | 50% | 10 | Best effort |
| High | 100% | 5 | Best effort |
| Very high | 200% | 0 | Best effort |

Press **Save** to apply your choice to all manual and scheduled backups. New
limits take effect when the next worker starts; a running backup keeps its
current limits. A 100% quota allows one full CPU core, and 200% allows two.
Lower `nice` values give the worker higher CPU priority. These limits affect the
backup worker and its CLI processes, not the desktop interface.

Schedules can be daily, weekly, or monthly. If a monthly day does not exist in
the current month, the last day of that month is used.

### 4. Check scheduling

Choose a **daily**, **weekly**, or **monthly** schedule and press **Save**.
Custos automatically enables and starts its scheduler; no terminal setup is
needed. Importing backup sets with enabled schedules also activates scheduling.
Manual-only backup sets do not activate it.

The status below the app title shows **Scheduling active** once the timer is
enabled and running. If activation fails, your backup settings remain saved and
the error stays visible. Press **Enable scheduling** to retry, or **↻** to check
the current status.

The timer runs as your user while logged in and catches up overdue backups after
your next login. It does not run while the computer is off or require user
lingering for this login-session behavior. Make sure Proton Drive is signed in
before expecting scheduled backups to succeed.

## Desktop theme

Custos follows your active Omarchy theme and updates its colors while open when
you switch themes. On other desktops, it uses the system's Qt color palette.

## Exclude folders such as node_modules

Enter exclusions one per line:

```text
node_modules
vendor
/home/you/projects/cache
```

A folder name excludes every matching folder at any depth. A full path excludes
only that file or folder. You can also select a specific path with **+**.

## Import and export backup sets

Use **Export** in the top-right to save your backup sets as a JSON file.
Use **Import** to load that file into Custos. Import replaces the configured set list.

The file contains sources, exclusions, schedules, and settings. Your backed-up
files, Proton login, and this computer's global resource preset are not included.

## Restore

1. Press **Restore** on a backup in **Recent backups** that has a recorded run.
2. Select a remote copy. Use the search field to filter by computer, backup name,
   copy, or status.
3. Select the verified files, or enter a source folder to restore.
4. Choose a separate destination folder and start the restore.

Incomplete copies expose only verified entries. Missing, failed, malformed, or
unverifiable items are not presented as successful restores.

For a reinstall or move to another computer, keep or transfer `~/.config/custos`
so the backup definitions and run history remain available, then install Custos
and authenticate `proton-drive` as the new machine's user. The current UI opens
remote discovery through **Restore** on a recorded backup; it does not yet have
a standalone remote-root discovery action for a fresh installation.

## Manage copies

- **↗** opens that copy in Proton Drive in your browser.
- **⋯ → Delete copy** confirms the exact copy before moving it to Proton Drive Trash.
- **Delete** on a backup set removes its configuration and schedule; remote copies remain.

## Daily operation

- Use **Preview** before a first backup or after changing sources and exclusions.
- Retention keeps three verified successful copies by default.
- Failed or incomplete runs do not remove older successful copies.
- The first cleanup proposal requires confirmation; later cleanups use the saved
  decision.
- To stop scheduling without uninstalling:

```bash
systemctl --user disable --now custos.timer
```

Custos shows **Scheduling paused**. Use **Enable scheduling** to resume. Saving
backup settings while an enabled schedule exists also reactivates the timer.
To make a backup manual-only, choose **disabled** for its schedule and save.

## Troubleshooting

Check the worker log:

```bash
journalctl --user -u custos.service
```

Check the timer and Proton CLI:

```bash
systemctl --user status custos.timer
command -v proton-drive
proton-drive filesystem info /my-files
```

An AC-power requirement makes a backup wait while on battery power. Missing or
unreadable source paths are reported in the preview and manifest.

## Uninstall

Stop scheduling first if it is enabled, then remove the package:

```bash
systemctl --user disable --now custos.timer
sudo pacman -Rns custos-git
```

Package removal does not delete `~/.config/custos`, the user-systemd resource
drop-in, or remote backups. Keep your configuration and backups if you may need
to restore later.

## Technical documentation

See [Technical Notes](TECHNICAL.md) for remote layout, manifests, verification,
cleanup behavior, and package/service details.

## Development

Build and test the native app from the repository root:

```bash
cmake -S native -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The main binaries are `build/custos` and `build/custos-worker`. The Arch package
recipe is in `pkgbuild/`.
