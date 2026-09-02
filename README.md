# Praefectus castri posterioris

Daily backup script for Proton Drive.

What it backs up:
- everything under `~/Downloads`
- everything under `~/work`
- except normal files under `~/work/projects`
- from `~/work/projects`, only `.env` and `.env.*`

Built-in limits:
- skips files larger than `100M` by default
- skips common heavy file types like archives, media, disk images, and DB files in `~/Downloads`
- keeps backups for `7` days locally and remotely
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
mkdir -p "$HOME/Praefectus castri posterioris"
cp expedi.sh "$HOME/Praefectus castri posterioris/expedi.sh"
chmod +x "$HOME/Praefectus castri posterioris/expedi.sh"
```

## Use

Default run:

```bash
"$HOME/Praefectus castri posterioris/expedi.sh"
```

Custom Proton binary or size limit:

```bash
PROTON_BIN=/path/to/proton-drive MAX_SIZE=50M "$HOME/Praefectus castri posterioris/expedi.sh"
```

## Cron

Run daily at `02:15`:

```cron
15 2 * * * "/home/YOUR_USER/Praefectus castri posterioris/expedi.sh" >> /tmp/praefectus-castri-posterioris.log 2>&1
```

## Notes

- Proton Drive CLI was not installed on this machine when this repo was created.
- The script expects the remote folder in `REMOTE_DIR` to exist.
