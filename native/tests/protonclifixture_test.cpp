#include <QTest>

#include <functional>

#include "../src/backupengine.h"
#include "../src/protonprovider.h"
#include "protonclifixture.h"

namespace {

bool writeFile(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

class AfterDownloadRunner final : public ProcessRunner
{
public:
    FilesystemRunner fixture;
    std::function<void()> afterDownload;

    ProcessOutput run(const QStringList &arguments) override
    {
        const ProcessOutput output = fixture.run(arguments);
        if (arguments.value(1) == "download" && output.successful() && afterDownload) afterDownload();
        return output;
    }
};

}

class ProtonCliFixtureTest final : public QObject
{
    Q_OBJECT

private slots:
    void transfersKeepBasenamesAndParentFolders();
    void conflictsHaveObservableEffects_data();
    void conflictsHaveObservableEffects();
    void destructiveConflictsReplaceOtherNodeTypes_data();
    void destructiveConflictsReplaceOtherNodeTypes();
    void failureInjectionProducesRealPartialOutput_data();
    void failureInjectionProducesRealPartialOutput();
    void uploadSkipsIdenticalFiles();
    void metadataWithAndWithoutChecksum_data();
    void metadataWithAndWithoutChecksum();
    void failedRestorePreservesExistingContent_data();
    void failedRestorePreservesExistingContent();
    void successfulRestorePreservesUnrelatedBasename_data();
    void successfulRestorePreservesUnrelatedBasename();
    void finalPlacementFailurePreservesExistingFolder();
    void finalPlacementFailurePreservesReadOnlyFile();
    void rejectsUnsafeRestoreDestinations_data();
    void rejectsUnsafeRestoreDestinations();
    void rejectsDestinationSymlinkIntroducedDuringDownload();
};

void ProtonCliFixtureTest::transfersKeepBasenamesAndParentFolders()
{
    QTemporaryDir local;
    QTemporaryDir destination;
    FilesystemRunner runner;
    QVERIFY(local.isValid() && destination.isValid() && runner.remote.isValid());
    const QString source = local.filePath("original name.txt");
    QVERIFY(writeFile(source, "source bytes"));
    QVERIFY(QDir().mkpath(runner.remoteFile("/parent folder")));
    // The CLI takes a parent, never a requested destination filename. A
    // provider must stage a renamed source to implement a requested-name change.
    const auto upload = runner.run({"filesystem", "upload", "-j", "-f", "replace", "-d", "replace", "-t", source, "/parent folder"});
    QVERIFY(upload.successful());
    QCOMPARE(readFile(runner.remoteFile("/parent folder/original name.txt")), QByteArray("source bytes"));
    QVERIFY(!QFileInfo::exists(runner.remoteFile("/parent folder/renamed.txt")));
    const auto download = runner.run({"filesystem", "download", "-j", "-f", "remove", "-d", "remove",
        "/parent folder/original name.txt", destination.path()});
    QVERIFY(download.successful());
    QCOMPARE(readFile(destination.filePath("original name.txt")), QByteArray("source bytes"));
    QCOMPARE(readFile(source), QByteArray("source bytes"));
    QVERIFY(!runner.run({"filesystem", "upload", "-f", "invalid", "-d", "replace", source, "/parent folder"}).successful());
}

void ProtonCliFixtureTest::conflictsHaveObservableEffects_data()
{
    QTest::addColumn<bool>("upload");
    QTest::addColumn<bool>("folder");
    QTest::addColumn<QString>("strategy");
    for (bool upload : {false, true}) {
        for (bool folder : {false, true}) {
            QStringList strategies {"rename", "skip", upload ? "replace" : "remove"};
            if (folder) strategies.append("merge");
            else if (upload) strategies.append("create-new-revision");
            for (const QString &strategy : strategies) {
                const QByteArray name = QString("%1 %2 %3").arg(upload ? "upload" : "download", folder ? "folder" : "file", strategy).toUtf8();
                QTest::newRow(name.constData()) << upload << folder << strategy;
            }
        }
    }
}

void ProtonCliFixtureTest::conflictsHaveObservableEffects()
{
    QFETCH(bool, upload);
    QFETCH(bool, folder);
    QFETCH(QString, strategy);
    QTemporaryDir local;
    FilesystemRunner runner;
    QVERIFY(local.isValid() && runner.remote.isValid());
    const QString source = upload ? local.filePath("source/node") : runner.remoteFile("/source/node");
    const QString parent = upload ? runner.remoteFile("/destination") : local.filePath("destination");
    const QString target = QDir(parent).filePath("node");
    const QString sourcePayload = folder ? QDir(source).filePath("payload.txt") : source;
    const QString targetPayload = folder ? QDir(target).filePath("payload.txt") : target;
    QVERIFY(writeFile(sourcePayload, "replacement"));
    QVERIFY(writeFile(targetPayload, "original"));
    if (folder) QVERIFY(writeFile(QDir(target).filePath("keep.txt"), "old-only child"));
    const auto output = runner.run({"filesystem", upload ? "upload" : "download", "-j",
        "-f", folder ? (upload ? "replace" : "remove") : strategy,
        "-d", folder ? strategy : (upload ? "replace" : "remove"),
        upload ? source : "/source/node", upload ? "/destination" : parent});
    QVERIFY2(output.successful(), qPrintable(output.standardError));
    QCOMPARE(readFile(sourcePayload), QByteArray("replacement"));
    if (strategy == "skip" || strategy == "rename") {
        QCOMPARE(readFile(targetPayload), QByteArray("original"));
        if (folder) QCOMPARE(readFile(QDir(target).filePath("keep.txt")), QByteArray("old-only child"));
        const QString renamed = target + ".1";
        QCOMPARE(QFileInfo::exists(renamed), strategy == "rename");
        if (strategy == "rename") QCOMPARE(readFile(folder ? QDir(renamed).filePath("payload.txt") : renamed), QByteArray("replacement"));
    } else {
        QCOMPARE(readFile(targetPayload), QByteArray("replacement"));
        if (folder) QCOMPARE(QFileInfo::exists(QDir(target).filePath("keep.txt")), strategy == "merge");
    }
    const bool trashesConflict = upload && (strategy == "replace" || strategy == "merge");
    QCOMPARE(!runner.trashedPaths.isEmpty(), trashesConflict);
    if (upload && strategy == "replace") {
        QCOMPARE(readFile(folder ? QDir(runner.trashedPaths.first()).filePath("payload.txt") : runner.trashedPaths.first()), QByteArray("original"));
    }
}

void ProtonCliFixtureTest::destructiveConflictsReplaceOtherNodeTypes_data()
{
    QTest::addColumn<bool>("upload");
    QTest::addColumn<bool>("sourceFolder");
    QTest::newRow("upload file replaces folder") << true << false;
    QTest::newRow("upload folder replaces file") << true << true;
    QTest::newRow("download file removes folder") << false << false;
    QTest::newRow("download folder removes file") << false << true;
}

void ProtonCliFixtureTest::destructiveConflictsReplaceOtherNodeTypes()
{
    QFETCH(bool, upload);
    QFETCH(bool, sourceFolder);
    QTemporaryDir local;
    FilesystemRunner runner;
    QVERIFY(local.isValid() && runner.remote.isValid());
    const QString source = upload ? local.filePath("source/node") : runner.remoteFile("/source/node");
    const QString parent = upload ? runner.remoteFile("/destination") : local.filePath("destination");
    const QString target = QDir(parent).filePath("node");
    QVERIFY(writeFile(sourceFolder ? QDir(source).filePath("new.txt") : source, "new bytes"));
    QVERIFY(writeFile(sourceFolder ? target : QDir(target).filePath("old.txt"), "old bytes"));
    const QString strategy = upload ? "replace" : "remove";
    QVERIFY(runner.run({"filesystem", upload ? "upload" : "download", "-f", strategy, "-d", strategy,
        upload ? source : "/source/node", upload ? "/destination" : parent}).successful());
    QCOMPARE(QFileInfo(target).isDir(), sourceFolder);
    QCOMPARE(readFile(sourceFolder ? QDir(target).filePath("new.txt") : target), QByteArray("new bytes"));
    QCOMPARE(runner.trashedPaths.size(), upload ? 1 : 0);
    if (upload) QCOMPARE(readFile(sourceFolder ? runner.trashedPaths.first() : QDir(runner.trashedPaths.first()).filePath("old.txt")), QByteArray("old bytes"));
}

void ProtonCliFixtureTest::failureInjectionProducesRealPartialOutput_data()
{
    QTest::addColumn<int>("injection");
    QTest::addColumn<bool>("success");
    QTest::addColumn<QByteArray>("output");
    QTest::newRow("error before transfer") << int(FilesystemRunner::Failure::TransferError) << false << QByteArray("old bytes");
    QTest::newRow("error after partial transfer") << int(FilesystemRunner::Failure::PartialOutput) << false << QByteArray("p");
    QTest::newRow("successful truncated transfer") << int(FilesystemRunner::Failure::TruncatedOutput) << true << QByteArray("p");
    QTest::newRow("successful same-size corrupt transfer") << int(FilesystemRunner::Failure::CorruptOutput) << true << QByteArray("!ayload");
}

void ProtonCliFixtureTest::failureInjectionProducesRealPartialOutput()
{
    QFETCH(int, injection);
    QFETCH(bool, success);
    QFETCH(QByteArray, output);
    QTemporaryDir local;
    FilesystemRunner runner;
    QVERIFY(local.isValid() && runner.remote.isValid());
    QVERIFY(writeFile(runner.remoteFile("/file.txt"), "payload"));
    QVERIFY(writeFile(local.filePath("file.txt"), "old bytes"));
    runner.downloadFailure = static_cast<FilesystemRunner::Failure>(injection);
    const auto result = runner.run({"filesystem", "download", "-f", "remove", "-d", "remove", "/file.txt", local.path()});
    QCOMPARE(result.successful(), success);
    QCOMPARE(readFile(local.filePath("file.txt")), output);
    QCOMPARE(readFile(runner.remoteFile("/file.txt")), QByteArray("payload"));
    QCOMPARE(result.standardError.isEmpty(), success);
}

void ProtonCliFixtureTest::uploadSkipsIdenticalFiles()
{
    QTemporaryDir local;
    FilesystemRunner runner;
    QVERIFY(local.isValid() && runner.remote.isValid());
    const QString source = local.filePath("same.txt");
    QVERIFY(writeFile(source, "identical"));
    QVERIFY(writeFile(runner.remoteFile("/parent/same.txt"), "identical"));
    QVERIFY(runner.run({"filesystem", "upload", "-f", "replace", "-d", "replace", source, "/parent"}).successful());
    QCOMPARE(readFile(runner.remoteFile("/parent/same.txt")), QByteArray("identical"));
    QVERIFY(runner.trashedPaths.isEmpty());
}

void ProtonCliFixtureTest::metadataWithAndWithoutChecksum_data()
{
    QTest::addColumn<bool>("checksum");
    QTest::newRow("documented CLI metadata without SHA-256") << false;
    QTest::newRow("optional SHA-256") << true;
}

void ProtonCliFixtureTest::metadataWithAndWithoutChecksum()
{
    QFETCH(bool, checksum);
    FilesystemRunner runner;
    QVERIFY(runner.remote.isValid());
    QVERIFY(writeFile(runner.remoteFile("/parent/file.txt"), "metadata payload"));
    runner.includeSha256 = checksum;
    ProtonProvider provider(runner);
    RemoteFile metadata;
    QVERIFY(provider.inspect("/parent/file.txt", &metadata));
    QCOMPARE(metadata.size, qint64(16));
    QCOMPARE(metadata.checksum, checksum ? QCryptographicHash::hash("metadata payload", QCryptographicHash::Sha256) : QByteArray());
    QVector<RemoteItem> items;
    QVERIFY(provider.list("/parent", &items));
    QCOMPARE(items.size(), 1);
    QCOMPARE(items.first().path, QString("/parent/file.txt"));
}

void ProtonCliFixtureTest::failedRestorePreservesExistingContent_data()
{
    QTest::addColumn<int>("failure");
    QTest::addColumn<bool>("sameBasename");
    for (auto failure : {FilesystemRunner::Failure::TransferError, FilesystemRunner::Failure::PartialOutput,
             FilesystemRunner::Failure::TruncatedOutput, FilesystemRunner::Failure::CorruptOutput}) {
        for (bool same : {false, true}) {
            const QByteArray name = QString("failure %1, %2 basename").arg(int(failure)).arg(same ? "same" : "different").toUtf8();
            QTest::newRow(name.constData()) << int(failure) << same;
        }
    }
}

void ProtonCliFixtureTest::failedRestorePreservesExistingContent()
{
    QFETCH(int, failure);
    QFETCH(bool, sameBasename);
    QTemporaryDir destination;
    FilesystemRunner runner;
    QVERIFY(destination.isValid() && runner.remote.isValid());
    const QByteArray contents("verified replacement");
    QVERIFY(writeFile(runner.remoteFile("/copy/notes.txt"), contents));
    const QString name = sameBasename ? "notes.txt" : "renamed.txt";
    QVERIFY(writeFile(destination.filePath(name), "existing destination"));
    if (!sameBasename) QVERIFY(writeFile(destination.filePath("notes.txt"), "unrelated basename"));
    const QStringList before = QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    runner.downloadFailure = static_cast<FilesystemRunner::Failure>(failure);
    const BackupEntry entry {"/source/notes.txt", "/copy/notes.txt", contents.size(),
        QCryptographicHash::hash(contents, QCryptographicHash::Sha256), name};
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString error;
    QVERIFY(!engine.restoreFile(entry, destination.path(), provider, &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(readFile(destination.filePath(name)), QByteArray("existing destination"));
    if (!sameBasename) QCOMPARE(readFile(destination.filePath("notes.txt")), QByteArray("unrelated basename"));
    QCOMPARE(QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden), before);
    QCOMPARE(runner.downloadedFolders.size(), 1);
    QVERIFY(!QFileInfo::exists(runner.downloadedFolders.first()));
    QCOMPARE(readFile(runner.remoteFile("/copy/notes.txt")), contents);
}

void ProtonCliFixtureTest::successfulRestorePreservesUnrelatedBasename_data()
{
    QTest::addColumn<bool>("folder");
    QTest::newRow("unrelated file") << false;
    QTest::newRow("unrelated folder") << true;
}

void ProtonCliFixtureTest::successfulRestorePreservesUnrelatedBasename()
{
    QFETCH(bool, folder);
    QTemporaryDir destination;
    FilesystemRunner runner;
    QVERIFY(destination.isValid() && runner.remote.isValid());
    const QByteArray contents("verified replacement");
    QVERIFY(writeFile(runner.remoteFile("/copy/notes.txt"), contents));
    QVERIFY(writeFile(destination.filePath("renamed.txt"), "existing destination"));
    const QString unrelated = destination.filePath(folder ? "notes.txt/keep.txt" : "notes.txt");
    QVERIFY(writeFile(unrelated, "unrelated basename"));
    const QStringList before = QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    const BackupEntry entry {"/source/notes.txt", "/copy/notes.txt", contents.size(),
        QCryptographicHash::hash(contents, QCryptographicHash::Sha256), "renamed.txt"};
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString error;
    QVERIFY2(engine.restoreFile(entry, destination.path(), provider, &error), qPrintable(error));
    QCOMPARE(readFile(destination.filePath("renamed.txt")), contents);
    QCOMPARE(readFile(unrelated), QByteArray("unrelated basename"));
    QCOMPARE(QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden), before);
    QVERIFY(!QFileInfo::exists(runner.downloadedFolders.first()));
}

void ProtonCliFixtureTest::finalPlacementFailurePreservesExistingFolder()
{
    QTemporaryDir destination;
    FilesystemRunner runner;
    QVERIFY(destination.isValid() && runner.remote.isValid());
    QVERIFY(writeFile(runner.remoteFile("/copy/notes.txt"), "replacement"));
    QVERIFY(writeFile(destination.filePath("notes.txt/keep.txt"), "existing folder child"));
    const BackupEntry entry {"/source/notes.txt", "/copy/notes.txt", 11,
        QCryptographicHash::hash("replacement", QCryptographicHash::Sha256), "notes.txt"};
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString error;
    QVERIFY(!engine.restoreFile(entry, destination.path(), provider, &error));
    QCOMPARE(readFile(destination.filePath("notes.txt/keep.txt")), QByteArray("existing folder child"));
    QCOMPARE(QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden), QStringList {"notes.txt"});
    QVERIFY(!provider.download("/copy/notes.txt", destination.filePath("notes.txt"), &error));
    QCOMPARE(readFile(destination.filePath("notes.txt/keep.txt")), QByteArray("existing folder child"));
}

void ProtonCliFixtureTest::finalPlacementFailurePreservesReadOnlyFile()
{
    QTemporaryDir destination;
    FilesystemRunner runner;
    QVERIFY(destination.isValid() && runner.remote.isValid());
    const QByteArray replacement("verified replacement");
    QVERIFY(writeFile(runner.remoteFile("/copy/notes.txt"), replacement));
    const QString existing = destination.filePath("notes.txt");
    QVERIFY(writeFile(existing, "existing destination"));
    QVERIFY(QFile::setPermissions(existing, QFileDevice::ReadOwner));
    const BackupEntry entry {"/source/notes.txt", "/copy/notes.txt", replacement.size(),
        QCryptographicHash::hash(replacement, QCryptographicHash::Sha256), "notes.txt"};
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString error;
    QVERIFY(!engine.restoreFile(entry, destination.path(), provider, &error));
    QCOMPARE(error, QString("The restored file could not be placed in the destination folder."));
    QCOMPARE(readFile(existing), QByteArray("existing destination"));
    QCOMPARE(QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden), QStringList {"notes.txt"});
    QCOMPARE(runner.downloadedFolders.size(), 1);
    QVERIFY(!QFileInfo::exists(runner.downloadedFolders.first()));
}

void ProtonCliFixtureTest::rejectsUnsafeRestoreDestinations_data()
{
    QTest::addColumn<QString>("scenario");
    QTest::newRow("parent traversal") << QString("../notes.txt");
    QTest::newRow("absolute path") << QString("/notes.txt");
    QTest::newRow("destination symlink") << QString("file-link");
    QTest::newRow("parent symlink") << QString("folder-link/notes.txt");
    QTest::newRow("root symlink") << QString("root-link");
}

void ProtonCliFixtureTest::rejectsUnsafeRestoreDestinations()
{
    QFETCH(QString, scenario);
    QTemporaryDir destination;
    QTemporaryDir outside;
    FilesystemRunner runner;
    QVERIFY(destination.isValid() && outside.isValid() && runner.remote.isValid());
    QVERIFY(writeFile(runner.remoteFile("/copy/notes.txt"), "replacement"));
    QVERIFY(writeFile(outside.filePath("notes.txt"), "untouched outside file"));
    QString root = destination.path();
    QString restorePath = scenario;
    if (scenario == "file-link") {
        QVERIFY(QFile::link(outside.filePath("notes.txt"), destination.filePath(scenario)));
    } else if (scenario == "folder-link/notes.txt") {
        QVERIFY(QFile::link(outside.path(), destination.filePath("folder-link")));
    } else if (scenario == "root-link") {
        root = destination.filePath("root-link");
        restorePath = "notes.txt";
        QVERIFY(QFile::link(outside.path(), root));
    }
    const BackupEntry entry {"/source/notes.txt", "/copy/notes.txt", 11,
        QCryptographicHash::hash("replacement", QCryptographicHash::Sha256), restorePath};
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString error;
    QVERIFY(!engine.restoreFile(entry, root, provider, &error));
    QCOMPARE(error, QString("The restore destination is outside the selected folder."));
    QCOMPARE(readFile(outside.filePath("notes.txt")), QByteArray("untouched outside file"));
    QVERIFY(runner.downloadedFolders.isEmpty());
}

void ProtonCliFixtureTest::rejectsDestinationSymlinkIntroducedDuringDownload()
{
    QTemporaryDir destination;
    QTemporaryDir outside;
    AfterDownloadRunner runner;
    QVERIFY(destination.isValid() && outside.isValid() && runner.fixture.remote.isValid());
    QVERIFY(writeFile(runner.fixture.remoteFile("/copy/notes.txt"), "replacement"));
    const QString target = destination.filePath("notes.txt");
    const QString outsideFile = outside.filePath("notes.txt");
    QVERIFY(writeFile(target, "existing destination"));
    QVERIFY(writeFile(outsideFile, "untouched outside file"));
    bool redirected = false;
    runner.afterDownload = [&] {
        QVERIFY(QFile::remove(target));
        QVERIFY(QFile::link(outsideFile, target));
        redirected = true;
    };
    const BackupEntry entry {"/source/notes.txt", "/copy/notes.txt", 11,
        QCryptographicHash::hash("replacement", QCryptographicHash::Sha256), "notes.txt"};
    ProtonProvider provider(runner);
    BackupEngine engine;
    QString error;
    QVERIFY(!engine.restoreFile(entry, destination.path(), provider, &error));
    QVERIFY(redirected);
    QCOMPARE(error, QString("The restore destination is outside the selected folder."));
    QCOMPARE(readFile(outsideFile), QByteArray("untouched outside file"));
    QVERIFY(QFileInfo(target).isSymLink());
    QCOMPARE(QDir(destination.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden), QStringList {"notes.txt"});
    QCOMPARE(runner.fixture.downloadedFolders.size(), 1);
    QVERIFY(!QFileInfo::exists(runner.fixture.downloadedFolders.first()));
}

QTEST_GUILESS_MAIN(ProtonCliFixtureTest)
#include "protonclifixture_test.moc"
