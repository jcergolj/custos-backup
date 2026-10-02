#pragma once

#include "backupprovider.h"

#include <QFutureWatcher>
#include <QObject>

struct RecentCopyTarget {
    QString setId;
    QString name;
    QString path;
    QString copyId;
    QDateTime createdAt;
    QString error;
};

class RecentBackupCopies final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    RecentBackupCopies(BackupProvider &provider, QString configPath, QString computerName, QObject *parent = nullptr);
    ~RecentBackupCopies() override;
    bool busy() const;
    Q_INVOKABLE void openCopy(const QString &setId);
    Q_INVOKABLE void requestDelete(const QString &setId);
    Q_INVOKABLE void confirmDelete();
    Q_INVOKABLE void cancelDelete();

signals:
    void busyChanged();
    void folderResolved(const QString &path);
    void deleteConfirmationReady(const QString &name, const QString &path);
    void copyDeleted(const QString &setId);
    void statusChanged(const QString &message);
    void failed(const QString &error);

private:
    enum class Operation { Browse, PrepareDelete, Delete };
    void start(Operation operation, const QString &setId);
    BackupProvider &provider;
    QString configPath;
    QString computerName;
    QFutureWatcher<RecentCopyTarget> watcher;
    RecentCopyTarget pending;
    Operation operation = Operation::Browse;
    bool resolving = false;
};
