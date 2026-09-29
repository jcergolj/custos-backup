#include "backupconfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace {

bool validSchedule(const BackupSchedule &schedule)
{
    return (schedule.frequency == QStringLiteral("disabled")
            || schedule.frequency == QStringLiteral("daily")
            || schedule.frequency == QStringLiteral("weekly")
            || schedule.frequency == QStringLiteral("monthly"))
        && schedule.hour >= 0 && schedule.hour <= 23
        && schedule.minute >= 0 && schedule.minute <= 59
        && schedule.weekday >= 1 && schedule.weekday <= 7
        && schedule.dayOfMonth >= 1 && schedule.dayOfMonth <= 31;
}

bool validHex(const QString &value)
{
    return value.size() % 2 == 0 && std::all_of(value.cbegin(), value.cend(), [](const QChar character) {
        const QChar lower = character.toLower();
        return (character >= QChar('0') && character <= QChar('9'))
            || (lower >= QChar('a') && lower <= QChar('f'));
    });
}

}

BackupConfigStore::BackupConfigStore(QString path)
    : path(std::move(path))
{
}

QString BackupConfigStore::filePath() const
{
    return path;
}

bool BackupConfigStore::load(BackupConfig *config, QString *error) const
{
    if (config == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("A destination for Custos backup configuration is required.");
        }

        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("The Custos backup configuration could not be opened.");
        }

        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("The Custos backup configuration is malformed.");
        }

        return false;
    }

    config->sets.clear();
    config->protonBinary = object.value(QStringLiteral("proton_binary")).toString(QStringLiteral("proton-drive"));

    if (object.contains(QStringLiteral("sets"))) {
        const QJsonValue setsValue = object.value(QStringLiteral("sets"));
        if (!setsValue.isArray() || config->protonBinary.isEmpty()) {
            if (error != nullptr) {
                *error = QStringLiteral("The Custos backup configuration is malformed.");
            }

            return false;
        }

        QSet<QString> setIds;
        for (const QJsonValue &setValue : setsValue.toArray()) {
            if (!setValue.isObject()) {
                if (error != nullptr) {
                    *error = QStringLiteral("The Custos backup configuration contains an invalid set.");
                }

                return false;
            }

            const QJsonObject setObject = setValue.toObject();
            const QJsonArray sources = setObject.value(QStringLiteral("source_directories")).toArray();
            const QJsonArray exclusions = setObject.value(QStringLiteral("exclusions")).toArray();
            BackupSet set {
                setObject.value(QStringLiteral("id")).toString(),
                setObject.value(QStringLiteral("name")).toString(),
                setObject.value(QStringLiteral("remote_root")).toString(),
            };

            for (const QJsonValue &source : sources) {
                set.sourceDirectories.append(source.toString());
            }
            for (const QJsonValue &exclusion : exclusions) {
                set.exclusions.append(exclusion.toString());
            }

            const QJsonObject schedule = setObject.value(QStringLiteral("schedule")).toObject();
            set.schedule.frequency = schedule.value(QStringLiteral("frequency")).toString(QStringLiteral("disabled"));
            set.schedule.hour = schedule.value(QStringLiteral("hour")).toInt(2);
            set.schedule.minute = schedule.value(QStringLiteral("minute")).toInt(0);
            set.schedule.weekday = schedule.value(QStringLiteral("weekday")).toInt(1);
            set.schedule.dayOfMonth = schedule.value(QStringLiteral("day_of_month")).toInt(1);
            set.retention = qMax(1, setObject.value(QStringLiteral("retention")).toInt(3));
            set.onlyOnAcPower = setObject.value(QStringLiteral("only_on_ac_power")).toBool(false);
            for (const QJsonValue &volumeValue : setObject.value(QStringLiteral("required_volumes")).toArray()) {
                const QJsonObject volume = volumeValue.toObject();
                const QString deviceId = volume.value(QStringLiteral("device_id")).toString();
                if (!volumeValue.isObject() || !validHex(deviceId)) {
                    if (error != nullptr) {
                        *error = QStringLiteral("The Custos backup configuration is malformed.");
                    }
                    return false;
                }
                set.requiredVolumes.append({
                    volume.value(QStringLiteral("mount_path")).toString(),
                    QByteArray::fromHex(deviceId.toLatin1()),
                });
            }

            if (set.id.trimmed().isEmpty() || set.name.trimmed().isEmpty() || set.remoteRoot.trimmed().isEmpty()
                || set.sourceDirectories.isEmpty()
                || !validSchedule(set.schedule)
                || setIds.contains(set.id)
                || std::any_of(set.sourceDirectories.cbegin(), set.sourceDirectories.cend(), [](const QString &source) {
                    return source.trimmed().isEmpty();
                })
                || std::any_of(set.requiredVolumes.cbegin(), set.requiredVolumes.cend(), [](const RequiredVolume &volume) {
                    return volume.mountPath.trimmed().isEmpty();
                })) {
                if (error != nullptr) {
                    *error = QStringLiteral("The Custos backup configuration is incomplete.");
                }

                return false;
            }

            setIds.insert(set.id);
            config->sets.append(set);
        }

        if (config->sets.isEmpty()) {
            if (error != nullptr) {
                *error = QStringLiteral("The Custos backup configuration is incomplete.");
            }

            return false;
        }

        config->sourceDirectory = config->sets.first().sourceDirectories.first();
        config->remoteRoot = config->sets.first().remoteRoot;

        return true;
    }

    const QString source = object.value(QStringLiteral("source_directory")).toString();
    const QString remote = object.value(QStringLiteral("remote_root")).toString();
    if (source.isEmpty() || remote.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The Custos backup configuration is incomplete.");
        }

        return false;
    }

    config->sourceDirectory = source;
    config->remoteRoot = remote;
    config->sets = {
        {
            QStringLiteral("default"),
            QStringLiteral("Default backup"),
            remote,
            {source},
            {},
        },
    };

    return true;
}

bool BackupConfigStore::save(const BackupConfig &config, QString *error) const
{
    if (config.protonBinary.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The Custos backup configuration is incomplete.");
        }

        return false;
    }

    QJsonObject object {
        {QStringLiteral("proton_binary"), config.protonBinary},
    };

    if (!config.sets.isEmpty()) {
        QJsonArray sets;
        QSet<QString> setIds;
        for (const BackupSet &set : config.sets) {
            if (set.id.trimmed().isEmpty() || set.name.trimmed().isEmpty() || set.remoteRoot.trimmed().isEmpty()
                || set.sourceDirectories.isEmpty() || !validSchedule(set.schedule) || setIds.contains(set.id)) {
                if (error != nullptr) {
                    *error = QStringLiteral("The Custos backup configuration is incomplete.");
                }

                return false;
            }
            setIds.insert(set.id);

            QJsonArray sources;
            for (const QString &source : set.sourceDirectories) {
                if (source.trimmed().isEmpty()) {
                    if (error != nullptr) {
                        *error = QStringLiteral("The Custos backup configuration is incomplete.");
                    }

                    return false;
                }
                sources.append(source);
            }

            QJsonArray exclusions;
            for (const QString &exclusion : set.exclusions) {
                exclusions.append(exclusion);
            }

            QJsonArray volumes;
            for (const RequiredVolume &volume : set.requiredVolumes) {
                if (volume.mountPath.trimmed().isEmpty()) {
                    if (error != nullptr) {
                        *error = QStringLiteral("The Custos backup configuration is incomplete.");
                    }

                    return false;
                }
                volumes.append(QJsonObject {
                    {QStringLiteral("mount_path"), volume.mountPath},
                    {QStringLiteral("device_id"), QString::fromLatin1(volume.deviceId.toHex())},
                });
            }

            const QJsonObject schedule {
                {QStringLiteral("frequency"), set.schedule.frequency},
                {QStringLiteral("hour"), set.schedule.hour},
                {QStringLiteral("minute"), set.schedule.minute},
                {QStringLiteral("weekday"), set.schedule.weekday},
                {QStringLiteral("day_of_month"), set.schedule.dayOfMonth},
            };

            sets.append(QJsonObject {
                {QStringLiteral("id"), set.id},
                {QStringLiteral("name"), set.name},
                {QStringLiteral("remote_root"), set.remoteRoot},
                {QStringLiteral("source_directories"), sources},
                {QStringLiteral("exclusions"), exclusions},
                {QStringLiteral("schedule"), schedule},
                {QStringLiteral("retention"), qMax(1, set.retention)},
                {QStringLiteral("only_on_ac_power"), set.onlyOnAcPower},
                {QStringLiteral("required_volumes"), volumes},
            });
        }
        object.insert(QStringLiteral("sets"), sets);
    } else {
        if (config.sourceDirectory.isEmpty() || config.remoteRoot.isEmpty()) {
            if (error != nullptr) {
                *error = QStringLiteral("The Custos backup configuration is incomplete.");
            }

            return false;
        }

        object.insert(QStringLiteral("source_directory"), config.sourceDirectory);
        object.insert(QStringLiteral("remote_root"), config.remoteRoot);
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to create the Custos configuration directory.");
        }

        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the Custos backup configuration.");
        }

        return false;
    }

    const QByteArray contents = QJsonDocument(object).toJson(QJsonDocument::Indented);

    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to finish writing the Custos backup configuration.");
        }

        return false;
    }

    return true;
}
