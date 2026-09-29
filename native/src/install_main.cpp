#include "serviceinstaller.h"
#include "qprocessrunner.h"
#include "systemdlauncher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const QString workerPath = application.arguments().value(1);
    if (workerPath.isEmpty()) {
        qCritical() << "Usage: custos-install <worker-path>";

        return 2;
    }

    const QString serviceDirectory = QDir::home().filePath(QStringLiteral(".config/systemd/user"));
    ServiceInstaller installer(serviceDirectory);
    QString error;
    if (!installer.install(workerPath, nullptr, &error)) {
        qCritical().noquote() << error;

        return 1;
    }

    QProcessRunner runner(QStringLiteral("systemctl"));
    SystemdLauncher systemd(runner);
    if (!systemd.enableUserTimer(QStringLiteral("custos.timer"), &error)) {
        qCritical().noquote() << error;

        return 1;
    }

    qInfo().noquote() << QStringLiteral("Installed user service in %1.").arg(serviceDirectory);

    return 0;
}
