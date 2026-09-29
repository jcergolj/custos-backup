#include "backupengine.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {

QString cleanAbsolutePath(const QString &path)
{
    return QFileInfo(path).absoluteFilePath();
}

bool isWithinPath(const QString &path, const QString &root)
{
    const QString cleanPath = QDir::cleanPath(path);
    const QString cleanRoot = QDir::cleanPath(root);

    return cleanPath == cleanRoot || (cleanRoot == QStringLiteral("/")
        ? cleanPath.startsWith('/')
        : cleanPath.startsWith(cleanRoot + QDir::separator()));
}

bool isExcluded(const QString &path, const QStringList &exclusions)
{
    return std::any_of(exclusions.cbegin(), exclusions.cend(), [&path](const QString &exclusion) {
        return isWithinPath(path, cleanAbsolutePath(exclusion));
    });
}

QString remoteSegment(QString value)
{
    value = value.trimmed();
    QString sanitized;
    for (const QChar character : value) {
        sanitized.append(character.isLetterOrNumber() || character == '.' || character == '_' || character == '-'
                ? character
                : QChar('_'));
    }

    return sanitized.isEmpty() ? QStringLiteral("source") : sanitized;
}

bool hasParentPathSegment(const QString &path)
{
    const QStringList parts = path.split('/', Qt::KeepEmptyParts);
    return std::any_of(parts.cbegin(), parts.cend(), [](const QString &part) {
        return part == QStringLiteral("..");
    });
}

bool sha256(QFile &file, QByteArray *checksum)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            return false;
        }
        hash.addData(chunk);
    }
    *checksum = hash.result();
    return true;
}

}

BackupEngine::BackupEngine(QObject *parent)
    : QObject(parent)
{
}

bool BackupEngine::validateSelection(const QString &sourceDirectory, QString *error) const
{
    const QFileInfo source(sourceDirectory);

    if (!source.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected folder does not exist.");
        }

        return false;
    }

    if (!source.isDir()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected path is not a folder.");
        }

        return false;
    }

    if (source.isSymLink()) {
        if (error != nullptr) {
            *error = QStringLiteral("Symbolic links are not valid backup roots.");
        }

        return false;
    }

    return true;
}

QStringList BackupEngine::selectableFiles(const QString &sourceDirectory) const
{
    return selectableFiles(QStringList {sourceDirectory}, {});
}

QStringList BackupEngine::selectableFiles(const QStringList &sourceDirectories, const QStringList &exclusions) const
{
    return preview(sourceDirectories, exclusions).includedFiles;
}

QVariantMap BackupEngine::previewSelection(const QStringList &sourceDirectories, const QStringList &exclusions) const
{
    const BackupPreview result = preview(sourceDirectories, exclusions);

    return {
        {QStringLiteral("included"), result.includedFiles},
        {QStringLiteral("excluded"), result.excludedFiles},
        {QStringLiteral("skipped"), result.skippedPaths},
        {QStringLiteral("missing"), result.missingPaths},
    };
}

BackupPreview BackupEngine::preview(const QStringList &sourceDirectories, const QStringList &exclusions) const
{
    BackupPreview result;
    QStringList included;
    QStringList excluded;
    QStringList skipped;
    QStringList missing;

    for (const QString &sourceDirectory : sourceDirectories) {
        const QFileInfo source(sourceDirectory);
        if (!source.exists()) {
            missing.append(source.absoluteFilePath());
            continue;
        }
        if (source.isSymLink() || !source.isReadable()) {
            skipped.append(source.absoluteFilePath());
            continue;
        }
        if (isExcluded(source.absoluteFilePath(), exclusions)) {
            excluded.append(source.absoluteFilePath());
            continue;
        }
        if (source.isFile()) {
            included.append(source.absoluteFilePath());
            continue;
        }
        if (!source.isDir()) {
            skipped.append(source.absoluteFilePath());
            continue;
        }

        QDirIterator iterator(source.absoluteFilePath(), QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo file = iterator.fileInfo();
            const QString path = file.absoluteFilePath();

            if (file.isSymLink()) {
                skipped.append(path);
            } else if (file.isDir() && !file.isReadable()) {
                skipped.append(path);
            } else if (file.isFile() && !file.isReadable()) {
                skipped.append(path);
            } else if (file.isFile() && isExcluded(path, exclusions)) {
                excluded.append(path);
            } else if (file.isFile()) {
                included.append(path);
            }
        }
    }

    included.removeDuplicates();
    excluded.removeDuplicates();
    skipped.removeDuplicates();
    missing.removeDuplicates();
    included.sort();
    excluded.sort();
    skipped.sort();
    missing.sort();
    result.includedFiles = included;
    result.excludedFiles = excluded;
    result.skippedPaths = skipped;
    result.missingPaths = missing;

    return result;
}

QString BackupEngine::previewError(const QString &sourceDirectory) const
{
    QString error;
    validateSelection(sourceDirectory, &error);

    return error;
}

bool BackupEngine::backup(const QString &sourceDirectory, const QString &remoteRoot, BackupProvider &provider, QString *manifestPath, QString *error) const
{
    return backup(QStringList {sourceDirectory}, remoteRoot, {}, provider, manifestPath, error);
}

bool BackupEngine::backup(const QStringList &sourceDirectories, const QString &remoteRoot, const QStringList &exclusions, BackupProvider &provider, QString *manifestPath, QString *error) const
{
    return backup(sourceDirectories, remoteRoot, exclusions, {}, provider, manifestPath, error);
}

bool BackupEngine::backup(const QStringList &sourceDirectories, const QString &remoteRoot, const QStringList &exclusions, const BackupCopyMetadata &metadata, BackupProvider &provider, QString *manifestPath, QString *error) const
{
    if (error != nullptr) {
        error->clear();
    }
    const QString normalizedRemoteRoot = QDir::cleanPath(remoteRoot);
    if (normalizedRemoteRoot.isEmpty() || normalizedRemoteRoot == QStringLiteral(".")
        || hasParentPathSegment(normalizedRemoteRoot)) {
        if (error != nullptr) {
            *error = QStringLiteral("The remote backup folder is invalid.");
        }

        return false;
    }

    const BackupPreview selection = preview(sourceDirectories, exclusions);
    if (selection.includedFiles.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected folder contains no regular files.");
        }

        return false;
    }

    QJsonArray entries;
    QStringList failures;
    QStringList expectedItems;
    QStringList failedItems;
    QStringList sourcePrefixes;
    QSet<QString> remotePaths;
    remotePaths.insert(QStringLiteral("manifest.json"));
    QString providerError;
    if (!provider.ensureDirectory(normalizedRemoteRoot, &providerError)) {
        if (error != nullptr) {
            *error = providerError.isEmpty() ? QStringLiteral("Unable to create the remote backup folder.") : providerError;
        }
        return false;
    }
    for (const QString &sourceDirectory : sourceDirectories) {
        sourcePrefixes.append(remoteSegment(QFileInfo(sourceDirectory).fileName()));
    }
    QHash<QString, int> prefixCounts;
    for (QString &prefix : sourcePrefixes) {
        const int count = ++prefixCounts[prefix];
        if (count > 1) {
            prefix += QStringLiteral("-") + QString::number(count);
        }
    }

    for (const QString &sourcePath : selection.includedFiles) {
        int sourceIndex = 0;
        for (int index = 0; index < sourceDirectories.size(); ++index) {
            if (isWithinPath(sourcePath, cleanAbsolutePath(sourceDirectories.at(index)))) {
                sourceIndex = index;
                break;
            }
        }

        QString relativePath = QDir(cleanAbsolutePath(sourceDirectories.at(sourceIndex))).relativeFilePath(sourcePath);
        if (relativePath == QStringLiteral(".") || relativePath.startsWith(QStringLiteral("../"))) {
            relativePath = QFileInfo(sourcePath).fileName();
        }
        const QString mappedPath = sourceDirectories.size() > 1
            ? QDir(sourcePrefixes.at(sourceIndex)).filePath(relativePath)
            : relativePath;
        QString remoteMappedPath = mappedPath;
        int suffix = 1;
        while (remotePaths.contains(remoteMappedPath)) {
            remoteMappedPath = QStringLiteral("%1.%2").arg(mappedPath).arg(suffix++);
        }
        remotePaths.insert(remoteMappedPath);
        const QString remotePath = QDir(normalizedRemoteRoot).filePath(remoteMappedPath);
        expectedItems.append(mappedPath);
        providerError.clear();

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            failures.append(QStringLiteral("The source file could not be read."));
            failedItems.append(mappedPath);
            continue;
        }
        const qint64 sourceSize = sourceFile.size();
        QByteArray sourceChecksum;
        if (!sha256(sourceFile, &sourceChecksum)) {
            failures.append(QStringLiteral("The source file could not be read."));
            failedItems.append(mappedPath);
            continue;
        }
        RemoteFile remoteFile;
        const bool alreadyVerified = provider.inspect(remotePath, &remoteFile, &providerError)
            && remoteFile.size == sourceSize
            && (remoteFile.checksum.isEmpty() || remoteFile.checksum == sourceChecksum);

        if (!alreadyVerified) {
            if (!provider.ensureDirectory(QFileInfo(remotePath).path(), &providerError)) {
                failures.append(providerError.isEmpty()
                    ? QStringLiteral("The remote folder could not be created.")
                    : providerError);
                failedItems.append(mappedPath);
                continue;
            }
            if (!provider.upload(sourcePath, remotePath, &providerError)) {
                failures.append(providerError.isEmpty()
                    ? QStringLiteral("The file could not be uploaded.")
                    : providerError);
                failedItems.append(mappedPath);
                continue;
            }

            if (!provider.inspect(remotePath, &remoteFile, &providerError)
                || remoteFile.size != sourceSize
                || (!remoteFile.checksum.isEmpty() && remoteFile.checksum != sourceChecksum)) {
                failures.append(providerError.isEmpty()
                    ? QStringLiteral("Remote verification failed.")
                    : providerError);
                failedItems.append(mappedPath);
                continue;
            }
        }

        entries.append(QJsonObject {
            {QStringLiteral("source"), sourcePath},
            {QStringLiteral("remote"), remotePath},
            {QStringLiteral("restore"), mappedPath},
            {QStringLiteral("size"), sourceSize},
            {QStringLiteral("sha256"), QString::fromLatin1(sourceChecksum.toHex())},
        });
    }

    const QString manifestDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(
        QStringLiteral("custos-manifest-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(manifestDirectory)
        || !QFile::setPermissions(manifestDirectory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to create the backup manifest folder.");
        }
        return false;
    }
    const QString path = QDir(manifestDirectory).filePath(QStringLiteral("manifest.json"));
    QSaveFile manifest(path);
    const bool incomplete = !failures.isEmpty() || !selection.missingPaths.isEmpty() || !selection.skippedPaths.isEmpty();
    failedItems.append(selection.missingPaths);
    failedItems.append(selection.skippedPaths);
    expectedItems.removeDuplicates();
    failedItems.removeDuplicates();
    QJsonObject manifestObject {
        {QStringLiteral("version"), metadata.copyId.isEmpty() ? 1 : 2},
        {QStringLiteral("entries"), entries},
    };
    if (!metadata.copyId.isEmpty()) {
        manifestObject.insert(QStringLiteral("application"), QStringLiteral("custos"));
        manifestObject.insert(QStringLiteral("computer"), metadata.computerName);
        manifestObject.insert(QStringLiteral("set_id"), metadata.setId);
        manifestObject.insert(QStringLiteral("set_name"), metadata.setName);
        manifestObject.insert(QStringLiteral("copy_id"), metadata.copyId);
        manifestObject.insert(QStringLiteral("created_at"), metadata.createdAt.toString(Qt::ISODateWithMs));
        manifestObject.insert(QStringLiteral("status"), incomplete ? QStringLiteral("incomplete") : QStringLiteral("complete"));
        QJsonArray expected;
        for (const QString &item : expectedItems) {
            expected.append(item);
        }
        QJsonArray failed;
        for (const QString &item : failedItems) {
            failed.append(item);
        }
        manifestObject.insert(QStringLiteral("expected"), expected);
        manifestObject.insert(QStringLiteral("failed"), failed);
    }
    const QByteArray manifestContents = QJsonDocument(manifestObject).toJson();
    if (!manifest.open(QIODevice::WriteOnly)
        || manifest.write(manifestContents) != manifestContents.size()
        || !manifest.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the backup manifest.");
        }

        return false;
    }

    const QString remoteManifestPath = QDir(normalizedRemoteRoot).filePath(QStringLiteral("manifest.json"));
    if (!provider.upload(path, remoteManifestPath, &providerError)) {
        if (error != nullptr) {
            *error = providerError.isEmpty() ? QStringLiteral("Unable to upload the backup manifest.") : providerError;
        }
        return false;
    }

    if (manifestPath != nullptr) {
        *manifestPath = path;
    }
    RemoteFile remoteManifest;
    if (!provider.inspect(remoteManifestPath, &remoteManifest, &providerError)
        || remoteManifest.size != QFileInfo(path).size()) {
        if (error != nullptr) {
            *error = providerError.isEmpty() ? QStringLiteral("Remote manifest verification failed.") : providerError;
        }
        return false;
    }

    if (incomplete) {
        if (error != nullptr) {
            *error = QStringLiteral("Backup incomplete: %1 item(s) could not be verified.")
                .arg(failures.size() + selection.missingPaths.size() + selection.skippedPaths.size());
        }

        return false;
    }

    return true;
}

bool BackupEngine::restoreFile(const BackupEntry &entry, const QString &destinationDirectory, BackupProvider &provider, QString *error) const
{
    if (error != nullptr) {
        error->clear();
    }
    const QString relativePath = entry.restorePath.isEmpty()
        ? (entry.sourcePath.startsWith('/') ? QFileInfo(entry.sourcePath).fileName() : entry.sourcePath)
        : entry.restorePath;
    const QStringList relativeParts = relativePath.split('/', Qt::KeepEmptyParts);
    if (relativePath.isEmpty() || relativePath.startsWith('/')
        || std::any_of(relativeParts.cbegin(), relativeParts.cend(), [](const QString &part) {
            return part == QStringLiteral("..");
        })) {
        if (error != nullptr) {
            *error = QStringLiteral("The restore destination is outside the selected folder.");
        }

        return false;
    }
    const QString destination = QDir(destinationDirectory).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(destinationDirectory).absoluteFilePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("The restore destination could not be created.");
        }
        return false;
    }
    const QString canonicalRoot = QFileInfo(destinationDirectory).canonicalFilePath();
    const QString destinationParent = QFileInfo(destination).absolutePath();
    if (canonicalRoot.isEmpty() || QFileInfo(destinationDirectory).isSymLink()
        || QFileInfo(destination).isSymLink()
        || !QDir().mkpath(destinationParent)) {
        if (error != nullptr) {
            *error = QStringLiteral("The restore destination is outside the selected folder.");
        }
        return false;
    }
    const QString canonicalParent = QFileInfo(destinationParent).canonicalFilePath();
    const QString canonicalDestination = QFileInfo(destination).exists()
        ? QFileInfo(destination).canonicalFilePath()
        : QDir(canonicalParent).filePath(QFileInfo(destination).fileName());

    if (canonicalRoot.isEmpty() || canonicalParent.isEmpty()
        || (canonicalParent != canonicalRoot && !canonicalParent.startsWith(canonicalRoot + QDir::separator()))
        || !canonicalDestination.startsWith(canonicalRoot + QDir::separator())) {
        if (error != nullptr) {
            *error = QStringLiteral("The restore destination is outside the selected folder.");
        }

        return false;
    }

    const QString temporaryDestination = QStringLiteral("%1.custos-restore-%2")
        .arg(destination, QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!provider.download(entry.remotePath, temporaryDestination, error)) {
        QFile::remove(temporaryDestination);
        return false;
    }

    QFile restoredFile(temporaryDestination);
    if (!restoredFile.open(QIODevice::ReadOnly)) {
        QFile::remove(temporaryDestination);
        if (error != nullptr) {
            *error = QStringLiteral("The restored file could not be opened for verification.");
        }

        return false;
    }

    QByteArray checksum;
    const bool hashed = sha256(restoredFile, &checksum);
    if (!hashed || restoredFile.size() != entry.size
        || (!entry.checksum.isEmpty() && checksum != entry.checksum)) {
        restoredFile.close();
        QFile::remove(temporaryDestination);
        if (error != nullptr) {
            *error = QStringLiteral("The restored file failed verification.");
        }

        return false;
    }

    restoredFile.close();
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
        QFile::remove(temporaryDestination);
        if (error != nullptr) {
            *error = QStringLiteral("The restore destination could not be replaced.");
        }
        return false;
    }
    if (!QFile::rename(temporaryDestination, destination)) {
        QFile::remove(temporaryDestination);
        if (error != nullptr) {
            *error = QStringLiteral("The restored file could not be placed in the destination folder.");
        }
        return false;
    }

    return true;
}
