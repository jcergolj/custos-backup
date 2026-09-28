#include "backupengine.h"
#include "localprovider.h"
#include "protonprovider.h"
#include "qprocessrunner.h"

#include <QCoreApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({{"s", "source"}, QStringLiteral("Source directory."), QStringLiteral("path")});
    parser.addOption({{"r", "remote"}, QStringLiteral("Remote destination."), QStringLiteral("path")});
    parser.process(application);

    const QString source = parser.value(QStringLiteral("source"));
    const QString remote = parser.value(QStringLiteral("remote"));
    if (source.isEmpty() || remote.isEmpty()) {
        parser.showHelp(2);
    }

    const QString protonBinary = qEnvironmentVariable("PRAEFECTUS_PROTON_BIN", QStringLiteral("proton-drive"));
    QProcessRunner runner(protonBinary);
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString manifestPath;
    QString error;

    if (!engine.backup(source, remote, provider, &manifestPath, &error)) {
        qCritical().noquote() << error;

        return 1;
    }

    qInfo().noquote() << manifestPath;

    return 0;
}
