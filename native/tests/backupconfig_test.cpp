#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "../src/backupconfig.h"

class BackupConfigTest final : public QObject
{
    Q_OBJECT

private slots:
    void savesAndLoadsConfiguration();
    void rejectsMalformedConfiguration();
    void rejectsIncompleteConfiguration();
    void rejectsNullOutput();
};

void BackupConfigTest::savesAndLoadsConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("config/settings.json")));
    const BackupConfig expected {
        QStringLiteral("/home/user/Documents"),
        QStringLiteral("/my-files/backups/computer/copy"),
        QStringLiteral("/usr/bin/proton-drive"),
    };

    QVERIFY(store.save(expected));
    BackupConfig actual;
    QVERIFY(store.load(&actual));
    QCOMPARE(actual.sourceDirectory, expected.sourceDirectory);
    QCOMPARE(actual.remoteRoot, expected.remoteRoot);
    QCOMPARE(actual.protonBinary, expected.protonBinary);
}

void BackupConfigTest::rejectsMalformedConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not-json");
    file.close();

    BackupConfigStore store(path);
    BackupConfig config;
    QString error;
    QVERIFY(!store.load(&config, &error));
    QCOMPARE(error, QStringLiteral("The native backup configuration is malformed."));
}

void BackupConfigTest::rejectsIncompleteConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("settings.json")));
    BackupConfig config;
    QString error;

    QVERIFY(!store.save(config, &error));
    QCOMPARE(error, QStringLiteral("The native backup configuration is incomplete."));
}

void BackupConfigTest::rejectsNullOutput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("settings.json")));
    QString error;

    QVERIFY(!store.load(nullptr, &error));
    QCOMPARE(error, QStringLiteral("A destination for native backup configuration is required."));
}

QTEST_MAIN(BackupConfigTest)
#include "backupconfig_test.moc"
