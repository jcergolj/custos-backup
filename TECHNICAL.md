# Technical Notes

This document records implementation details that are useful for maintainers
and advanced users but are not needed for the quick start.

## Local State

Custos stores user state under `~/.config/custos`:

```text
custos-backup.json
custos-backup-runs.json
custos-backup-cleanup.json
```

The first file contains named backup configurations. Run history and pending
cleanup decisions are stored separately. Discovered remote backups are not written into
the local schedule or backup queue, so reinstall discovery never reactivates an
old schedule.

## Remote Layout

Each copy is stored independently below:

```text
<remote-root>/<computer>/<backup-name>/<copy-id>/
```

Every copy has a manifest containing its computer, backup identity and name,
copy, timestamp, completion state, expected items, failed items, paths, sizes,
and checksums.
This provenance keeps similarly named backups on different computers distinct.

The interface calls each saved configuration a **backup** and each run's output
a **copy**. Internal `BackupSet` types and persisted `sets`, `set_id`, and
`set_name` fields retain their existing names for compatibility with saved
configurations, run history, cleanup decisions, and remote manifests.

Legacy `required_volumes` settings are ignored on load and omitted on save;
backups no longer wait for external drives to be mounted.

Backup-set exports use a version-1 JSON document with `application: "custos"`
and a `sets` array. They contain the saved set definitions, without the CLI
executable, run history, cleanup decisions, files, or credentials. Import validates
the whole document before replacing the set list and preserves the local CLI
executable setting. Import is blocked while a worker or queue update is active.

Exclusions containing a path separator match that path and its descendants.
Bare names such as `node_modules` match directory names at every depth; regular
files with the same name remain included. Matching is exact and case-sensitive.
Excluded symbolic links and unreadable paths do not mark a copy incomplete.

## Provider Behavior

The UI checks the CLI connection asynchronously with
`filesystem info /my-files --json` at startup, when the window becomes active,
and every 30 seconds.
When disconnected, **Sign in to Proton** launches the configured CLI's `auth login`
through `xdg-terminal-exec`. Credentials remain managed by Proton's CLI.
Connection failures also expose a retry button and the CLI error in the sign-in
button's tooltip.

The Proton Drive provider uses the official CLI for:

- uploads with a parent folder and replace conflict handling;
- downloads with remove conflict handling;
- discovery through `filesystem list`;
- cleanup through per-item `trash` followed by `delete`.

Custos never calls `empty-trash`. Remote content size is verified after upload and
download using `size` or `activeRevision.claimedSize`, not encrypted storage size.
The manifest records the local SHA-256 checksum; a provider SHA-256
field is used when the CLI exposes one.

Recent-backup browser links open the copy recorded for that run in
the signed-in Proton Drive web app. Custos resolves the folder's node ID and an
ancestor's share ID through read-only CLI metadata requests in the background;
it does not create public sharing links.

Runs persist the exact copy folder as `remote_copy_path`. For older run records,
Custos identifies the newest matching manifest inside that backup's folder and
remembers its path. Browsing and manual deletion use the same recorded copy.
Manual deletion requires confirmation of the exact path, rechecks its Custos
manifest and identity, and moves that copy to Proton Drive Trash. Older copies
and the backup set remain; the deleted entry disappears from Recent backups.
Worker and run-state locks prevent deletion during a backup or queue update.

## Retention And Cleanup

Retention considers only positively identified Custos copies for the
correct computer and backup. Successful verified copies are retained according to
the backup's limit. Failed or incomplete copies cannot cause an older successful
copy to be removed.

Before the first cleanup, the exact remote paths are persisted as a proposal and
shown in the UI. No deletion occurs until the proposal is confirmed. If
permanent deletion fails after trashing, the target and cleanup phase are
persisted so the next attempt resumes without expanding the deletion scope.

## Restore Safety

Restores are limited to manifest entries that passed verification. Destination
traversal and symbolic-link escapes are rejected. The default destination is a
separate folder, and restore metadata is not added to the local backup queue.

## Desktop Theme

The UI reads Omarchy's `colors.toml` from `$XDG_STATE_HOME/omarchy/current/theme`
(default `~/.local/state`) or the legacy `$XDG_CONFIG_HOME/omarchy/current/theme`
(default `~/.config`). File and directory watches pick up palette edits and
whole-directory replacements during theme switches. Missing or invalid palettes
fall back to the system Qt palette. UI surfaces, text, controls, selections, and
disabled colors share these live palette roles.

## Services And Packaging

The package installs these user units:

```text
/usr/lib/systemd/user/custos.service
/usr/lib/systemd/user/custos.timer
```

The worker runs at low priority with idle I/O scheduling and a 10% CPU quota.
The **Resource usage (all backups)** control offers five global presets:
very low (10%, nice 19), low (25%, nice 15), medium (50%, nice 10), high (100%,
nice 5), and very high (200%, nice 0). Very low and low use idle I/O scheduling;
medium, high, and very high use best-effort I/O with priorities 7, 5, and 4.

Saving a changed preset writes a managed drop-in at
`$XDG_CONFIG_HOME/systemd/user/custos.service.d/50-custos-resources.conf`
(default `~/.config/systemd/user/...`) and asynchronously runs
`systemctl --user daemon-reload`. Reload failures restore the previous file and
report an error. No root privileges are required. The drop-in is also the
persistent preset store; backup-set import/export does not change it.
Both manual and scheduled backups start the same service, so the limits apply
to the worker and its CLI subprocesses from the next worker start. A running
backup is not restarted. Quotas are measured against one CPU core, so 200%
permits up to two cores. Priorities never exceed normal (`nice=0`).

The package installs the timer without enabling it. After a successful backup
configuration save or import with any enabled schedule, the UI asynchronously
runs `systemctl --user daemon-reload`, then
`systemctl --user enable --now custos.timer`. Activation waits for any
resource-preset update to finish, so an immediately due backup starts with the
saved limits. Invalid or failed saves,
preview, and manual-only configurations do not enable the timer.

The scheduler checks the timer's `LoadState`, `ActiveState`, and `UnitFileState`
at startup, after activation, on window focus, and every 30 seconds. It reports
active, paused, session-only, and unavailable states in the UI. Activation errors
leave the saved schedules intact and provide an in-app retry. Opening the app
does not automatically resume an externally paused timer. Disabling a backup's
schedule stops future scheduled runs for that set; the shared timer may remain
active to process queued work and retries.

The display name is **Custos Backup**. The technical package, executable, unit,
and configuration names are `custos-git`, `custos`, and `custos.*`.
