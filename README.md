# Praefectus Castrorum Posteriorum

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
mkdir -p ~/bin
cp backup.sh ~/bin/praefectus-castrorum-posteriorum.sh
chmod +x ~/bin/praefectus-castrorum-posteriorum.sh
```

## Use

Default run:

```bash
~/bin/praefectus-castrorum-posteriorum.sh
```

Custom Proton binary or size limit:

```bash
PROTON_BIN=/path/to/proton-drive MAX_SIZE=50M ~/bin/praefectus-castrorum-posteriorum.sh
```

## Cron

Run daily at `02:15`:

```cron
15 2 * * * /home/YOUR_USER/bin/praefectus-castrorum-posteriorum.sh >> /tmp/praefectus-castrorum-posteriorum.log 2>&1
```

## Notes

- Proton Drive CLI was not installed on this machine when this repo was created.
- The script expects the remote folder in `REMOTE_DIR` to exist.
