#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QTest>

#include "../src/backupengine.h"
#include "../src/backupmanifest.h"
#include "../src/localprovider.h"

class BackupEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingSource();
    void rejectsUnsafeRemoteRoot();
    void listsRegularFilesAndSkipsSymlinks();
    void backsUpVerifiesAndRestoresOneFile();
    void backsUpStoresVerifiedChecksum();
    void previewsMultipleSourcesAndExclusions();
    void backsUpMultipleSourcesWithoutCollisions();
    void preservesVerifiedItemsInAnIncompleteCopy();
    void reusesAnExistingVerifiedCopyOnRetry();
    void localProviderRejectsUnsafePaths();
};

void BackupEngineTest::rejectsMissingSource()
{
    BackupEngine engine;
    QString error;

    QVERIFY(!engine.validateSelection(QStringLiteral("/tmp/praefectus-does-not-exist"), &error));
    QCOMPARE(error, QStringLiteral("The selected folder does not exist."));
}

void BackupEngineTest::rejectsUnsafeRemoteRoot()
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
    QString error;
    QVERIFY(!engine.backup(source.path(), QStringLiteral("../outside"), provider, nullptr, &error));
    QCOMPARE(error, QStringLiteral("The remote backup folder is invalid."));
}

void BackupEngineTest::backsUpVerifiesAndRestoresOneFile()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QTemporaryDir destination;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());

    QFile original(source.filePath(QStringLiteral("notes with spaces.txt")));
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("important content");
    original.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;

    QVERIFY(engine.backup(source.path(), QStringLiteral("copy"), provider, &manifestPath, &error));
    QVERIFY2(QFileInfo::exists(manifestPath), qPrintable(error));

    const BackupEntry entry {
        original.fileName(),
        QStringLiteral("copy/notes with spaces.txt"),
        original.size(),
    };
    QVERIFY(engine.restoreFile(entry, destination.path(), provider, &error));

    QFile restored(QDir(destination.path()).filePath(original.fileName()));
    QVERIFY(restored.open(QIODevice::ReadOnly));
    QCOMPARE(restored.readAll(), QByteArray("important content"));
}

void BackupEngineTest::backsUpStoresVerifiedChecksum()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());

    QFile original(source.filePath(QStringLiteral("notes.txt")));
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("important content");
    original.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;
    QVERIFY(engine.backup(source.path(), QStringLiteral("copy"), provider, &manifestPath, &error));

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(manifestPath, &entries, &error));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().checksum,
        QCryptographicHash::hash("important content", QCryptographicHash::Sha256));
}

void BackupEngineTest::previewsMultipleSourcesAndExclusions()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(QDir().mkpath(first.filePath(QStringLiteral("cache"))));

    QFile included(first.filePath(QStringLiteral("keep.txt")));
    QVERIFY(included.open(QIODevice::WriteOnly));
    included.write("keep");
    included.close();
    QFile excluded(first.filePath(QStringLiteral("cache/drop.txt")));
    QVERIFY(excluded.open(QIODevice::WriteOnly));
    excluded.write("drop");
    excluded.close();
    QFile secondFile(second.filePath(QStringLiteral("same-name.txt")));
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    secondFile.write("second");
    secondFile.close();

    BackupEngine engine;
    const BackupPreview preview = engine.preview(
        {first.path(), second.path()},
        {first.filePath(QStringLiteral("cache"))}
    );

    QCOMPARE(preview.includedFiles.size(), 2);
    QCOMPARE(preview.excludedFiles, QStringList {excluded.fileName()});
    QVERIFY(preview.includedFiles.contains(included.fileName()));
    QVERIFY(preview.includedFiles.contains(secondFile.fileName()));
}

void BackupEngineTest::backsUpMultipleSourcesWithoutCollisions()
{
    QTemporaryDir first;
    QTemporaryDir second;
    QTemporaryDir remote;
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QVERIFY(remote.isValid());

    QFile firstFile(first.filePath(QStringLiteral("same.txt")));
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    firstFile.write("first");
    firstFile.close();
    QFile secondFile(second.filePath(QStringLiteral("same.txt")));
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    secondFile.write("second");
    secondFile.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;
    QVERIFY(engine.backup(
        {first.path(), second.path()}, QStringLiteral("copy"), {}, provider, &manifestPath, &error
    ));

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(manifestPath, &entries, &error));
    QCOMPARE(entries.size(), 2);
    QVERIFY(entries.at(0).remotePath != entries.at(1).remotePath);
    QVERIFY(QFileInfo::exists(remote.filePath(entries.at(0).remotePath)));
    QVERIFY(QFileInfo::exists(remote.filePath(entries.at(1).remotePath)));
}

void BackupEngineTest::preservesVerifiedItemsInAnIncompleteCopy()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());

    QFile file(source.filePath(QStringLiteral("available.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("available");
    file.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;
    QVERIFY(!engine.backup(
        {source.path(), source.filePath(QStringLiteral("missing"))},
        QStringLiteral("copy"), {}, provider, &manifestPath, &error
    ));
    QVERIFY(error.startsWith(QStringLiteral("Backup incomplete:")));
    QVERIFY(QFileInfo::exists(manifestPath));

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(manifestPath, &entries, &error));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().sourcePath, file.fileName());
}

void BackupEngineTest::reusesAnExistingVerifiedCopyOnRetry()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());

    QFile file(source.filePath(QStringLiteral("retry.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("retry content");
    file.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;
    QVERIFY(engine.backup(source.path(), QStringLiteral("copy"), provider, &manifestPath, &error));
    QVERIFY(engine.backup(source.path(), QStringLiteral("copy"), provider, &manifestPath, &error));
}

void BackupEngineTest::listsRegularFilesAndSkipsSymlinks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QFile keep(directory.filePath(QStringLiteral("keep.txt")));
    QVERIFY(keep.open(QIODevice::WriteOnly));
    keep.write("content");
    keep.close();

    QVERIFY(QDir().mkdir(directory.filePath(QStringLiteral("nested"))));
    QFile nested(directory.filePath(QStringLiteral("nested/zero-byte")));
    QVERIFY(nested.open(QIODevice::WriteOnly));
    nested.close();

    QVERIFY(QFile::link(keep.fileName(), directory.filePath(QStringLiteral("link.txt"))));

    BackupEngine engine;
    const QStringList files = engine.selectableFiles(directory.path());

    const QStringList expected {
        QFileInfo(keep).absoluteFilePath(),
        QFileInfo(nested).absoluteFilePath(),
    };

    QCOMPARE(files, expected);
}

void BackupEngineTest::localProviderRejectsUnsafePaths()
{
    QTemporaryDir remote;
    QVERIFY(remote.isValid());
    LocalProvider provider(remote.path());
    QString error;

    QVERIFY(!provider.upload(QStringLiteral("/tmp/file"), QStringLiteral("../outside"), &error));
    QCOMPARE(error, QStringLiteral("The provider path is invalid."));

    error.clear();
    QVERIFY(!provider.inspect(QStringLiteral("/absolute/file"), nullptr, &error));
    QCOMPARE(error, QStringLiteral("The provider path is invalid."));
}

QTEST_MAIN(BackupEngineTest)
#include "backupengine_test.moc"
