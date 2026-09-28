#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QVector>

struct RemoteFile {
    QString path;
    qint64 size = 0;
    QByteArray checksum;
};

struct RemoteItem {
    QString path;
    QString name;
    bool directory = false;
    qint64 size = 0;
    QDateTime modified;
};

class BackupProvider
{
public:
    virtual ~BackupProvider() = default;

    virtual bool upload(const QString &localPath, const QString &remotePath, QString *error = nullptr) = 0;
    virtual bool ensureDirectory(const QString &remotePath, QString *error = nullptr) = 0;
    virtual bool download(const QString &remotePath, const QString &localPath, QString *error = nullptr) = 0;
    virtual bool inspect(const QString &remotePath, RemoteFile *file, QString *error = nullptr) = 0;
    virtual bool list(const QString &remotePath, QVector<RemoteItem> *items, QString *error = nullptr) = 0;
    virtual bool trash(const QString &remotePath, QString *error = nullptr) = 0;
    virtual bool permanentlyDelete(const QString &remotePath, QString *error = nullptr) = 0;
};
