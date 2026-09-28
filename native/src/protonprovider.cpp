#include "protonprovider.h"

#include <QJsonDocument>
#include <QJsonObject>

ProtonProvider::ProtonProvider(ProcessRunner &runner)
    : runner(runner)
{
}

bool ProtonProvider::upload(const QString &localPath, const QString &remotePath, QString *error)
{
    return run({QStringLiteral("filesystem"), QStringLiteral("upload"), QStringLiteral("-j"), localPath, remotePath}, error);
}

bool ProtonProvider::download(const QString &remotePath, const QString &localPath, QString *error)
{
    return run({QStringLiteral("filesystem"), QStringLiteral("download"), QStringLiteral("-j"), remotePath, localPath}, error);
}

bool ProtonProvider::inspect(const QString &remotePath, RemoteFile *file, QString *error)
{
    if (file == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("A destination for remote file metadata is required.");
        }

        return false;
    }

    const ProcessOutput output = runner.run({QStringLiteral("filesystem"), QStringLiteral("info"), QStringLiteral("-j"), remotePath});
    if (!output.successful()) {
        if (error != nullptr) {
            *error = output.standardError.isEmpty() ? QStringLiteral("Unable to inspect the Proton Drive file.") : output.standardError.trimmed();
        }

        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output.standardOutput.toUtf8(), &parseError);
    const QJsonObject object = document.object();
    const QJsonValue size = object.value(QStringLiteral("size"));

    if (parseError.error != QJsonParseError::NoError || !document.isObject() || !size.isDouble()) {
        if (error != nullptr) {
            *error = QStringLiteral("Proton Drive returned invalid file metadata.");
        }

        return false;
    }

    file->path = remotePath;
    file->size = static_cast<qint64>(size.toDouble());
    file->checksum = object.value(QStringLiteral("sha256")).toString().toLatin1();

    return true;
}

bool ProtonProvider::run(const QStringList &arguments, QString *error) const
{
    const ProcessOutput output = runner.run(arguments);
    if (output.successful()) {
        return true;
    }

    if (error != nullptr) {
        *error = output.standardError.isEmpty() ? QStringLiteral("The Proton Drive command failed.") : output.standardError.trimmed();
    }

    return false;
}
