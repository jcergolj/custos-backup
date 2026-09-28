#include "backupworker.h"

BackupWorker::BackupWorker(BackupEngine &engine, QObject *parent)
    : QObject(parent)
    , engine(engine)
{
}

void BackupWorker::run(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider *provider)
{
    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));

        return;
    }

    BackupJob job(engine);
    connect(&job, &BackupJob::started, this, &BackupWorker::started);
    connect(&job, &BackupJob::progressChanged, this, &BackupWorker::progressChanged);
    connect(&job, &BackupJob::succeeded, this, &BackupWorker::succeeded);
    connect(&job, &BackupJob::failed, this, &BackupWorker::failed);
    job.run(sourceDirectory, remoteRoot, *provider);
}
