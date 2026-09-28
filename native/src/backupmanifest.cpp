#include "backupmanifest.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QtMath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {

bool stringArray(const QJsonValue &value)
{
    if (!value.isArray()) {
        return false;
    }
    for (const QJsonValue &item : value.toArray()) {
        if (!item.isString()) {
            return false;
        }
    }
    return true;
}

}

bool BackupManifest::load(const QString &path, QVector<BackupEntry> *entries, QString *error)
{
    return load(path, entries, nullptr, error);
}

bool BackupManifest::load(const QString &path, QVector<BackupEntry> *entries, BackupManifestInfo *info, QString *error)
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
    const int version = root.value(QStringLiteral("version")).toInt();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || (version != 1 && version != 2)) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup manifest is malformed or unsupported.");
        }

        return false;
    }

    if (version == 2 && (root.value(QStringLiteral("application")).toString() != QStringLiteral("praefectus")
            || root.value(QStringLiteral("computer")).toString().isEmpty()
            || root.value(QStringLiteral("set_id")).toString().isEmpty()
            || root.value(QStringLiteral("copy_id")).toString().isEmpty()
            || !QDateTime::fromString(root.value(QStringLiteral("created_at")).toString(), Qt::ISODateWithMs).isValid()
            || (root.value(QStringLiteral("status")).toString() != QStringLiteral("complete")
                && root.value(QStringLiteral("status")).toString() != QStringLiteral("incomplete"))
            || !stringArray(root.value(QStringLiteral("expected")))
            || !stringArray(root.value(QStringLiteral("failed"))))) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup manifest is malformed or unsupported.");
        }
        return false;
    }

    const QJsonValue entriesValue = root.value(QStringLiteral("entries"));
    if (!entriesValue.isArray()) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup manifest does not contain an entries list.");
        }
        return false;
    }
    const QJsonArray manifestEntries = entriesValue.toArray();
    if (info != nullptr) {
        info->version = version;
        info->application = root.value(QStringLiteral("application")).toString();
        info->computerName = root.value(QStringLiteral("computer")).toString();
        info->setId = root.value(QStringLiteral("set_id")).toString();
        info->setName = root.value(QStringLiteral("set_name")).toString();
        info->copyId = root.value(QStringLiteral("copy_id")).toString();
        info->createdAt = QDateTime::fromString(root.value(QStringLiteral("created_at")).toString(), Qt::ISODateWithMs);
        info->status = root.value(QStringLiteral("status")).toString(version == 1 ? QStringLiteral("complete") : QString());
        info->expectedItems.clear();
        for (const QJsonValue &item : root.value(QStringLiteral("expected")).toArray()) {
            if (item.isString()) {
                info->expectedItems.append(item.toString());
            }
        }
        info->failedItems.clear();
        for (const QJsonValue &item : root.value(QStringLiteral("failed")).toArray()) {
            if (item.isString()) {
                info->failedItems.append(item.toString());
            }
        }
    }
    for (const QJsonValue &value : manifestEntries) {
        if (!value.isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup manifest contains an unsafe path.");
            }

            return false;
        }

        const QJsonObject object = value.toObject();
        const QString source = object.value(QStringLiteral("source")).toString();
        const QString remote = object.value(QStringLiteral("remote")).toString();
        const QString restore = object.value(QStringLiteral("restore")).toString();
        const QJsonValue sizeValue = object.value(QStringLiteral("size"));
        const QByteArray checksum = QByteArray::fromHex(object.value(QStringLiteral("sha256")).toString().toLatin1());
        const QStringList remoteParts = remote.split('/', Qt::KeepEmptyParts);
        const bool containsParentSegment = std::any_of(
            remoteParts.cbegin(), remoteParts.cend(), [](const QString &part) {
                return part == QStringLiteral("..");
            });
        const QString restorePath = restore.isEmpty() ? QFileInfo(source).fileName() : restore;
        const QStringList restoreParts = restorePath.split('/', Qt::KeepEmptyParts);
        const bool unsafeRestorePath = restorePath.isEmpty() || restorePath.startsWith('/')
            || std::any_of(restoreParts.cbegin(), restoreParts.cend(), [](const QString &part) {
                return part == QStringLiteral("..");
            });
        if (source.isEmpty() || remote.isEmpty() || unsafeRestorePath
            || containsParentSegment || !sizeValue.isDouble()
            || sizeValue.toDouble() < 0 || sizeValue.toDouble() != qFloor(sizeValue.toDouble())
            || checksum.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256)) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup manifest contains an unsafe path.");
            }

            return false;
        }

        entries->append({source, remote, static_cast<qint64>(sizeValue.toDouble()), checksum, restorePath});
    }

    return true;
}
