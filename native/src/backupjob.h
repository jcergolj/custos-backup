#pragma once

#include "backupengine.h"

#include <QObject>

class BackupJob final : public QObject
{
    Q_OBJECT

public:
    explicit BackupJob(BackupEngine &engine, QObject *parent = nullptr);

    Q_INVOKABLE void run(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider &provider);

signals:
    void started();
    void progressChanged(const QString &message);
    void succeeded(const QString &manifestPath);
    void failed(const QString &error);

private:
    BackupEngine &engine;
};
