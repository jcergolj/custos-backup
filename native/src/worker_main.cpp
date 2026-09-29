#include "backupengine.h"
#include "backupconfig.h"
#include "backuprunstore.h"
#include "backupprerequisites.h"
#include "backupschedule.h"
#include "backupcatalog.h"
#include "backupecleanup.h"
#include "protonprovider.h"
#include "qprocessrunner.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QSysInfo>
#include <QUuid>

#include <algorithm>

namespace {

bool authenticationFailure(const QString &error)
{
    const QString message = error.toLower();
    return message.contains(QStringLiteral("auth"))
        || message.contains(QStringLiteral("sign in"))
        || message.contains(QStringLiteral("login"))
        || message.contains(QStringLiteral("401"));
}

QString remoteSegment(QString value)
{
    QString result;
    for (const QChar character : value.trimmed()) {
        result.append(character.isLetterOrNumber() || character == '-' || character == '_' || character == '.'
                ? character : QChar('_'));
    }
    return result.isEmpty() ? QStringLiteral("computer") : result;
}

QString copyId()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddTHHmmsszzz"))
        + QStringLiteral("-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

QString setRemoteSegment(const BackupSet &set, const QString &computerName)
{
    return QDir(set.remoteRoot).filePath(
        QDir(remoteSegment(computerName)).filePath(remoteSegment(set.name)));
}

}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({{"c", "config"}, QStringLiteral("Configuration file."), QStringLiteral("path")});
    parser.process(application);

    const QString configuredPath = parser.value(QStringLiteral("config")).isEmpty()
        ? QDir::home().filePath(QStringLiteral(".config/custos/custos-backup.json"))
        : parser.value(QStringLiteral("config"));
    const QString configPath = QFileInfo(configuredPath).absoluteFilePath();
    QLockFile processLock(configPath + QStringLiteral(".worker.lock"));
    if (!processLock.tryLock(0)) {
        return 0;
    }
    const QString stateDirectory = QFileInfo(configPath).absolutePath();
    QLockFile runStateLock(QDir(stateDirectory).filePath(QStringLiteral("custos-backup-runs.json.lock")));
    if (!runStateLock.tryLock(0)) {
        return 0;
    }

    BackupConfig config;
    QString error;
    if (!BackupConfigStore(configPath).load(&config, &error)) {
        qCritical().noquote() << error;

        return 1;
    }

    QProcessRunner runner(config.protonBinary);
    ProtonProvider provider(runner);
    BackupEngine engine;
    const QString cleanupPath = QDir(stateDirectory).filePath(QStringLiteral("custos-backup-cleanup.json"));
    CleanupStore cleanupStore(cleanupPath);
    if (!cleanupStore.load(&error)) {
        qCritical().noquote() << error;
        return 1;
    }
    BackupRunStore runStore(QDir(stateDirectory).filePath(QStringLiteral("custos-backup-runs.json")));
    if (!runStore.load(&error)) {
        qCritical().noquote() << error;

        return 1;
    }
    for (const BackupSet &set : config.sets) {
        runStore.ensureSet(set.id);
        BackupRunRecord *record = runStore.find(set.id);
        const QDateTime now = QDateTime::currentDateTime();
        if (set.schedule.enabled()) {
            QDateTime due = record->nextScheduled;
            if (record->lastScheduled.isValid()) {
                const QDateTime recalculated = BackupScheduleCalculator::nextRun(set.schedule, record->lastScheduled);
                if (recalculated.isValid() && recalculated != due) {
                    due = recalculated;
                    record->nextScheduled = recalculated;
                }
            }
            if (!due.isValid()) {
                const QDateTime currentDue = BackupScheduleCalculator::dueRun(set.schedule, now);
                if (currentDue.isValid() && currentDue <= now) {
                    due = currentDue;
                } else {
                    record->nextScheduled = BackupScheduleCalculator::nextRun(set.schedule, now);
                }
            }
            if (due.isValid() && due <= now) {
                record->lastScheduled = due;
                record->nextScheduled = BackupScheduleCalculator::nextRun(set.schedule, now);
                runStore.enqueue(set.id, QStringLiteral("schedule"), due);
            }
        }
    }
    if (!runStore.save(&error)) {
        qCritical().noquote() << error;

        return 1;
    }

    SystemBackupPrerequisiteProbe prerequisites;
    const QDateTime now = QDateTime::currentDateTime();
    for (const int index : runStore.readyIndexes(now)) {
        BackupRunRecord &record = runStore.records()[index];
        const auto setIterator = std::find_if(config.sets.cbegin(), config.sets.cend(), [&record](const BackupSet &set) {
            return set.id == record.setId;
        });
        if (setIterator == config.sets.cend()) {
            continue;
        }

        const BackupPrerequisiteResult prerequisite = BackupPrerequisites::check(*setIterator, prerequisites);
        if (!prerequisite.ready) {
            runStore.markWaiting(record, prerequisite.reason, now);
            continue;
        }

        runStore.markRunning(record);
        QString manifestPath;
        const QString computerName = QSysInfo::machineHostName();
        const QString copy = copyId();
        const QString copyRoot = QDir(setIterator->remoteRoot).filePath(
            QDir(remoteSegment(computerName)).filePath(
                QDir(remoteSegment(setIterator->name)).filePath(copy)));
        const BackupCopyMetadata metadata {
            computerName,
            setIterator->id,
            setIterator->name,
            copy,
            QDateTime::currentDateTimeUtc(),
        };
        if (engine.backup(setIterator->sourceDirectories, copyRoot, setIterator->exclusions, metadata, provider, &manifestPath, &error)) {
            runStore.markSuccess(record, QDateTime::currentDateTime());
            qInfo().noquote() << setIterator->name << manifestPath;

            QVector<RemoteCopy> copies;
            QString catalogError;
            if (BackupCatalog::discover(provider, setIterator->remoteRoot, &copies, &catalogError)) {
                const QStringList targets = BackupCleanup::eligibleTargets(
                    copies, setIterator->retention, computerName, setIterator->id);
                CleanupState &cleanupState = cleanupStore.states()[setIterator->id];
                if (cleanupState.decision == QStringLiteral("confirmed")) {
                    cleanupStore.setTargets(setIterator->id, targets);
                    QString cleanupError;
                    if (!BackupCleanup::apply(provider, cleanupStore, setIterator->id,
                            setRemoteSegment(*setIterator, computerName), &cleanupError)) {
                        qCritical().noquote() << setIterator->name << QStringLiteral("Cleanup failed:") << cleanupError;
                    }
                } else if (cleanupState.targets.isEmpty()) {
                    cleanupStore.setPending(setIterator->id, targets);
                }
                if (!cleanupStore.save(&catalogError)) {
                    qCritical().noquote() << catalogError;
                }
            } else {
                qCritical().noquote() << setIterator->name << QStringLiteral("Cleanup preview unavailable:") << catalogError;
            }
        } else if (authenticationFailure(error)) {
            runStore.markAuthenticationRequired(record, error, QDateTime::currentDateTime());
            qCritical().noquote() << setIterator->name << error;
        } else if (!manifestPath.isEmpty()) {
            runStore.markIncomplete(record, error, QDateTime::currentDateTime());
            qCritical().noquote() << setIterator->name << error << manifestPath;
        } else {
            runStore.markRetrying(record, error, QDateTime::currentDateTime());
            qCritical().noquote() << setIterator->name << error;
        }

        if (!runStore.save(&error)) {
            qCritical().noquote() << error;

            return 1;
        }
    }

    return 0;
}
