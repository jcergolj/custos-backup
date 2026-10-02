#include <QFile>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTest>

#include "../src/backupmanifest.h"
#include "../src/localprovider.h"

class BackupManifestTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsVersionedEntries();
    void rejectsTraversalPaths();
    void acceptsDotsInsideFileNames();
    void acceptsAbsoluteRemotePaths();
    void rejectsCompleteCopyWithMissingExpectedEntry();
    void rejectsFailedEntryPresentedAsVerified();
    void rejectsMalformedEntries();
    void rejectsNullOutput();
    void restoresOnlyTheSelectedFile();
    void rejectsTamperedRestore();
    void preservesExistingDestinationWhenRestoreFails();
    void rejectsRestoreThroughDestinationSymlink();
};

void BackupManifestTest::loadsVersionedEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"copy/file.txt","size":12,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]})");
    file.close();

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(path, &entries));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().remotePath, QStringLiteral("copy/file.txt"));
    QCOMPARE(entries.first().size, qint64(12));
    QCOMPARE(entries.first().checksum.size(), 32);
}

void BackupManifestTest::restoresOnlyTheSelectedFile()
{
    QTemporaryDir remote;
    QTemporaryDir destination;
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());
    QDir().mkpath(remote.filePath(QStringLiteral("copy")));

    QFile selected(remote.filePath(QStringLiteral("copy/selected.txt")));
    QVERIFY(selected.open(QIODevice::WriteOnly));
    selected.write("selected");
    selected.close();
    QFile unrelated(remote.filePath(QStringLiteral("copy/unrelated.txt")));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.write("unrelated");
    unrelated.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString error;
    QVERIFY(engine.restoreFile({
        QStringLiteral("/source/selected.txt"),
        QStringLiteral("copy/selected.txt"),
        8,
        QCryptographicHash::hash("selected", QCryptographicHash::Sha256),
    }, destination.path(), provider, &error));
    QVERIFY(QFileInfo::exists(destination.filePath(QStringLiteral("selected.txt"))));
    QVERIFY(!QFileInfo::exists(destination.filePath(QStringLiteral("unrelated.txt"))));
}

void BackupManifestTest::rejectsTamperedRestore()
{
    QTemporaryDir remote;
    QTemporaryDir destination;
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());
    QDir().mkpath(remote.filePath(QStringLiteral("copy")));

    QFile selected(remote.filePath(QStringLiteral("copy/selected.txt")));
    QVERIFY(selected.open(QIODevice::WriteOnly));
    selected.write("selected");
    selected.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString error;
    const BackupEntry entry {
        QStringLiteral("/source/selected.txt"),
        QStringLiteral("copy/selected.txt"),
        8,
        QCryptographicHash::hash("different", QCryptographicHash::Sha256),
    };

    QVERIFY(!engine.restoreFile(entry, destination.path(), provider, &error));
    QCOMPARE(error, QStringLiteral("The restored file failed verification."));
    QVERIFY(!QFileInfo::exists(destination.filePath(QStringLiteral("selected.txt"))));
}

void BackupManifestTest::preservesExistingDestinationWhenRestoreFails()
{
    QTemporaryDir remote;
    QTemporaryDir destination;
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());
    QDir().mkpath(remote.filePath(QStringLiteral("copy")));

    QFile selected(remote.filePath(QStringLiteral("copy/selected.txt")));
    QVERIFY(selected.open(QIODevice::WriteOnly));
    selected.write("selected");
    selected.close();
    QFile existing(destination.filePath(QStringLiteral("selected.txt")));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.write("keep this");
    existing.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString error;
    QVERIFY(!engine.restoreFile({
        QStringLiteral("/source/selected.txt"),
        QStringLiteral("copy/selected.txt"),
        8,
        QCryptographicHash::hash("different", QCryptographicHash::Sha256),
    }, destination.path(), provider, &error));

    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("keep this"));
}

void BackupManifestTest::rejectsRestoreThroughDestinationSymlink()
{
    QTemporaryDir remote;
    QTemporaryDir destination;
    QTemporaryDir outside;
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());
    QVERIFY(outside.isValid());
    QDir().mkpath(remote.filePath(QStringLiteral("copy")));

    QFile selected(remote.filePath(QStringLiteral("copy/selected.txt")));
    QVERIFY(selected.open(QIODevice::WriteOnly));
    selected.write("selected");
    selected.close();
    QVERIFY(QFile::link(outside.path(), destination.filePath(QStringLiteral("linked"))));

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString error;
    QVERIFY(!engine.restoreFile({
        QStringLiteral("linked/selected.txt"),
        QStringLiteral("copy/selected.txt"),
        8,
        QCryptographicHash::hash("selected", QCryptographicHash::Sha256),
    }, destination.path(), provider, &error));
    QCOMPARE(error, QStringLiteral("The restore destination is outside the selected folder."));
}

void BackupManifestTest::rejectsTraversalPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"../outside","size":12,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]})");
    file.close();

    QVector<BackupEntry> entries;
    QString error;
    QVERIFY(!BackupManifest::load(path, &entries, &error));
    QCOMPARE(error, QStringLiteral("The backup manifest contains an unsafe path."));
}

void BackupManifestTest::acceptsDotsInsideFileNames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"copy/release..txt","size":12,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]})");
    file.close();

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(path, &entries));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().remotePath, QStringLiteral("copy/release..txt"));
}

void BackupManifestTest::acceptsAbsoluteRemotePaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"/my-files/backups/file.txt","size":12,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]})");
    file.close();

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(path, &entries));
    QCOMPARE(entries.first().remotePath, QStringLiteral("/my-files/backups/file.txt"));
}

void BackupManifestTest::rejectsCompleteCopyWithMissingExpectedEntry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":2,"application":"omacustos","computer":"computer","set_id":"set","set_name":"Set","copy_id":"copy","created_at":"2026-09-28T12:00:00.000Z","status":"complete","expected":["file.txt"],"failed":[],"entries":[]})");
    file.close();

    QVector<BackupEntry> entries;
    QString error;
    QVERIFY(!BackupManifest::load(path, &entries, &error));
    QCOMPARE(error, QStringLiteral("The backup manifest is malformed or unsupported."));
}

void BackupManifestTest::rejectsMalformedEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"copy/file.txt","size":-1}]})");
    file.close();

    QVector<BackupEntry> entries;
    QString error;
    QVERIFY(!BackupManifest::load(path, &entries, &error));
    QCOMPARE(error, QStringLiteral("The backup manifest contains an unsafe path."));
}

void BackupManifestTest::rejectsFailedEntryPresentedAsVerified()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile file(directory.filePath("manifest.json"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":2,"application":"omacustos","computer":"computer","set_id":"set","copy_id":"copy","created_at":"2026-09-28T12:00:00.000Z","status":"incomplete","expected":["file.txt"],"failed":["file.txt"],"entries":[{"source":"/safe/file.txt","remote":"copy/file.txt","restore":"file.txt","size":12,"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]})");
    file.close();
    QVector<BackupEntry> entries;
    QVERIFY(!BackupManifest::load(file.fileName(), &entries));
    QVERIFY(entries.isEmpty());
}

void BackupManifestTest::rejectsNullOutput()
{
    QString error;
    QVERIFY(!BackupManifest::load(QStringLiteral("/missing/manifest.json"), nullptr, &error));
    QCOMPARE(error, QStringLiteral("A destination for manifest entries is required."));
}

QTEST_MAIN(BackupManifestTest)
#include "backupmanifest_test.moc"
