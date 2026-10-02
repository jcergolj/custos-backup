#pragma once

#include "backupengine.h"
#include "backupcatalog.h"

#include <QObject>
#include <QFutureWatcher>
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
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString loadingMessage READ loadingMessage NOTIFY busyChanged)
    Q_PROPERTY(bool showingCachedData READ showingCachedData NOTIFY cachedDataChanged)
    Q_PROPERTY(bool restoreEligible READ restoreEligible NOTIFY restoreEligibilityChanged)
    Q_PROPERTY(int currentCopyIndex READ currentCopyIndex NOTIFY currentCopyIndexChanged)

public:
    explicit BackupRestoreController(BackupEngine &engine, BackupProvider *provider = nullptr, QObject *parent = nullptr);
    ~BackupRestoreController() override;

    QStringList entries() const;
    QString defaultDestination() const;
    QStringList copies() const;
    QString copySearch() const;
    void setCopySearch(const QString &search);
    QStringList unavailableEntries() const;
    Q_INVOKABLE void loadManifest(const QString &path);
    bool busy() const;
    QString loadingMessage() const;
    bool showingCachedData() const;
    bool restoreEligible() const;
    int currentCopyIndex() const;
    Q_INVOKABLE void discover(const QString &backupFolder, const QString &setId = QString());
    Q_INVOKABLE void selectCopy(int index);
    Q_INVOKABLE void restore(int index, const QString &destinationDirectory);
    Q_INVOKABLE void restoreSelected(const QVariantList &indexes, const QString &destinationDirectory);
    Q_INVOKABLE void restoreFolder(const QString &folder, const QString &destinationDirectory);

signals:
    void busyChanged();
    void cachedDataChanged();
    void restoreEligibilityChanged();
    void currentCopyIndexChanged();
    void entriesChanged();
    void copiesChanged();
    void statusChanged(const QString &status);
    void failed(const QString &error);
    void restoreCompleted();

private:
    struct BrowseResult {
        QVector<RemoteCopy> copies;
        RemoteCopy copy;
        QString error;
        bool success = false;
    };
    struct RestoreResult {
        int restoredCount = 0;
        QString error;
        bool success = false;
    };
    QFutureWatcher<BrowseResult> watcher;
    QFutureWatcher<RestoreResult> restoreWatcher;
    bool loading = false;
    bool restoring = false;
    bool verified = false;
    bool cachedData = false;
    bool discovering = false;
    QString expectedSetId;
    QString activeBackupFolder;
    QString pendingSelectedCopyPath;
    BackupEngine &engine;
    BackupProvider *provider;
    QVector<BackupEntry> manifestEntries;
    QVector<RemoteCopy> remoteCopies;
    int selectedCopyIndex = -1;

    QVector<int> filteredCopyIndexes() const;
    void setCachedData(bool cached);
    QString searchText;
};
