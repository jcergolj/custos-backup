#include "backuprestorecontroller.h"

#include "backupmanifest.h"

#include <QDir>
#include <QDateTime>
#include <QtConcurrentRun>

BackupRestoreController::BackupRestoreController(BackupEngine &engine, BackupProvider *provider, QObject *parent)
    : QObject(parent)
    , engine(engine)
    , provider(provider)
{
    connect(&watcher, &QFutureWatcher<BrowseResult>::finished, this, [this] {
        const BrowseResult result = watcher.result();
        if (discovering) {
            if (result.success) {
                remoteCopies = result.copies;
                selectedCopyIndex = -1;
                if (!pendingSelectedCopyPath.isEmpty()) {
                    for (int index = 0; index < remoteCopies.size(); ++index) {
                        if (remoteCopies.at(index).rootPath == pendingSelectedCopyPath) {
                            selectedCopyIndex = index;
                            break;
                        }
                    }
                }
                if (selectedCopyIndex < 0) {
                    manifestEntries.clear();
                }
                setCachedData(false);
            }
        } else if (result.success) {
            remoteCopies[selectedCopyIndex] = result.copy;
            manifestEntries = result.copy.entries;
            setCachedData(false);
            if (!verified) {
                verified = true;
                emit restoreEligibilityChanged();
            }
        }
        loading = false;
        emit copiesChanged();
        emit currentCopyIndexChanged();
        emit entriesChanged();
        emit busyChanged();
        if (!result.success) {
            setCachedData(!remoteCopies.isEmpty() || !manifestEntries.isEmpty());
            if (!discovering && verified) {
                verified = false;
                emit restoreEligibilityChanged();
            }
            emit failed(result.error);
        } else if (discovering) {
            emit statusChanged(QStringLiteral("%1 backup copies found. Select one to load and verify its files.").arg(remoteCopies.size()));
        } else {
            const QString status = QStringLiteral("%1 verified files available; %2 unavailable or failed.")
                .arg(manifestEntries.size()).arg(unavailableEntries().size());
            emit statusChanged(result.error.isEmpty() ? status : status + QStringLiteral(" ") + result.error);
        }
    });
    connect(&restoreWatcher, &QFutureWatcher<RestoreResult>::finished, this, [this] {
        const RestoreResult result = restoreWatcher.result();
        restoring = false;
        emit busyChanged();
        if (!result.success) {
            emit failed(result.error);
            return;
        }

        emit statusChanged(result.restoredCount == 1
            ? QStringLiteral("File restored successfully.")
            : QStringLiteral("%1 files restored successfully.").arg(result.restoredCount));
        emit restoreCompleted();
    });
}

BackupRestoreController::~BackupRestoreController()
{
    watcher.waitForFinished();
    restoreWatcher.waitForFinished();
}

bool BackupRestoreController::busy() const
{
    return loading || restoring;
}

QString BackupRestoreController::loadingMessage() const
{
    return restoring ? QStringLiteral("Restoring files…") : !loading ? QString() : discovering
        ? QStringLiteral("Loading backup copies…") : QStringLiteral("Loading and verifying files…");
}

bool BackupRestoreController::restoreEligible() const
{
    return verified && !busy();
}

bool BackupRestoreController::showingCachedData() const
{
    return cachedData;
}

void BackupRestoreController::setCachedData(bool cached)
{
    if (cachedData == cached) {
        return;
    }
    cachedData = cached;
    emit cachedDataChanged();
}

int BackupRestoreController::currentCopyIndex() const
{
    return filteredCopyIndexes().indexOf(selectedCopyIndex);
}

QStringList BackupRestoreController::entries() const
{
    QStringList paths;
    for (const BackupEntry &entry : manifestEntries) {
        paths.append(entry.sourcePath);
    }

    return paths;
}

QString BackupRestoreController::defaultDestination() const
{
    return QDir::home().filePath(QStringLiteral("OmaCustos restore"));
}

QStringList BackupRestoreController::copies() const
{
    QStringList result;
    for (const int index : filteredCopyIndexes()) {
        const RemoteCopy &copy = remoteCopies.at(index);
        result.append(QStringLiteral("%1 / %2 / %3%4 (%5)%6")
            .arg(copy.computerName, copy.setName,
                copy.copyId, copy.createdAt.isValid()
                    ? QStringLiteral(" / ") + copy.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                    : QString(), copy.status,
                copy.unavailableItems.isEmpty() && copy.failedItems.isEmpty()
                    ? QString()
                    : QStringLiteral(" - some items unavailable")));
    }
    return result;
}

QString BackupRestoreController::copySearch() const
{
    return searchText;
}

void BackupRestoreController::setCopySearch(const QString &search)
{
    if (busy() || searchText == search) {
        return;
    }
    searchText = search;
    selectedCopyIndex = -1;
    manifestEntries.clear();
    setCachedData(false);
    if (verified) {
        verified = false;
        emit restoreEligibilityChanged();
    }
    emit copiesChanged();
    emit currentCopyIndexChanged();
    emit entriesChanged();
}

QVector<int> BackupRestoreController::filteredCopyIndexes() const
{
    QVector<int> result;
    const QString query = searchText.trimmed().toLower();
    for (int index = 0; index < remoteCopies.size(); ++index) {
        const RemoteCopy &copy = remoteCopies.at(index);
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
            .arg(copy.computerName, copy.setName, copy.copyId, copy.status, copy.createdAt.toString());
        // Keep the chosen copy visible when verification changes its name/status.
        // Editing the search clears the selection before recalculating this list.
        if (index == selectedCopyIndex || query.isEmpty() || haystack.toLower().contains(query)) {
            result.append(index);
        }
    }
    return result;
}

QStringList BackupRestoreController::unavailableEntries() const
{
    if (selectedCopyIndex < 0 || selectedCopyIndex >= remoteCopies.size()) {
        return {};
    }
    QStringList unavailable = remoteCopies.at(selectedCopyIndex).unavailableItems;
    unavailable.append(remoteCopies.at(selectedCopyIndex).failedItems);
    unavailable.removeDuplicates();
    return unavailable;
}

void BackupRestoreController::loadManifest(const QString &path)
{
    if (busy()) {
        return;
    }
    QVector<BackupEntry> loadedEntries;
    QString error;
    if (!BackupManifest::load(path, &loadedEntries, &error)) {
        if (verified) {
            verified = false;
            emit restoreEligibilityChanged();
        }
        manifestEntries.clear();
        remoteCopies.clear();
        setCachedData(false);
        selectedCopyIndex = -1;
        emit copiesChanged();
        emit currentCopyIndexChanged();
        emit entriesChanged();
        emit failed(error);

        return;
    }

    manifestEntries = std::move(loadedEntries);
    setCachedData(false);
    if (!verified) {
        verified = true;
        emit restoreEligibilityChanged();
    }
    remoteCopies.clear();
    selectedCopyIndex = -1;
    emit copiesChanged();
    emit currentCopyIndexChanged();
    emit entriesChanged();
    emit statusChanged(QStringLiteral("%1 files available for restore.").arg(manifestEntries.size()));
}

void BackupRestoreController::discover(const QString &backupFolder, const QString &setId)
{
    if (busy()) {
        return;
    }
    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));
        return;
    }

    const bool sameContext = activeBackupFolder == backupFolder && expectedSetId == setId;
    pendingSelectedCopyPath = sameContext && selectedCopyIndex >= 0 && selectedCopyIndex < remoteCopies.size()
        ? remoteCopies.at(selectedCopyIndex).rootPath : QString();
    if (!sameContext) {
        remoteCopies.clear();
        manifestEntries.clear();
        selectedCopyIndex = -1;
    }
    setCachedData(sameContext && (!remoteCopies.isEmpty() || !manifestEntries.isEmpty()));
    if (verified) {
        verified = false;
        emit restoreEligibilityChanged();
    }
    expectedSetId = setId;
    activeBackupFolder = backupFolder;
    searchText.clear();
    discovering = true;
    loading = true;
    emit busyChanged();
    emit copiesChanged();
    emit currentCopyIndexChanged();
    emit entriesChanged();
    watcher.setFuture(QtConcurrent::run([this, backupFolder] {
        BrowseResult result;
        result.success = BackupCatalog::listCopies(*provider, backupFolder, &result.copies, &result.error);
        return result;
    }));
}

void BackupRestoreController::selectCopy(int index)
{
    if (busy()) {
        return;
    }
    const QVector<int> indexes = filteredCopyIndexes();
    const int actualIndex = index >= 0 && index < indexes.size() ? indexes.at(index) : -1;
    const int previousIndex = selectedCopyIndex;
    selectedCopyIndex = actualIndex;
    if (actualIndex != previousIndex) {
        manifestEntries.clear();
    }
    if (verified) {
        verified = false;
        emit restoreEligibilityChanged();
    }
    setCachedData(actualIndex == previousIndex && !manifestEntries.isEmpty());
    if (actualIndex >= 0) {
        // Reverify on each selection: remote files may have changed since the last visit.
        remoteCopies[actualIndex].entries.clear();
        remoteCopies[actualIndex].unavailableItems.clear();
        remoteCopies[actualIndex].failedItems.clear();
        discovering = false;
        loading = true;
        emit busyChanged();
    }
    emit currentCopyIndexChanged();
    emit entriesChanged();
    if (actualIndex < 0) {
        return;
    }
    const QString path = remoteCopies.at(actualIndex).rootPath;
    const QString setId = expectedSetId;
    watcher.setFuture(QtConcurrent::run([this, path, setId] {
        BrowseResult result;
        result.success = BackupCatalog::verifyCopy(*provider, path, setId, &result.copy, &result.error);
        return result;
    }));
}

void BackupRestoreController::restore(int index, const QString &destinationDirectory)
{
    if (busy()) {
        return;
    }
    if (destinationDirectory.trimmed().isEmpty()) {
        emit failed(QStringLiteral("A restore destination folder is required."));

        return;
    }

    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));

        return;
    }

    if (!restoreEligible()) {
        emit failed(QStringLiteral("The selected backup copy is not currently verified."));
        return;
    }

    if (index < 0 || index >= manifestEntries.size()) {
        emit failed(QStringLiteral("The selected restore file is invalid."));

        return;
    }

    const QVector<BackupEntry> entries {manifestEntries.at(index)};
    const QString destination = destinationDirectory;
    BackupEngine *enginePointer = &engine;
    BackupProvider *providerPointer = provider;
    restoring = true;
    emit busyChanged();
    restoreWatcher.setFuture(QtConcurrent::run([enginePointer, providerPointer, entries, destination] {
        RestoreResult result;
        result.success = enginePointer->restoreFile(entries.first(), destination, *providerPointer, &result.error);
        result.restoredCount = result.success ? 1 : 0;
        return result;
    }));
}

void BackupRestoreController::restoreSelected(const QVariantList &indexes, const QString &destinationDirectory)
{
    if (busy()) {
        return;
    }
    if (indexes.isEmpty()) {
        emit failed(QStringLiteral("At least one restore file must be selected."));
        return;
    }

    if (destinationDirectory.trimmed().isEmpty()) {
        emit failed(QStringLiteral("A restore destination folder is required."));
        return;
    }

    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));
        return;
    }

    if (!restoreEligible()) {
        emit failed(QStringLiteral("The selected backup copy is not currently verified."));
        return;
    }

    QVector<BackupEntry> entries;
    entries.reserve(indexes.size());
    for (const QVariant &value : indexes) {
        const int index = value.toInt();
        if (index < 0 || index >= manifestEntries.size()) {
            emit failed(QStringLiteral("The selected restore file is invalid."));
            return;
        }
        entries.append(manifestEntries.at(index));
    }

    const QString destination = destinationDirectory;
    BackupEngine *enginePointer = &engine;
    BackupProvider *providerPointer = provider;
    restoring = true;
    emit busyChanged();
    restoreWatcher.setFuture(QtConcurrent::run([enginePointer, providerPointer, entries, destination] {
        RestoreResult result;
        result.success = true;
        for (const BackupEntry &entry : entries) {
            if (!enginePointer->restoreFile(entry, destination, *providerPointer, &result.error)) {
                result.success = false;
                if (result.error.isEmpty()) {
                    result.error = QStringLiteral("The selected restore file could not be restored.");
                }
                return result;
            }
            ++result.restoredCount;
        }
        return result;
    }));
}

void BackupRestoreController::restoreFolder(const QString &folder, const QString &destinationDirectory)
{
    const QString prefix = QDir::cleanPath(folder).trimmed();
    QVariantList indexes;
    for (int index = 0; index < manifestEntries.size(); ++index) {
        const QString path = manifestEntries.at(index).restorePath;
        if (path == prefix || path.startsWith(prefix + QDir::separator())) {
            indexes.append(index);
        }
    }
    restoreSelected(indexes, destinationDirectory);
}
