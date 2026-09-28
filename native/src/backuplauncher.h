#pragma once

#include "backupconfig.h"
#include "processrunner.h"
#include "qprocessrunner.h"
#include "systemdlauncher.h"

#include <QObject>

class BackupLauncher final : public QObject
{
    Q_OBJECT

public:
    explicit BackupLauncher(QObject *parent = nullptr);

public slots:
    void startBackup();
    void startBackup(const QString &setId);
    void startBackup(const QString &sourceDirectory, const QString &remoteRoot);

signals:
    void started();
    void failed(const QString &error);

private:
    void startService();

    QProcessRunner runner;
    SystemdLauncher systemd;
};
