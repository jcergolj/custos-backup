#include "backupmanifest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QtMath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool BackupManifest::load(const QString &path, QVector<BackupEntry> *entries, QString *error)
{
    if (entries == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("A destination for manifest entries is required.");
        }

        return false;
    }

    entries->clear();

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
        if (!value.isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup manifest contains an invalid entry.");
            }

            return false;
        }

        const QJsonObject object = value.toObject();
        const QString source = object.value(QStringLiteral("source")).toString();
        const QString remote = object.value(QStringLiteral("remote")).toString();
        const QJsonValue sizeValue = object.value(QStringLiteral("size"));
        const QByteArray checksum = QByteArray::fromHex(object.value(QStringLiteral("sha256")).toString().toLatin1());
        if (source.isEmpty() || remote.isEmpty() || remote.startsWith('/')
            || remote.contains(QStringLiteral("..")) || !sizeValue.isDouble()
            || sizeValue.toDouble() < 0 || sizeValue.toDouble() != qFloor(sizeValue.toDouble())
            || checksum.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256)) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup manifest contains an unsafe path.");
            }

            return false;
        }

        entries->append({source, remote, static_cast<qint64>(sizeValue.toDouble()), checksum});
    }

    return true;
}
