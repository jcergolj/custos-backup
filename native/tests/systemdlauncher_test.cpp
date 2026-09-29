#include <QTest>

#include "../src/systemdlauncher.h"

class FakeRunner final : public ProcessRunner
{
public:
    QStringList arguments;
    ProcessOutput response;

    ProcessOutput run(const QStringList &requestedArguments) override
    {
        arguments = requestedArguments;
        return response;
    }
};

class SystemdLauncherTest final : public QObject
{
    Q_OBJECT

private slots:
    void startsOnlyTheRequestedUserService();
    void rejectsInvalidServiceNames();
    void returnsSystemdErrors();
    void enablesTimerAfterReloadingManager();
};

void SystemdLauncherTest::startsOnlyTheRequestedUserService()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    SystemdLauncher launcher(runner);

    QVERIFY(launcher.startUserService(QStringLiteral("custos.service")));
    const QStringList expected {
        QStringLiteral("--user"), QStringLiteral("--no-block"), QStringLiteral("start"),
        QStringLiteral("custos.service"),
    };

    QCOMPARE(runner.arguments, expected);
}

void SystemdLauncherTest::rejectsInvalidServiceNames()
{
    FakeRunner runner;
    SystemdLauncher launcher(runner);
    QString error;

    QVERIFY(!launcher.startUserService(QStringLiteral("../other.service"), &error));
    QCOMPARE(error, QStringLiteral("The backup service name is invalid."));
    QVERIFY(runner.arguments.isEmpty());
}

void SystemdLauncherTest::returnsSystemdErrors()
{
    FakeRunner runner;
    runner.response.exitCode = 1;
    runner.response.standardError = QStringLiteral("unit is masked");
    SystemdLauncher launcher(runner);
    QString error;

    QVERIFY(!launcher.startUserService(QStringLiteral("custos.service"), &error));
    QCOMPARE(error, QStringLiteral("unit is masked"));
}

void SystemdLauncherTest::enablesTimerAfterReloadingManager()
{
    class SequenceRunner final : public ProcessRunner
    {
    public:
        QVector<QStringList> calls;
        ProcessOutput run(const QStringList &arguments) override
        {
            calls.append(arguments);
            return {0, {}, {}};
        }
    } runner;
    SystemdLauncher launcher(runner);

    QVERIFY(launcher.enableUserTimer(QStringLiteral("custos.timer")));
    QCOMPARE(runner.calls.size(), 2);
    const QStringList reloadArguments {QStringLiteral("--user"), QStringLiteral("daemon-reload")};
    QCOMPARE(runner.calls.at(0), reloadArguments);
    const QStringList enableArguments {
        QStringLiteral("--user"), QStringLiteral("enable"), QStringLiteral("--now"),
        QStringLiteral("custos.timer"),
    };
    QCOMPARE(runner.calls.at(1), enableArguments);
}

QTEST_MAIN(SystemdLauncherTest)
#include "systemdlauncher_test.moc"
