#pragma once

#include "protonfolderlink.h"

#include <QFutureWatcher>
#include <QObject>
#include <QUrl>
#include <QSet>
#include <QStringList>

class ProtonFolderBrowser final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit ProtonFolderBrowser(ProcessRunner &runner, QObject *parent = nullptr);
    ProtonFolderBrowser(ProcessRunner &runner, QString cachePath, QObject *parent = nullptr);
    ~ProtonFolderBrowser() override;

    bool busy() const;
    Q_INVOKABLE void openFolder(const QString &remotePath);
    void prefetchFolders(const QStringList &remotePaths, bool retryFailures = false);

signals:
    void busyChanged();
    void folderResolved(const QUrl &url);
    void failed(const QString &error);

private:
    void startPrefetch();
    void finishOpen(const ProtonFolderLookup &result);
    ProcessRunner &runner;
    QString cachePath;
    QFutureWatcher<ProtonFolderLookup> watcher;
    QFutureWatcher<ProtonFolderLookup> prefetchWatcher;
    QString requestedPath;
    QString prefetchPath;
    QStringList pendingPrefetch;
    QSet<QString> prefetchedPaths;
    bool resolving = false;
};
