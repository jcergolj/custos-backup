#include "protonfolderlink.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>

namespace {

QJsonObject readCache(const QString &path)
{
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool validUrl(const QUrl &url)
{
    const QStringList parts = url.path().split('/', Qt::SkipEmptyParts);
    return url.isValid() && url.scheme() == QStringLiteral("https")
        && url.host() == QStringLiteral("drive.proton.me") && url.port() == -1
        && url.userInfo().isEmpty() && !url.hasQuery() && !url.hasFragment()
        && parts.size() == 3 && parts.at(1) == QStringLiteral("folder");
}

void remember(const QString &cachePath, const QString &remotePath, const QUrl &url)
{
    // This cache is optional and independent of the worker/run-state locks.
    // Merge under its own lock so GUI prefetch and a worker cannot lose entries.
    if (cachePath.isEmpty() || !QDir().mkpath(QFileInfo(cachePath).absolutePath())) return;
    QLockFile lock(cachePath + QStringLiteral(".lock"));
    if (!lock.tryLock(1000)) return;
    QJsonObject cache = readCache(cachePath);
    cache.insert(remotePath, url.toString(QUrl::FullyEncoded));
    while (cache.size() > 200) {
        auto victim = cache.begin();
        if (victim.key() == remotePath) ++victim;
        cache.erase(victim);
    }
    QSaveFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) return;
    const QByteArray bytes = QJsonDocument(cache).toJson(QJsonDocument::Compact);
    if (file.write(bytes) == bytes.size()) file.commit();
}

}

QString ProtonFolderLink::cachePath(const QString &configPath)
{
    return QDir(QFileInfo(configPath).absolutePath()).filePath(QStringLiteral("omacustos-browser-links.json"));
}

bool ProtonFolderLink::validPath(const QString &remotePath)
{
    return remotePath.startsWith('/') && remotePath != QStringLiteral("/")
        && QDir::cleanPath(remotePath) == remotePath;
}

QUrl ProtonFolderLink::cached(const QString &cachePath, const QString &remotePath)
{
    if (!validPath(remotePath)) return {};
    const QUrl url(readCache(cachePath).value(remotePath).toString());
    return validUrl(url) ? url : QUrl();
}

ProtonFolderLookup ProtonFolderLink::resolve(ProcessRunner &runner, const QString &remotePath,
    const QString &cachePath)
{
    if (!validPath(remotePath)) {
        return {{}, QStringLiteral("The remote backup folder is invalid.")};
    }
    const QUrl cachedUrl = cached(cachePath, remotePath);
    if (!cachedUrl.isEmpty()) return {cachedUrl, {}};

    // A normal /my-files copy needs only its node ID and the root share ID.
    // Retain the ancestor fallback for providers that expose a share on a
    // nearer parent instead of the top-level folder.
    QStringList paths {remotePath};
    const QString root = QStringLiteral("/") + remotePath.split('/', Qt::SkipEmptyParts).first();
    if (root != remotePath) paths.append(root);
    for (QString parent = QFileInfo(remotePath).path(); parent != QStringLiteral("/") && parent != root;
         parent = QFileInfo(parent).path()) {
        paths.append(parent);
    }
    QString volumeId;
    QString folderId;
    for (const QString &path : paths) {
        const ProcessOutput output = runner.run({
            QStringLiteral("filesystem"), QStringLiteral("info"), QStringLiteral("-j"), path,
        });
        if (!output.successful()) {
            return {{}, output.standardError.trimmed().isEmpty()
                    ? QStringLiteral("Unable to locate the backup folder in Proton Drive.")
                    : output.standardError.trimmed()};
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(output.standardOutput.toUtf8(), &parseError);
        const QJsonObject metadata = document.object();
        const QStringList uid = metadata.value(QStringLiteral("uid")).toString().split('~');
        if (parseError.error != QJsonParseError::NoError || !document.isObject()
            || metadata.value(QStringLiteral("type")).toString() != QStringLiteral("folder")
            || uid.size() != 2 || uid.at(0).isEmpty() || uid.at(1).isEmpty()
            || (!volumeId.isEmpty() && uid.at(0) != volumeId)) {
            return {{}, QStringLiteral("Proton Drive returned invalid folder metadata.")};
        }
        if (folderId.isEmpty()) {
            volumeId = uid.at(0);
            folderId = uid.at(1);
        }
        const QString shareId = metadata.value(QStringLiteral("deprecatedShareId")).toString();
        if (!shareId.isEmpty()) {
            const QByteArray url = QByteArray("https://drive.proton.me/")
                + QUrl::toPercentEncoding(shareId) + QByteArray("/folder/") + QUrl::toPercentEncoding(folderId);
            const QUrl resolved = QUrl::fromEncoded(url);
            if (!validUrl(resolved)) {
                return {{}, QStringLiteral("Proton Drive returned invalid folder metadata.")};
            }
            remember(cachePath, remotePath, resolved);
            return {resolved, {}};
        }
    }
    return {{}, QStringLiteral("The Proton Drive folder's browser link is unavailable.")};
}
