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
