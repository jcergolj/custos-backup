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
};

void SystemdLauncherTest::startsOnlyTheRequestedUserService()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    SystemdLauncher launcher(runner);

    QVERIFY(launcher.startUserService(QStringLiteral("praefectus-native.service")));
    const QStringList expected {
        QStringLiteral("--user"), QStringLiteral("--no-block"), QStringLiteral("start"),
        QStringLiteral("praefectus-native.service"),
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

    QVERIFY(!launcher.startUserService(QStringLiteral("praefectus-native.service"), &error));
    QCOMPARE(error, QStringLiteral("unit is masked"));
}

QTEST_MAIN(SystemdLauncherTest)
#include "systemdlauncher_test.moc"
