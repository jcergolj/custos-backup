#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "backupschedule.h"

struct BackupSet {
    QString id;
    QString name;
    QString remoteRoot;
    QStringList sourceDirectories;
    QStringList exclusions;
    BackupSchedule schedule;
    int retention = 3;
    bool onlyOnAcPower = false;
};

struct BackupConfig {
    QString sourceDirectory;
    QString remoteRoot;
    QString protonBinary = QStringLiteral("proton-drive");
    QVector<BackupSet> sets;
};

class BackupConfigStore
{
public:
    explicit BackupConfigStore(QString path);

    bool load(BackupConfig *config, QString *error = nullptr) const;
    bool save(const BackupConfig &config, QString *error = nullptr) const;
    QString filePath() const;

private:
    QString path;
};
