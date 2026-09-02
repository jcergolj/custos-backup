#!/usr/bin/env bash
set -euo pipefail

PROTON_BIN="${PROTON_BIN:-$HOME/bin/proton-drive}"
REMOTE_DIR="${REMOTE_DIR:-/my-files/work-backups}"
STAGING_DIR="${STAGING_DIR:-$HOME/.local/share/work-backups}"
RETENTION_DAYS="${RETENTION_DAYS:-7}"
MAX_SIZE="${MAX_SIZE:-100M}"
DATE="$(date +%F)"

mkdir -p "$STAGING_DIR"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

MANIFEST="$TMP_DIR/manifest.txt"
ARCHIVE="$STAGING_DIR/backup-$DATE.tar.gz"
HASH_FILE="$STAGING_DIR/last.sha256"

require_tools() {
  command -v "$PROTON_BIN" >/dev/null 2>&1 || {
    printf 'Missing Proton Drive CLI at %s\n' "$PROTON_BIN" >&2
    exit 1
  }

  command -v jq >/dev/null 2>&1 || {
    printf 'Missing required tool: jq\n' >&2
    exit 1
  }
}

build_manifest() {
  {
    find "$HOME/Downloads" \
      -type d \( \
        -name node_modules -o \
        -name vendor -o \
        -name .git \
      \) -prune -o \
      -type f \
      ! -size +"$MAX_SIZE" \
      ! -name '*.zip' \
      ! -name '*.tar' \
      ! -name '*.gz' \
      ! -name '*.bz2' \
      ! -name '*.xz' \
      ! -name '*.7z' \
      ! -name '*.rar' \
      ! -name '*.iso' \
      ! -name '*.dmg' \
      ! -name '*.pkg' \
      ! -name '*.mp4' \
      ! -name '*.mkv' \
      ! -name '*.mov' \
      ! -name '*.avi' \
      ! -name '*.webm' \
      ! -name '*.mp3' \
      ! -name '*.wav' \
      ! -name '*.flac' \
      ! -name '*.sqlite' \
      ! -name '*.db' \
      -print 2>/dev/null

    find "$HOME/work" \
      -path "$HOME/work/projects" -prune -o \
      -type d \( \
        -name node_modules -o \
        -name vendor -o \
        -name .git -o \
        -name dist -o \
        -name build -o \
        -name .next -o \
        -name coverage \
      \) -prune -o \
      -type f \
      ! -size +"$MAX_SIZE" \
      -print 2>/dev/null

    find "$HOME/work/projects" \
      -type f \
      \( -name '.env' -o -name '.env.*' \) \
      ! -size +"$MAX_SIZE" \
      -print 2>/dev/null
  } | sort -u
}

compute_hash() {
  xargs -d '\n' sha256sum < "$MANIFEST" | sha256sum | cut -d ' ' -f1
}

cleanup_local() {
  find "$STAGING_DIR" -maxdepth 1 -type f -name 'backup-*.tar.gz' -mtime +"$RETENTION_DAYS" -delete
}

cleanup_remote() {
  "$PROTON_BIN" filesystem list "$REMOTE_DIR" --json 2>/dev/null \
    | jq -r '.[].Name // empty' \
    | grep -E '^backup-[0-9]{4}-[0-9]{2}-[0-9]{2}\.tar\.gz$' \
    | while read -r file; do
        file_date="${file#backup-}"
        file_date="${file_date%.tar.gz}"
        ts="$(date -d "$file_date" +%s 2>/dev/null || true)"

        if [[ "$ts" =~ ^[0-9]+$ ]]; then
          age_days=$(( ( $(date +%s) - ts ) / 86400 ))
          if [ "$age_days" -gt "$RETENTION_DAYS" ]; then
            "$PROTON_BIN" filesystem delete "$REMOTE_DIR/$file"
          fi
        fi
      done
}

main() {
  require_tools
  build_manifest > "$MANIFEST"

  if [ ! -s "$MANIFEST" ]; then
    printf 'No files matched backup rules\n'
    exit 0
  fi

  current_hash="$(compute_hash)"

  if [ -f "$HASH_FILE" ] && [ "$(<"$HASH_FILE")" = "$current_hash" ]; then
    cleanup_local
    cleanup_remote
    printf 'No changes since last backup\n'
    exit 0
  fi

  tar -czf "$ARCHIVE" -T "$MANIFEST"
  "$PROTON_BIN" filesystem upload "$ARCHIVE" "$REMOTE_DIR"
  printf '%s' "$current_hash" > "$HASH_FILE"

  cleanup_local
  cleanup_remote
  printf 'Uploaded %s\n' "$ARCHIVE"
}

main "$@"
