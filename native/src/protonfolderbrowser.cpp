#include "protonfolderbrowser.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrentRun>

namespace {

ProtonFolderLookup resolveFolder(ProcessRunner &runner, const QString &remotePath)
{
    if (!remotePath.startsWith('/') || QDir::cleanPath(remotePath) != remotePath) {
        return {{}, QStringLiteral("The remote backup folder is invalid.")};
    }

    QString path = remotePath;
    QString volumeId;
    QString folderId;
    while (path != QStringLiteral("/")) {
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
            // The Drive web app uses the ancestor share ID and the target node ID.
            const QByteArray url = QByteArray("https://drive.proton.me/")
                + QUrl::toPercentEncoding(shareId) + QByteArray("/folder/") + QUrl::toPercentEncoding(folderId);
            return {QUrl::fromEncoded(url), {}};
        }
        path = QFileInfo(path).path();
    }

    return {{}, QStringLiteral("The Proton Drive folder's browser link is unavailable.")};
}

}

ProtonFolderBrowser::ProtonFolderBrowser(ProcessRunner &runner, QObject *parent)
    : QObject(parent)
    , runner(runner)
{
    connect(&watcher, &QFutureWatcher<ProtonFolderLookup>::finished, this, [this] {
        const ProtonFolderLookup result = watcher.result();
        resolving = false;
        emit busyChanged();
        if (result.url.isEmpty()) {
            emit failed(result.error);
        } else {
            emit folderResolved(result.url);
        }
    });
}

ProtonFolderBrowser::~ProtonFolderBrowser()
{
    watcher.waitForFinished();
}

bool ProtonFolderBrowser::busy() const
{
    return resolving;
}

void ProtonFolderBrowser::openFolder(const QString &remotePath)
{
    if (resolving) {
        return;
    }
    resolving = true;
    emit busyChanged();
    watcher.setFuture(QtConcurrent::run([this, remotePath] { return resolveFolder(runner, remotePath); }));
}
