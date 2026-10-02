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
#include <QScopeGuard>
#include <QTemporaryDir>
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

bool isExcluded(const QFileInfo &file, const QStringList &exclusions)
{
    const QString path = file.absoluteFilePath();
    QStringList folders = file.absolutePath().split('/', Qt::SkipEmptyParts);
    if (file.isDir() || file.isSymLink()) {
        folders.append(file.fileName());
    }
    return std::any_of(exclusions.cbegin(), exclusions.cend(), [&path, &folders](const QString &exclusion) {
        if (exclusion.trimmed().isEmpty()) {
            return false;
        }
        const QString rule = QDir::cleanPath(exclusion.trimmed());
        if (!rule.contains('/') && rule != QStringLiteral(".") && rule != QStringLiteral("..")) {
            return folders.contains(rule);
        }
        return isWithinPath(path, cleanAbsolutePath(rule));
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

bool copyAndHash(QFile &source, QFile &snapshot, QByteArray *checksum)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(1024 * 1024);
        if ((chunk.isEmpty() && source.error() != QFileDevice::NoError)
            || snapshot.write(chunk) != chunk.size()) {
            return false;
        }
        hash.addData(chunk);
    }
    if (!snapshot.flush()) {
        return false;
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
        if (isExcluded(source, exclusions)) {
            excluded.append(source.absoluteFilePath());
            continue;
        }
        if (source.isSymLink() || !source.isReadable()) {
            skipped.append(source.absoluteFilePath());
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

        QDirIterator iterator(source.absoluteFilePath(), QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
            QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo file = iterator.fileInfo();
            const QString path = file.absoluteFilePath();

            if (isExcluded(file, exclusions)) {
                if (file.isFile() || file.isSymLink()) {
                    excluded.append(path);
                }
            } else if (file.isSymLink()) {
                skipped.append(path);
            } else if (file.isDir() && !file.isReadable()) {
                skipped.append(path);
            } else if (file.isFile() && !file.isReadable()) {
                skipped.append(path);
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

bool BackupEngine::backup(const QStringList &sourceDirectories, const QString &remoteRoot, const QStringList &exclusions, const BackupCopyMetadata &metadata, BackupProvider &provider, QString *manifestPath, QString *error, const std::function<void(const BackupProgress &)> &reportProgress, BackupResult *result) const
{
    BackupResult outcome;
    outcome.reported = true;
    const auto finishResult = qScopeGuard([&] {
        if (result != nullptr) {
            *result = outcome;
        }
    });
    if (manifestPath != nullptr) {
        manifestPath->clear();
    }
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
    for (const QString &path : selection.missingPaths) {
        outcome.issues.append({path, QStringLiteral("selection"), QStringLiteral("The source path does not exist.")});
    }
    for (const QString &path : selection.skippedPaths) {
        const QFileInfo file(path);
        outcome.issues.append({path, QStringLiteral("selection"), file.isSymLink()
            ? QStringLiteral("Symbolic links are not backed up.")
            : !file.isReadable() ? QStringLiteral("The source path is unreadable.")
                                 : QStringLiteral("The source path is not a regular file or folder.")});
    }
    if (selection.includedFiles.isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected folder contains no regular files.");
        }

        return false;
    }

    BackupProgress progress;
    progress.failedItems = outcome.issues.size();
    progress.phase = QStringLiteral("preparing");
    progress.totalFiles = selection.includedFiles.size();
    QHash<QString, qint64> plannedSizes;
    for (const QString &path : selection.includedFiles) {
        const qint64 size = qMax(qint64(0), QFileInfo(path).size());
        plannedSizes.insert(path, size);
        progress.totalBytes += size;
    }
    if (reportProgress) {
        reportProgress(progress);
    }

    QJsonArray entries;
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
        // Count attempted files, including failures, without presenting those
        // failures as verified uploads. This measures remaining work only.
        const auto finishedFile = qScopeGuard([&] {
            ++progress.processedFiles;
            progress.processedBytes += plannedSizes.value(sourcePath);
            progress.currentFile.clear();
            progress.currentFileBytes = 0;
            if (reportProgress) {
                reportProgress(progress);
            }
        });
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

        const auto failFile = [&](const QString &phase, const QString &reason) {
            outcome.issues.append({sourcePath, phase, reason});
            failedItems.append(mappedPath);
            ++progress.failedItems;
        };
        const auto reportPhase = [&](const QString &phase) {
            progress.currentFile = sourcePath;
            progress.currentFileBytes = plannedSizes.value(sourcePath);
            progress.phase = phase;
            if (reportProgress) {
                reportProgress(progress);
            }
        };
        reportPhase(QStringLiteral("reading"));

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            failFile(QStringLiteral("reading"), QStringLiteral("The source file could not be read: %1").arg(sourceFile.errorString()));
            continue;
        }
        // Hash the bytes copied into a private snapshot and upload that same
        // read-only file, never a source pathname that can change afterward.
        QTemporaryDir staging(QDir::temp().filePath(QStringLiteral("omacustos-payload-XXXXXX")));
        if (!staging.isValid()) {
            failFile(QStringLiteral("reading"), QStringLiteral("The backup staging folder could not be created."));
            continue;
        }
        QFile snapshot(staging.filePath(QFileInfo(remotePath).fileName()));
        if (!snapshot.open(QIODevice::WriteOnly)) {
            failFile(QStringLiteral("reading"), QStringLiteral("The backup payload could not be staged: %1").arg(snapshot.errorString()));
            continue;
        }
        QByteArray sourceChecksum;
        if (!copyAndHash(sourceFile, snapshot, &sourceChecksum)) {
            failFile(QStringLiteral("reading"), sourceFile.error() != QFileDevice::NoError
                ? QStringLiteral("The source file could not be read: %1").arg(sourceFile.errorString())
                : QStringLiteral("The backup payload could not be staged: %1").arg(snapshot.errorString()));
            continue;
        }
        const qint64 sourceSize = snapshot.size();
        snapshot.close();
        sourceFile.close();
        if (!snapshot.setPermissions(QFileDevice::ReadOwner)) {
            failFile(QStringLiteral("reading"), QStringLiteral("The staged backup payload could not be made read-only."));
            continue;
        }
        RemoteFile remoteFile;
        reportPhase(QStringLiteral("checking"));
        // Size-only metadata cannot establish that an existing payload matches
        // this snapshot, so providers without checksums must upload it again.
        const bool alreadyVerified = provider.inspect(remotePath, &remoteFile, &providerError)
            && remoteFile.size == sourceSize
            && !remoteFile.checksum.isEmpty() && remoteFile.checksum == sourceChecksum;

        if (!alreadyVerified) {
            if (!provider.ensureDirectory(QFileInfo(remotePath).path(), &providerError)) {
                failFile(QStringLiteral("preparing"), providerError.isEmpty()
                    ? QStringLiteral("The remote folder could not be created.")
                    : providerError);
                continue;
            }
            reportPhase(QStringLiteral("uploading"));
            if (!provider.upload(snapshot.fileName(), remotePath, &providerError)) {
                failFile(QStringLiteral("uploading"), providerError.isEmpty()
                    ? QStringLiteral("The file could not be uploaded.")
                    : providerError);
                continue;
            }
            reportPhase(QStringLiteral("verifying"));
            if (!provider.inspect(remotePath, &remoteFile, &providerError)
                || remoteFile.size != sourceSize
                || (!remoteFile.checksum.isEmpty() && remoteFile.checksum != sourceChecksum)) {
                failFile(QStringLiteral("verifying"), providerError.isEmpty()
                    ? QStringLiteral("Remote verification failed.")
                    : providerError);
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
        ++outcome.verifiedFiles;
        outcome.verifiedBytes += sourceSize;
        progress.verifiedFiles = outcome.verifiedFiles;
        progress.verifiedBytes = outcome.verifiedBytes;
    }

    progress.finalizing = true;
    progress.phase = QStringLiteral("finalizing");
    if (reportProgress) {
        reportProgress(progress);
    }
    const QString manifestDirectory = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(
        QStringLiteral("omacustos-manifest-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(manifestDirectory)
        || !QFile::setPermissions(manifestDirectory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to create the backup manifest folder.");
        }
        return false;
    }
    const QString path = QDir(manifestDirectory).filePath(QStringLiteral("manifest.json"));
    QSaveFile manifest(path);
    const bool incomplete = !outcome.issues.isEmpty();
    failedItems.append(selection.missingPaths);
    failedItems.append(selection.skippedPaths);
    expectedItems.removeDuplicates();
    failedItems.removeDuplicates();
    QJsonObject manifestObject {
        {QStringLiteral("version"), metadata.copyId.isEmpty() ? 1 : 2},
        {QStringLiteral("entries"), entries},
    };
    QJsonArray issues;
    for (const BackupIssue &issue : outcome.issues) {
        issues.append(QJsonObject {{QStringLiteral("path"), issue.path},
            {QStringLiteral("phase"), issue.phase}, {QStringLiteral("reason"), issue.reason}});
    }
    manifestObject.insert(QStringLiteral("issues"), issues);
    if (!metadata.copyId.isEmpty()) {
        manifestObject.insert(QStringLiteral("application"), QStringLiteral("omacustos"));
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

    RemoteFile remoteManifest;
    if (!provider.inspect(remoteManifestPath, &remoteManifest, &providerError)
        || remoteManifest.size != QFileInfo(path).size()) {
        if (error != nullptr) {
            *error = providerError.isEmpty() ? QStringLiteral("Remote manifest verification failed.") : providerError;
        }
        return false;
    }

    outcome.manifestVerified = true;
    if (manifestPath != nullptr) {
        *manifestPath = path;
    }

    if (incomplete) {
        if (error != nullptr) {
            *error = (outcome.verifiedFiles > 0
                ? QStringLiteral("Backup incomplete: %1 files backed up · %2 items failed.")
                : QStringLiteral("Backup failed: %1 files backed up · %2 items failed."))
                .arg(outcome.verifiedFiles).arg(outcome.issues.size());
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

    const QString temporaryDestination = QStringLiteral("%1.omacustos-restore-%2")
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
