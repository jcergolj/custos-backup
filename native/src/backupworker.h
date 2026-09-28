#pragma once

#include "backupjob.h"

#include <QObject>

class BackupWorker final : public QObject
{
    Q_OBJECT

public:
    explicit BackupWorker(BackupEngine &engine, QObject *parent = nullptr);

public slots:
    void run(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider *provider);

signals:
    void started();
    void progressChanged(const QString &message);
    void succeeded(const QString &manifestPath);
    void failed(const QString &error);

private:
    BackupEngine &engine;
};
