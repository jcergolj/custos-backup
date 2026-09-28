#include "backupengine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

BackupEngine::BackupEngine(QObject *parent)
    : QObject(parent)
{
}

bool BackupEngine::validateSelection(const QString &sourceDirectory, QString *error) const
{
    const QFileInfo source(sourceDirectory);

    if (!source.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected folder does not exist.");
        }

        return false;
    }

    if (!source.isDir()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected path is not a folder.");
        }

        return false;
    }

    if (source.isSymLink()) {
        if (error != nullptr) {
            *error = QStringLiteral("Symbolic links are not valid backup roots.");
        }

        return false;
    }

    return true;
}

QStringList BackupEngine::selectableFiles(const QString &sourceDirectory) const
{
    if (!validateSelection(sourceDirectory)) {
        return {};
    }

    QStringList files;
    QDirIterator iterator(
        sourceDirectory,
        QDir::Files | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
    );

    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo file = iterator.fileInfo();

        if (!file.isSymLink()) {
            files.append(file.absoluteFilePath());
        }
    }

    files.sort();

    return files;
}

QString BackupEngine::previewError(const QString &sourceDirectory) const
{
    QString error;
    validateSelection(sourceDirectory, &error);

    return error;
}

bool BackupEngine::backup(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider &provider, QString *manifestPath, QString *error) const
{
    const QStringList files = selectableFiles(sourceDirectory);
    if (files.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected folder contains no regular files.");
        }

        return false;
    }

    QJsonArray entries;
    for (const QString &sourcePath : files) {
        const QString relativePath = QDir(sourceDirectory).relativeFilePath(sourcePath);
        const QString remotePath = QDir(remoteRoot).filePath(relativePath);
        QString providerError;

        if (!provider.upload(sourcePath, remotePath, &providerError)) {
            if (error != nullptr) {
                *error = providerError;
            }

            return false;
        }

        RemoteFile remoteFile;
        if (!provider.inspect(remotePath, &remoteFile, &providerError)
            || remoteFile.size != QFileInfo(sourcePath).size()) {
            if (error != nullptr) {
                *error = providerError.isEmpty()
                    ? QStringLiteral("Remote verification failed.")
                    : providerError;
            }

            return false;
        }

        entries.append(QJsonObject {
            {QStringLiteral("source"), sourcePath},
            {QStringLiteral("remote"), remotePath},
            {QStringLiteral("size"), remoteFile.size},
            {QStringLiteral("sha256"), QString::fromLatin1(remoteFile.checksum.toHex())},
        });
    }

    const QString path = QDir(remoteRoot).filePath(QStringLiteral("manifest.json"));
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile manifest(path);
    if (!manifest.open(QIODevice::WriteOnly)
        || manifest.write(QJsonDocument(QJsonObject {
            {QStringLiteral("version"), 1},
            {QStringLiteral("entries"), entries},
        }).toJson()) == -1
        || !manifest.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the backup manifest.");
        }

        return false;
    }

    if (manifestPath != nullptr) {
        *manifestPath = path;
    }

    return true;
}

bool BackupEngine::restoreFile(const BackupEntry &entry, const QString &destinationDirectory, BackupProvider &provider, QString *error) const
{
    const QString relativePath = entry.sourcePath.startsWith('/')
        ? QFileInfo(entry.sourcePath).fileName()
        : entry.sourcePath;
    const QString destination = QDir(destinationDirectory).filePath(relativePath);
    const QString canonicalRoot = QFileInfo(destinationDirectory).canonicalFilePath();
    const QString canonicalDestination = QFileInfo(destination).absoluteFilePath();

    if (canonicalRoot.isEmpty() || !canonicalDestination.startsWith(canonicalRoot + QDir::separator())) {
        if (error != nullptr) {
            *error = QStringLiteral("The restore destination is outside the selected folder.");
        }

        return false;
    }

    QDir().mkpath(QFileInfo(destination).absolutePath());

    return provider.download(entry.remotePath, destination, error);
}
