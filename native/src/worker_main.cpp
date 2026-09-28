#include "backupengine.h"
#include "backupconfig.h"
#include "localprovider.h"
#include "protonprovider.h"
#include "qprocessrunner.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({{"c", "config"}, QStringLiteral("Configuration file."), QStringLiteral("path")});
    parser.process(application);

    const QString configPath = parser.value(QStringLiteral("config"));
    if (configPath.isEmpty()) {
        parser.showHelp(2);
    }

    BackupConfig config;
    QString error;
    if (!BackupConfigStore(configPath).load(&config, &error)) {
        qCritical().noquote() << error;

        return 1;
    }

    QProcessRunner runner(config.protonBinary);
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString manifestPath;

    if (!engine.backup(config.sourceDirectory, config.remoteRoot, provider, &manifestPath, &error)) {
        qCritical().noquote() << error;

        return 1;
    }

    qInfo().noquote() << manifestPath;

    return 0;
}
