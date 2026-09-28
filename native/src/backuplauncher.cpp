#include "backuplauncher.h"

#include <QDir>

BackupLauncher::BackupLauncher(QObject *parent)
    : QObject(parent)
    , runner(QStringLiteral("systemctl"))
    , systemd(runner)
{
}

void BackupLauncher::startBackup(const QString &sourceDirectory, const QString &remoteRoot)
{
    BackupConfig config {
        sourceDirectory,
        remoteRoot,
        qEnvironmentVariable("PRAEFECTUS_PROTON_BIN", QStringLiteral("proton-drive")),
    };
    const QString configPath = QDir::home().filePath(QStringLiteral(".config/praefectus/native-backup.json"));
    QString error;
    if (!BackupConfigStore(configPath).save(config, &error)) {
        emit failed(error);

        return;
    }

    if (!systemd.startUserService(QStringLiteral("praefectus-native.service"), &error)) {
        emit failed(error);

        return;
    }

    emit started();
}
