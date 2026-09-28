#pragma once

#include "backupprovider.h"

#include <QObject>
#include <QString>
#include <QStringList>

struct BackupEntry {
    QString sourcePath;
    QString remotePath;
    qint64 size = 0;
};

class BackupEngine final : public QObject
{
    Q_OBJECT

public:
    explicit BackupEngine(QObject *parent = nullptr);

    Q_INVOKABLE bool validateSelection(const QString &sourceDirectory, QString *error = nullptr) const;
    Q_INVOKABLE QStringList selectableFiles(const QString &sourceDirectory) const;
    Q_INVOKABLE QString previewError(const QString &sourceDirectory) const;
    bool backup(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider &provider, QString *manifestPath, QString *error = nullptr) const;
    bool restoreFile(const BackupEntry &entry, const QString &destinationDirectory, BackupProvider &provider, QString *error = nullptr) const;
};
