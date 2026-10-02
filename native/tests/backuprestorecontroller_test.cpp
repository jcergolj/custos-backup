#include <QCryptographicHash>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../src/backuprestorecontroller.h"
#include "../src/localprovider.h"

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
};

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
