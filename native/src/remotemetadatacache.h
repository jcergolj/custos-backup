#pragma once

#include "backupprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

// Read-only metadata snapshots for one verification pass. Construct only after
// uploads finish: a listing taken before an upload cannot verify its new bytes.
class RemoteMetadataCache final
{
public:
    explicit RemoteMetadataCache(BackupProvider &provider) : provider(provider) {}

    // Return whether a provider call was made, so callers can check cancellation
    // immediately afterward. Unsupported listings disable further bulk attempts.
    bool loadDirectory(const QString &parent, QString *error)
    {
        if (!tryBulk || directories.contains(parent)) return false;
        QVector<RemoteFile> files;
        if (!provider.inspectDirectoryFiles(parent, &files, error)) {
            tryBulk = false;
            return true;
        }
        QHash<QString, RemoteFile> metadata;
        QSet<QString> ambiguous;
        for (const RemoteFile &file : files) {
            if (QFileInfo(file.path).path() != parent || file.path != QDir::cleanPath(file.path)) continue;
            if (metadata.contains(file.path)) ambiguous.insert(file.path);
            metadata.insert(file.path, file);
        }
        for (const QString &path : ambiguous) metadata.remove(path);
        directories.insert(parent, metadata);
        return true;
    }

    bool lookup(const QString &path, RemoteFile *file) const
    {
        const auto directory = directories.constFind(QFileInfo(path).path());
        if (directory == directories.cend()) return false;
        const auto found = directory->constFind(path);
        if (found == directory->cend()) return false;
        *file = *found;
        return true;
    }

private:
    BackupProvider &provider;
    bool tryBulk = true;
    QHash<QString, QHash<QString, RemoteFile>> directories;
};
