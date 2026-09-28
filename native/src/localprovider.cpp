#include "localprovider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

LocalProvider::LocalProvider(QString rootPath)
    : rootPath(std::move(rootPath))
{
}

bool LocalProvider::upload(const QString &localPath, const QString &remotePath, QString *error)
{
    const QString destination = QDir(rootPath).filePath(remotePath);
    QDir().mkpath(QFileInfo(destination).absolutePath());

    if (QFile::copy(localPath, destination)) {
        return true;
    }

    if (error != nullptr) {
        *error = QStringLiteral("Unable to copy the file to the provider.");
    }

    return false;
}

bool LocalProvider::download(const QString &remotePath, const QString &localPath, QString *error)
{
    if (QFile::exists(localPath)) {
        QFile::remove(localPath);
    }

    if (QFile::copy(QDir(rootPath).filePath(remotePath), localPath)) {
        return true;
    }

    if (error != nullptr) {
        *error = QStringLiteral("Unable to download the file from the provider.");
    }

    return false;
}

bool LocalProvider::inspect(const QString &remotePath, RemoteFile *file, QString *error)
{
    const QFileInfo info(QDir(rootPath).filePath(remotePath));

    if (!info.isFile()) {
        if (error != nullptr) {
            *error = QStringLiteral("The remote file is unavailable.");
        }

        return false;
    }

    QFile source(info.filePath());
    if (!source.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("The remote file could not be read.");
        }

        return false;
    }

    file->path = remotePath;
    file->size = info.size();
    file->checksum = QCryptographicHash::hash(source.readAll(), QCryptographicHash::Sha256);

    return true;
}
