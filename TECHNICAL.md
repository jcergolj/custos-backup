# Technical Notes

This document records implementation details that are useful for maintainers
and advanced users but are not needed for the quick start.

## Local State

OmaCustos stores user state under `~/.config/omacustos`:

```text
omacustos-backup.json
omacustos-backup-runs.json
omacustos-backup-cleanup.json
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
New manifests also preserve an `issues` array containing each failed source path,
phase, and reason. The existing `failed` path list remains compatible with older
readers. An incomplete manifest can list an attempted path in both `expected`
and `failed`; a verified entry cannot also be listed as failed.
This provenance keeps similarly named backups on different computers distinct.

The interface calls each saved configuration a **backup** and each run's output
a **copy**. Internal `BackupSet` types and persisted `sets`, `set_id`, and
`set_name` fields retain their existing names for compatibility with saved
configurations, run history, cleanup decisions, and remote manifests.

Legacy `required_volumes` settings are ignored on load and omitted on save;
backups no longer wait for external drives to be mounted.

Backup-set exports use a version-1 JSON document with `application: "omacustos"`
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

OmaCustos never calls `empty-trash`. Remote content size is verified after upload and
download using `size` or `activeRevision.claimedSize`, not encrypted storage size.
The manifest records the local SHA-256 checksum; a provider SHA-256
field is used when the CLI exposes one.

Recent-backup browser links open the copy recorded for that run in
the signed-in Proton Drive web app. OmaCustos resolves the folder's node ID and an
ancestor's share ID through read-only CLI metadata requests in the background;
it does not create public sharing links.

Runs persist the exact copy folder as `remote_copy_path`. For older run records,
OmaCustos identifies the newest matching manifest inside that backup's folder and
remembers its path. Browsing and manual deletion use the same recorded copy.
Manual deletion requires confirmation of the exact path, rechecks its OmaCustos
manifest and identity, and moves that copy to Proton Drive Trash. Older copies
and the backup set remain; the deleted entry disappears from Recent backups.
Worker and run-state locks prevent deletion during a backup or queue update.

## Backup Progress And Remaining Time

The engine reports planned file sizes, processed files and bytes, and a finalizing
phase, along with verified file/byte counts, failed-item counts, current file and
size, and reading/checking/uploading/verifying phases. The worker persists these
progress samples in run state, throttled to approximately once per second except
for initial progress, file/phase changes, and finalization.
Processed counts include failed attempts; they describe work done, not verified
backup contents. Manifest verification remains the authority for successful files.

Remaining time uses the slower of measured per-file and byte-throughput estimates
and counts down between samples. The last successful run's duration, bytes, and
file count provide an initial estimate for later runs, including single-file
backups. Current-run measurements take over once files finish. A first single-file
backup cannot provide an estimate before that file finishes. Older run records
without progress remain compatible and show an estimating state.

The UI polls once per second while a backup is running and every five seconds
otherwise. Expired estimates show **Taking longer than estimated…** rather than
claiming zero remaining time. Manifest upload, verification, and post-backup
cleanup show **Finalizing backup…** until the worker records its final result.

The work-progress bar measures processed file attempts rather than upload bytes
or verified contents. The CLI has no documented live byte-progress feed; the
current path, planned size, and phase remain visible during single-file transfers.

Run state also stores a structured `result` with verified payload counts, issues,
and whether the remote manifest passed verification. Only a verified manifest with
some verified entries can produce an **Incomplete** result. An attempt with no
verified entries or a failed manifest produces **Failed**. Both retain automatic
retry backoff. Only **Successful** runs trigger retention cleanup and update the
historical duration used for estimates. Older run records without these fields
remain readable and do not display invented verified-file counts.

## Retention And Cleanup

Retention considers only positively identified OmaCustos copies for the
correct computer and backup. Successful verified copies are retained according to
the backup's limit. Failed or incomplete copies cannot cause an older successful
copy to be removed.

Before the first cleanup, the exact remote paths are persisted as a proposal and
shown in the UI. No deletion occurs until the proposal is confirmed. If
permanent deletion fails after trashing, the target and cleanup phase are
persisted so the next attempt resumes without expanding the deletion scope.

## Restore Safety

Opening Restore lists only direct copy folders for the selected backup, using
the recorded run's copy parent when available and the configured computer/backup
folder otherwise. Listing runs in a background task; it does not traverse payload
directories, download manifests, or inspect files. No copy is selected automatically.
Selecting a copy downloads its manifest and verifies only that copy's files in
the background. Loading indicators distinguish listing from verification; conflicting
restore actions are disabled while either task runs. Each selection rechecks remote
files, and manifest backup/copy identity must match the selection. Full verified
catalog discovery remains available internally for retention.

Restores are limited to manifest entries that passed verification. Destination
traversal and symbolic-link escapes are rejected. The UI requires users to tick
files and specify a destination folder before enabling its single **Start
restore** action. Restore metadata is not added to the local backup queue.
The controller emits completion only after every selected file restores
successfully. The UI then closes the restore panel, clears its selection and
destination, and scrolls back to the dashboard. Failures keep the panel open.

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
/usr/lib/systemd/user/omacustos.service
/usr/lib/systemd/user/omacustos.timer
```

By default, the worker and its Proton CLI subprocesses use normal system
scheduling with no CPU quota. Packaged and generated base service units do not
set CPU or I/O resource limits. **Use system defaults** is checked unless an
explicit resource preset has been saved. Previously selected presets remain
selected after an upgrade.

The **Resource usage (all backups)** control offers five opt-in global presets:
very low (10%, nice 19), low (25%, nice 15), medium (50%, nice 10), high (100%,
nice 5), and very high (200%, nice 0). Very low and low use idle I/O scheduling;
medium, high, and very high use best-effort I/O with priorities 7, 5, and 4.

Saving a changed preset writes a managed drop-in at
`$XDG_CONFIG_HOME/systemd/user/omacustos.service.d/50-omacustos-resources.conf`
(default `~/.config/systemd/user/...`) and asynchronously runs
`systemctl --user daemon-reload`. Reload failures restore the previous file and
report an error. No root privileges are required. The drop-in is also the
persistent preset store; backup-set import/export does not change it.
Returning to **Use system defaults** clears `CPUQuota`, sets `Nice=0`, and
restores normal I/O scheduling (`IOSchedulingClass=none`, priority 0). These
explicit resets also override limits in older base service units. The default
selection is represented internally by preset index -1; indices 0–4 remain the
five optional presets.
Both manual and scheduled backups start the same service, so the limits apply
to the worker and its CLI subprocesses from the next worker start. A running
backup is not restarted. Quotas are measured against one CPU core, so 200%
permits up to two cores. Priorities never exceed normal (`nice=0`).

The package installs the timer without enabling it. After a successful backup
configuration save or import with any enabled schedule, the UI asynchronously
runs `systemctl --user daemon-reload`, then
`systemctl --user enable --now omacustos.timer`. Activation waits for any
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

The full project and desktop launcher name is **OmaCustos for Proton Drive**;
the interface uses **OmaCustos**. The package is `omacustos-git`, the executables
are `omacustos`, `omacustos-worker`, and `omacustos-install`, and the user units
are `omacustos.service` and `omacustos.timer`. The QML module is `OmaCustos`.
`OMACUSTOS_PROTON_BIN` overrides the default Proton CLI executable when creating
a configuration; saved configurations retain their `proton_binary` setting.
