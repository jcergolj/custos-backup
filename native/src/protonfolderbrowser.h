#pragma once

#include "processrunner.h"

#include <QFutureWatcher>
#include <QObject>
#include <QUrl>

struct ProtonFolderLookup {
    QUrl url;
    QString error;
};

class ProtonFolderBrowser final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit ProtonFolderBrowser(ProcessRunner &runner, QObject *parent = nullptr);
    ~ProtonFolderBrowser() override;

    bool busy() const;
    Q_INVOKABLE void openFolder(const QString &remotePath);

signals:
    void busyChanged();
    void folderResolved(const QUrl &url);
    void failed(const QString &error);

private:
    ProcessRunner &runner;
    QFutureWatcher<ProtonFolderLookup> watcher;
    bool resolving = false;
};
