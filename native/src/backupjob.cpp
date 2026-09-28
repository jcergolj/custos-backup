#include "backupjob.h"

BackupJob::BackupJob(BackupEngine &engine, QObject *parent)
    : QObject(parent)
    , engine(engine)
{
}

void BackupJob::run(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider &provider)
{
    emit started();
    emit progressChanged(QStringLiteral("Preparing backup..."));

    QString manifestPath;
    QString error;
    if (!engine.backup(sourceDirectory, remoteRoot, provider, &manifestPath, &error)) {
        emit failed(error);

        return;
    }

    emit progressChanged(QStringLiteral("Backup verified."));
    emit succeeded(manifestPath);
}
