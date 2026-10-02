#include <QFile>
#include <QJsonArray>
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
    void removingFinalSetPersistsEmptyConfiguration_data();
    void removingFinalSetPersistsEmptyConfiguration();
    void exportCannotOverwriteLocalState();
    void importIsBlockedWhileWorkerRuns();
    void successfulSaveNotifiesSchedulingButPreviewAndFailedSaveDoNot();
    void remainingTimeIsReportedForTheRunningSetOnly();
    void reportsVerifiedCountsAndFailureDetailsWithoutInventingLegacyCounts();
};

void BackupSetControllerTest::remainingTimeIsReportedForTheRunningSetOnly()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString configPath = home.filePath("settings.json");
    BackupConfig config;
    config.sets = {{"documents", "Documents", "/my-files/backups", {"/safe/documents"}, {}},
                   {"photos", "Photos", "/my-files/backups", {"/safe/photos"}, {}}};
    QVERIFY(BackupConfigStore(configPath).save(config));
    BackupRunStore runs(home.filePath("omacustos-backup-runs.json"));
    runs.ensureSet("documents");
    auto &record = *runs.find("documents");
    runs.markRunning(record);
    QVERIFY(runs.save());
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    controller.setCurrentIndex(1);
    QCOMPARE(controller.remainingTimes().value("documents").toString(), QString("Estimating time remaining…"));
    QVERIFY(!controller.remainingTimes().contains("photos"));
    record.progress = {4000, 1000, 4, 1, false};
    record.progressElapsedMs = 10000;
    record.progressUpdatedAt = QDateTime::currentDateTimeUtc();
    record.progress.verifiedFiles = 1;
    record.progress.failedItems = 1;
    record.progress.currentFile = "/safe/large file";
    record.progress.currentFileBytes = 3000;
    record.progress.phase = "uploading";
    QVERIFY(runs.save());
    controller.refreshRunState();
    QVERIFY(controller.remainingTimes().value("documents").toString().startsWith("Est. remaining: 00:"));
    const auto transfer = controller.transferProgress().value("documents").toMap();
    QCOMPARE(transfer.value("fraction").toDouble(), 0.25);
    QVERIFY(transfer.value("text").toString().contains("1 of 4 files processed · 1 verified · 1 items failed"));
    QVERIFY(transfer.value("text").toString().contains("Uploading: /safe/large file"));
    QVERIFY(!controller.transferProgress().contains("photos"));
    record.progress.finalizing = true;
    QVERIFY(runs.save());
    controller.refreshRunState();
    QCOMPARE(controller.remainingTimes().value("documents").toString(), QString("Finalizing backup…"));
    runs.markSuccess(record, QDateTime::currentDateTimeUtc());
    QVERIFY(runs.save());
    controller.refreshRunState();
    QVERIFY(controller.remainingTimes().isEmpty());
    QVERIFY(controller.transferProgress().isEmpty());
}

void BackupSetControllerTest::reportsVerifiedCountsAndFailureDetailsWithoutInventingLegacyCounts()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString configPath = home.filePath("settings.json");
    BackupConfig config;
    config.sets = {{"documents", "Documents", "/my-files/backups", {"/safe/documents"}, {}}};
    QVERIFY(BackupConfigStore(configPath).save(config));
    BackupRunStore runs(home.filePath("omacustos-backup-runs.json"));
    runs.ensureSet("documents");
    auto &record = *runs.find("documents");
    runs.markIncomplete(record, "Backup incomplete", QDateTime::currentDateTimeUtc());
    record.result = {true, true, 97, 1000, {{"/safe/a", "uploading", "Connection interrupted"},
        {"/safe/b", "uploading", "Connection interrupted"}, {"/safe/c", "reading", "Permission denied"}}};
    QVERIFY(runs.save());
    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QCOMPARE(controller.recentBackups().first(), QString("Documents\nIncomplete"));
    auto details = controller.runDetails().value("documents").toMap();
    QCOMPARE(details.value("summary").toString(), QString("97 files backed up · 3 items failed"));
    const auto issues = details.value("issues").toList();
    QCOMPARE(issues.size(), 3);
    QCOMPARE(issues.last().toMap().value("path").toString(), QString("/safe/c"));
    QCOMPARE(issues.last().toMap().value("reason").toString(), QString("Permission denied"));
    record.result.manifestVerified = false;
    runs.markFailed(record, "Unable to upload manifest", QDateTime::currentDateTimeUtc());
    QVERIFY(runs.save());
    controller.refreshRunState();
    details = controller.backupDetails("documents");
    QCOMPARE(details.value("status").toString(), QString("Failed"));
    QVERIFY(details.value("summary").toString().isEmpty());
    QCOMPARE(details.value("error").toString(), QString("Unable to upload manifest"));
    record.result = {};
    record.status = "success";
    record.lastError.clear();
    QVERIFY(runs.save());
    controller.refreshRunState();
    QVERIFY(controller.backupDetails("documents").value("summary").toString().isEmpty());
    QCOMPARE(controller.recentBackups().first(), QString("Documents\nSuccessful"));
}

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
    BackupRunStore runs(directory.filePath(QStringLiteral("omacustos-backup-runs.json")));
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

    BackupRunStore runs(directory.filePath(QStringLiteral("omacustos-backup-runs.json")));
    runs.ensureSet(documentsId);
    runs.find(documentsId)->remoteCopyPath = QStringLiteral("/my-files/backups/previous-computer/Original_name/copy-id");
    QVERIFY(runs.save());
    controller.refreshRunState();
    QCOMPARE(controller.recentBackupFolderPath(documentsId), QStringLiteral("/my-files/backups/previous-computer/Original_name"));
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
    BackupRunStore runs(directory.filePath(QStringLiteral("omacustos-backup-runs.json")));
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
    QCOMPARE(document.value(QStringLiteral("application")).toString(), QStringLiteral("omacustos"));
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
    QTest::newRow("unknown version") << QByteArray(R"({"application":"omacustos","version":2,"sets":[]})");
    QTest::newRow("invalid sources") << QByteArray(R"({"application":"omacustos","version":1,"sets":[{"id":"a","name":"A","remote_root":"/my-files/backups","source_directories":[]}]})");
    QTest::newRow("duplicate identities") << QByteArray(R"({"application":"omacustos","version":1,"sets":[{"id":"a","name":"A","remote_root":"/my-files/backups","source_directories":["/safe/a"]},{"id":"a","name":"B","remote_root":"/my-files/backups","source_directories":["/safe/b"]}]})");
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

void BackupSetControllerTest::removingFinalSetPersistsEmptyConfiguration_data()
{
    QTest::addColumn<bool>("legacy");
    QTest::newRow("saved scheduled backup") << false;
    QTest::newRow("legacy single-source backup") << true;
}

void BackupSetControllerTest::removingFinalSetPersistsEmptyConfiguration()
{
    QFETCH(bool, legacy);
    QTemporaryDir directory;
    QTemporaryDir remote;
    QVERIFY(directory.isValid());
    QVERIFY(remote.isValid());
    const QString configPath = directory.filePath("settings.json");
    const QString source = directory.filePath("Documents");
    const QString copyPath = remote.filePath("computer/Documents/copy");
    QVERIFY(QDir().mkpath(copyPath));
    QFile payload(QDir(copyPath).filePath("notes.txt"));
    QVERIFY(payload.open(QIODevice::WriteOnly));
    const QByteArray contents("Existing remote backup contents");
    QCOMPARE(payload.write(contents), qint64(contents.size()));
    payload.close();

    BackupConfig config;
    config.protonBinary = "/custom/proton-drive";
    if (legacy) {
        config.sourceDirectory = source;
        config.remoteRoot = remote.path();
    } else {
        BackupSet set {"documents", "Documents", remote.path(), {source}, {}};
        set.schedule.frequency = "daily";
        config.sets = {set};
    }
    BackupConfigStore store(configPath);
    QVERIFY(store.save(config));

    BackupEngine engine;
    BackupSetController controller(engine, configPath);
    QCOMPARE(controller.setNames(), QStringList {legacy ? "Default backup" : "Documents"});
    QCOMPARE(controller.currentSources(), QStringList {source});
    QCOMPARE(controller.currentScheduleFrequency(), QString(legacy ? "disabled" : "daily"));
    QSignalSpy saved(&controller, &BackupSetController::configurationSaved);
    QSignalSpy failed(&controller, &BackupSetController::failed);
    controller.removeCurrentSet();
    QVERIFY(failed.isEmpty());
    QCOMPARE(saved.count(), 1);
    QVERIFY(controller.setNames().isEmpty());
    QCOMPARE(controller.currentIndex(), -1);

    QFile persisted(configPath);
    QVERIFY(persisted.open(QIODevice::ReadOnly));
    const QJsonObject document = QJsonDocument::fromJson(persisted.readAll()).object();
    QVERIFY(document.value("sets").isArray());
    QVERIFY(document.value("sets").toArray().isEmpty());
    QVERIFY(!document.contains("source_directory"));
    QVERIFY(!document.contains("remote_root"));
    BackupConfig reloaded;
    QVERIFY(store.load(&reloaded));
    QVERIFY(reloaded.sets.isEmpty());
    QCOMPARE(reloaded.protonBinary, config.protonBinary);

    BackupSetController reopened(engine, configPath);
    QVERIFY(reopened.setNames().isEmpty());
    QVERIFY(reopened.setIds().isEmpty());
    QVERIFY(reopened.currentSources().isEmpty());
    QVERIFY(reopened.recentBackups().isEmpty());
    QCOMPARE(reopened.currentIndex(), -1);
    QCOMPARE(reopened.currentScheduleFrequency(), QStringLiteral("disabled"));
    QCOMPARE(reopened.currentNextRun(), QStringLiteral("Not scheduled"));
    QVERIFY(payload.open(QIODevice::ReadOnly));
    QCOMPARE(payload.readAll(), contents);
    QVERIFY(QFileInfo(copyPath).isDir());
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
    QVERIFY(!controller.exportSets(directory.filePath(QStringLiteral("omacustos-backup-runs.json"))));
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
