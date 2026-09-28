#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include "../src/backupworker.h"
#include "../src/localprovider.h"

class BackupWorkerTest final : public QObject
{
    Q_OBJECT

private slots:
    void reportsSuccessFromAWorkerThread();
    void rejectsMissingProvider();
};

void BackupWorkerTest::reportsSuccessFromAWorkerThread()
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
    BackupWorker worker(engine);
    QThread thread;
    worker.moveToThread(&thread);
    QSignalSpy succeeded(&worker, &BackupWorker::succeeded);
    QSignalSpy failed(&worker, &BackupWorker::failed);
    thread.start();

    QMetaObject::invokeMethod(
        &worker,
        "run",
        Qt::QueuedConnection,
        Q_ARG(QString, source.path()),
        Q_ARG(QString, QStringLiteral("copy")),
        Q_ARG(BackupProvider *, static_cast<BackupProvider *>(&provider))
    );

    QTRY_COMPARE_WITH_TIMEOUT(succeeded.count(), 1, 2000);
    QCOMPARE(failed.count(), 0);
    thread.quit();
    QVERIFY(thread.wait(2000));
}

void BackupWorkerTest::rejectsMissingProvider()
{
    BackupEngine engine;
    BackupWorker worker(engine);
    QSignalSpy failed(&worker, &BackupWorker::failed);

    worker.run(QStringLiteral("/tmp/source"), QStringLiteral("copy"), nullptr);

    QCOMPARE(failed.count(), 1);
    QCOMPARE(failed.at(0).at(0).toString(), QStringLiteral("No backup provider is configured."));
}

QTEST_MAIN(BackupWorkerTest)
#include "backupworker_test.moc"
