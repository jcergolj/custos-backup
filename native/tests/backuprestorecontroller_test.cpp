#include <QCryptographicHash>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSemaphore>
#include <QScopeGuard>
#include <QTimer>

#include "../src/backuprestorecontroller.h"
#include "../src/localprovider.h"

class RestoreTestProvider final : public BackupProvider
{
public:
    explicit RestoreTestProvider(const QString &root) : local(root) {}
    LocalProvider local;
    QStringList listedPaths;
    QStringList downloadedPaths;
    QStringList inspectedPaths;
    QSemaphore entered;
    QSemaphore release;
    bool blockList = false;
    bool blockInspect = false;

    bool upload(const QString &source, const QString &path, QString *error) override { return local.upload(source, path, error); }
    bool ensureDirectory(const QString &path, QString *error) override { return local.ensureDirectory(path, error); }
    bool trash(const QString &path, QString *error) override { return local.trash(path, error); }
    bool permanentlyDelete(const QString &path, QString *error) override { return local.permanentlyDelete(path, error); }
    bool download(const QString &path, const QString &destination, QString *error) override
    {
        downloadedPaths.append(path);
        return local.download(path, destination, error);
    }
    bool inspect(const QString &path, RemoteFile *file, QString *error) override
    {
        inspectedPaths.append(path);
        if (blockInspect) {
            entered.release();
            release.tryAcquire(1, 5000);
        }
        return local.inspect(path, file, error);
    }
    bool list(const QString &path, QVector<RemoteItem> *items, QString *error) override
    {
        listedPaths.append(path);
        if (blockList) {
            entered.release();
            release.tryAcquire(1, 5000);
        }
        if (!local.list(path, items, error)) {
            return false;
        }
        // A provider response must not expand discovery beyond direct child folders.
        items->append({"backups/other-computer/Other/copy", "copy", true, 0, {}});
        items->append({path + "/copy/nested", "nested", true, 0, {}});
        return true;
    }
};

static bool createCopy(const QTemporaryDir &remote, const QString &folder, const QString &setId, const QString &copyId)
{
    if (!QDir().mkpath(remote.filePath(folder + "/nested"))) {
        return false;
    }
    QFile file(remote.filePath(folder + "/nested/notes.txt"));
    if (!file.open(QIODevice::WriteOnly) || file.write("notes") != 5) {
        return false;
    }
    file.close();
    const QJsonArray entries {QJsonObject {{"source", "/source/notes.txt"}, {"remote", folder + "/nested/notes.txt"},
        {"restore", "nested/notes.txt"}, {"size", 5},
        {"sha256", QString::fromLatin1(QCryptographicHash::hash("notes", QCryptographicHash::Sha256).toHex())}}};
    QFile manifest(remote.filePath(folder + "/manifest.json"));
    return manifest.open(QIODevice::WriteOnly)
        && manifest.write(QJsonDocument(QJsonObject {{"version", 2}, {"application", "omacustos"},
            {"computer", "computer"}, {"set_id", setId}, {"set_name", "Documents"}, {"copy_id", copyId},
            {"created_at", "2026-10-02T12:00:00.000Z"}, {"status", "complete"},
            {"expected", QJsonArray {"nested/notes.txt"}}, {"failed", QJsonArray {}}, {"entries", entries}}).toJson()) > 0;
}

class BackupRestoreControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsAndRestoresSelectedEntry();
    void rejectsInvalidSelection();
    void clearsEntriesWhenManifestFailsToLoad();
    void rejectsEmptyDestination();
    void completesOnlyAfterAllSelectedFilesAreRestored_data();
    void completesOnlyAfterAllSelectedFilesAreRestored();
    void browsesWithoutBlockingAndVerifiesOnlyTheSelectedCopy();
    void discoveryFailureClearsPreviousResultsAndAllowsRetry();
    void rejectsUnidentifiableCopies_data();
    void rejectsUnidentifiableCopies();
};

void BackupRestoreControllerTest::browsesWithoutBlockingAndVerifiesOnlyTheSelectedCopy()
{
    QTemporaryDir remote;
    QVERIFY(remote.isValid());
    const QString folder = "backups/computer/Documents";
    const QString newest = folder + "/20261002-copy";
    QVERIFY(createCopy(remote, newest, "set-id", "20261002-copy"));
    QVERIFY(createCopy(remote, folder + "/20261001-copy", "set-id", "20261001-copy"));
    QVERIFY(createCopy(remote, "backups/other-computer/Other/copy", "other-id", "copy"));
    BackupEngine engine;
    RestoreTestProvider provider(remote.path());
    provider.blockList = true;
    BackupRestoreController controller(engine, &provider);
    const auto unblock = qScopeGuard([&] { provider.release.release(10); });
    QSignalSpy failures(&controller, &BackupRestoreController::failed);

    controller.discover(folder, "set-id");
    QVERIFY(controller.busy());
    QTRY_VERIFY(provider.entered.available() > 0);
    provider.entered.acquire();
    bool heartbeat = false;
    QTimer::singleShot(0, &controller, [&] { heartbeat = true; });
    QTRY_VERIFY(heartbeat);
    QVERIFY(controller.busy());
    QVERIFY(controller.copies().isEmpty());
    controller.discover("backups/other-computer/Other", "other-id");
    controller.setCopySearch("ignored while loading");
    provider.release.release();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(provider.listedPaths, QStringList {folder});
    QCOMPARE(controller.copies().size(), 2);
    QVERIFY(controller.copies().first().contains("20261002-copy"));
    QCOMPARE(controller.currentCopyIndex(), -1);
    QVERIFY(controller.entries().isEmpty());
    QVERIFY(provider.downloadedPaths.isEmpty());
    QVERIFY(provider.inspectedPaths.isEmpty());

    controller.setCopySearch("not verified");
    provider.blockInspect = true;
    controller.selectCopy(0);
    QVERIFY(controller.busy());
    QTRY_VERIFY(provider.entered.available() > 0);
    provider.entered.acquire();
    heartbeat = false;
    QTimer::singleShot(0, &controller, [&] { heartbeat = true; });
    QTRY_VERIFY(heartbeat);
    QVERIFY(controller.entries().isEmpty());
    controller.selectCopy(1); // Busy selections cannot replace the in-flight result.
    provider.release.release();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.currentCopyIndex(), 0);
    QCOMPARE(controller.copies().size(), 2);
    QCOMPARE(provider.downloadedPaths, QStringList {newest + "/manifest.json"});
    QCOMPARE(provider.inspectedPaths, QStringList {newest + "/nested/notes.txt"});
    QCOMPARE(controller.entries(), QStringList {"/source/notes.txt"});
    QCOMPARE(provider.listedPaths, QStringList {folder});
    QVERIFY(failures.isEmpty());

    // Changed files are rechecked instead of exposing stale verified entries.
    provider.blockInspect = false;
    QFile changed(remote.filePath(newest + "/nested/notes.txt"));
    QVERIFY(changed.open(QIODevice::WriteOnly));
    changed.write("other");
    changed.close();
    controller.selectCopy(0);
    QVERIFY(controller.entries().isEmpty());
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.entries().isEmpty());
    QCOMPARE(controller.unavailableEntries(), QStringList {"nested/notes.txt"});
    QCOMPARE(provider.downloadedPaths.size(), 2);
    QCOMPARE(provider.inspectedPaths.size(), 2);
}

void BackupRestoreControllerTest::discoveryFailureClearsPreviousResultsAndAllowsRetry()
{
    QTemporaryDir remote;
    QVERIFY(remote.isValid());
    const QString folder = "backups/computer/Documents";
    QVERIFY(createCopy(remote, folder + "/copy", "set-id", "copy"));
    BackupEngine engine;
    RestoreTestProvider provider(remote.path());
    BackupRestoreController controller(engine, &provider);
    QSignalSpy failures(&controller, &BackupRestoreController::failed);
    controller.discover(folder, "set-id");
    QTRY_VERIFY(!controller.busy());
    controller.selectCopy(0);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.entries().isEmpty());
    controller.discover("backups/missing", "set-id");
    QVERIFY(controller.entries().isEmpty());
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(failures.count(), 1);
    QVERIFY(controller.copies().isEmpty());
    QCOMPARE(controller.currentCopyIndex(), -1);
    controller.discover(folder, "set-id");
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.copies().size(), 1);
}

void BackupRestoreControllerTest::rejectsUnidentifiableCopies_data()
{
    QTest::addColumn<QString>("manifestSetId");
    QTest::addColumn<QString>("manifestCopyId");
    QTest::addColumn<bool>("removeManifest");
    QTest::newRow("wrong backup") << "other-id" << "copy" << false;
    QTest::newRow("wrong copy") << "set-id" << "other-copy" << false;
    QTest::newRow("missing manifest") << "set-id" << "copy" << true;
}

void BackupRestoreControllerTest::rejectsUnidentifiableCopies()
{
    QFETCH(QString, manifestSetId);
    QFETCH(QString, manifestCopyId);
    QFETCH(bool, removeManifest);
    QTemporaryDir remote;
    QVERIFY(remote.isValid());
    const QString folder = "backups/computer/Documents";
    QVERIFY(createCopy(remote, folder + "/copy", manifestSetId, manifestCopyId));
    if (removeManifest) {
        QVERIFY(QFile::remove(remote.filePath(folder + "/copy/manifest.json")));
    }
    BackupEngine engine;
    RestoreTestProvider provider(remote.path());
    BackupRestoreController controller(engine, &provider);
    QSignalSpy failures(&controller, &BackupRestoreController::failed);
    controller.discover(folder, "set-id");
    QTRY_VERIFY(!controller.busy());
    controller.selectCopy(0);
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(failures.count(), 1);
    QVERIFY(controller.entries().isEmpty());
    QVERIFY(provider.inspectedPaths.isEmpty());
}

void BackupRestoreControllerTest::completesOnlyAfterAllSelectedFilesAreRestored_data()
{
    QTest::addColumn<bool>("failSecondFile");
    QTest::newRow("all files restored") << false;
    QTest::newRow("second file fails") << true;
}

void BackupRestoreControllerTest::completesOnlyAfterAllSelectedFilesAreRestored()
{
    QFETCH(bool, failSecondFile);
    QTemporaryDir remote;
    QTemporaryDir destination;
    QTemporaryDir metadata;
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());
    QVERIFY(metadata.isValid());
    QJsonArray entries;
    for (const QString &name : {QString("one.txt"), QString("two.txt")}) {
        QFile file(remote.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("notes");
        entries.append(QJsonObject {{"source", "/source/" + name}, {"remote", name}, {"size", 5},
            {"sha256", QString::fromLatin1(QCryptographicHash::hash("notes", QCryptographicHash::Sha256).toHex())}});
    }
    QFile manifest(metadata.filePath("manifest.json"));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(QJsonDocument(QJsonObject {{"version", 1}, {"entries", entries}}).toJson());
    manifest.close();
    BackupEngine engine;
    LocalProvider provider(remote.path());
    BackupRestoreController controller(engine, &provider);
    controller.loadManifest(manifest.fileName());
    QSignalSpy completed(&controller, &BackupRestoreController::restoreCompleted);
    QSignalSpy failed(&controller, &BackupRestoreController::failed);
    if (failSecondFile) {
        QVERIFY(QFile::remove(remote.filePath("two.txt")));
    }
    controller.restoreSelected({0, 1}, destination.path());
    QVERIFY(QFile::exists(destination.filePath("one.txt")));
    QCOMPARE(completed.count(), failSecondFile ? 0 : 1);
    QCOMPARE(failed.count(), failSecondFile ? 1 : 0);
    QCOMPARE(QFile::exists(destination.filePath("two.txt")), !failSecondFile);
}

void BackupRestoreControllerTest::loadsAndRestoresSelectedEntry()
{
    QTemporaryDir remote;
    QTemporaryDir destination;
    QTemporaryDir manifestDirectory;
    QVERIFY(remote.isValid());
    QVERIFY(destination.isValid());
    QVERIFY(manifestDirectory.isValid());
    QDir().mkpath(remote.filePath(QStringLiteral("copy")));

    QFile file(remote.filePath(QStringLiteral("copy/notes.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("notes");
    file.close();

    QFile manifest(manifestDirectory.filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"version":1,"entries":[{"source":"/source/notes.txt","remote":"copy/notes.txt","size":5,"sha256":"ab5aa97074c454a0632057e704220d9a6678fbf773a0a5806fc09b8173b07309"}]})");
    manifest.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    BackupRestoreController controller(engine, &provider);
    controller.loadManifest(manifest.fileName());
    QCOMPARE(controller.entries(), QStringList {QStringLiteral("/source/notes.txt")});

    QSignalSpy statusSpy(&controller, &BackupRestoreController::statusChanged);
    controller.restore(0, destination.path());
    QVERIFY(!statusSpy.isEmpty());

    QFile restored(destination.filePath(QStringLiteral("notes.txt")));
    QVERIFY(restored.open(QIODevice::ReadOnly));
    QCOMPARE(restored.readAll(), QByteArray("notes"));
}

void BackupRestoreControllerTest::rejectsInvalidSelection()
{
    BackupEngine engine;
    BackupRestoreController controller(engine);
    QSignalSpy failureSpy(&controller, &BackupRestoreController::failed);

    controller.restore(0, QStringLiteral("/tmp"));

    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.first().at(0).toString(), QStringLiteral("No backup provider is configured."));
}

void BackupRestoreControllerTest::clearsEntriesWhenManifestFailsToLoad()
{
    QTemporaryDir manifestDirectory;
    QVERIFY(manifestDirectory.isValid());

    QFile manifest(manifestDirectory.filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifest.open(QIODevice::WriteOnly));
    manifest.write(R"({"version":1,"entries":[]})");
    manifest.close();

    BackupEngine engine;
    BackupRestoreController controller(engine);
    controller.loadManifest(manifest.fileName());
    QVERIFY(controller.entries().isEmpty());

    QSignalSpy failureSpy(&controller, &BackupRestoreController::failed);
    controller.loadManifest(manifestDirectory.filePath(QStringLiteral("missing.json")));

    QVERIFY(!failureSpy.isEmpty());
    QVERIFY(controller.entries().isEmpty());
}

void BackupRestoreControllerTest::rejectsEmptyDestination()
{
    BackupEngine engine;
    LocalProvider provider(QDir::homePath());
    BackupRestoreController controller(engine, &provider);
    QSignalSpy failureSpy(&controller, &BackupRestoreController::failed);

    controller.restore(0, QStringLiteral("  "));

    QCOMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.first().at(0).toString(), QStringLiteral("A restore destination folder is required."));
}

QTEST_MAIN(BackupRestoreControllerTest)
#include "backuprestorecontroller_test.moc"
