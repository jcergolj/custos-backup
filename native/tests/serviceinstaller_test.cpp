#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "../src/serviceinstaller.h"

class ServiceInstallerTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesUserServiceWithSystemResourceDefaults();
    void rejectsNonExecutableWorker();
};

void ServiceInstallerTest::writesUserServiceWithSystemResourceDefaults()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString workerPath = directory.filePath(QStringLiteral("worker"));
    QFile worker(workerPath);
    QVERIFY(worker.open(QIODevice::WriteOnly));
    worker.write("#!/bin/sh\n");
    worker.close();
    QVERIFY(worker.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    ServiceInstaller installer(directory.filePath(QStringLiteral("systemd")));
    QString servicePath;
    QString error;
    QVERIFY(installer.install(workerPath, &servicePath, &error));
    QVERIFY2(QFileInfo::exists(servicePath), qPrintable(error));
    QVERIFY(QFileInfo::exists(directory.filePath(QStringLiteral("systemd/omacustos.timer"))));

    QFile service(servicePath);
    QVERIFY(service.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(service.readAll());
    QVERIFY(contents.contains(QStringLiteral("ExecStart=").append(workerPath)));
    QVERIFY(contents.contains(QStringLiteral("--config %h/.config/omacustos/omacustos-backup.json")));
    QVERIFY(!contents.contains(QStringLiteral("Nice=")));
    QVERIFY(!contents.contains(QStringLiteral("CPUQuota=")));
    QVERIFY(!contents.contains(QStringLiteral("IOSchedulingClass=")));
    QVERIFY(contents.contains(QStringLiteral("NoNewPrivileges=true")));

    QFile timer(directory.filePath(QStringLiteral("systemd/omacustos.timer")));
    QVERIFY(timer.open(QIODevice::ReadOnly));
    const QString timerContents = QString::fromUtf8(timer.readAll());
    QVERIFY(timerContents.contains(QStringLiteral("Persistent=true")));
    QVERIFY(timerContents.contains(QStringLiteral("Unit=omacustos.service")));
}

void ServiceInstallerTest::rejectsNonExecutableWorker()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ServiceInstaller installer(directory.path());
    QString error;

    QVERIFY(!installer.install(directory.filePath(QStringLiteral("missing-worker")), nullptr, &error));
    QCOMPARE(error, QStringLiteral("The OmaCustos worker must be an executable absolute path."));
}

QTEST_MAIN(ServiceInstallerTest)
#include "serviceinstaller_test.moc"
