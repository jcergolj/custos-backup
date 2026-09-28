#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "backupengine.h"
#include "backuplauncher.h"
#include "backuprestorecontroller.h"
#include "localprovider.h"
#include <QDir>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine;
    BackupEngine backupEngine;
    BackupLauncher backupLauncher;
    LocalProvider restoreProvider(QDir::homePath());
    BackupRestoreController restoreController(backupEngine, &restoreProvider);

    engine.rootContext()->setContextProperty(QStringLiteral("backupEngine"), &backupEngine);
    engine.rootContext()->setContextProperty(QStringLiteral("backupLauncher"), &backupLauncher);
    engine.rootContext()->setContextProperty(QStringLiteral("restoreController"), &restoreController);
    engine.loadFromModule(QStringLiteral("Praefectus"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
