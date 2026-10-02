#include <QTemporaryDir>
#include <QTest>
#include <QFile>

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
    void remainingTimeCountsDownAndHandlesIncompleteProgress();
    void progressPersistsAndResetsForANewAttempt();
    void olderRunRecordsHaveNoMadeUpEstimate();
    void previousSuccessProvidesAnInitialSingleFileEstimate();
};

void ReliabilityTest::previousSuccessProvidesAnInitialSingleFileEstimate()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    BackupRunStore store(home.filePath("runs.json"));
    store.ensureSet("documents");
    auto &record = *store.find("documents");
    const QDateTime now(QDate(2026, 10, 2), QTime(12, 0));
    store.markRunning(record);
    record.progress = {1000, 1000, 1, 1, true};
    record.progressElapsedMs = 20000;
    store.markSuccess(record, now);
    QVERIFY(store.save());
    BackupRunStore reopened(home.filePath("runs.json"));
    QVERIFY(reopened.load());
    QVERIFY(reopened.enqueue("documents", "manual", now));
    auto &next = *reopened.find("documents");
    reopened.markRunning(next);
    next.progress = {2000, 0, 1, 0, false};
    next.progressUpdatedAt = now;
    QCOMPARE(next.estimatedRemainingSeconds(now), qint64(40));
    QCOMPARE(next.estimatedRemainingSeconds(now.addSecs(5)), qint64(35));
    next.progress = {2000, 1000, 2, 1, false};
    next.progressElapsedMs = 10000;
    QCOMPARE(next.estimatedRemainingSeconds(now), qint64(10));
}

void ReliabilityTest::remainingTimeCountsDownAndHandlesIncompleteProgress()
{
    const QDateTime now(QDate(2026, 10, 2), QTime(12, 0));
    BackupRunRecord record;
    record.status = "running";
    record.progress = {1000, 200, 10, 2, false};
    record.progressElapsedMs = 20000;
    record.progressUpdatedAt = now;
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(80));
    QCOMPARE(record.estimatedRemainingSeconds(now.addSecs(5)), qint64(75));
    QCOMPARE(record.estimatedRemainingSeconds(now.addSecs(100)), qint64(0));
    QCOMPARE(record.estimatedRemainingSeconds(now.addSecs(-1)), qint64(-1));
    record.progress.totalBytes = 10000;
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(980));
    record.progress = {0, 0, 3, 1, false};
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(40));
    record.progress.totalBytes = 100;
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(-1));
    record.progress = {1000, 200, 10, 0, false};
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(-1));
    record.progress = {1000, 1000, 10, 10, true};
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(-1));
    record.progress = {1000, 200, 10, 2, false};
    record.status = "success";
    QCOMPARE(record.estimatedRemainingSeconds(now), qint64(-1));
}

void ReliabilityTest::progressPersistsAndResetsForANewAttempt()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    BackupRunStore store(home.filePath("runs.json"));
    store.ensureSet("documents");
    auto &record = *store.find("documents");
    store.markRunning(record);
    record.progress = {1000, 200, 10, 2, false};
    record.progressElapsedMs = 20000;
    record.progressUpdatedAt = QDateTime::currentDateTimeUtc();
    QVERIFY(store.save());
    BackupRunStore reopened(home.filePath("runs.json"));
    QVERIFY(reopened.load());
    auto &restored = *reopened.find("documents");
    QCOMPARE(restored.progress.totalBytes, qint64(1000));
    QCOMPARE(restored.progress.processedBytes, qint64(200));
    QCOMPARE(restored.progress.totalFiles, 10);
    QCOMPARE(restored.progress.processedFiles, 2);
    QCOMPARE(restored.progressUpdatedAt, record.progressUpdatedAt);
    QCOMPARE(restored.estimatedRemainingSeconds(record.progressUpdatedAt), qint64(80));
    reopened.markRunning(restored);
    QCOMPARE(restored.progress.totalFiles, 0);
    QCOMPARE(restored.progressElapsedMs, qint64(0));
    QVERIFY(!restored.progressUpdatedAt.isValid());
}

void ReliabilityTest::olderRunRecordsHaveNoMadeUpEstimate()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QFile file(home.filePath("runs.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"runs":[{"set_id":"documents","status":"running"}]})");
    file.close();
    BackupRunStore store(file.fileName());
    QVERIFY(store.load());
    const auto *record = store.find("documents");
    QVERIFY(record != nullptr);
    QCOMPARE(record->estimatedRemainingSeconds(QDateTime::currentDateTimeUtc()), qint64(-1));
}

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
