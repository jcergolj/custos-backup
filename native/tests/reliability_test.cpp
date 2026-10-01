#include <QTemporaryDir>
#include <QTest>

#include "../src/backupprerequisites.h"
#include "../src/backuprunstore.h"
#include "../src/backupschedule.h"

class FakePrerequisiteProbe final : public BackupPrerequisiteProbe
{
public:
    bool ac = true;

    bool onAcPower() const override { return ac; }
};

class ReliabilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void monthlySchedulesUseTheLastDay();
    void runStoreCoalescesAndPersistsRetries();
    void prerequisitesGateAcPowerOnlyWhenRequired();
};

void ReliabilityTest::monthlySchedulesUseTheLastDay()
{
    const BackupSchedule schedule {
        QStringLiteral("monthly"),
        9,
        30,
        1,
        31,
    };
    const QDateTime after(QDate(2026, 1, 31), QTime(10, 0));
    const QDateTime next = BackupScheduleCalculator::nextRun(schedule, after);

    QCOMPARE(next.date(), QDate(2026, 2, 28));
    QCOMPARE(next.time(), QTime(9, 30));
}

void ReliabilityTest::runStoreCoalescesAndPersistsRetries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("runs.json"));
    const QDateTime now(QDate(2026, 1, 1), QTime(12, 0));
    BackupRunStore store(path);
    QVERIFY(store.load());
    QVERIFY(store.enqueue(QStringLiteral("set-a"), QStringLiteral("schedule"), now));
    QVERIFY(!store.enqueue(QStringLiteral("set-a"), QStringLiteral("schedule"), now));
    QCOMPARE(store.readyIndexes(now).size(), 1);

    BackupRunRecord *record = store.find(QStringLiteral("set-a"));
    QVERIFY(record != nullptr);
    store.markRunning(*record);
    store.markRetrying(*record, QStringLiteral("offline"), now);
    QVERIFY(store.save());

    BackupRunStore restored(path);
    QVERIFY(restored.load());
    const BackupRunRecord *restoredRecord = restored.find(QStringLiteral("set-a"));
    QVERIFY(restoredRecord != nullptr);
    QCOMPARE(restoredRecord->status, QStringLiteral("retrying"));
    QCOMPARE(restoredRecord->attempts, 1);
    QCOMPARE(restoredRecord->nextAttempt, now.addSecs(5));
}

void ReliabilityTest::prerequisitesGateAcPowerOnlyWhenRequired()
{
    BackupSet set;
    set.onlyOnAcPower = true;
    FakePrerequisiteProbe probe;

    probe.ac = false;
    BackupPrerequisiteResult result = BackupPrerequisites::check(set, probe);
    QVERIFY(!result.ready);
    QCOMPARE(result.reason, QStringLiteral("Waiting for AC power."));

    probe.ac = true;
    QVERIFY(BackupPrerequisites::check(set, probe).ready);

    probe.ac = false;
    set.onlyOnAcPower = false;
    QVERIFY(BackupPrerequisites::check(set, probe).ready);
}

QTEST_MAIN(ReliabilityTest)
#include "reliability_test.moc"
