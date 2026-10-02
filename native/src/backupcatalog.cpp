#include "backupcatalog.h"

#include "backupmanifest.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>

namespace {

bool inspectFolder(BackupProvider &provider, const QString &path, QVector<RemoteItem> *items, QString *error)
{
    return provider.list(path, items, error);
}

bool inside(const QString &path, const QString &root)
{
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanRoot = QDir::cleanPath(root);
    return cleanPath == cleanRoot || (cleanRoot == QStringLiteral("/")
        ? cleanPath.startsWith('/')
        : cleanPath.startsWith(cleanRoot + QDir::separator()));
}

void warning(QString *error, const QString &message)
{
    if (error != nullptr && error->isEmpty()) {
        *error = message;
    }
}

bool visit(BackupProvider &provider, const QString &path, const QString &rootPath,
    QVector<RemoteCopy> *copies, QSet<QString> *visited, QString *error)
{
    const QString cleanPath = QDir::cleanPath(path);
    if (visited->contains(cleanPath)) {
        return true;
    }
    visited->insert(cleanPath);

    QVector<RemoteItem> items;
    if (!inspectFolder(provider, path, &items, error)) {
        return false;
    }

    for (const RemoteItem &item : items) {
        if (!inside(item.path, rootPath)) {
            warning(error, QStringLiteral("The provider returned a path outside the discovery root: %1").arg(item.path));
            continue;
        }
        if (item.directory) {
            if (!visit(provider, item.path, rootPath, copies, visited, error)) {
                return false;
            }
            continue;
        }
        if (item.name != QStringLiteral("manifest.json")) {
            continue;
        }

        QTemporaryDir temporary;
        if (!temporary.isValid()) {
            if (error != nullptr) {
                *error = QStringLiteral("Unable to create a temporary manifest folder.");
            }
            return false;
        }
        const QString manifestPath = temporary.filePath(QStringLiteral("manifest.json"));
        QString providerError;
        if (!provider.download(item.path, manifestPath, &providerError)) {
            warning(error, providerError.isEmpty() ? QStringLiteral("A remote manifest could not be downloaded: %1").arg(item.path) : providerError);
            continue;
        }

        QVector<BackupEntry> entries;
        BackupManifestInfo info;
        QString manifestError;
        if (!BackupManifest::load(manifestPath, &entries, &info, &manifestError)) {
            warning(error, QStringLiteral("The remote manifest %1 is unavailable: %2").arg(item.path, manifestError));
            continue;
        }
        if (info.version != 2 || info.application != QStringLiteral("omacustos")
            || info.computerName.isEmpty() || info.setId.isEmpty() || info.copyId.isEmpty()
            || !info.createdAt.isValid()
            || (info.status != QStringLiteral("complete") && info.status != QStringLiteral("incomplete"))) {
            warning(error, QStringLiteral("The remote manifest %1 is not a supported OmaCustos copy.").arg(item.path));
            continue;
        }

        RemoteCopy copy {
            QDir::cleanPath(QDir(item.path).filePath(QStringLiteral(".."))),
            item.path,
            info.computerName,
            info.setId,
            info.setName,
            info.copyId,
            info.status,
            info.createdAt,
            {},
            {},
            info.failedItems,
        };
        for (const BackupEntry &entry : entries) {
            if (!inside(entry.remotePath, copy.rootPath)) {
                warning(error, QStringLiteral("The remote manifest %1 points outside its copy.").arg(item.path));
                copy.unavailableItems.append(entry.restorePath);
                continue;
            }
            RemoteFile remoteFile;
            if (!provider.inspect(entry.remotePath, &remoteFile, &providerError)
                || remoteFile.size != entry.size
                || (!remoteFile.checksum.isEmpty() && remoteFile.checksum != entry.checksum)) {
                copy.unavailableItems.append(entry.restorePath);
                continue;
            }
            copy.entries.append(entry);
        }
        copies->append(copy);
    }
    return true;
}

}

bool BackupCatalog::discover(BackupProvider &provider, const QString &remoteRoot, QVector<RemoteCopy> *copies, QString *error)
{
    if (copies == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("A destination for remote copies is required.");
        }
        return false;
    }
    copies->clear();
    if (remoteRoot.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("A remote backup folder is required.");
        }
        return false;
    }

    const QString normalizedRoot = QDir::cleanPath(remoteRoot);
    QSet<QString> visited;
    if (!visit(provider, normalizedRoot, normalizedRoot, copies, &visited, error)) {
        copies->clear();
        return false;
    }
    std::sort(copies->begin(), copies->end(), [](const RemoteCopy &left, const RemoteCopy &right) {
        return left.createdAt > right.createdAt;
    });
    return true;
}
