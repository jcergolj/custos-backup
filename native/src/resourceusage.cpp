#include "resourceusage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <array>

namespace {
struct Preset {
    const char *id;
    int cpuQuota;
    int nice;
    bool idleIo;
    int ioPriority;
};
constexpr std::array<Preset, 5> presets {{{"very-low", 10, 19, true, 7},
    {"low", 25, 15, true, 7}, {"medium", 50, 10, false, 7},
    {"high", 100, 5, false, 5}, {"very-high", 200, 0, false, 4}}};
}

ResourceUsage::ResourceUsage(QObject *parent)
    : ResourceUsage(QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                        .filePath(QStringLiteral("systemd/user")), QStringLiteral("systemctl"), parent)
{
}

ResourceUsage::ResourceUsage(QString serviceDirectory, QString systemctl, QObject *parent)
    : QObject(parent)
    , dropInPath(QDir(serviceDirectory).filePath(QStringLiteral("custos.service.d/50-custos-resources.conf")))
    , systemctl(std::move(systemctl))
{
    QFile file(dropInPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray data = file.readAll();
        for (int index = -1; index < int(presets.size()); ++index) {
            if (data == contents(index)) {
                savedIndex = index;
                break;
            }
        }
    }
    timeout.setSingleShot(true);
    timeout.setInterval(10000);
    connect(&timeout, &QTimer::timeout, this, [this] {
        timedOut = true;
        reload.kill();
    });
    connect(&reload, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish(false, tr("Unable to start systemctl to apply backup resource settings."));
        }
    });
    connect(&reload, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString error = timedOut ? tr("Reloading the user systemd manager timed out.")
            : QString::fromLocal8Bit(reload.readAllStandardError()).trimmed();
        finish(!timedOut && code == 0 && status == QProcess::NormalExit, error);
    });
}

ResourceUsage::~ResourceUsage()
{
    if (applying) {
        // Finish the short reload before exiting so its result and any rollback
        // are handled while all controller members are still alive.
        if (!reload.waitForFinished(1000) && applying) {
            reload.kill();
            reload.waitForFinished(1000);
            if (applying) {
                finish(false, tr("Resource settings could not be applied before closing."));
            }
        }
    }
    disconnect(&reload, nullptr, this, nullptr);
}

QStringList ResourceUsage::names() const
{
    return {tr("Very low"), tr("Low"), tr("Medium"), tr("High"), tr("Very high")};
}

QStringList ResourceUsage::descriptions() const
{
    QStringList result;
    for (const auto &preset : presets) {
        result.append(tr("CPU limit: %1% · CPU priority (nice): %2 · I/O: %3")
                          .arg(preset.cpuQuota).arg(preset.nice)
                          .arg(preset.idleIo ? tr("idle") : tr("best effort")));
    }
    return result;
}

QByteArray ResourceUsage::contents(int index)
{
    if (index == -1) {
        // Explicitly clear the quota and reset scheduling when opting out, even
        // if a service from an older installation still contains low limits.
        return QByteArray("# Managed by Custos Backup: system-defaults\n[Service]\n"
                          "CPUQuota=\nNice=0\nIOSchedulingClass=none\nIOSchedulingPriority=0\n");
    }
    const auto &preset = presets.at(index);
    return QStringLiteral("# Managed by Custos Backup: %1\n[Service]\nCPUQuota=%2%\nNice=%3\n"
                          "IOSchedulingClass=%4\nIOSchedulingPriority=%5\n")
        .arg(QString::fromLatin1(preset.id)).arg(preset.cpuQuota).arg(preset.nice)
        .arg(preset.idleIo ? QStringLiteral("idle") : QStringLiteral("best-effort"))
        .arg(preset.ioPriority).toUtf8();
}

bool ResourceUsage::write(const QByteArray &data)
{
    QSaveFile file(dropInPath);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

void ResourceUsage::save(int index)
{
    if (applying) {
        return;
    }
    if (index < -1 || index >= int(presets.size())) {
        emit failed(tr("Choose a valid resource usage preset."));
        return;
    }
    QFile previous(dropInPath);
    hadPreviousFile = previous.exists();
    previousContents.clear();
    if (hadPreviousFile) {
        if (!previous.open(QIODevice::ReadOnly)) {
            emit failed(tr("Unable to read the existing backup resource settings."));
            return;
        }
        previousContents = previous.readAll();
        previous.close();
    }
    if (index == savedIndex && ((!hadPreviousFile && index == -1) || previousContents == contents(index))) {
        return;
    }
    if (!QDir().mkpath(QFileInfo(dropInPath).absolutePath()) || !write(contents(index))) {
        emit failed(tr("Unable to save backup resource settings."));
        return;
    }
    pendingIndex = index;
    applying = true;
    timedOut = false;
    emit busyChanged();
    timeout.start();
    reload.start(systemctl, {QStringLiteral("--user"), QStringLiteral("daemon-reload")});
}

void ResourceUsage::finish(bool success, const QString &error)
{
    if (!applying) {
        return;
    }
    timeout.stop();
    applying = false;
    if (success) {
        savedIndex = pendingIndex;
        emit presetChanged();
        emit busyChanged();
        emit statusChanged(tr("Resource usage saved. New limits apply when the next backup worker starts."));
    } else {
        const bool restored = hadPreviousFile ? write(previousContents) : QFile::remove(dropInPath);
        emit busyChanged();
        QString message = error.isEmpty() ? tr("Unable to apply backup resource settings.") : error;
        if (!restored) {
            message += tr(" The previous resource settings could not be restored.");
        }
        emit failed(message);
    }
}
