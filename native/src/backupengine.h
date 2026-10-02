#pragma once

#include "backupprovider.h"
#include "backupprogress.h"
#include "backupresult.h"

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

struct BackupEntry {
    QString sourcePath;
    QString remotePath;
    qint64 size = 0;
    QByteArray checksum;
    QString restorePath;
};

struct BackupPreview {
    QStringList includedFiles;
    QStringList excludedFiles;
    QStringList skippedPaths;
    QStringList missingPaths;
};

struct BackupCopyMetadata {
    QString computerName;
    QString setId;
    QString setName;
    QString copyId;
    QDateTime createdAt;
};

class BackupEngine final : public QObject
{
    Q_OBJECT

public:
    explicit BackupEngine(QObject *parent = nullptr);

    Q_INVOKABLE bool validateSelection(const QString &sourceDirectory, QString *error = nullptr) const;
    Q_INVOKABLE QStringList selectableFiles(const QString &sourceDirectory) const;
    Q_INVOKABLE QStringList selectableFiles(const QStringList &sourceDirectories, const QStringList &exclusions) const;
    Q_INVOKABLE QVariantMap previewSelection(const QStringList &sourceDirectories, const QStringList &exclusions) const;
    Q_INVOKABLE QString previewError(const QString &sourceDirectory) const;
    bool backup(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider &provider, QString *manifestPath, QString *error = nullptr) const;
    bool backup(const QStringList &sourceDirectories, const QString &remoteRoot, const QStringList &exclusions, BackupProvider &provider, QString *manifestPath, QString *error = nullptr) const;
    bool backup(const QStringList &sourceDirectories, const QString &remoteRoot, const QStringList &exclusions, const BackupCopyMetadata &metadata, BackupProvider &provider, QString *manifestPath, QString *error = nullptr, const std::function<void(const BackupProgress &)> &reportProgress = {}, BackupResult *result = nullptr) const;
    bool restoreFile(const BackupEntry &entry, const QString &destinationDirectory, BackupProvider &provider, QString *error = nullptr) const;

    BackupPreview preview(const QStringList &sourceDirectories, const QStringList &exclusions) const;
};
