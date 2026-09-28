#include "backupprerequisites.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

bool SystemBackupPrerequisiteProbe::onAcPower() const
{
    const QDir powerSupply(QStringLiteral("/sys/class/power_supply"));
    const QStringList entries = powerSupply.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    bool foundPowerSource = false;
    for (const QString &entry : entries) {
        if (!entry.startsWith(QStringLiteral("AC")) && !entry.startsWith(QStringLiteral("ADP"))) {
            continue;
        }

        foundPowerSource = true;
        QFile online(powerSupply.filePath(entry + QStringLiteral("/online")));
        if (online.open(QIODevice::ReadOnly) && online.readAll().trimmed() == QByteArray("1")) {
            return true;
        }
    }

    return !foundPowerSource;
}

bool SystemBackupPrerequisiteProbe::volumeReady(const RequiredVolume &volume, QString *reason) const
{
    const QStorageInfo storage(volume.mountPath);
    const QString expectedMount = QDir::cleanPath(QFileInfo(volume.mountPath).absoluteFilePath());
    const QString actualMount = QDir::cleanPath(storage.rootPath());
    if (!storage.isValid() || !storage.isReady() || actualMount != expectedMount) {
        if (reason != nullptr) {
            *reason = QStringLiteral("Required volume %1 is not mounted.").arg(volume.mountPath);
        }
        return false;
    }

    if (!volume.deviceId.isEmpty() && storage.device() != volume.deviceId) {
        if (reason != nullptr) {
            *reason = QStringLiteral("A different device is mounted at %1.").arg(volume.mountPath);
        }
        return false;
    }

    return true;
}

BackupPrerequisiteResult BackupPrerequisites::check(const BackupSet &set, const BackupPrerequisiteProbe &probe)
{
    if (set.onlyOnAcPower && !probe.onAcPower()) {
        return {false, QStringLiteral("Waiting for AC power.")};
    }

    for (const RequiredVolume &volume : set.requiredVolumes) {
        QString reason;
        if (!probe.volumeReady(volume, &reason)) {
            return {false, reason};
        }
    }

    return {};
}

RequiredVolume BackupPrerequisites::captureVolume(const QString &path)
{
    const QStorageInfo storage(path);
    return {
        QDir::cleanPath(storage.rootPath()),
        storage.device(),
    };
}
