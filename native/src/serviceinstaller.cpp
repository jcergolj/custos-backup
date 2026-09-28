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
            *error = QStringLiteral("The native worker must be an executable absolute path.");
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

    const QString path = QDir(serviceDirectory).filePath(QStringLiteral("praefectus-native.service"));
    QSaveFile service(path);
    if (!service.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to write the systemd user service.");
        }

        return false;
    }

    const QByteArray contents = QStringLiteral(
        "[Unit]\n"
        "Description=Praefectus native backup worker\n\n"
        "[Service]\n"
        "Type=oneshot\n"
        "ExecStart=%1 --config %2/.config/praefectus/native-backup.json\n"
        "Nice=19\n"
        "CPUQuota=10%\n"
        "IOSchedulingClass=idle\n"
        "NoNewPrivileges=true\n"
    ).arg(workerPath, QStringLiteral("%h")).toUtf8();

    if (service.write(contents) != contents.size() || !service.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("Unable to finish writing the systemd user service.");
        }

        return false;
    }

    if (installedPath != nullptr) {
        *installedPath = path;
    }

    return true;
}
