#pragma once

#include <QByteArray>
#include <QString>

struct RemoteFile {
    QString path;
    qint64 size = 0;
    QByteArray checksum;
};

class BackupProvider
{
public:
    virtual ~BackupProvider() = default;

    virtual bool upload(const QString &localPath, const QString &remotePath, QString *error = nullptr) = 0;
    virtual bool download(const QString &remotePath, const QString &localPath, QString *error = nullptr) = 0;
    virtual bool inspect(const QString &remotePath, RemoteFile *file, QString *error = nullptr) = 0;
};
