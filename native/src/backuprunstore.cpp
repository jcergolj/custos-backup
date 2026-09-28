#include "backuprunstore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QDateTime readDate(const QJsonObject &object, const QString &key)
{
    return QDateTime::fromString(object.value(key).toString(), Qt::ISODateWithMs);
}

void writeDate(QJsonObject &object, const QString &key, const QDateTime &value)
{
    if (value.isValid()) {
        object.insert(key, value.toString(Qt::ISODateWithMs));
    }
}

}

BackupRunStore::BackupRunStore(QString path)
    : path(std::move(path))
{
}

bool BackupRunStore::load(QString *error)
{
    runRecords.clear();
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup run state could not be opened.");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup run state is malformed.");
        }
        return false;
    }

    const QJsonArray records = document.object().value(QStringLiteral("runs")).toArray();
    for (const QJsonValue &value : records) {
        if (!value.isObject()) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup run state contains an invalid record.");
            }
            return false;
        }
        const QJsonObject object = value.toObject();
        BackupRunRecord record;
        record.setId = object.value(QStringLiteral("set_id")).toString();
        record.status = object.value(QStringLiteral("status")).toString(QStringLiteral("idle"));
        record.reason = object.value(QStringLiteral("reason")).toString();
        record.lastError = object.value(QStringLiteral("last_error")).toString();
        record.attempts = object.value(QStringLiteral("attempts")).toInt();
        record.scheduledFor = readDate(object, QStringLiteral("scheduled_for"));
        record.nextAttempt = readDate(object, QStringLiteral("next_attempt"));
        record.lastScheduled = readDate(object, QStringLiteral("last_scheduled"));
        record.nextScheduled = readDate(object, QStringLiteral("next_scheduled"));
        record.lastSuccess = readDate(object, QStringLiteral("last_success"));
        record.lastFailure = readDate(object, QStringLiteral("last_failure"));
        if (record.setId.isEmpty()) {
            if (error != nullptr) {
                *error = QStringLiteral("The backup run state contains an invalid record.");
            }
            return false;
        }
        runRecords.append(record);
    }

    return true;
}

bool BackupRunStore::save(QString *error) const
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to create the backup run state directory.");
        }
        return false;
    }

    QJsonArray records;
    for (const BackupRunRecord &record : runRecords) {
        QJsonObject object {
            {QStringLiteral("set_id"), record.setId},
            {QStringLiteral("status"), record.status},
            {QStringLiteral("reason"), record.reason},
            {QStringLiteral("last_error"), record.lastError},
            {QStringLiteral("attempts"), record.attempts},
        };
        writeDate(object, QStringLiteral("scheduled_for"), record.scheduledFor);
        writeDate(object, QStringLiteral("next_attempt"), record.nextAttempt);
        writeDate(object, QStringLiteral("last_scheduled"), record.lastScheduled);
        writeDate(object, QStringLiteral("next_scheduled"), record.nextScheduled);
        writeDate(object, QStringLiteral("last_success"), record.lastSuccess);
        writeDate(object, QStringLiteral("last_failure"), record.lastFailure);
        records.append(object);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the backup run state.");
        }
        return false;
    }
    const QByteArray contents = QJsonDocument(QJsonObject {{QStringLiteral("runs"), records}}).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to finish writing the backup run state.");
        }
        return false;
    }
    return true;
}

QString BackupRunStore::filePath() const
{
    return path;
}

QVector<BackupRunRecord> &BackupRunStore::records()
{
    return runRecords;
}

const QVector<BackupRunRecord> &BackupRunStore::records() const
{
    return runRecords;
}

void BackupRunStore::ensureSet(const QString &setId)
{
    if (find(setId) == nullptr) {
        runRecords.append({setId});
    }
}

bool BackupRunStore::enqueue(const QString &setId, const QString &reason, const QDateTime &scheduledFor)
{
    ensureSet(setId);
    BackupRunRecord *record = find(setId);
    if (record->status == QStringLiteral("pending") || record->status == QStringLiteral("running")
        || record->status == QStringLiteral("retrying") || record->status == QStringLiteral("waiting")) {
        return false;
    }

    record->status = QStringLiteral("pending");
    record->reason = reason;
    record->lastError.clear();
    record->scheduledFor = scheduledFor;
    record->nextAttempt = scheduledFor.isValid() ? scheduledFor : QDateTime::currentDateTime();
    return true;
}

QVector<int> BackupRunStore::readyIndexes(const QDateTime &now) const
{
    QVector<int> indexes;
    for (int index = 0; index < runRecords.size(); ++index) {
        const BackupRunRecord &record = runRecords.at(index);
        if ((record.status == QStringLiteral("pending") || record.status == QStringLiteral("retrying")
             || record.status == QStringLiteral("waiting") || record.status == QStringLiteral("incomplete"))
            && (!record.nextAttempt.isValid() || record.nextAttempt <= now)) {
            indexes.append(index);
        }
    }
    return indexes;
}

BackupRunRecord *BackupRunStore::find(const QString &setId)
{
    for (BackupRunRecord &record : runRecords) {
        if (record.setId == setId) {
            return &record;
        }
    }
    return nullptr;
}

const BackupRunRecord *BackupRunStore::find(const QString &setId) const
{
    for (const BackupRunRecord &record : runRecords) {
        if (record.setId == setId) {
            return &record;
        }
    }
    return nullptr;
}

void BackupRunStore::markRunning(BackupRunRecord &record)
{
    record.status = QStringLiteral("running");
    record.attempts++;
}

void BackupRunStore::markSuccess(BackupRunRecord &record, const QDateTime &now)
{
    record.status = QStringLiteral("success");
    record.lastSuccess = now;
    record.lastError.clear();
    record.nextAttempt = {};
    record.attempts = 0;
}

void BackupRunStore::markWaiting(BackupRunRecord &record, const QString &reason, const QDateTime &now)
{
    record.status = QStringLiteral("waiting");
    record.lastError = reason;
    record.nextAttempt = now.addSecs(60);
}

void BackupRunStore::markRetrying(BackupRunRecord &record, const QString &error, const QDateTime &now)
{
    record.status = QStringLiteral("retrying");
    record.lastError = error;
    record.lastFailure = now;
    record.nextAttempt = now.addSecs(retryDelaySeconds(record.attempts));
}

void BackupRunStore::markIncomplete(BackupRunRecord &record, const QString &error, const QDateTime &now)
{
    record.status = QStringLiteral("incomplete");
    record.lastError = error;
    record.lastFailure = now;
    record.nextAttempt = now.addSecs(retryDelaySeconds(record.attempts));
}

void BackupRunStore::markAuthenticationRequired(BackupRunRecord &record, const QString &error, const QDateTime &now)
{
    record.status = QStringLiteral("authentication_required");
    record.lastError = error;
    record.lastFailure = now;
    record.nextAttempt = {};
}

int BackupRunStore::retryDelaySeconds(int attempt)
{
    return qMin(3600, 5 * (1 << qMin(9, qMax(0, attempt - 1))));
}
