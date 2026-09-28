#pragma once

#include "backupengine.h"

#include <QObject>
#include <QStringList>
#include <QVector>

class BackupRestoreController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList entries READ entries NOTIFY entriesChanged)

public:
    explicit BackupRestoreController(BackupEngine &engine, BackupProvider *provider = nullptr, QObject *parent = nullptr);

    QStringList entries() const;
    Q_INVOKABLE void loadManifest(const QString &path);
    Q_INVOKABLE void restore(int index, const QString &destinationDirectory);

signals:
    void entriesChanged();
    void statusChanged(const QString &status);
    void failed(const QString &error);

private:
    BackupEngine &engine;
    BackupProvider *provider;
    QVector<BackupEntry> manifestEntries;
};
