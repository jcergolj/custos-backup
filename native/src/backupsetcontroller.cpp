#include "backupsetcontroller.h"

#include "backupprerequisites.h"

#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace {

BackupSet newSet(int number)
{
    return {
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        QStringLiteral("Backup set %1").arg(number),
        QStringLiteral("/my-files/backups/set-%1").arg(number),
        {},
        {},
    };
}

}

BackupSetController::BackupSetController(BackupEngine &engine, QString configPath, QObject *parent)
    : QObject(parent)
    , engine(engine)
    , store(configPath)
    , runStore(QDir(QFileInfo(configPath).absolutePath()).filePath(QStringLiteral("native-backup-runs.json")))
    , cleanupStore(QDir(QFileInfo(configPath).absolutePath()).filePath(QStringLiteral("native-backup-cleanup.json")))
{
    connect(&stateTimer, &QTimer::timeout, this, &BackupSetController::refreshRunState);
    stateTimer.start(5000);
    runStore.load();
    cleanupStore.load();
    QString error;
    if (store.load(&config, &error)) {
        selectedIndex = 0;
    } else if (QFileInfo::exists(store.filePath())) {
        emit failed(error);
    } else {
        config.protonBinary = qEnvironmentVariable("PRAEFECTUS_PROTON_BIN", QStringLiteral("proton-drive"));
        config.sets.append(newSet(1));
        selectedIndex = 0;
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

QStringList BackupSetController::currentRequiredMounts() const
{
    QStringList mounts;
    const BackupSet *set = currentSet();
    if (set == nullptr) {
        return mounts;
    }
    for (const RequiredVolume &volume : set->requiredVolumes) {
        mounts.append(volume.mountPath);
    }
    return mounts;
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

void BackupSetController::setCurrentRequiredMounts(const QStringList &mounts)
{
    if (BackupSet *set = currentSet()) {
        set->requiredVolumes.clear();
        for (const QString &mount : mounts) {
            const RequiredVolume volume = BackupPrerequisites::captureVolume(mount);
            set->requiredVolumes.append({mount, volume.mountPath == QDir::cleanPath(QFileInfo(mount).absoluteFilePath())
                ? volume.deviceId
                : QByteArray()});
        }
        emit currentSetChanged();
    }
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
    clearPreview();
}

void BackupSetController::removeCurrentSet()
{
    if (selectedIndex < 0 || selectedIndex >= config.sets.size()) {
        return;
    }

    config.sets.removeAt(selectedIndex);
    if (config.sets.isEmpty()) {
        config.sets.append(newSet(1));
    }
    selectedIndex = qMin(selectedIndex, config.sets.size() - 1);
    emit setsChanged();
    emit currentIndexChanged();
    emit currentSetChanged();
    clearPreview();
}

void BackupSetController::preview()
{
    const BackupSet *set = currentSet();
    if (set == nullptr) {
        clearPreview();
        emit failed(QStringLiteral("A backup set must be selected."));

        return;
    }

    previewResult = engine.preview(set->sourceDirectories, set->exclusions);
    emit previewChanged();
    emit statusChanged(QStringLiteral("%1 included, %2 excluded, %3 skipped, %4 missing.")
        .arg(previewResult.includedFiles.size())
        .arg(previewResult.excludedFiles.size())
        .arg(previewResult.skippedPaths.size())
        .arg(previewResult.missingPaths.size()));
}

bool BackupSetController::save()
{
    const BackupSet *set = currentSet();
    if (set == nullptr || set->name.trimmed().isEmpty() || set->remoteRoot.trimmed().isEmpty()
        || set->sourceDirectories.isEmpty()) {
        emit failed(QStringLiteral("The selected backup set is incomplete."));

        return false;
    }

    QString error;
    if (!store.save(config, &error)) {
        emit failed(error);

        return false;
    }

    emit statusChanged(QStringLiteral("Backup sets saved."));
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

void BackupSetController::clearPreview()
{
    previewResult = {};
    emit previewChanged();
}

void BackupSetController::refreshRunState()
{
    if (runStore.load()) {
        emit runStateChanged();
    }
    if (cleanupStore.load()) {
        emit cleanupChanged();
    }
}
