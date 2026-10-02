#include "serviceinstaller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

ServiceInstaller::ServiceInstaller(QString serviceDirectory)
    : serviceDirectory(std::move(serviceDirectory))
{
}

bool ServiceInstaller::install(const QString &workerPath, QString *installedPath, QString *error) const
{
    const QFileInfo worker(workerPath);
    if (!worker.isAbsolute() || !worker.isFile() || !worker.isExecutable()) {
        if (error != nullptr) {
            *error = QStringLiteral("The Custos worker must be an executable absolute path.");
        }

        return false;
    }

    if (serviceDirectory.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("The systemd user service directory is not configured.");
        }

        return false;
    }

    if (!QDir().mkpath(serviceDirectory)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to create the systemd user service directory.");
        }

        return false;
    }

    const QString path = QDir(serviceDirectory).filePath(QStringLiteral("custos.service"));
    QSaveFile service(path);
    if (!service.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the systemd user service.");
        }

        return false;
    }

    const QByteArray contents = QStringLiteral(
        "[Unit]\n"
        "Description=Custos Backup worker\n\n"
        "[Service]\n"
        "Type=oneshot\n"
        "ExecStart=%1 --config %2/.config/custos/custos-backup.json\n"
        "NoNewPrivileges=true\n"
    ).arg(workerPath, QStringLiteral("%h")).toUtf8();

    if (service.write(contents) != contents.size() || !service.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to finish writing the systemd user service.");
        }

        return false;
    }

    const QString timerPath = QDir(serviceDirectory).filePath(QStringLiteral("custos.timer"));
    QSaveFile timer(timerPath);
    if (!timer.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the Custos systemd timer.");
        }

        return false;
    }
    const QByteArray timerContents = QByteArray(
        "[Unit]\n"
        "Description=Run Custos Backup scheduler\n\n"
        "[Timer]\n"
        "OnCalendar=*-*-* *:*:00\n"
        "Persistent=true\n"
        "Unit=custos.service\n\n"
        "[Install]\n"
        "WantedBy=timers.target\n"
    );
    if (timer.write(timerContents) != timerContents.size() || !timer.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to finish writing the Custos systemd timer.");
        }

        return false;
    }

    if (installedPath != nullptr) {
        *installedPath = path;
    }

    return true;
}
