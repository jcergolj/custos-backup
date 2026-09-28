#include "systemdlauncher.h"

SystemdLauncher::SystemdLauncher(ProcessRunner &runner)
    : runner(runner)
{
}

bool SystemdLauncher::startUserService(const QString &serviceName, QString *error)
{
    if (serviceName.trimmed().isEmpty() || serviceName.contains(QChar('/'))) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup service name is invalid.");
        }

        return false;
    }

    const ProcessOutput output = runner.run({
        QStringLiteral("--user"),
        QStringLiteral("--no-block"),
        QStringLiteral("start"),
        serviceName,
    });

    if (output.successful()) {
        return true;
    }

    if (error != nullptr) {
        *error = output.standardError.isEmpty()
            ? QStringLiteral("Unable to start the backup service.")
            : output.standardError.trimmed();
    }

    return false;
}

bool SystemdLauncher::enableUserTimer(const QString &timerName, QString *error)
{
    if (timerName.trimmed().isEmpty() || timerName.contains(QChar('/'))) {
        if (error != nullptr) {
            *error = QStringLiteral("The backup timer name is invalid.");
        }

        return false;
    }

    const ProcessOutput reload = runner.run({QStringLiteral("--user"), QStringLiteral("daemon-reload")});
    if (!reload.successful()) {
        if (error != nullptr) {
            *error = reload.standardError.isEmpty()
                ? QStringLiteral("Unable to reload the user systemd manager.")
                : reload.standardError.trimmed();
        }

        return false;
    }

    const ProcessOutput enable = runner.run({
        QStringLiteral("--user"), QStringLiteral("enable"), QStringLiteral("--now"), timerName,
    });
    if (enable.successful()) {
        return true;
    }

    if (error != nullptr) {
        *error = enable.standardError.isEmpty()
            ? QStringLiteral("Unable to enable the backup timer.")
            : enable.standardError.trimmed();
    }

    return false;
}
