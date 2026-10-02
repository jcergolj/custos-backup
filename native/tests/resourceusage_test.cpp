#include "../src/resourceusage.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ResourceUsageTest final : public QObject
{
    Q_OBJECT

    static bool script(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size()
            && file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }

private slots:
    void systemDefaultsAreUsedUnlessAPresetIsExplicitlySelected()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        ResourceUsage resources(home.path(), home.filePath("missing"));
        QSignalSpy errors(&resources, &ResourceUsage::failed);
        QCOMPARE(resources.presetIndex(), -1);
        resources.save(-1);
        QVERIFY(!resources.busy());
        QVERIFY(errors.isEmpty());
        QVERIFY(!QFile::exists(home.filePath("custos.service.d/50-custos-resources.conf")));
    }

    void returningToSystemDefaultsRemovesLimitsAndPersistsTheChoice()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString systemctl = home.filePath("systemctl");
        QVERIFY(script(systemctl, "#!/bin/sh\nexit 0\n"));
        ResourceUsage resources(home.path(), systemctl);
        resources.save(0);
        QTRY_VERIFY(!resources.busy());
        QCOMPARE(resources.presetIndex(), 0);
        resources.save(-1);
        QTRY_VERIFY(!resources.busy());
        QCOMPARE(resources.presetIndex(), -1);
        QFile dropIn(home.filePath("custos.service.d/50-custos-resources.conf"));
        QVERIFY(dropIn.open(QIODevice::ReadOnly));
        const QByteArray contents = dropIn.readAll();
        QVERIFY(contents.contains("CPUQuota=\n"));
        QVERIFY(contents.contains("Nice=0\n"));
        QVERIFY(contents.contains("IOSchedulingClass=none\n"));
        QVERIFY(contents.contains("IOSchedulingPriority=0\n"));
        ResourceUsage reopened(home.path(), systemctl);
        QCOMPARE(reopened.presetIndex(), -1);
    }

    void savesAndReloadsEachPreset_data()
    {
        QTest::addColumn<int>("index");
        QTest::addColumn<int>("quota");
        QTest::addColumn<int>("nice");
        QTest::addColumn<QByteArray>("ioClass");
        QTest::newRow("very low") << 0 << 10 << 19 << QByteArray("idle");
        QTest::newRow("low") << 1 << 25 << 15 << QByteArray("idle");
        QTest::newRow("medium") << 2 << 50 << 10 << QByteArray("best-effort");
        QTest::newRow("high") << 3 << 100 << 5 << QByteArray("best-effort");
        QTest::newRow("very high") << 4 << 200 << 0 << QByteArray("best-effort");
    }

    void savesAndReloadsEachPreset()
    {
        QFETCH(int, index);
        QFETCH(int, quota);
        QFETCH(int, nice);
        QFETCH(QByteArray, ioClass);
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString systemctl = home.filePath("fake systemctl");
        QVERIFY(script(systemctl, "#!/bin/sh\n[ \"$*\" = '--user daemon-reload' ] || exit 2\nexit 0\n"));
        ResourceUsage resources(home.path(), systemctl);
        QCOMPARE(resources.names(), QStringList({"Very low", "Low", "Medium", "High", "Very high"}));
        QCOMPARE(resources.presetIndex(), -1);
        QSignalSpy errors(&resources, &ResourceUsage::failed);
        // Exercise changing between explicit profiles as well as opting in.
        resources.save(index == 4 ? 3 : 4);
        QTRY_VERIFY(!resources.busy());
        resources.save(index);
        QTRY_VERIFY(!resources.busy());
        QVERIFY(errors.isEmpty());
        QCOMPARE(resources.presetIndex(), index);
        QFile dropIn(home.filePath("custos.service.d/50-custos-resources.conf"));
        QVERIFY(dropIn.open(QIODevice::ReadOnly));
        const QByteArray contents = dropIn.readAll();
        QVERIFY(contents.contains("CPUQuota=" + QByteArray::number(quota) + "%\n"));
        QVERIFY(contents.contains("Nice=" + QByteArray::number(nice) + "\n"));
        QVERIFY(contents.contains("IOSchedulingClass=" + ioClass + "\n"));
        QVERIFY(!contents.contains("ExecStart") && !contents.contains("[Timer]"));
        ResourceUsage reopened(home.path(), systemctl);
        QCOMPARE(reopened.presetIndex(), index);
        // The unchanged preset needs no manager reload.
        reopened.save(index);
        QVERIFY(!reopened.busy());
    }

    void failedReloadRestoresPreviousSettings_data()
    {
        QTest::addColumn<bool>("existing");
        QTest::addColumn<bool>("missingSystemctl");
        QTest::addColumn<int>("targetIndex");
        QTest::newRow("new file") << false << false << 4;
        QTest::newRow("existing file") << true << false << 4;
        QTest::newRow("missing systemctl") << true << true << 4;
        QTest::newRow("return to system defaults") << true << false << -1;
    }

    void failedReloadRestoresPreviousSettings()
    {
        QFETCH(bool, existing);
        QFETCH(bool, missingSystemctl);
        QFETCH(int, targetIndex);
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString systemctl = home.filePath("systemctl");
        const QString dropInPath = home.filePath("custos.service.d/50-custos-resources.conf");
        QByteArray previous;
        if (existing) {
            QVERIFY(script(systemctl, "#!/bin/sh\nexit 0\n"));
            ResourceUsage initial(home.path(), systemctl);
            initial.save(1);
            QTRY_VERIFY(!initial.busy());
            QCOMPARE(initial.presetIndex(), 1);
            QFile file(dropInPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            previous = file.readAll();
        }
        QVERIFY(script(systemctl, "#!/bin/sh\nprintf 'Manager unavailable' >&2\nexit 1\n"));
        ResourceUsage resources(home.path(), missingSystemctl ? home.filePath("missing") : systemctl);
        const int original = resources.presetIndex();
        QSignalSpy errors(&resources, &ResourceUsage::failed);
        resources.save(targetIndex);
        QTRY_VERIFY(!resources.busy());
        QCOMPARE(resources.presetIndex(), original);
        QCOMPARE(errors.count(), 1);
        if (existing) {
            QFile file(dropInPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), previous);
        } else {
            QVERIFY(!QFile::exists(dropInPath));
        }
    }

    void invalidIndexDoesNotWriteSettings()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        ResourceUsage resources(home.path(), home.filePath("missing"));
        QSignalSpy errors(&resources, &ResourceUsage::failed);
        resources.save(-2);
        resources.save(5);
        QCOMPARE(errors.count(), 2);
        QCOMPARE(resources.presetIndex(), -1);
        QVERIFY(!QFile::exists(home.filePath("custos.service.d/50-custos-resources.conf")));
    }
};

QTEST_GUILESS_MAIN(ResourceUsageTest)
#include "resourceusage_test.moc"
