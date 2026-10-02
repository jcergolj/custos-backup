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
            remoteCopies = result.success ? result.copies : QVector<RemoteCopy> {};
        } else if (result.success) {
            remoteCopies[selectedCopyIndex] = result.copy;
            manifestEntries = result.copy.entries;
        }
        loading = false;
        emit copiesChanged();
        emit currentCopyIndexChanged();
        emit entriesChanged();
        emit busyChanged();
        if (!result.success) {
            emit failed(result.error);
        } else if (discovering) {
            emit statusChanged(QStringLiteral("%1 backup copies found. Select one to load and verify its files.").arg(remoteCopies.size()));
        } else {
            const QString status = QStringLiteral("%1 verified files available; %2 unavailable or failed.")
                .arg(manifestEntries.size()).arg(unavailableEntries().size());
            emit statusChanged(result.error.isEmpty() ? status : status + QStringLiteral(" ") + result.error);
        }
    });
}

BackupRestoreController::~BackupRestoreController()
{
    watcher.waitForFinished();
}

bool BackupRestoreController::busy() const
{
    return loading;
}

QString BackupRestoreController::loadingMessage() const
{
    return !loading ? QString() : discovering
        ? QStringLiteral("Loading backup copies…") : QStringLiteral("Loading and verifying files…");
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
    if (loading || searchText == search) {
        return;
    }
    searchText = search;
    selectedCopyIndex = -1;
    manifestEntries.clear();
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
        if (query.isEmpty() || haystack.toLower().contains(query)) {
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
    if (loading) {
        return;
    }
    QVector<BackupEntry> loadedEntries;
    QString error;
    if (!BackupManifest::load(path, &loadedEntries, &error)) {
        manifestEntries.clear();
        remoteCopies.clear();
        selectedCopyIndex = -1;
        emit copiesChanged();
        emit currentCopyIndexChanged();
        emit entriesChanged();
        emit failed(error);

        return;
    }

    manifestEntries = std::move(loadedEntries);
    remoteCopies.clear();
    selectedCopyIndex = -1;
    emit copiesChanged();
    emit currentCopyIndexChanged();
    emit entriesChanged();
    emit statusChanged(QStringLiteral("%1 files available for restore.").arg(manifestEntries.size()));
}

void BackupRestoreController::discover(const QString &backupFolder, const QString &setId)
{
    if (loading) {
        return;
    }
    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));
        return;
    }

    remoteCopies.clear();
    manifestEntries.clear();
    selectedCopyIndex = -1;
    expectedSetId = setId;
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
    if (loading) {
        return;
    }
    const QVector<int> indexes = filteredCopyIndexes();
    const int actualIndex = index >= 0 && index < indexes.size() ? indexes.at(index) : -1;
    selectedCopyIndex = actualIndex;
    manifestEntries.clear();
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
    if (loading) {
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

    if (index < 0 || index >= manifestEntries.size()) {
        emit failed(QStringLiteral("The selected restore file is invalid."));

        return;
    }

    QString error;
    if (!engine.restoreFile(manifestEntries.at(index), destinationDirectory, *provider, &error)) {
        emit failed(error);

        return;
    }

    emit statusChanged(QStringLiteral("File restored successfully."));
    emit restoreCompleted();
}

void BackupRestoreController::restoreSelected(const QVariantList &indexes, const QString &destinationDirectory)
{
    if (loading) {
        return;
    }
    if (indexes.isEmpty()) {
        emit failed(QStringLiteral("At least one restore file must be selected."));
        return;
    }
    for (const QVariant &value : indexes) {
        const int index = value.toInt();
        if (index < 0 || index >= manifestEntries.size()) {
            emit failed(QStringLiteral("The selected restore file is invalid."));
            return;
        }
        QString error;
        if (provider == nullptr || !engine.restoreFile(manifestEntries.at(index), destinationDirectory, *provider, &error)) {
            emit failed(error.isEmpty() ? QStringLiteral("The selected restore file could not be restored.") : error);
            return;
        }
    }
    emit statusChanged(QStringLiteral("%1 files restored successfully.").arg(indexes.size()));
    emit restoreCompleted();
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
