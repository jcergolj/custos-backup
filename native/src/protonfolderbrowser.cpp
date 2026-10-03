#include "protonfolderbrowser.h"

#include <QtConcurrentRun>

ProtonFolderBrowser::ProtonFolderBrowser(ProcessRunner &runner, QObject *parent)
    : ProtonFolderBrowser(runner, QString(), parent)
{
}

ProtonFolderBrowser::ProtonFolderBrowser(ProcessRunner &runner, QString cachePath, QObject *parent)
    : QObject(parent)
    , runner(runner)
    , cachePath(std::move(cachePath))
{
    connect(&watcher, &QFutureWatcher<ProtonFolderLookup>::finished, this, [this] {
        finishOpen(watcher.result());
        startPrefetch();
    });
    connect(&prefetchWatcher, &QFutureWatcher<ProtonFolderLookup>::finished, this, [this] {
        const QString path = prefetchPath;
        prefetchPath.clear();
        if (resolving && requestedPath == path) finishOpen(prefetchWatcher.result());
        startPrefetch();
    });
}

ProtonFolderBrowser::~ProtonFolderBrowser()
{
    watcher.waitForFinished();
    prefetchWatcher.waitForFinished();
}

bool ProtonFolderBrowser::busy() const
{
    return resolving;
}

void ProtonFolderBrowser::finishOpen(const ProtonFolderLookup &result)
{
    requestedPath.clear();
    resolving = false;
    emit busyChanged();
    if (result.url.isEmpty()) {
        emit failed(result.error);
    } else {
        emit folderResolved(result.url);
    }
}

void ProtonFolderBrowser::openFolder(const QString &remotePath)
{
    if (resolving) return;
    const QUrl cached = ProtonFolderLink::cached(cachePath, remotePath);
    if (!cached.isEmpty()) {
        emit folderResolved(cached);
        return;
    }
    requestedPath = remotePath;
    resolving = true;
    emit busyChanged();
    // Join an in-flight prefetch of this exact copy. Other copies' background
    // lookups never delay a user-initiated lookup or a cached link.
    if (prefetchPath == remotePath) return;
    pendingPrefetch.removeAll(remotePath);
    watcher.setFuture(QtConcurrent::run([this, remotePath] {
        return ProtonFolderLink::resolve(runner, remotePath, cachePath);
    }));
}

void ProtonFolderBrowser::prefetchFolders(const QStringList &remotePaths, bool retryFailures)
{
    if (cachePath.isEmpty()) return;
    for (const QString &path : remotePaths) {
        if (!ProtonFolderLink::validPath(path) || (!retryFailures && prefetchedPaths.contains(path))) continue;
        prefetchedPaths.insert(path);
        if (ProtonFolderLink::cached(cachePath, path).isEmpty() && path != requestedPath
            && path != prefetchPath && !pendingPrefetch.contains(path)) {
            pendingPrefetch.append(path);
        }
    }
    startPrefetch();
}

void ProtonFolderBrowser::startPrefetch()
{
    if (resolving || !prefetchPath.isEmpty() || pendingPrefetch.isEmpty()) return;
    prefetchPath = pendingPrefetch.takeFirst();
    const QString path = prefetchPath;
    prefetchWatcher.setFuture(QtConcurrent::run([this, path] {
        return ProtonFolderLink::resolve(runner, path, cachePath);
    }));
}
