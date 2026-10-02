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
    void reservesManifestPathForSourceFiles();
    void previewsMultipleSourcesAndExclusions();
    void excludesMatchingFolderNamesAtEveryDepth_data();
    void excludesMatchingFolderNamesAtEveryDepth();
    void absoluteExclusionDoesNotExcludeSameNamedFoldersElsewhere();
    void backsUpMultipleSourcesWithoutCollisions();
    void preservesVerifiedItemsInAnIncompleteCopy();
    void reusesAnExistingVerifiedCopyOnRetry();
    void localProviderRejectsUnsafePaths();
    void reportsProgressForIncludedFilesAndFinalization_data();
    void reportsProgressForIncludedFilesAndFinalization();
};

void BackupEngineTest::reportsProgressForIncludedFilesAndFinalization_data()
{
    QTest::addColumn<bool>("removeFile");
    QTest::newRow("successful files") << false;
    QTest::newRow("file becomes unreadable") << true;
}

void BackupEngineTest::reportsProgressForIncludedFilesAndFinalization()
{
    QFETCH(bool, removeFile);
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());
    for (const auto &name : {"one", "two", "three", "excluded"}) {
        QFile file(source.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(name == QByteArray("excluded") ? QByteArray(100, 'x') : QByteArray(10, 'x'));
    }
    BackupEngine engine;
    LocalProvider provider(remote.path());
    QVector<BackupProgress> updates;
    QString error;
    QString manifest;
    const bool success = engine.backup({source.path()}, "copy", {source.filePath("excluded")}, {},
        provider, &manifest, &error, [&](const BackupProgress &progress) {
            if (updates.isEmpty() && removeFile) {
                QVERIFY(QFile::remove(source.filePath("one")));
            }
            if (progress.finalizing) {
                QVERIFY(!QFile::exists(remote.filePath("copy/manifest.json")));
            }
            updates.append(progress);
        });
    QCOMPARE(success, !removeFile);
    QCOMPARE(updates.size(), 5);
    QCOMPARE(updates.first().totalFiles, 3);
    QCOMPARE(updates.first().totalBytes, qint64(30));
    QCOMPARE(updates.first().processedFiles, 0);
    for (int index = 1; index <= 3; ++index) {
        QCOMPARE(updates.at(index).processedFiles, index);
        QCOMPARE(updates.at(index).processedBytes, qint64(index * 10));
        QVERIFY(!updates.at(index).finalizing);
    }
    QVERIFY(updates.last().finalizing);
    QVector<BackupEntry> verified;
    QVERIFY(BackupManifest::load(manifest, &verified));
    QCOMPARE(verified.size(), removeFile ? 2 : 3);
}

void BackupEngineTest::rejectsMissingSource()
{
    BackupEngine engine;
    QString error;

    QVERIFY(!engine.validateSelection(QStringLiteral("/tmp/custos-does-not-exist"), &error));
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

void BackupEngineTest::reservesManifestPathForSourceFiles()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());

    QFile original(source.filePath(QStringLiteral("manifest.json")));
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("source manifest");
    original.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;
    QVERIFY(engine.backup(source.path(), QStringLiteral("copy"), provider, &manifestPath, &error));

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(manifestPath, &entries, &error));
    QCOMPARE(entries.size(), 1);
    QVERIFY(entries.first().remotePath != QStringLiteral("copy/manifest.json"));
    QVERIFY(QFileInfo::exists(remote.filePath(QStringLiteral("copy/manifest.json"))));

    QFile stored(remote.filePath(entries.first().remotePath));
    QVERIFY(stored.open(QIODevice::ReadOnly));
    QCOMPARE(stored.readAll(), QByteArray("source manifest"));
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

void BackupEngineTest::excludesMatchingFolderNamesAtEveryDepth_data()
{
    QTest::addColumn<QString>("rule");
    QTest::newRow("folder name") << QStringLiteral("node_modules");
    QTest::newRow("trailing slash") << QStringLiteral("node_modules/");
}

void BackupEngineTest::excludesMatchingFolderNamesAtEveryDepth()
{
    QFETCH(QString, rule);
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());
    const QStringList included {QStringLiteral("src/app.js"), QStringLiteral("node_modules-old/keep.js"), QStringLiteral("notes/node_modules")};
    const QStringList excluded {QStringLiteral("node_modules/package/index.js"), QStringLiteral("projects/app/node_modules/dependency/index.js")};
    for (const QString &relative : included + excluded) {
        QVERIFY(QDir().mkpath(QFileInfo(source.filePath(relative)).path()));
        QFile file(source.filePath(relative));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
    }
    const QString link = source.filePath(QStringLiteral("node_modules/linked-package"));
    QVERIFY(QFile::link(source.filePath(QStringLiteral("src/app.js")), link));

    BackupEngine engine;
    const BackupPreview preview = engine.preview({source.path()}, {rule});
    QCOMPARE(preview.includedFiles.size(), included.size());
    QCOMPARE(preview.excludedFiles.size(), excluded.size() + 1);
    QVERIFY(preview.excludedFiles.contains(link));
    QVERIFY(preview.skippedPaths.isEmpty());
    for (const QString &relative : included) QVERIFY(preview.includedFiles.contains(source.filePath(relative)));
    for (const QString &relative : excluded) QVERIFY(preview.excludedFiles.contains(source.filePath(relative)));

    LocalProvider provider(remote.path());
    QString manifestPath;
    QString error;
    QVERIFY2(engine.backup({source.path()}, QStringLiteral("copy"), {rule}, provider, &manifestPath, &error), qPrintable(error));
    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(manifestPath, &entries, &error));
    QCOMPARE(entries.size(), included.size());
    QVERIFY(!QFileInfo::exists(remote.filePath(QStringLiteral("copy/node_modules"))));
    QVERIFY(!QFileInfo::exists(remote.filePath(QStringLiteral("copy/projects/app/node_modules"))));
}

void BackupEngineTest::absoluteExclusionDoesNotExcludeSameNamedFoldersElsewhere()
{
    QTemporaryDir source;
    QVERIFY(source.isValid());
    for (const QString &project : {QStringLiteral("one"), QStringLiteral("two")}) {
        const QString folder = source.filePath(project + QStringLiteral("/node_modules"));
        QVERIFY(QDir().mkpath(folder));
        QFile file(QDir(folder).filePath(QStringLiteral("index.js")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
    }
    BackupEngine engine;
    const BackupPreview preview = engine.preview({source.path()}, {source.filePath(QStringLiteral("one/node_modules"))});
    QCOMPARE(preview.includedFiles, QStringList {source.filePath(QStringLiteral("two/node_modules/index.js"))});
    QCOMPARE(preview.excludedFiles, QStringList {source.filePath(QStringLiteral("one/node_modules/index.js"))});
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

    QTemporaryDir outside;
    QVERIFY(outside.isValid());
    QFile outsideFile(outside.filePath(QStringLiteral("outside.txt")));
    QVERIFY(outsideFile.open(QIODevice::WriteOnly));
    outsideFile.write("outside");
    outsideFile.close();
    QVERIFY(QFile::link(outsideFile.fileName(), remote.filePath(QStringLiteral("link.txt"))));
    QVERIFY(!provider.inspect(QStringLiteral("link.txt"), nullptr, &error));
    QCOMPARE(error, QStringLiteral("The provider path is invalid."));
    QVERIFY(QFile::link(outside.path(), remote.filePath(QStringLiteral("link-dir"))));
    QVERIFY(!provider.ensureDirectory(QStringLiteral("link-dir/new"), &error));
    QCOMPARE(error, QStringLiteral("The provider path is invalid."));
}

QTEST_MAIN(BackupEngineTest)
#include "backupengine_test.moc"
