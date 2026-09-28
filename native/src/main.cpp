#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "backupengine.h"
#include "backupconfig.h"
#include "backuplauncher.h"
#include "backuprestorecontroller.h"
#include "backupsetcontroller.h"
#include "protonprovider.h"
#include "qprocessrunner.h"
#include <QDir>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QQmlApplicationEngine engine;
    BackupEngine backupEngine;
    BackupLauncher backupLauncher;
    BackupSetController backupSetController(
        backupEngine,
        QDir::home().filePath(QStringLiteral(".config/praefectus/native-backup.json"))
    );
    BackupConfig config;
    QString configError;
    const QString configPath = QDir::home().filePath(QStringLiteral(".config/praefectus/native-backup.json"));
    const QString protonBinary = BackupConfigStore(configPath).load(&config, &configError)
        ? config.protonBinary
        : qEnvironmentVariable("PRAEFECTUS_PROTON_BIN", QStringLiteral("proton-drive"));
    QProcessRunner restoreRunner(
        protonBinary
    );
    ProtonProvider restoreProvider(restoreRunner);
    BackupRestoreController restoreController(backupEngine, &restoreProvider);

    engine.rootContext()->setContextProperty(QStringLiteral("backupEngine"), &backupEngine);
    engine.rootContext()->setContextProperty(QStringLiteral("backupLauncher"), &backupLauncher);
    engine.rootContext()->setContextProperty(QStringLiteral("backupSetController"), &backupSetController);
    engine.rootContext()->setContextProperty(QStringLiteral("restoreController"), &restoreController);
    engine.loadFromModule(QStringLiteral("Praefectus"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
