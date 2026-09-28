#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "backupengine.h"

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine;
    BackupEngine backupEngine;

    engine.rootContext()->setContextProperty(QStringLiteral("backupEngine"), &backupEngine);
    engine.loadFromModule(QStringLiteral("Praefectus"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
