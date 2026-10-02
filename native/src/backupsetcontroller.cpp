#include "backupsetcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSysInfo>
#include <QUuid>

#include <algorithm>

namespace {

bool isRunActive(const QString &status)
{
    return status == QStringLiteral("running");
}

BackupSet newSet(int number)
{
    return {
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        QStringLiteral("Backup %1").arg(number),
        QStringLiteral("/my-files/backups"),
        {},
        {},
    };
}

}

BackupSetController::BackupSetController(BackupEngine &engine, QString configPath, QObject *parent)
    : QObject(parent)
    , engine(engine)
    , store(configPath)
    , runStore(QDir(QFileInfo(configPath).absolutePath()).filePath(QStringLiteral("custos-backup-runs.json")))
    , cleanupStore(QDir(QFileInfo(configPath).absolutePath()).filePath(QStringLiteral("custos-backup-cleanup.json")))
{
    connect(&stateTimer, &QTimer::timeout, this, &BackupSetController::refreshRunState);
    stateTimer.start(5000);
    runStore.load();
    cleanupStore.load();
    QString error;
    if (store.load(&config, &error)) {
        selectedIndex = config.sets.isEmpty() ? -1 : 0;
    } else if (QFileInfo::exists(store.filePath())) {
        emit failed(error);
    } else {
        config.protonBinary = qEnvironmentVariable("CUSTOS_PROTON_BIN", QStringLiteral("proton-drive"));
        selectedIndex = -1;
    }
}

QStringList BackupSetController::setNames() const
{
    QStringList names;
    for (const BackupSet &set : config.sets) {
        names.append(set.name);
    }

    return names;
}

QStringList BackupSetController::setIds() const
{
    QStringList ids;
    for (const BackupSet &set : config.sets) {
        ids.append(set.id);
    }
    return ids;
}

QString BackupSetController::currentId() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QString() : set->id;
}

int BackupSetController::currentIndex() const
{
    return selectedIndex;
}

void BackupSetController::setCurrentIndex(int index)
{
    if (config.sets.isEmpty()) {
        selectedIndex = -1;
    } else {
        selectedIndex = qBound(0, index, config.sets.size() - 1);
    }

    emit currentIndexChanged();
    emit currentSetChanged();
    clearPreview();
}

QString BackupSetController::currentName() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QString() : set->name;
}

void BackupSetController::setCurrentName(const QString &name)
{
    if (BackupSet *set = currentSet()) {
        set->name = name;
        emit setsChanged();
        emit currentSetChanged();
    }
}

QString BackupSetController::currentRemoteRoot() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QString() : set->remoteRoot;
}

void BackupSetController::setCurrentRemoteRoot(const QString &remoteRoot)
{
    if (BackupSet *set = currentSet()) {
        set->remoteRoot = remoteRoot;
        emit currentSetChanged();
    }
}

QStringList BackupSetController::currentSources() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QStringList() : set->sourceDirectories;
}

void BackupSetController::setCurrentSources(const QStringList &sources)
{
    if (BackupSet *set = currentSet()) {
        set->sourceDirectories = sources;
        emit currentSetChanged();
    }
}

QStringList BackupSetController::currentExclusions() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QStringList() : set->exclusions;
}

void BackupSetController::setCurrentExclusions(const QStringList &exclusions)
{
    if (BackupSet *set = currentSet()) {
        set->exclusions = exclusions;
        emit currentSetChanged();
    }
}

QString BackupSetController::currentScheduleFrequency() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QStringLiteral("disabled") : set->schedule.frequency;
}

void BackupSetController::setCurrentScheduleFrequency(const QString &frequency)
{
    if (BackupSet *set = currentSet()) {
        set->schedule.frequency = frequency;
        emit currentSetChanged();
    }
}

int BackupSetController::currentScheduleHour() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? 2 : set->schedule.hour;
}

void BackupSetController::setCurrentScheduleHour(int hour)
{
    if (BackupSet *set = currentSet()) {
        set->schedule.hour = qBound(0, hour, 23);
        emit currentSetChanged();
    }
}

int BackupSetController::currentScheduleMinute() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? 0 : set->schedule.minute;
}

void BackupSetController::setCurrentScheduleMinute(int minute)
{
    if (BackupSet *set = currentSet()) {
        set->schedule.minute = qBound(0, minute, 59);
        emit currentSetChanged();
    }
}

int BackupSetController::currentScheduleWeekday() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? 1 : set->schedule.weekday;
}

void BackupSetController::setCurrentScheduleWeekday(int weekday)
{
    if (BackupSet *set = currentSet()) {
        set->schedule.weekday = qBound(1, weekday, 7);
        emit currentSetChanged();
    }
}

int BackupSetController::currentScheduleDayOfMonth() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? 1 : set->schedule.dayOfMonth;
}

void BackupSetController::setCurrentScheduleDayOfMonth(int day)
{
    if (BackupSet *set = currentSet()) {
        set->schedule.dayOfMonth = qBound(1, day, 31);
        emit currentSetChanged();
    }
}

int BackupSetController::currentRetention() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? 3 : set->retention;
}

void BackupSetController::setCurrentRetention(int retention)
{
    if (BackupSet *set = currentSet()) {
        set->retention = qMax(1, retention);
        emit currentSetChanged();
    }
}

bool BackupSetController::currentOnlyOnAcPower() const
{
    const BackupSet *set = currentSet();
    return set != nullptr && set->onlyOnAcPower;
}

void BackupSetController::setCurrentOnlyOnAcPower(bool enabled)
{
    if (BackupSet *set = currentSet()) {
        set->onlyOnAcPower = enabled;
        emit currentSetChanged();
    }
}

QString BackupSetController::currentNextRun() const
{
    const BackupSet *set = currentSet();
    if (set == nullptr || !set->schedule.enabled()) {
        return QStringLiteral("Not scheduled");
    }

    const QDateTime next = BackupScheduleCalculator::nextRun(set->schedule, QDateTime::currentDateTime());
    return next.isValid() ? next.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("Not scheduled");
}

QString BackupSetController::currentRunStatus() const
{
    const BackupSet *set = currentSet();
    const BackupRunRecord *record = set == nullptr ? nullptr : runStore.find(set->id);
    return record == nullptr ? QStringLiteral("idle") : record->status;
}

QString BackupSetController::currentRunError() const
{
    const BackupSet *set = currentSet();
    const BackupRunRecord *record = set == nullptr ? nullptr : runStore.find(set->id);
    return record == nullptr ? QString() : record->lastError;
}

QStringList BackupSetController::runningSetIds() const
{
    QStringList ids;
    for (const BackupSet &set : config.sets) {
        const BackupRunRecord *record = runStore.find(set.id);
        if (record != nullptr && isRunActive(record->status)) {
            ids.append(set.id);
        }
    }
    return ids;
}

QStringList BackupSetController::recentBackups() const
{
    QStringList summaries;
    for (const int index : recentBackupIndexes()) {
        const BackupSet &set = config.sets.at(index);
        const BackupRunRecord *record = runStore.find(set.id);
        if (record == nullptr) {
            summaries.append(QStringLiteral("%1\n%2").arg(set.name, QStringLiteral("No backup run yet")));
            continue;
        }

        QString detail = record->status;
        if (!record->lastError.isEmpty()) {
            detail += QStringLiteral(" | %1").arg(record->lastError);
        }
        summaries.append(QStringLiteral("%1\n%2").arg(set.name, detail));
    }

    return summaries;
}

QStringList BackupSetController::recentBackupSetIds() const
{
    QStringList ids;
    for (const int index : recentBackupIndexes()) {
        ids.append(config.sets.at(index).id);
    }
    return ids;
}

QStringList BackupSetController::recentBackupTimestamps() const
{
    QStringList timestamps;
    for (const int index : recentBackupIndexes()) {
        const BackupRunRecord *record = runStore.find(config.sets.at(index).id);
        if (record == nullptr) {
            timestamps.append(QString());
            continue;
        }

        QDateTime latest = record->lastSuccess;
        if (record->lastFailure > latest) {
            latest = record->lastFailure;
        }
        if (record->lastScheduled > latest) {
            latest = record->lastScheduled;
        }
        timestamps.append(latest.isValid()
            ? latest.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"))
            : QString());
    }
    return timestamps;
}

QString BackupSetController::recentBackupFolderPath(const QString &setId) const
{
    for (const BackupSet &set : config.sets) {
        if (set.id == setId) {
            return set.remoteFolder(QSysInfo::machineHostName());
        }
    }
    return {};
}

QStringList BackupSetController::previewIncluded() const
{
    return previewResult.includedFiles;
}

QStringList BackupSetController::previewExcluded() const
{
    return previewResult.excludedFiles;
}

QStringList BackupSetController::previewSkipped() const
{
    return previewResult.skippedPaths;
}

QStringList BackupSetController::previewMissing() const
{
    return previewResult.missingPaths;
}

QStringList BackupSetController::cleanupTargets() const
{
    const BackupSet *set = currentSet();
    return set == nullptr ? QStringList() : cleanupStore.state(set->id).targets;
}

bool BackupSetController::cleanupConfirmationRequired() const
{
    const BackupSet *set = currentSet();
    if (set == nullptr) {
        return false;
    }
    const CleanupState state = cleanupStore.state(set->id);
    return state.decision != QStringLiteral("confirmed") && !state.targets.isEmpty();
}

void BackupSetController::addSet()
{
    config.sets.append(newSet(config.sets.size() + 1));
    selectedIndex = config.sets.size() - 1;
    emit setsChanged();
    emit currentIndexChanged();
    emit currentSetChanged();
    emit dashboardChanged();
    clearPreview();
}

void BackupSetController::removeCurrentSet()
{
    removeSet(selectedIndex);
}

void BackupSetController::removeSet(int index)
{
    if (index < 0 || index >= config.sets.size()) {
        return;
    }

    BackupConfig updated = config;
    updated.sets.removeAt(index);
    QString error;
    if (!store.save(updated, &error)) {
        emit failed(error);
        return;
    }

    config = updated;
    if (selectedIndex > index) {
        --selectedIndex;
    } else if (selectedIndex == index) {
        selectedIndex = qMin(selectedIndex, config.sets.size() - 1);
    }
    emit setsChanged();
    emit currentIndexChanged();
    emit currentSetChanged();
    emit dashboardChanged();
    clearPreview();
    emit statusChanged(QStringLiteral("Backup set removed."));
}

void BackupSetController::preview()
{
    const BackupSet *set = currentSet();
    if (set == nullptr) {
        clearPreview();
        emit failed(QStringLiteral("A backup must be selected."));

        return;
    }

    previewResult = engine.preview(set->sourceDirectories, set->exclusions);
    emit previewChanged();
    emit statusChanged(QString());
}

bool BackupSetController::save()
{
    const BackupSet *set = currentSet();
    if (set == nullptr || set->name.trimmed().isEmpty() || set->remoteRoot.trimmed().isEmpty()
        || set->sourceDirectories.isEmpty()) {
        emit failed(QStringLiteral("Enter a backup name, choose at least one source, and check the destination in Advanced settings."));

        return false;
    }

    QString error;
    if (!store.save(config, &error)) {
        emit failed(error);

        return false;
    }

    emit statusChanged(QStringLiteral("Backup saved."));
    return true;
}

bool BackupSetController::exportSets(const QString &filePath)
{
    const QFileInfo destination(filePath);
    for (const QString &statePath : {store.filePath(), runStore.filePath(), cleanupStore.filePath()}) {
        const QFileInfo state(statePath);
        if (QDir::cleanPath(destination.absoluteFilePath()) == QDir::cleanPath(state.absoluteFilePath())
            || (!destination.canonicalFilePath().isEmpty() && destination.canonicalFilePath() == state.canonicalFilePath())) {
            emit failed(QStringLiteral("Choose a separate file for exporting your backup sets."));
            return false;
        }
    }
    BackupConfig saved;
    QString error;
    if (!store.load(&saved, &error) || !BackupConfigStore(filePath).exportSets(saved, &error)) {
        emit failed(error);
        return false;
    }
    emit statusChanged(QStringLiteral("Backup sets exported."));
    return true;
}

bool BackupSetController::importSets(const QString &filePath)
{
    BackupConfig imported;
    QString error;
    if (!BackupConfigStore(filePath).importSets(&imported, &error)) {
        emit failed(error);
        return false;
    }
    QLockFile workerLock(store.filePath() + QStringLiteral(".worker.lock"));
    QLockFile runLock(runStore.filePath() + QStringLiteral(".lock"));
    if (!workerLock.tryLock(0) || !runLock.tryLock(0)) {
        emit failed(QStringLiteral("Wait for the current backup or queue update to finish before importing sets."));
        return false;
    }
    BackupConfig updated = config;
    updated.sets = imported.sets;
    updated.sourceDirectory.clear();
    updated.remoteRoot.clear();
    if (!store.save(updated, &error)) {
        emit failed(error);
        return false;
    }
    config = updated;
    selectedIndex = config.sets.isEmpty() ? -1 : 0;
    clearPreview();
    emit setsChanged();
    emit currentIndexChanged();
    emit currentSetChanged();
    emit dashboardChanged();
    emit statusChanged(QStringLiteral("Backup sets imported."));
    return true;
}

bool BackupSetController::confirmCleanup()
{
    const BackupSet *set = currentSet();
    if (set == nullptr || cleanupStore.state(set->id).targets.isEmpty()) {
        return false;
    }
    cleanupStore.confirm(set->id);
    QString error;
    if (!cleanupStore.save(&error)) {
        emit failed(error);
        return false;
    }
    emit cleanupChanged();
    emit statusChanged(QStringLiteral("Retention cleanup confirmed; it will run after the next verified backup."));
    return true;
}

BackupSet *BackupSetController::currentSet()
{
    return selectedIndex >= 0 && selectedIndex < config.sets.size()
        ? &config.sets[selectedIndex]
        : nullptr;
}

const BackupSet *BackupSetController::currentSet() const
{
    return selectedIndex >= 0 && selectedIndex < config.sets.size()
        ? &config.sets.at(selectedIndex)
        : nullptr;
}

QVector<int> BackupSetController::recentBackupIndexes() const
{
    QVector<int> indexes;
    indexes.reserve(config.sets.size());
    for (int index = 0; index < config.sets.size(); ++index) {
        const BackupRunRecord *record = runStore.find(config.sets.at(index).id);
        if (record == nullptr || record->status != QStringLiteral("copy_deleted")) {
            indexes.append(index);
        }
    }

    const auto latestActivity = [this](int index) {
        const BackupRunRecord *record = runStore.find(config.sets.at(index).id);
        if (record == nullptr) {
            return QDateTime();
        }

        QDateTime latest = record->lastSuccess;
        if (record->lastFailure > latest) {
            latest = record->lastFailure;
        }
        if (record->lastScheduled > latest) {
            latest = record->lastScheduled;
        }
        return latest;
    };

    std::stable_sort(indexes.begin(), indexes.end(), [&latestActivity](int left, int right) {
        return latestActivity(left) > latestActivity(right);
    });
    return indexes;
}

void BackupSetController::clearPreview()
{
    previewResult = {};
    emit previewChanged();
}

void BackupSetController::refreshRunState()
{
    if (runStore.load()) {
        emit runStateChanged();
        emit dashboardChanged();
    }
    if (cleanupStore.load()) {
        emit cleanupChanged();
    }
}
