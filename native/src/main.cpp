#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "backupengine.h"
#include "backuplauncher.h"
#include "qprocessrunner.h"
#include "systemdlauncher.h"

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine;
    BackupEngine backupEngine;
    BackupLauncher backupLauncher;

    engine.rootContext()->setContextProperty(QStringLiteral("backupEngine"), &backupEngine);
    engine.rootContext()->setContextProperty(QStringLiteral("backupLauncher"), &backupLauncher);
    engine.loadFromModule(QStringLiteral("Praefectus"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
