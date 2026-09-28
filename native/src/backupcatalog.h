#pragma once

#include "backupengine.h"
#include "backupprovider.h"

#include <QDateTime>
#include <QStringList>
#include <QVector>

struct RemoteCopy {
    QString rootPath;
    QString manifestPath;
    QString computerName;
    QString setId;
    QString setName;
    QString copyId;
    QString status;
    QDateTime createdAt;
    QVector<BackupEntry> entries;
    QStringList unavailableItems;
    QStringList failedItems;

    bool complete() const
    {
        return status == QStringLiteral("complete") && unavailableItems.isEmpty() && failedItems.isEmpty();
    }
};

class BackupCatalog final
{
public:
    static bool discover(BackupProvider &provider, const QString &remoteRoot, QVector<RemoteCopy> *copies, QString *error = nullptr);
};
