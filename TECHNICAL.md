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

## Provider Behavior

The Proton Drive provider uses the official CLI for:

- uploads with a parent folder and replace conflict handling;
- downloads with remove conflict handling;
- discovery through `filesystem list`;
- cleanup through per-item `trash` followed by `delete`.

Custos never calls `empty-trash`. Remote size is verified after upload and
download. The manifest records the local SHA-256 checksum; a provider SHA-256
field is used when the CLI exposes one.

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

## Services And Packaging

The package installs these user units:

```text
/usr/lib/systemd/user/custos.service
/usr/lib/systemd/user/custos.timer
```

The worker runs at low priority with idle I/O scheduling and a 10% CPU quota.
The timer is intentionally not enabled by the package install hook because the
CLI must be authenticated and a backup must exist first.

The display name is **Custos Backup**. The technical package, executable, unit,
and configuration names are `custos-git`, `custos`, and `custos.*`.
