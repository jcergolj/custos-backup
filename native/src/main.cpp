#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "backupengine.h"
#include "backupconfig.h"
#include "backuplauncher.h"
#include "backuprestorecontroller.h"
#include "backupsetcontroller.h"
#include "protonprovider.h"
#include "protonfolderbrowser.h"
#include "recentbackupcopies.h"
#include "qprocessrunner.h"
#include "protonauthcontroller.h"
#include "themecolors.h"
#include "resourceusage.h"
#include <QDir>
#include <QSysInfo>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Custos Backup"));
    application.setApplicationDisplayName(QStringLiteral("Custos Backup"));
    QQmlApplicationEngine engine;
    BackupEngine backupEngine;
    BackupLauncher backupLauncher;
    BackupSetController backupSetController(
        backupEngine,
        QDir::home().filePath(QStringLiteral(".config/custos/custos-backup.json"))
    );
    BackupConfig config;
    QString configError;
    const QString configPath = QDir::home().filePath(QStringLiteral(".config/custos/custos-backup.json"));
    const QString protonBinary = BackupConfigStore(configPath).load(&config, &configError)
        ? config.protonBinary
        : qEnvironmentVariable("CUSTOS_PROTON_BIN", QStringLiteral("proton-drive"));
    QProcessRunner restoreRunner(
        protonBinary
    );
    ProtonProvider restoreProvider(restoreRunner);
    ProtonFolderBrowser protonFolderBrowser(restoreRunner);
    RecentBackupCopies recentBackupCopies(restoreProvider, configPath, QSysInfo::machineHostName());
    BackupRestoreController restoreController(backupEngine, &restoreProvider);
    ProtonAuthController protonAuth(protonBinary);
    ThemeColors themeColors;
    ResourceUsage resourceUsage;

    engine.rootContext()->setContextProperty(QStringLiteral("backupEngine"), &backupEngine);
    engine.rootContext()->setContextProperty(QStringLiteral("backupLauncher"), &backupLauncher);
    engine.rootContext()->setContextProperty(QStringLiteral("backupSetController"), &backupSetController);
    engine.rootContext()->setContextProperty(QStringLiteral("restoreController"), &restoreController);
    engine.rootContext()->setContextProperty(QStringLiteral("protonFolderBrowser"), &protonFolderBrowser);
    engine.rootContext()->setContextProperty(QStringLiteral("recentBackupCopies"), &recentBackupCopies);
    engine.rootContext()->setContextProperty(QStringLiteral("protonAuth"), &protonAuth);
    engine.rootContext()->setContextProperty(QStringLiteral("themeColors"), &themeColors);
    engine.rootContext()->setContextProperty(QStringLiteral("resourceUsage"), &resourceUsage);
    engine.loadFromModule(QStringLiteral("Custos"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}
