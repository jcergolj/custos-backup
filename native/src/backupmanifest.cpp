#include "backupmanifest.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool BackupManifest::load(const QString &path, QVector<BackupEntry> *entries, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup manifest could not be opened.");
        }

        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject root = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || root.value(QStringLiteral("version")).toInt() != 1) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup manifest is malformed or unsupported.");
        }

        return false;
    }

    const QJsonArray manifestEntries = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue &value : manifestEntries) {
        const QJsonObject object = value.toObject();
        const QString source = object.value(QStringLiteral("source")).toString();
        const QString remote = object.value(QStringLiteral("remote")).toString();
        if (source.isEmpty() || remote.isEmpty() || remote.startsWith('/') || remote.contains(QStringLiteral(".."))) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup manifest contains an unsafe path.");
            }

            return false;
        }

        entries->append({source, remote, static_cast<qint64>(object.value(QStringLiteral("size")).toDouble())});
    }

    return true;
}
