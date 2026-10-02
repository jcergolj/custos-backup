#pragma once

#include "backupengine.h"
#include "backupcatalog.h"

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class BackupRestoreController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString defaultDestination READ defaultDestination CONSTANT)
    Q_PROPERTY(QStringList copies READ copies NOTIFY copiesChanged)
    Q_PROPERTY(QString copySearch READ copySearch WRITE setCopySearch NOTIFY copiesChanged)
    Q_PROPERTY(QStringList unavailableEntries READ unavailableEntries NOTIFY entriesChanged)

public:
    explicit BackupRestoreController(BackupEngine &engine, BackupProvider *provider = nullptr, QObject *parent = nullptr);

    QStringList entries() const;
    QString defaultDestination() const;
    QStringList copies() const;
    QString copySearch() const;
    void setCopySearch(const QString &search);
    QStringList unavailableEntries() const;
    Q_INVOKABLE void loadManifest(const QString &path);
    Q_INVOKABLE void discover(const QString &remoteRoot);
    Q_INVOKABLE void selectCopy(int index);
    Q_INVOKABLE void restore(int index, const QString &destinationDirectory);
    Q_INVOKABLE void restoreSelected(const QVariantList &indexes, const QString &destinationDirectory);
    Q_INVOKABLE void restoreFolder(const QString &folder, const QString &destinationDirectory);

signals:
    void entriesChanged();
    void copiesChanged();
    void statusChanged(const QString &status);
    void failed(const QString &error);
    void restoreCompleted();

private:
    BackupEngine &engine;
    BackupProvider *provider;
    QVector<BackupEntry> manifestEntries;
    QVector<RemoteCopy> remoteCopies;
    int selectedCopyIndex = -1;

    QVector<int> filteredCopyIndexes() const;
    QString searchText;
};
