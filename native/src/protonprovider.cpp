#include "protonprovider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtMath>

ProtonProvider::ProtonProvider(ProcessRunner &runner)
    : runner(runner)
{
}

bool ProtonProvider::upload(const QString &localPath, const QString &remotePath, QString *error)
{
    return run({
        QStringLiteral("filesystem"), QStringLiteral("upload"), QStringLiteral("-j"),
        QStringLiteral("-f"), QStringLiteral("replace"), QStringLiteral("-d"), QStringLiteral("replace"),
        QStringLiteral("-t"), localPath, QFileInfo(remotePath).path(),
    }, error);
}

bool ProtonProvider::ensureDirectory(const QString &remotePath, QString *error)
{
    if (remotePath.isEmpty() || !remotePath.startsWith('/')) {
        if (error != nullptr) {
            *error = QStringLiteral("The provider path is invalid.");
        }
        return false;
    }
    const QStringList parts = remotePath.split('/', Qt::SkipEmptyParts);
    QString current = QStringLiteral("/");
    for (const QString &part : parts) {
        if (current != QStringLiteral("/")) {
            current += QStringLiteral("/");
        }
        current += part;
        QVector<RemoteItem> children;
        QString listError;
        if (list(current, &children, &listError)) {
            continue;
        }
        if (!run({QStringLiteral("filesystem"), QStringLiteral("create-folder"), QFileInfo(current).path(), part}, error)) {
            return false;
        }
    }
    return true;
}

bool ProtonProvider::download(const QString &remotePath, const QString &localPath, QString *error)
{
    const QString localFolder = QFileInfo(localPath).absolutePath();
    if (!QDir().mkpath(localFolder)) {
        if (error != nullptr) {
            *error = QStringLiteral("The local download folder could not be created.");
        }
        return false;
    }
    if (!run({
        QStringLiteral("filesystem"), QStringLiteral("download"), QStringLiteral("-j"),
        QStringLiteral("-f"), QStringLiteral("remove"), QStringLiteral("-d"), QStringLiteral("remove"),
        remotePath, localFolder,
    }, error)) {
        return false;
    }

    const QString downloaded = QDir(localFolder).filePath(QFileInfo(remotePath).fileName());
    if (downloaded != localPath && QFileInfo::exists(downloaded)) {
        QFile::remove(localPath);
        if (!QFile::rename(downloaded, localPath)) {
            if (error != nullptr) {
                *error = QStringLiteral("The downloaded Proton Drive file could not be placed at the requested path.");
            }
            return false;
        }
    }
    return QFileInfo::exists(localPath);
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
    const QJsonObject revision = object.value(QStringLiteral("activeRevision")).toObject();
    const QJsonValue size = object.contains(QStringLiteral("size"))
        ? object.value(QStringLiteral("size"))
        : object.contains(QStringLiteral("totalStorageSize"))
            ? object.value(QStringLiteral("totalStorageSize"))
            : revision.value(QStringLiteral("claimedSize"));

    const QByteArray checksum = QByteArray::fromHex(object.value(QStringLiteral("sha256")).toString().toLatin1());
    if (parseError.error != QJsonParseError::NoError || !document.isObject() || !size.isDouble()
        || size.toDouble() < 0 || size.toDouble() != qFloor(size.toDouble())
        || (!checksum.isEmpty() && checksum.size() != QCryptographicHash::hashLength(QCryptographicHash::Sha256))) {
        if (error != nullptr) {
            *error = QStringLiteral("Proton Drive returned invalid file metadata.");
        }

        return false;
    }

    file->path = remotePath;
    file->size = static_cast<qint64>(size.toDouble());
    file->checksum = checksum;

    return true;
}

bool ProtonProvider::list(const QString &remotePath, QVector<RemoteItem> *items, QString *error)
{
    if (items == nullptr) {
        if (error != nullptr) {
            *error = QStringLiteral("A destination for remote items is required.");
        }
        return false;
    }

    const ProcessOutput output = runner.run({
        QStringLiteral("filesystem"), QStringLiteral("list"), QStringLiteral("-j"), remotePath,
    });
    if (!output.successful()) {
        if (error != nullptr) {
            *error = output.standardError.isEmpty() ? QStringLiteral("Unable to list the Proton Drive folder.") : output.standardError.trimmed();
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output.standardOutput.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error != nullptr) {
            *error = QStringLiteral("Proton Drive returned invalid folder metadata.");
        }
        return false;
    }

    QJsonArray values;
    if (document.isArray()) {
        values = document.array();
    } else if (document.isObject()) {
        const QJsonObject object = document.object();
        values = object.value(QStringLiteral("items")).toArray();
        if (values.isEmpty()) {
            values = object.value(QStringLiteral("entries")).toArray();
        }
    }

    items->clear();
    for (const QJsonValue &value : values) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const QJsonValue nameValue = object.value(QStringLiteral("name"));
        const QString name = nameValue.isObject()
            ? nameValue.toObject().value(QStringLiteral("value")).toString()
            : nameValue.toString();
        const QString path = object.value(QStringLiteral("path")).toString(
            name.isEmpty() ? QString() : QDir(remotePath).filePath(name));
        const QString type = object.value(QStringLiteral("type")).toString().toLower();
        const bool directory = object.value(QStringLiteral("directory")).toBool(false)
            || object.value(QStringLiteral("is_dir")).toBool(false)
            || type == QStringLiteral("directory") || type == QStringLiteral("folder");
        if (name.isEmpty() && path.isEmpty()) {
            continue;
        }
        items->append({
            path,
            name.isEmpty() ? QFileInfo(path).fileName() : name,
            directory,
            static_cast<qint64>(object.value(QStringLiteral("size")).toDouble(
                object.value(QStringLiteral("totalStorageSize")).toDouble())),
            QDateTime::fromString(object.value(QStringLiteral("modified")).toString(
                object.value(QStringLiteral("modificationTime")).toString()), Qt::ISODateWithMs),
        });
    }
    return true;
}

bool ProtonProvider::trash(const QString &remotePath, QString *error)
{
    return run({QStringLiteral("filesystem"), QStringLiteral("trash"), remotePath}, error);
}

bool ProtonProvider::permanentlyDelete(const QString &remotePath, QString *error)
{
    return run({QStringLiteral("filesystem"), QStringLiteral("delete"), remotePath}, error);
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
