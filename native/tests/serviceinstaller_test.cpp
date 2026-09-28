#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "../src/serviceinstaller.h"

class ServiceInstallerTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesUserServiceWithWorkerLimits();
    void rejectsNonExecutableWorker();
};

void ServiceInstallerTest::writesUserServiceWithWorkerLimits()
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

    QFile service(servicePath);
    QVERIFY(service.open(QIODevice::ReadOnly));
    const QString contents = QString::fromUtf8(service.readAll());
    QVERIFY(contents.contains(QStringLiteral("ExecStart=").append(workerPath)));
    QVERIFY(contents.contains(QStringLiteral("Nice=19")));
    QVERIFY(contents.contains(QStringLiteral("CPUQuota=10%")));
    QVERIFY(contents.contains(QStringLiteral("NoNewPrivileges=true")));
}

void ServiceInstallerTest::rejectsNonExecutableWorker()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    ServiceInstaller installer(directory.path());
    QString error;

    QVERIFY(!installer.install(directory.filePath(QStringLiteral("missing-worker")), nullptr, &error));
    QCOMPARE(error, QStringLiteral("The native worker must be an executable absolute path."));
}

QTEST_MAIN(ServiceInstallerTest)
#include "serviceinstaller_test.moc"
