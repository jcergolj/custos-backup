#pragma once

#include "backupengine.h"

#include <QString>
#include <QVector>

class BackupManifest
{
public:
    static bool load(const QString &path, QVector<BackupEntry> *entries, QString *error = nullptr);
};
