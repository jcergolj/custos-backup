# Praefectus Native

Praefectus Native is a Qt desktop application and worker for backing up selected
files to Proton Drive. It runs as the logged-in desktop user and does not
require root access.

## Requirements

- Linux with Qt 6 and Qt Test
- CMake 3.21 or newer
- A C++17 compiler
- `proton-drive`, authenticated for the current user
- A user systemd session for service installation

## Build

Configure and build with CMake:

```bash
cmake -S native -B build
cmake --build build
```

Run the native test suite:

```bash
ctest --test-dir build --output-on-failure
```

The application executable is `build/praefectus-native`. The worker executable
is `build/praefectus-native-worker`.

## Proton Drive

Install and authenticate the Proton Drive CLI for the current user:

```bash
command -v proton-drive
proton-drive auth login
proton-drive filesystem info /my-files
```

The application uses the Proton CLI for uploads, downloads, and remote file
verification.

## Configuration

The native application stores its backup configuration at:

```text
~/.config/praefectus/native-backup.json
```

The configuration contains the source directory, remote backup root, Proton
CLI path, and backup schedule. The application validates source paths and
rejects symbolic-link backup roots.

## Services

Build the installer executable and install the user systemd service:

```bash
cmake --build build --target praefectus-native-install
build/praefectus-native-install
```

The installer writes user-level systemd units under
`~/.config/systemd/user`, reloads the user manager, and enables the backup
service.

## Restore

The application can load a versioned backup manifest and restore selected files.
Restored files are checked against the manifest size and SHA-256 checksum.
Destination traversal and symbolic-link escapes are rejected.
