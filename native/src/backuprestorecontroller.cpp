#include "backuprestorecontroller.h"

#include "backupmanifest.h"

BackupRestoreController::BackupRestoreController(BackupEngine &engine, BackupProvider *provider, QObject *parent)
    : QObject(parent)
    , engine(engine)
    , provider(provider)
{
}

QStringList BackupRestoreController::entries() const
{
    QStringList paths;
    for (const BackupEntry &entry : manifestEntries) {
        paths.append(entry.sourcePath);
    }

    return paths;
}

void BackupRestoreController::loadManifest(const QString &path)
{
    QVector<BackupEntry> loadedEntries;
    QString error;
    if (!BackupManifest::load(path, &loadedEntries, &error)) {
        emit failed(error);

        return;
    }

    manifestEntries = std::move(loadedEntries);
    emit entriesChanged();
    emit statusChanged(QStringLiteral("%1 files available for restore.").arg(manifestEntries.size()));
}

void BackupRestoreController::restore(int index, const QString &destinationDirectory)
{
    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));

        return;
    }

    if (index < 0 || index >= manifestEntries.size()) {
        emit failed(QStringLiteral("The selected restore file is invalid."));

        return;
    }

    QString error;
    if (!engine.restoreFile(manifestEntries.at(index), destinationDirectory, *provider, &error)) {
        emit failed(error);

        return;
    }

    emit statusChanged(QStringLiteral("File restored successfully."));
}
