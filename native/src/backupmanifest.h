#pragma once

#include "backupengine.h"

#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QVector>

struct BackupManifestInfo {
    int version = 0;
    QString application;
    QString computerName;
    QString setId;
    QString setName;
    QString copyId;
    QDateTime createdAt;
    QString status;
    QStringList expectedItems;
    QStringList failedItems;
};

class BackupManifest
{
public:
    static bool load(const QString &path, QVector<BackupEntry> *entries, QString *error = nullptr);
    static bool load(const QString &path, QVector<BackupEntry> *entries, BackupManifestInfo *info, QString *error = nullptr);
};
