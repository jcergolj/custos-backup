#include "backuprestorecontroller.h"

#include "backupmanifest.h"

#include <QDir>
#include <QDateTime>

BackupRestoreController::BackupRestoreController(BackupEngine &engine, BackupProvider *provider, QObject *parent)
    : QObject(parent)
    , engine(engine)
    , provider(provider)
{
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
    return QDir::home().filePath(QStringLiteral("Custos restore"));
}

QStringList BackupRestoreController::copies() const
{
    QStringList result;
    for (const int index : filteredCopyIndexes()) {
        const RemoteCopy &copy = remoteCopies.at(index);
        result.append(QStringLiteral("%1 / %2 / %3 / %4 (%5)%6")
            .arg(copy.computerName, copy.setName,
                copy.copyId, copy.createdAt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")), copy.status,
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
    if (searchText == search) {
        return;
    }
    searchText = search;
    selectedCopyIndex = -1;
    manifestEntries.clear();
    emit copiesChanged();
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
    QVector<BackupEntry> loadedEntries;
    QString error;
    if (!BackupManifest::load(path, &loadedEntries, &error)) {
        manifestEntries.clear();
        remoteCopies.clear();
        selectedCopyIndex = -1;
        emit copiesChanged();
        emit entriesChanged();
        emit failed(error);

        return;
    }

    manifestEntries = std::move(loadedEntries);
    remoteCopies.clear();
    selectedCopyIndex = -1;
    emit copiesChanged();
    emit entriesChanged();
    emit statusChanged(QStringLiteral("%1 files available for restore.").arg(manifestEntries.size()));
}

void BackupRestoreController::discover(const QString &remoteRoot)
{
    if (provider == nullptr) {
        emit failed(QStringLiteral("No backup provider is configured."));
        return;
    }

    QVector<RemoteCopy> discovered;
    QString error;
    if (!BackupCatalog::discover(*provider, remoteRoot, &discovered, &error)) {
        remoteCopies.clear();
        manifestEntries.clear();
        selectedCopyIndex = -1;
        emit copiesChanged();
        emit entriesChanged();
        emit failed(error);
        return;
    }

    remoteCopies = std::move(discovered);
    manifestEntries.clear();
    selectedCopyIndex = -1;
    emit copiesChanged();
    emit entriesChanged();
    const QString status = QStringLiteral("%1 remote copies discovered. Select one to browse verified files.").arg(remoteCopies.size());
    emit statusChanged(error.isEmpty() ? status : status + QStringLiteral(" ") + error);
}

void BackupRestoreController::selectCopy(int index)
{
    const QVector<int> indexes = filteredCopyIndexes();
    const int actualIndex = index >= 0 && index < indexes.size() ? indexes.at(index) : -1;
    if (actualIndex < 0) {
        selectedCopyIndex = -1;
        manifestEntries.clear();
    } else {
        selectedCopyIndex = actualIndex;
        manifestEntries = remoteCopies.at(actualIndex).entries;
    }
    emit entriesChanged();
    emit statusChanged(selectedCopyIndex < 0
        ? QStringLiteral("No remote copy selected.")
        : QStringLiteral("%1 verified files available; %2 unavailable or failed.")
            .arg(manifestEntries.size()).arg(unavailableEntries().size()));
}

void BackupRestoreController::restore(int index, const QString &destinationDirectory)
{
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
}

void BackupRestoreController::restoreSelected(const QVariantList &indexes, const QString &destinationDirectory)
{
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
