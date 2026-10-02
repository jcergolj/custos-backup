#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <csignal>
#include <cstdio>

#include "../src/backupengine.h"
#include "../src/protonprovider.h"
#include "../src/qprocessrunner.h"

class QProcessRunnerTest final : public QObject
{
    Q_OBJECT

private slots:
    void progressingTransfersOutliveMetadataLimit_data();
    void progressingTransfersOutliveMetadataLimit();
    void metadataRemainsBounded();
    void stalledTransfersRemainBounded_data();
    void stalledTransfersRemainBounded();
    void distinguishesProcessFailures_data();
    void distinguishesProcessFailures();
    void transferTimeoutEnvironment_data();
    void transferTimeoutEnvironment();
    void timedOutBackupCannotBecomeSuccessful();
};

void QProcessRunnerTest::progressingTransfersOutliveMetadataLimit_data()
{
    QTest::addColumn<QString>("operation");
    QTest::newRow("upload") << QString("upload");
    QTest::newRow("download") << QString("download");
}

void QProcessRunnerTest::progressingTransfersOutliveMetadataLimit()
{
    QFETCH(QString, operation);
    // Scale the old five-minute boundary down to 200 ms. The fixture reports
    // progress for 600 ms; the injectable transfer limit allows it to finish.
    QProcessRunner runner(QCoreApplication::applicationFilePath(), {200, 5000});
    const auto output = runner.run({"filesystem", operation, "progress"});
    QVERIFY2(output.successful(), qPrintable(output.standardError));
    QCOMPARE(output.standardOutput, QString("progress\nprogress\nprogress\n"));
}

void QProcessRunnerTest::metadataRemainsBounded()
{
    QProcessRunner runner(QCoreApplication::applicationFilePath(), {200, 5000});
    const auto output = runner.run({"filesystem", "info", "stall"});
    QVERIFY(!output.successful());
    QVERIFY(output.standardError.contains("command timed out"));
    QVERIFY(output.standardError.contains("total runtime limit"));
    QVERIFY(output.standardError.contains("fixture waiting"));
}

void QProcessRunnerTest::stalledTransfersRemainBounded_data()
{
    QTest::addColumn<QString>("operation");
    QTest::addColumn<QString>("mode");
    QTest::newRow("silent upload") << QString("upload") << QString("stall");
    QTest::newRow("silent download") << QString("download") << QString("stall");
    QTest::newRow("output cannot bypass deadline") << QString("upload") << QString("progress");
}

void QProcessRunnerTest::stalledTransfersRemainBounded()
{
    QFETCH(QString, operation);
    QFETCH(QString, mode);
    QProcessRunner runner(QCoreApplication::applicationFilePath(), {5000, 200});
    const auto output = runner.run({"filesystem", operation, mode});
    QVERIFY(!output.successful());
    QCOMPARE(output.exitCode, -1);
    QVERIFY(output.standardError.contains("transfer timed out"));
    QVERIFY(output.standardError.contains("total runtime limit"));
}

void QProcessRunnerTest::distinguishesProcessFailures_data()
{
    QTest::addColumn<QString>("mode");
    QTest::addColumn<QString>("message");
    QTest::newRow("missing executable") << QString("missing") << QString("Unable to start");
    QTest::newRow("crash") << QString("crash") << QString("command crashed");
    QTest::newRow("nonzero exit") << QString("failure") << QString("exit code 7");
    QTest::newRow("cli error") << QString("cli-error") << QString("not authenticated");
}

void QProcessRunnerTest::distinguishesProcessFailures()
{
    QFETCH(QString, mode);
    QFETCH(QString, message);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString executable = mode == "missing" ? directory.filePath("missing") : QCoreApplication::applicationFilePath();
    QProcessRunner runner(executable, {5000, 5000});
    const auto output = runner.run({"filesystem", "info", mode});
    QVERIFY(!output.successful());
    QVERIFY2(output.standardError.contains(message), qPrintable(output.standardError));
    QVERIFY(!output.standardError.contains("timed out"));
}

void QProcessRunnerTest::transferTimeoutEnvironment_data()
{
    QTest::addColumn<QByteArray>("value");
    QTest::addColumn<int>("milliseconds");
    constexpr int defaultTimeout = 24 * 60 * 60 * 1000;
    QTest::newRow("default") << QByteArray() << defaultTimeout;
    QTest::newRow("custom") << QByteArray("7200") << 7200000;
    QTest::newRow("zero is not unbounded") << QByteArray("0") << defaultTimeout;
    QTest::newRow("negative") << QByteArray("-1") << defaultTimeout;
    QTest::newRow("invalid") << QByteArray("invalid") << defaultTimeout;
    QTest::newRow("overflow") << QByteArray("2147484") << defaultTimeout;
}

void QProcessRunnerTest::transferTimeoutEnvironment()
{
    QFETCH(QByteArray, value);
    QFETCH(int, milliseconds);
    const bool wasSet = qEnvironmentVariableIsSet("OMACUSTOS_TRANSFER_TIMEOUT_SECONDS");
    const QByteArray previous = qgetenv("OMACUSTOS_TRANSFER_TIMEOUT_SECONDS");
    qputenv("OMACUSTOS_TRANSFER_TIMEOUT_SECONDS", value);
    const auto timeouts = ProcessTimeouts::fromEnvironment();
    if (wasSet) {
        qputenv("OMACUSTOS_TRANSFER_TIMEOUT_SECONDS", previous);
    } else {
        qunsetenv("OMACUSTOS_TRANSFER_TIMEOUT_SECONDS");
    }
    QCOMPARE(timeouts.transferMilliseconds, milliseconds);
    QCOMPARE(timeouts.metadataMilliseconds, 5 * 60 * 1000);
}

void QProcessRunnerTest::timedOutBackupCannotBecomeSuccessful()
{
    QTemporaryDir source;
    QVERIFY(source.isValid());
    QFile file(source.filePath("notes.txt"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("important content");
    file.close();
    QProcessRunner runner(QCoreApplication::applicationFilePath(), {5000, 200});
    ProtonProvider provider(runner);
    BackupEngine engine;
    BackupResult result;
    QString error;
    QString manifest;
    const BackupCopyMetadata metadata {"computer", "set", "Documents", "copy", QDateTime::currentDateTimeUtc()};
    QVERIFY(!engine.backup({source.path()}, "/my-files/backups/computer/Documents/copy", {},
        metadata, provider, &manifest, &error, {}, &result));
    QVERIFY(error.contains("transfer timed out"));
    QVERIFY(manifest.isEmpty());
    QVERIFY(!result.manifestVerified);
    QCOMPARE(result.verifiedFiles, 0);
    QCOMPARE(result.issues.size(), 1);
    QCOMPARE(result.issues.first().phase, QString("uploading"));
    QVERIFY(result.issues.first().reason.contains("transfer timed out"));
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const auto arguments = application.arguments();
    if (arguments.value(1) == "filesystem") {
        const QString mode = arguments.value(3);
        if (mode == "crash") {
            std::raise(SIGKILL);
        }
        if (mode == "failure") return 7;
        if (mode == "cli-error") {
            std::fputs("not authenticated", stderr);
            return 1;
        }
        if (mode == "progress") {
            for (int step = 0; step < 3; ++step) {
                std::fputs("progress\n", stdout);
                std::fflush(stdout);
                QThread::msleep(200);
            }
            return 0;
        }
        if (arguments.value(2) == "list") {
            std::fputs("[]", stdout);
            return 0;
        }
        if (arguments.value(2) == "info" && mode != "stall") return 1;
        std::fputs("fixture waiting\n", stderr);
        std::fflush(stderr);
        QThread::msleep(10000);
        return 0;
    }
    QProcessRunnerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "qprocessrunner_test.moc"
