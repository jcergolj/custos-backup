#include <QCryptographicHash>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "../src/backuprestorecontroller.h"
#include "../src/localprovider.h"

class BackupRestoreControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void loadsAndRestoresSelectedEntry();
    void rejectsInvalidSelection();
};

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

QTEST_MAIN(BackupRestoreControllerTest)
#include "backuprestorecontroller_test.moc"
