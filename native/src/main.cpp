#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "backupengine.h"
#include "qprocessrunner.h"
#include "systemdlauncher.h"

class BackupLauncher : public QObject
{
    Q_OBJECT

public:
    explicit BackupLauncher(QObject *parent = nullptr)
        : QObject(parent)
        , runner(QStringLiteral("systemctl"))
        , launcher(runner)
    {
    }

public slots:
    void startBackup()
    {
        QString error;
        if (!launcher.startUserService(QStringLiteral("praefectus-native.service"), &error)) {
            emit failed(error);

            return;
        }

        emit started();
    }

signals:
    void started();
    void failed(const QString &error);

private:
    QProcessRunner runner;
    SystemdLauncher launcher;
};

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
