#include <QSignalSpy>
#include <QTest>

#include "../src/backupjob.h"
#include "../src/localprovider.h"

class BackupJobTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsVerifiedSuccess();
    void reportsFailureWithoutSuccess();
};

void BackupJobTest::reportsVerifiedSuccess()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());

    QFile file(source.filePath(QStringLiteral("file.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("content");
    file.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    BackupJob job(engine);
    QSignalSpy started(&job, &BackupJob::started);
    QSignalSpy progress(&job, &BackupJob::progressChanged);
    QSignalSpy succeeded(&job, &BackupJob::succeeded);

    job.run(source.path(), QStringLiteral("copy"), provider);

    QCOMPARE(started.count(), 1);
    QCOMPARE(progress.count(), 2);
    QCOMPARE(succeeded.count(), 1);
    QVERIFY(QFileInfo::exists(succeeded.at(0).at(0).toString()));
}

void BackupJobTest::reportsFailureWithoutSuccess()
{
    BackupEngine engine;
    LocalProvider provider(QStringLiteral("/tmp/praefectus-no-provider"));
    BackupJob job(engine);
    QSignalSpy failed(&job, &BackupJob::failed);
    QSignalSpy succeeded(&job, &BackupJob::succeeded);

    job.run(QStringLiteral("/tmp/praefectus-does-not-exist"), QStringLiteral("copy"), provider);

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("The selected folder contains no regular files."));
    QCOMPARE(succeeded.count(), 0);
}

QTEST_MAIN(BackupJobTest)
#include "backupjob_test.moc"
