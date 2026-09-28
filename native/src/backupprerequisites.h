#pragma once

#include "backupconfig.h"

#include <QByteArray>
#include <QString>

struct BackupPrerequisiteResult {
    bool ready = true;
    QString reason;
};

class BackupPrerequisiteProbe
{
public:
    virtual ~BackupPrerequisiteProbe() = default;
    virtual bool onAcPower() const = 0;
    virtual bool volumeReady(const RequiredVolume &volume, QString *reason = nullptr) const = 0;
};

class SystemBackupPrerequisiteProbe final : public BackupPrerequisiteProbe
{
public:
    bool onAcPower() const override;
    bool volumeReady(const RequiredVolume &volume, QString *reason = nullptr) const override;
};

class BackupPrerequisites final
{
public:
    static BackupPrerequisiteResult check(const BackupSet &set, const BackupPrerequisiteProbe &probe);
    static RequiredVolume captureVolume(const QString &path);
};
