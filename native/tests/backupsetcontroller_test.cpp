#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTest>

#include "../src/backupsetcontroller.h"

class BackupSetControllerTest final : public QObject
{
    Q_OBJECT

private slots:
    void previewUpdatesFilesWithoutCountMessage_data();
    void previewUpdatesFilesWithoutCountMessage();
    void recentBackupTimestampIncludesLocalDateAndSeconds();
    void folderPathUsesBackupIdentityAndWorkerNaming();
    void deletedCopyDisappearsFromRecentBackupsButKeepsItsSet();
    void exportsSavedSetsAndImportsTheirSettings();
    void invalidImportLeavesExistingSetsUntouched_data();
    void invalidImportLeavesExistingSetsUntouched();
    void emptyImportDoesNotRestoreLegacySources();
    void exportCannotOverwriteLocalState();
    void importIsBlockedWhileWorkerRuns();
    void successfulSaveNotifiesSchedulingButPreviewAndFailedSaveDoNot();
};

void BackupSetControllerTest::successfulSaveNotifiesSchedulingButPreviewAndFailedSaveDoNot()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupEngine engine;
    BackupSetController controller(engine, directory.filePath("settings.json"));
    QSignalSpy saved(&controller, &BackupSetController::configurationSaved);
    controller.addSet();
    controller.setCurrentScheduleFrequency("daily");
    controller.preview();
    QVERIFY(saved.isEmpty());
    QVERIFY(!controller.save());
    QVERIFY(saved.isEmpty());
    controller.setCurrentSources({"/safe/documents"});
    QVERIFY(controller.save());
    QCOMPARE(saved.count(), 1);
    controller.removeCurrentSet();
    QCOMPARE(saved.count(), 2);
}

void BackupSetControllerTest::previewUpdatesFilesWithoutCountMessage_data()
{
    QTest::addColumn<bool>("hasSource");
    QTest::newRow("empty selection") << false;
    QTest::newRow("selected file") << true;
}

void BackupSetControllerTest::previewUpdatesFilesWithoutCountMessage()
{
    QFETCH(bool, hasSource);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile source(directory.filePath(QStringLiteral("notes.txt")));
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.write("important content");
    source.close();

    BackupEngine engine;
    BackupSetController controller(engine, directory.filePath(QStringLiteral("settings.json")));
    controller.addSet();
    if (hasSource) {
        controller.setCurrentSources({source.fileName()});
    }
    QSignalSpy previewSpy(&controller, &BackupSetController::previewChanged);
    QSignalSpy statusSpy(&controller, &BackupSetController::statusChanged);
    QSignalSpy failureSpy(&controller, &BackupSetController::failed);

    controller.preview();

    QCOMPARE(previewSpy.count(), 1);
    QCOMPARE(controller.previewIncluded(), hasSource ? QStringList {source.fileName()} : QStringList {});
    QCOMPARE(statusSpy.count(), 1);
    QVERIFY(statusSpy.first().first().toString().isEmpty());
    QVERIFY(failureSpy.isEmpty());
}

void BackupSetControllerTest::recentBackupTimestampIncludesLocalDateAndSeconds()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("settings.json"));
    BackupConfig config;
    config.sets = {{QStringLiteral("documents-id"), QStringLiteral("Documents"),
        QStringLiteral("/my-files/backups"), {QStringLiteral("/safe/documents")}, {}}};
    QVERIFY(BackupConfigStore(configPath).save(config));
    BackupRunStore runs(directory.filePath(QStringLiteral("custos-backup-runs.json")));
    runs.ensureSet(QStringLiteral("documents-id"));
    runs.markSuccess(*runs.find(QStringLiteral("documents-id")), QDateTime(QDate(2026, 10, 2), QTime(9, 30, 45)));
    QVERIFY(runs.save());

    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QCOMPARE(controller.recentBackupTimestamps(), QStringList {QStringLiteral("02/10/2026 09:30:45")});
}

void BackupSetControllerTest::folderPathUsesBackupIdentityAndWorkerNaming()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupEngine engine;
    BackupSetController controller(engine, directory.filePath(QStringLiteral("settings.json")));
    controller.addSet();
    controller.setCurrentName(QStringLiteral("Documents / notes"));
    controller.setCurrentRemoteRoot(QStringLiteral("/my-files/custom-backups"));
    const QString documentsId = controller.currentId();
    controller.addSet();
    controller.setCurrentName(QStringLiteral("Photos"));

    QString hostname = QSysInfo::machineHostName();
    hostname.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}._-]")), QStringLiteral("_"));
    const QString expected = QStringLiteral("/my-files/custom-backups/%1/Documents___notes").arg(hostname);
    QCOMPARE(controller.recentBackupFolderPath(documentsId), expected);
    QVERIFY(controller.recentBackupFolderPath(QStringLiteral("removed-id")).isEmpty());
}

void BackupSetControllerTest::deletedCopyDisappearsFromRecentBackupsButKeepsItsSet()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("settings.json"));
    BackupConfig config;
    config.sets = {{QStringLiteral("documents-id"), QStringLiteral("Documents"),
        QStringLiteral("/my-files/backups"), {QStringLiteral("/safe/documents")}, {}}};
    QVERIFY(BackupConfigStore(configPath).save(config));
    BackupRunStore runs(directory.filePath(QStringLiteral("custos-backup-runs.json")));
    runs.ensureSet(QStringLiteral("documents-id"));
    runs.find(QStringLiteral("documents-id"))->status = QStringLiteral("copy_deleted");
    QVERIFY(runs.save());
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QCOMPARE(controller.setNames(), QStringList {QStringLiteral("Documents")});
    QVERIFY(controller.recentBackups().isEmpty());
    QVERIFY(controller.recentBackupSetIds().isEmpty());
    QVERIFY(controller.recentBackupTimestamps().isEmpty());
}

void BackupSetControllerTest::exportsSavedSetsAndImportsTheirSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString originalPath = directory.filePath(QStringLiteral("original/settings.json"));
    BackupConfig original;
    original.protonBinary = QStringLiteral("/old-machine/proton-drive");
    BackupSet projects {QStringLiteral("projects-id"), QStringLiteral("Projects"), QStringLiteral("/my-files/projects"),
        {QStringLiteral("/home/user/projects"), QStringLiteral("/home/user/notes.txt")},
        {QStringLiteral("node_modules"), QStringLiteral("/home/user/projects/cache")}};
    projects.schedule = {QStringLiteral("monthly"), 9, 30, 2, 31};
    projects.retention = 7;
    projects.onlyOnAcPower = true;
    original.sets = {projects, {QStringLiteral("photos-id"), QStringLiteral("Photos"),
        QStringLiteral("/my-files/backups"), {QStringLiteral("/home/user/photos")}, {}}};
    QVERIFY(BackupConfigStore(originalPath).save(original));
    BackupEngine engine;
    BackupSetController source(engine, originalPath);
    source.setCurrentName(QStringLiteral("Unsaved draft"));
    const QString exportPath = directory.filePath(QStringLiteral("backup sets.json"));
    QVERIFY(source.exportSets(exportPath));
    QFile exported(exportPath);
    QVERIFY(exported.open(QIODevice::ReadOnly));
    const QJsonObject document = QJsonDocument::fromJson(exported.readAll()).object();
    QCOMPARE(document.value(QStringLiteral("application")).toString(), QStringLiteral("custos"));
    QVERIFY(!document.contains(QStringLiteral("proton_binary")));
    QVERIFY(!document.contains(QStringLiteral("runs")));

    const QString destinationPath = directory.filePath(QStringLiteral("destination/settings.json"));
    BackupConfig previous;
    previous.protonBinary = QStringLiteral("/this-machine/proton-drive");
    previous.sets = {{QStringLiteral("previous-id"), QStringLiteral("Previous"),
        QStringLiteral("/my-files/backups"), {QStringLiteral("/safe/previous")}, {}}};
    QVERIFY(BackupConfigStore(destinationPath).save(previous));
    BackupSetController destination(engine, destinationPath);
    QSignalSpy changed(&destination, &BackupSetController::setsChanged);
    QVERIFY(destination.importSets(exportPath));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(destination.setNames(), (QStringList {QStringLiteral("Projects"), QStringLiteral("Photos")}));
    QCOMPARE(destination.currentId(), projects.id);
    QCOMPARE(destination.currentSources(), projects.sourceDirectories);
    QCOMPARE(destination.currentExclusions(), projects.exclusions);
    QCOMPARE(destination.currentScheduleFrequency(), QStringLiteral("monthly"));
    QCOMPARE(destination.currentScheduleHour(), 9);
    QCOMPARE(destination.currentScheduleMinute(), 30);
    QCOMPARE(destination.currentScheduleDayOfMonth(), 31);
    QCOMPARE(destination.currentRetention(), 7);
    QVERIFY(destination.currentOnlyOnAcPower());
    BackupConfig restored;
    QVERIFY(BackupConfigStore(destinationPath).load(&restored));
    QCOMPARE(restored.protonBinary, previous.protonBinary);
    QCOMPARE(restored.sets.size(), 2);
    QCOMPARE(restored.sets.first().remoteRoot, projects.remoteRoot);
}

void BackupSetControllerTest::invalidImportLeavesExistingSetsUntouched_data()
{
    QTest::addColumn<QByteArray>("contents");
    QTest::newRow("invalid JSON") << QByteArray("{");
    QTest::newRow("not an export") << QByteArray(R"({"sets":[]})");
    QTest::newRow("unknown version") << QByteArray(R"({"application":"custos","version":2,"sets":[]})");
    QTest::newRow("invalid sources") << QByteArray(R"({"application":"custos","version":1,"sets":[{"id":"a","name":"A","remote_root":"/my-files/backups","source_directories":[]}]})");
    QTest::newRow("duplicate identities") << QByteArray(R"({"application":"custos","version":1,"sets":[{"id":"a","name":"A","remote_root":"/my-files/backups","source_directories":["/safe/a"]},{"id":"a","name":"B","remote_root":"/my-files/backups","source_directories":["/safe/b"]}]})");
}

void BackupSetControllerTest::invalidImportLeavesExistingSetsUntouched()
{
    QFETCH(QByteArray, contents);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("settings.json"));
    BackupConfig config;
    config.sets = {{QStringLiteral("documents-id"), QStringLiteral("Documents"),
        QStringLiteral("/my-files/backups"), {QStringLiteral("/safe/documents")}, {}}};
    QVERIFY(BackupConfigStore(configPath).save(config));
    QFile saved(configPath);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const QByteArray before = saved.readAll();
    saved.close();
    QFile invalid(directory.filePath(QStringLiteral("invalid.json")));
    QVERIFY(invalid.open(QIODevice::WriteOnly));
    invalid.write(contents);
    invalid.close();
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QSignalSpy failure(&controller, &BackupSetController::failed);
    QVERIFY(!controller.importSets(invalid.fileName()));
    QCOMPARE(failure.count(), 1);
    QCOMPARE(controller.setNames(), QStringList {QStringLiteral("Documents")});
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), before);
}

void BackupSetControllerTest::emptyImportDoesNotRestoreLegacySources()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("settings.json"));
    BackupConfig config {QStringLiteral("/safe/legacy"), QStringLiteral("/my-files/backups"), QStringLiteral("proton-drive")};
    QVERIFY(BackupConfigStore(configPath).save(config));
    const QString exportPath = directory.filePath(QStringLiteral("empty.json"));
    QVERIFY(BackupConfigStore(exportPath).exportSets(BackupConfig {}));
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QVERIFY(controller.importSets(exportPath));
    QVERIFY(controller.setNames().isEmpty());
    BackupConfig reloaded;
    QVERIFY(BackupConfigStore(configPath).load(&reloaded));
    QVERIFY(reloaded.sets.isEmpty());
}

void BackupSetControllerTest::exportCannotOverwriteLocalState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(BackupConfigStore(configPath).save(BackupConfig {}));
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QVERIFY(!controller.exportSets(configPath));
    QVERIFY(!controller.exportSets(directory.filePath(QStringLiteral("custos-backup-runs.json"))));
    QVERIFY(!controller.exportSets(directory.path()));
    BackupConfig reloaded;
    QVERIFY(BackupConfigStore(configPath).load(&reloaded));
    QCOMPARE(reloaded.protonBinary, QStringLiteral("proton-drive"));
}

void BackupSetControllerTest::importIsBlockedWhileWorkerRuns()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString configPath = directory.filePath(QStringLiteral("settings.json"));
    QVERIFY(BackupConfigStore(configPath).save(BackupConfig {}));
    const QString exportPath = directory.filePath(QStringLiteral("sets.json"));
    QVERIFY(BackupConfigStore(exportPath).exportSets(BackupConfig {}));
    QLockFile lock(configPath + QStringLiteral(".worker.lock"));
    QVERIFY(lock.tryLock(0));
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QSignalSpy failure(&controller, &BackupSetController::failed);
    QVERIFY(!controller.importSets(exportPath));
    QCOMPARE(failure.count(), 1);
}

QTEST_GUILESS_MAIN(BackupSetControllerTest)
#include "backupsetcontroller_test.moc"
