#include "backupconfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

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
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("The native backup configuration could not be opened.");
        }

        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const QJsonObject object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("The native backup configuration is malformed.");
        }

        return false;
    }

    const QString source = object.value(QStringLiteral("source_directory")).toString();
    const QString remote = object.value(QStringLiteral("remote_root")).toString();
    if (source.isEmpty() || remote.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The native backup configuration is incomplete.");
        }

        return false;
    }

    config->sourceDirectory = source;
    config->remoteRoot = remote;
    config->protonBinary = object.value(QStringLiteral("proton_binary")).toString(QStringLiteral("proton-drive"));

    return true;
}

bool BackupConfigStore::save(const BackupConfig &config, QString *error) const
{
    if (config.sourceDirectory.isEmpty() || config.remoteRoot.isEmpty() || config.protonBinary.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The native backup configuration is incomplete.");
        }

        return false;
    }

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to create the native configuration directory.");
        }

        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the native backup configuration.");
        }

        return false;
    }

    const QByteArray contents = QJsonDocument(QJsonObject {
        {QStringLiteral("source_directory"), config.sourceDirectory},
        {QStringLiteral("remote_root"), config.remoteRoot},
        {QStringLiteral("proton_binary"), config.protonBinary},
    }).toJson(QJsonDocument::Indented);

    if (file.write(contents) != contents.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to finish writing the native backup configuration.");
        }

        return false;
    }

    return true;
}
