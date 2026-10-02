#include <QTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>
#include <QTemporaryDir>

#include "../src/backupengine.h"
#include "../src/backupmanifest.h"
#include "../src/protonprovider.h"
#include "protonclifixture.h"

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
    void uploadsAtExactRequestedPath_data();
    void uploadsAtExactRequestedPath();
    void rejectsInvalidUploadPaths_data();
    void rejectsInvalidUploadPaths();
    void stagingFailureDoesNotUpload();
    void uploadFailurePreservesFilesAndCleansStaging();
    void backsUpAndRestoresReservedAndCollisionNames();
    void failedPayloadsAreNotVerified_data();
    void failedPayloadsAreNotVerified();
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

    QVERIFY(provider.upload(QStringLiteral("/tmp/file.txt"), QStringLiteral("/backups/file.txt")));
    const QStringList expected {
        QStringLiteral("filesystem"), QStringLiteral("upload"), QStringLiteral("-j"),
        QStringLiteral("-f"), QStringLiteral("replace"), QStringLiteral("-d"), QStringLiteral("replace"),
        QStringLiteral("-t"), QStringLiteral("/tmp/file.txt"), QStringLiteral("/backups"),
    };

    QCOMPARE(runner.arguments, expected);
}

void ProtonProviderTest::uploadsAtExactRequestedPath_data()
{
    QTest::addColumn<QString>("remotePath");
    QTest::newRow("unchanged basename") << QStringLiteral("/backups/source.txt");
    QTest::newRow("renamed basename") << QStringLiteral("/backups/renamed.txt");
    QTest::newRow("nested folder and spaces") << QStringLiteral("/backups/nested folder/renamed file.txt");
    QTest::newRow("root destination") << QStringLiteral("/renamed.txt");
}

void ProtonProviderTest::uploadsAtExactRequestedPath()
{
    QFETCH(QString, remotePath);
    QTemporaryDir source;
    FilesystemRunner runner;
    QVERIFY(source.isValid() && runner.remote.isValid());
    QFile file(source.filePath(QStringLiteral("source.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    const QByteArray contents("payload\0with binary bytes", 25);
    QCOMPARE(file.write(contents), contents.size());
    file.close();
    QVERIFY(QDir().mkpath(QFileInfo(runner.remoteFile(remotePath)).path()));
    ProtonProvider provider(runner);
    QString error;
    QVERIFY2(provider.upload(file.fileName(), remotePath, &error), qPrintable(error));

    QFile uploaded(runner.remoteFile(remotePath));
    QVERIFY(uploaded.open(QIODevice::ReadOnly));
    QCOMPARE(uploaded.readAll(), contents);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), contents);
    QCOMPARE(runner.uploadedPaths.size(), 1);
    const QString cliPath = runner.uploadedPaths.first();
    QCOMPARE(QFileInfo(cliPath).fileName(), QFileInfo(remotePath).fileName());
    if (remotePath.endsWith(QStringLiteral("/source.txt"))) {
        QCOMPARE(cliPath, file.fileName());
    } else {
        QVERIFY(!QFileInfo::exists(QFileInfo(cliPath).path()));
        QVERIFY(!QFileInfo::exists(QDir(QFileInfo(uploaded.fileName()).path()).filePath("source.txt")));
    }
}

void ProtonProviderTest::rejectsInvalidUploadPaths_data()
{
    QTest::addColumn<QString>("remotePath");
    QTest::newRow("empty") << QString();
    QTest::newRow("relative") << QStringLiteral("backups/file.txt");
    QTest::newRow("root") << QStringLiteral("/");
    QTest::newRow("folder") << QStringLiteral("/backups/");
    QTest::newRow("dot") << QStringLiteral("/backups/.");
    QTest::newRow("parent") << QStringLiteral("/backups/../file.txt");
}

void ProtonProviderTest::rejectsInvalidUploadPaths()
{
    QFETCH(QString, remotePath);
    FakeRunner runner;
    ProtonProvider provider(runner);
    QString error;
    QVERIFY(!provider.upload(QStringLiteral("/tmp/source.txt"), remotePath, &error));
    QCOMPARE(error, QStringLiteral("The provider upload path is invalid."));
    QVERIFY(runner.arguments.isEmpty());
}

void ProtonProviderTest::stagingFailureDoesNotUpload()
{
    QTemporaryDir source;
    QVERIFY(source.isValid());
    FakeRunner runner;
    ProtonProvider provider(runner);
    QString error;
    QVERIFY(!provider.upload(source.filePath("missing.txt"), QStringLiteral("/backups/renamed.txt"), &error));
    QVERIFY(error.startsWith(QStringLiteral("The file could not be staged for Proton Drive upload:")));
    QVERIFY(runner.arguments.isEmpty());
}

void ProtonProviderTest::uploadFailurePreservesFilesAndCleansStaging()
{
    QTemporaryDir source;
    FilesystemRunner runner;
    QVERIFY(source.isValid() && runner.remote.isValid());
    QFile file(source.filePath("manifest.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("source payload");
    file.close();
    QVERIFY(QDir().mkpath(runner.remoteFile("/backups")));
    QFile existing(runner.remoteFile("/backups/manifest.json"));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("existing application manifest");
    existing.close();
    runner.failUploadName = QStringLiteral("manifest.json.1");
    ProtonProvider provider(runner);
    QString error;
    QVERIFY(!provider.upload(file.fileName(), QStringLiteral("/backups/manifest.json.1"), &error));
    QCOMPARE(error, QStringLiteral("Connection interrupted"));
    QCOMPARE(runner.uploadedPaths.size(), 1);
    QVERIFY(!QFileInfo::exists(QFileInfo(runner.uploadedPaths.first()).path()));
    QVERIFY(!QFileInfo::exists(runner.remoteFile("/backups/manifest.json.1")));
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("existing application manifest"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("source payload"));
}

void ProtonProviderTest::backsUpAndRestoresReservedAndCollisionNames()
{
    QTemporaryDir source;
    QTemporaryDir restored;
    FilesystemRunner runner;
    QVERIFY(source.isValid() && restored.isValid() && runner.remote.isValid());
    const QStringList names {"manifest.json", "manifest.json.1", "manifest.json.1.1"};
    for (const QString &name : names) {
        QFile file(source.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray contents = (QStringLiteral("payload for ") + name).toUtf8();
        QCOMPARE(file.write(contents), contents.size());
    }
    BackupEngine engine;
    ProtonProvider provider(runner);
    QString manifest;
    QString error;
    QVERIFY2(engine.backup(source.path(), QStringLiteral("/backups/copy"), provider, &manifest, &error), qPrintable(error));
    const auto cleanupManifest = qScopeGuard([&] { QDir(QFileInfo(manifest).path()).removeRecursively(); });
    QVector<BackupEntry> entries;
    QVERIFY2(BackupManifest::load(manifest, &entries, &error), qPrintable(error));
    QCOMPARE(entries.size(), names.size());
    QFile applicationManifest(runner.remoteFile("/backups/copy/manifest.json"));
    QVERIFY(applicationManifest.open(QIODevice::ReadOnly));
    QVERIFY(QJsonDocument::fromJson(applicationManifest.readAll()).isObject());
    for (qsizetype index = 0; index < names.size(); ++index) {
        const QString name = names.at(index);
        const BackupEntry entry = entries.at(index);
        QCOMPARE(entry.sourcePath, source.filePath(name));
        QCOMPARE(entry.remotePath, QStringLiteral("/backups/copy/") + name + QStringLiteral(".1"));
        const QByteArray expected = (QStringLiteral("payload for ") + name).toUtf8();
        QFile uploaded(runner.remoteFile(entry.remotePath));
        QVERIFY(uploaded.open(QIODevice::ReadOnly));
        QCOMPARE(uploaded.readAll(), expected);
        QVERIFY2(engine.restoreFile(entry, restored.path(), provider, &error), qPrintable(error));
        QFile restoredFile(restored.filePath(name));
        QVERIFY(restoredFile.open(QIODevice::ReadOnly));
        QCOMPARE(restoredFile.readAll(), expected);
        QFile original(source.filePath(name));
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), expected);
    }
    QCOMPARE(QDir(runner.remoteFile("/backups/copy")).entryList(QDir::Files).size(), 4);
    for (const QString &path : runner.uploadedPaths) {
        if (!path.startsWith(source.path() + '/')) {
            QVERIFY(!QFileInfo::exists(QFileInfo(path).path()) || path == manifest);
        }
    }
}

void ProtonProviderTest::failedPayloadsAreNotVerified_data()
{
    QTest::addColumn<bool>("truncateUpload");
    QTest::newRow("CLI upload failure") << false;
    QTest::newRow("remote verification failure") << true;
}

void ProtonProviderTest::failedPayloadsAreNotVerified()
{
    QFETCH(bool, truncateUpload);
    QTemporaryDir source;
    FilesystemRunner runner;
    QVERIFY(source.isValid() && runner.remote.isValid());
    for (const QString &name : {QString("good.txt"), QString("manifest.json")}) {
        QFile file(source.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("important payload");
    }
    if (truncateUpload) {
        runner.truncateUploadName = QStringLiteral("manifest.json.1");
    } else {
        runner.failUploadName = QStringLiteral("manifest.json.1");
    }
    BackupEngine engine;
    ProtonProvider provider(runner);
    QString manifest;
    QString error;
    const BackupCopyMetadata metadata {"computer", "set", "Documents", "copy", QDateTime::currentDateTimeUtc()};
    QVERIFY(!engine.backup({source.path()}, QStringLiteral("/backups/copy"), {}, metadata, provider, &manifest, &error));
    QVERIFY(!manifest.isEmpty());
    const auto cleanupManifest = qScopeGuard([&] { QDir(QFileInfo(manifest).path()).removeRecursively(); });
    QVector<BackupEntry> entries;
    BackupManifestInfo info;
    QVERIFY2(BackupManifest::load(manifest, &entries, &info, &error), qPrintable(error));
    QCOMPARE(info.status, QStringLiteral("incomplete"));
    QCOMPARE(info.failedItems, QStringList {QStringLiteral("manifest.json")});
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().remotePath, QStringLiteral("/backups/copy/good.txt"));
    QFile good(runner.remoteFile(entries.first().remotePath));
    QVERIFY(good.open(QIODevice::ReadOnly));
    QCOMPARE(good.readAll(), QByteArray("important payload"));
    QFile remoteManifest(runner.remoteFile("/backups/copy/manifest.json"));
    QVERIFY(remoteManifest.open(QIODevice::ReadOnly));
    QFile localManifest(manifest);
    QVERIFY(localManifest.open(QIODevice::ReadOnly));
    QCOMPARE(remoteManifest.readAll(), localManifest.readAll());
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
