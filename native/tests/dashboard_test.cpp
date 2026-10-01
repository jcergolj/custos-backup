#include <QGuiApplication>
#include <QPalette>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQuickTest>

class DashboardSetup final : public QObject
{
    Q_OBJECT

public:
    DashboardSetup()
    {
        if (!home.isValid()) {
            qFatal("Cannot create an isolated home for dashboard tests.");
        }
        qputenv("HOME", home.path().toUtf8());
        qputenv("XDG_CONFIG_HOME", home.filePath("config").toUtf8());
        qputenv("XDG_CACHE_HOME", home.filePath("cache").toUtf8());
        QStandardPaths::setTestModeEnabled(true);
    }

public slots:
    void applicationAvailable()
    {
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor("#12161c"));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        QGuiApplication::setPalette(darkPalette);
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        QQmlComponent component(engine, QUrl::fromLocalFile(QStringLiteral(QUICK_TEST_SOURCE_DIR "/DashboardControllers.qml")));
        QObject *controllers = component.create();
        if (!controllers) {
            qFatal("Cannot load dashboard controller doubles: %s", qPrintable(component.errorString()));
        }
        controllers->setParent(engine);
        for (const char *name : {"backupSetController", "backupLauncher", "restoreController"}) {
            engine->rootContext()->setContextProperty(QString::fromLatin1(name), controllers->property(name).value<QObject *>());
        }
        engine->rootContext()->setContextProperty(QStringLiteral("dashboardScreenshotPath"), qEnvironmentVariable("CUSTOS_TEST_SCREENSHOT"));
    }

private:
    QTemporaryDir home;
};

QUICK_TEST_MAIN_WITH_SETUP(dashboard, DashboardSetup)
#include "dashboard_test.moc"
