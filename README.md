# Praefectus castri posterioris

Daily backup script for Proton Drive.

What it backs up:
- everything under `~/downloads`
- everything under `~/work`
- except normal files under `~/work/projects`
- from `~/work/projects`, only `.env` and `.env.*`

Built-in limits:
- skips files larger than `100M` by default
- skips common heavy file types like archives, media, disk images, and DB files in `~/downloads`
- keeps backups for `7` days locally and remotely
- uploads to `/my-files/backups/<computer-name>/<date>` in Proton Drive
- uploads only when content changed

## Requirements

- Linux
- `proton-drive`
- `jq`
- `tar`
- `find`
- `sha256sum`

Check tools:

```bash
command -v proton-drive
command -v jq
```

Authenticate Proton Drive once:

```bash
proton-drive auth login
```

## Install

```bash
mkdir -p "$HOME/scripts/praefectus-castri-posterioris"
cp expedi.sh "$HOME/scripts/praefectus-castri-posterioris/expedi.sh"
chmod +x "$HOME/scripts/praefectus-castri-posterioris/expedi.sh"
```

## Use

Default run:

```bash
"$HOME/scripts/praefectus-castri-posterioris/expedi.sh"
```

Custom Proton binary or size limit:

```bash
PROTON_BIN=/path/to/proton-drive MAX_SIZE=50M "$HOME/scripts/praefectus-castri-posterioris/expedi.sh"
```

Custom computer name or remote root:

```bash
COMPUTER_NAME=my-laptop REMOTE_ROOT=/my-files/backups/my-laptop "$HOME/scripts/praefectus-castri-posterioris/expedi.sh"
```

## Scheduler

Install the systemd user timer to run daily at `02:15`:

```bash
mkdir -p "$HOME/.config/systemd/user"
cp systemd/praefectus-castri-posterioris.service "$HOME/.config/systemd/user/"
cp systemd/praefectus-castri-posterioris.timer "$HOME/.config/systemd/user/"
systemctl --user daemon-reload
systemctl --user enable --now praefectus-castri-posterioris.timer
systemctl --user list-timers praefectus-castri-posterioris.timer
```

The timer is persistent, so a missed run is started after the next login.

## Notes

- Proton Drive CLI was not installed on this machine when this repo was created.
- The script uploads to `/my-files/backups/<computer-name>/<date>` by default.
