# Offline Proton CLI filesystem fixture

`protonclifixture.h` supplies a reusable `ProcessRunner` that produces real files
in a `QTemporaryDir`. It is shared by `protonprovider-test` and
`protonclifixture-test`. No account, network access, or installed CLI is required
to run these tests.

## Checked contract

Checked on 2026-10-02 against the installed **Proton Drive CLI
`cli-drive@0.8.0+06e8c605`**, SDK `js@0.21.0+06e8c605`, using only:

```sh
proton-drive --version
proton-drive filesystem upload --help
proton-drive filesystem download --help
```

The documented commands accept sources followed by a **parent folder**; transfers
keep source basenames. The fixture deliberately does not accept a requested
destination filename as a separate argument. This makes provider name-mapping
errors observable in the resulting filesystem.

| Operation | File strategies | Folder strategies |
| --- | --- | --- |
| Upload | `create-new-revision`, `rename`, `replace`, `skip` | `merge`, `rename`, `replace`, `skip` |
| Download | `rename`, `remove`, `skip` | `merge`, `rename`, `remove`, `skip` |

Upload `replace` moves the old file or folder into an isolated fixture trash tree;
download `remove` permanently removes the conflicting local node. Folder merge
preserves nonconflicting children and applies the file strategy to conflicting
children. Uploads automatically skip files with identical contents, as documented.
Explicit strategies are required; interactive prompting is not modeled.

`list` returns actual children, `create-folder` requires an existing parent, and
`info` supplies `activeRevision.claimedSize`. SHA-256 is absent by default, matching
the supported CLI metadata; `includeSha256` enables the optional provider field.
Listings default to encrypted-storage-size-only metadata to exercise the
conservative fallback. `includeListingContentSize` adds `activeRevision.claimedSize`
to file entries; it also includes SHA-256 when `includeSha256` is enabled.
`omitListingMetadataName` and `failListPath` exercise partial metadata and failed
listings. The upstream CLI's `commandFileSystemList.ts` prints SDK node entities
as JSON, and the SDK's `NodeEntity.activeRevision` can contain `claimedSize`;
the provider enables bulk verification only when that field (or content `size`)
is actually present and valid. The installed CLI's `filesystem list --help` and
`filesystem info --help` were also checked on 2026-10-03; `info` accepts one path.

## Failure injection and scope

- `failUploadName`: fail before a matching file or folder writes output, including
  descendants of a recursive folder upload.
- `truncateUploadName`: report upload success but write truncated matching content,
  including descendants of a recursive folder upload.
- `downloadFailure`: fail before transfer, fail after partial output, report
  success with truncated output, or report success with same-size corrupt output.
- `uploadedPaths`, `downloadedFolders`, and `trashedPaths`: inspect staging,
  cleanup, and conflict side effects.

This is a documented-contract fixture, not a complete cloud emulator. It does not
model encryption, authentication, thumbnails, remote IDs, revision history,
permissions, ambiguous remote names, or escaped slashes in node names. Rename
uses a deterministic `.1`, `.2`, … suffix; the CLI help promises uniqueness,
not this exact spelling. Revision creation models current content only.
The fixture uses small files and whole-file comparison for identical uploads.

When upgrading the supported CLI, recheck the two help commands and update the
strategy table and conflict tests together. Live authenticated cloud testing is
separate from this suite.

## Regression coverage

The provider suite checks requested-name mismatches, application `manifest.json`
collisions, payload transfer failures, verified manifest entries, and restored
contents. Fresh folder-backup tests also cover one recursive payload command,
mapped paths across multiple sources, hidden files, exclusions, immutable snapshots,
whole-folder retries, partial results, and staging removal before verification on
both success and failure. The fixture suite checks its own file/folder strategy matrix and
metadata variants, then uses the real `ProtonProvider` and `BackupEngine` for
failed size/checksum verification, partial downloads, successful replacement,
unrelated remote-basename files/folders, final-placement failure, preservation
of existing content, and removal of staging artifacts. Restore staging and atomic
replacement fixes land with these regressions so the suite remains green.

```sh
cmake -S native -B build
cmake --build build --target protonprovider-test protonclifixture-test
ctest --test-dir build -R '^(protonprovider|protonclifixture)-test$' --output-on-failure
```
