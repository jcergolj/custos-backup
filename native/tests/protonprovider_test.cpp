#include <QTest>
#include <QCryptographicHash>

#include "../src/protonprovider.h"

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

class ProtonProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void uploadUsesJsonCliArguments();
    void inspectParsesVerifiedMetadata();
    void inspectParsesCliMetadataWithoutSha256();
    void inspectUsesContentSizeInsteadOfEncryptedStorageSize();
    void inspectRejectsStorageSizeWithoutContentSize();
    void successfulInspectClearsEarlierErrors();
    void commandErrorsAreActionable();
    void rejectsNullMetadataOutput();
    void listsRemoteItemsAndUsesExactCleanupCommands();
};

void ProtonProviderTest::uploadUsesJsonCliArguments()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    ProtonProvider provider(runner);

    QVERIFY(provider.upload(QStringLiteral("/tmp/file.txt"), QStringLiteral("/backups/copy")));
    const QStringList expected {
        QStringLiteral("filesystem"), QStringLiteral("upload"), QStringLiteral("-j"),
        QStringLiteral("-f"), QStringLiteral("replace"), QStringLiteral("-d"), QStringLiteral("replace"),
        QStringLiteral("-t"), QStringLiteral("/tmp/file.txt"), QStringLiteral("/backups"),
    };

    QCOMPARE(runner.arguments, expected);
}

void ProtonProviderTest::inspectParsesVerifiedMetadata()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    runner.response.standardOutput = QStringLiteral(R"({"size":17,"sha256":"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"})");
    ProtonProvider provider(runner);
    RemoteFile file;

    QVERIFY(provider.inspect(QStringLiteral("/backups/file.txt"), &file));
    QCOMPARE(file.path, QStringLiteral("/backups/file.txt"));
    QCOMPARE(file.size, qint64(17));
    QCOMPARE(file.checksum, QByteArray::fromHex("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"));
}

void ProtonProviderTest::inspectParsesCliMetadataWithoutSha256()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    runner.response.standardOutput = QStringLiteral(R"({"type":"file","totalStorageSize":17,"activeRevision":{"claimedSize":17}})");
    ProtonProvider provider(runner);
    RemoteFile file;

    QVERIFY(provider.inspect(QStringLiteral("/backups/file.txt"), &file));
    QCOMPARE(file.size, qint64(17));
    QVERIFY(file.checksum.isEmpty());
}

void ProtonProviderTest::inspectUsesContentSizeInsteadOfEncryptedStorageSize()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    runner.response.standardOutput = QStringLiteral(R"({"type":"file","totalStorageSize":463208,"activeRevision":{"storageSize":463208,"claimedSize":463105}})");
    ProtonProvider provider(runner);
    RemoteFile file;

    QVERIFY(provider.inspect(QStringLiteral("/backups/file.pdf"), &file));
    QCOMPARE(file.size, qint64(463105));
}

void ProtonProviderTest::inspectRejectsStorageSizeWithoutContentSize()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    runner.response.standardOutput = QStringLiteral(R"({"type":"file","totalStorageSize":513,"activeRevision":{"storageSize":513}})");
    ProtonProvider provider(runner);
    RemoteFile file;
    QString error;

    QVERIFY(!provider.inspect(QStringLiteral("/backups/manifest.json"), &file, &error));
    QCOMPARE(error, QStringLiteral("Proton Drive returned invalid file metadata."));
}

void ProtonProviderTest::successfulInspectClearsEarlierErrors()
{
    FakeRunner runner;
    runner.response.exitCode = 1;
    runner.response.standardError = QStringLiteral("Node not found: file.txt");
    ProtonProvider provider(runner);
    RemoteFile file;
    QString error;

    QVERIFY(!provider.inspect(QStringLiteral("/backups/file.txt"), &file, &error));
    QCOMPARE(error, QStringLiteral("Node not found: file.txt"));

    runner.response = {0, QStringLiteral(R"({"activeRevision":{"claimedSize":17}})"), {}};
    QVERIFY(provider.inspect(QStringLiteral("/backups/file.txt"), &file, &error));
    QVERIFY(error.isEmpty());
}

void ProtonProviderTest::commandErrorsAreActionable()
{
    FakeRunner runner;
    runner.response.exitCode = 1;
    runner.response.standardError = QStringLiteral("not authenticated");
    ProtonProvider provider(runner);
    QString error;

    QVERIFY(!provider.download(QStringLiteral("/remote/file"), QStringLiteral("/tmp/file"), &error));
    QCOMPARE(error, QStringLiteral("not authenticated"));
}

void ProtonProviderTest::rejectsNullMetadataOutput()
{
    FakeRunner runner;
    ProtonProvider provider(runner);
    QString error;

    QVERIFY(!provider.inspect(QStringLiteral("/remote/file"), nullptr, &error));
    QCOMPARE(error, QStringLiteral("A destination for remote file metadata is required."));
    QVERIFY(runner.arguments.isEmpty());
}

void ProtonProviderTest::listsRemoteItemsAndUsesExactCleanupCommands()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    runner.response.standardOutput = QStringLiteral(R"([{"name":{"ok":true,"value":"computer"},"type":"folder"},{"name":{"ok":true,"value":"notes.txt"},"type":"file","totalStorageSize":5,"modificationTime":"2026-09-28T12:00:00.000Z"}])");
    ProtonProvider provider(runner);
    QVector<RemoteItem> items;
    QVERIFY(provider.list(QStringLiteral("/my-files/backups"), &items));
    QCOMPARE(items.size(), 2);
    QVERIFY(items.first().directory);
    QCOMPARE(items.last().path, QStringLiteral("/my-files/backups/notes.txt"));

    QVERIFY(provider.trash(QStringLiteral("/my-files/backups/computer/set/copy")));
    const QStringList trashArguments {
        QStringLiteral("filesystem"), QStringLiteral("trash"), QStringLiteral("/my-files/backups/computer/set/copy"),
    };
    QCOMPARE(runner.arguments, trashArguments);
    QVERIFY(provider.permanentlyDelete(QStringLiteral("/my-files/backups/computer/set/copy")));
    const QStringList deleteArguments {
        QStringLiteral("filesystem"), QStringLiteral("delete"), QStringLiteral("/my-files/backups/computer/set/copy"),
    };
    QCOMPARE(runner.arguments, deleteArguments);
}

QTEST_MAIN(ProtonProviderTest)
#include "protonprovider_test.moc"
