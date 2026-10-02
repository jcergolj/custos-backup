#include "backupscheduler.h"

#include "backupconfig.h"

#include <QFileInfo>
#include <QMap>
#include <algorithm>

BackupScheduler::BackupScheduler(QString configPath, QString systemctl, QObject *parent)
    : QObject(parent), configPath(std::move(configPath)), systemctl(std::move(systemctl))
{
    timeout.setSingleShot(true);
    timeout.setInterval(10000);
    connect(&timeout, &QTimer::timeout, this, [this] {
        timedOut = true;
        process.kill();
    });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            completed(false, {}, tr("Unable to start systemctl. Check that user systemd is available."));
        }
    });
    connect(&process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus exitStatus) {
        completed(!timedOut && code == 0 && exitStatus == QProcess::NormalExit,
                  QString::fromLocal8Bit(process.readAllStandardOutput()),
                  timedOut ? tr("The scheduling command timed out.")
                           : QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
    });
    poll.setInterval(30000);
    connect(&poll, &QTimer::timeout, this, &BackupScheduler::refresh);
    poll.start();
    QTimer::singleShot(0, this, &BackupScheduler::refresh);
}

BackupScheduler::~BackupScheduler()
{
    poll.stop();
    // Give an in-flight save time to finish its reload/enable/verification chain.
    // QProcess's wait also delivers finished, which advances to the next phase.
    for (int attempt = 0; working && attempt < 4; ++attempt) {
        if (!process.waitForFinished(1000)) {
            break;
        }
    }
    activationPending = false;
    disconnect(&process, nullptr, this, nullptr);
    if (process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(1000);
    }
}

QString BackupScheduler::status() const
{
    if (working && phase != Phase::Check) {
        return tr("Starting scheduled backups…");
    }
    if (!checked) {
        return tr("Checking scheduling…");
    }
    if (!lastError.isEmpty()) {
        return tr("Scheduling needs attention");
    }
    if (ready()) {
        return scheduled ? tr("Scheduling active") : tr("Scheduler active · no enabled schedules");
    }
    if (active) {
        return tr("Scheduling active for this session only");
    }
    return scheduled ? tr("Scheduling paused") : tr("No scheduled backups");
}

bool BackupScheduler::readSchedules()
{
    BackupConfig config;
    QString error;
    if (QFileInfo::exists(configPath) && !BackupConfigStore(configPath).load(&config, &error)) {
        scheduled = false;
        activationPending = false;
        lastError = error;
        checked = true;
        emit stateChanged();
        return false;
    }
    scheduled = std::any_of(config.sets.cbegin(), config.sets.cend(), [](const BackupSet &set) {
        return set.schedule.enabled();
    });
    return true;
}

void BackupScheduler::refresh()
{
    if (!working && readSchedules()) {
        start(Phase::Check);
    }
}

void BackupScheduler::applySavedSchedules()
{
    if (!readSchedules()) {
        emit failed(lastError);
        return;
    }
    // Only persisted schedules can activate the timer. Merely opening the app,
    // editing a schedule, or previewing a backup preserves a user's paused timer.
    activationPending = scheduled;
    if (!scheduled) {
        activationError.clear();
    }
    emit stateChanged();
    if (!working) {
        if (scheduled) {
            enable();
        } else {
            refresh();
        }
    }
}

void BackupScheduler::enable()
{
    if (!readSchedules()) {
        emit failed(lastError);
        return;
    }
    if (!scheduled) {
        emit failed(tr("Save a daily, weekly, or monthly schedule first."));
        return;
    }
    activationPending = true;
    if (!working) {
        activationPending = false;
        start(Phase::Reload);
    }
}

void BackupScheduler::start(Phase nextPhase)
{
    phase = nextPhase;
    working = true;
    timedOut = false;
    QStringList arguments {QStringLiteral("--user")};
    if (phase == Phase::Reload) {
        arguments.append(QStringLiteral("daemon-reload"));
    } else if (phase == Phase::Enable) {
        arguments.append({QStringLiteral("enable"), QStringLiteral("--now"), QStringLiteral("custos.timer")});
    } else {
        arguments.append({QStringLiteral("show"), QStringLiteral("custos.timer"),
            QStringLiteral("--property=LoadState"), QStringLiteral("--property=ActiveState"),
            QStringLiteral("--property=UnitFileState")});
    }
    emit stateChanged();
    timeout.start();
    process.start(systemctl, arguments);
}

void BackupScheduler::completed(bool success, const QString &output, const QString &error)
{
    timeout.stop();
    if (!success) {
        lastError = error.isEmpty() ? tr("Unable to activate or check the backup scheduler.") : error;
        checked = true;
        if (phase != Phase::Check) {
            activationError = lastError;
            emit failed(tr("Backup settings saved, but scheduling could not be activated: %1").arg(lastError));
        }
        finish();
        return;
    }
    if (phase == Phase::Reload) {
        if (scheduled) {
            start(Phase::Enable);
        } else {
            finish();
        }
        return;
    }
    if (phase == Phase::Enable) {
        start(Phase::Verify);
        return;
    }
    QMap<QString, QString> properties;
    for (const QString &line : output.split('\n')) {
        const int separator = line.indexOf('=');
        if (separator > 0) {
            properties.insert(line.left(separator), line.mid(separator + 1).trimmed());
        }
    }
    active = properties.value(QStringLiteral("ActiveState")) == QStringLiteral("active");
    enabled = properties.value(QStringLiteral("UnitFileState")) == QStringLiteral("enabled");
    checked = true;
    lastError.clear();
    if (properties.value(QStringLiteral("LoadState")) != QStringLiteral("loaded")
        || !properties.contains(QStringLiteral("ActiveState")) || !properties.contains(QStringLiteral("UnitFileState"))) {
        lastError = tr("The custos.timer unit is unavailable. Check your Custos installation.");
    } else if (phase == Phase::Verify && !ready()) {
        lastError = tr("The backup timer is not enabled and active. Try enabling scheduling again.");
    }
    if (active && enabled && lastError.isEmpty()) {
        activationError.clear();
    } else if (phase == Phase::Verify) {
        activationError = lastError;
    }
    if (lastError.isEmpty()) {
        lastError = activationError;
    }
    if (phase == Phase::Verify) {
        if (lastError.isEmpty()) {
            emit messageChanged(tr("Backup settings saved. Scheduling is active."));
        } else {
            emit failed(lastError);
        }
    }
    finish();
}

void BackupScheduler::finish()
{
    working = false;
    emit stateChanged();
    if (activationPending && scheduled) {
        enable();
    }
}
