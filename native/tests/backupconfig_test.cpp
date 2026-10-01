#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "../src/backupconfig.h"

class BackupConfigTest final : public QObject
{
    Q_OBJECT

private slots:
    void savesAndLoadsConfiguration();
    void savesAndLoadsIndependentSets();
    void savesAndLoadsEmptySetList();
    void ignoresAndDropsLegacyExternalDriveRequirements();
    void rejectsMalformedConfiguration();
    void rejectsIncompleteConfiguration();
    void rejectsDuplicateSetIdsAndInvalidSchedules();
    void rejectsNullOutput();
};

void BackupConfigTest::savesAndLoadsConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("config/settings.json")));
    const BackupConfig expected {
        QStringLiteral("/home/user/Documents"),
        QStringLiteral("/my-files/backups/computer/copy"),
        QStringLiteral("/usr/bin/proton-drive"),
    };

    QVERIFY(store.save(expected));
    BackupConfig actual;
    QVERIFY(store.load(&actual));
    QCOMPARE(actual.sourceDirectory, expected.sourceDirectory);
    QCOMPARE(actual.remoteRoot, expected.remoteRoot);
    QCOMPARE(actual.protonBinary, expected.protonBinary);
}

void BackupConfigTest::savesAndLoadsIndependentSets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("config/settings.json")));
    BackupConfig expected;
    expected.protonBinary = QStringLiteral("/usr/bin/proton-drive");
    expected.sets = {
        {
            QStringLiteral("documents"),
            QStringLiteral("Documents"),
            QStringLiteral("backups/documents"),
            {QStringLiteral("/home/user/Documents"), QStringLiteral("/home/user/Notes")},
            {QStringLiteral("/home/user/Documents/cache")},
        },
        {
            QStringLiteral("configs"),
            QStringLiteral("Configs"),
            QStringLiteral("backups/configs"),
            {QStringLiteral("/home/user/.config")},
            {},
        },
    };
    expected.sets[0].schedule = {QStringLiteral("monthly"), 8, 45, 2, 31};
    expected.sets[0].retention = 5;
    expected.sets[0].onlyOnAcPower = true;

    QVERIFY(store.save(expected));
    BackupConfig actual;
    QVERIFY(store.load(&actual));
    QCOMPARE(actual.protonBinary, expected.protonBinary);
    QCOMPARE(actual.sets.size(), 2);
    QCOMPARE(actual.sets.at(0).name, QStringLiteral("Documents"));
    QCOMPARE(actual.sets.at(0).sourceDirectories, expected.sets.at(0).sourceDirectories);
    QCOMPARE(actual.sets.at(0).exclusions, expected.sets.at(0).exclusions);
    QCOMPARE(actual.sets.at(0).schedule.frequency, QStringLiteral("monthly"));
    QCOMPARE(actual.sets.at(0).schedule.hour, 8);
    QCOMPARE(actual.sets.at(0).retention, 5);
    QCOMPARE(actual.sets.at(0).onlyOnAcPower, true);
    QCOMPARE(actual.sets.at(1).remoteRoot, QStringLiteral("backups/configs"));
}

void BackupConfigTest::savesAndLoadsEmptySetList()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("config/settings.json")));
    BackupConfig expected;
    expected.protonBinary = QStringLiteral("/usr/bin/proton-drive");

    QVERIFY(store.save(expected));
    BackupConfig actual;
    QVERIFY(store.load(&actual));
    QCOMPARE(actual.protonBinary, expected.protonBinary);
    QVERIFY(actual.sets.isEmpty());
}

void BackupConfigTest::ignoresAndDropsLegacyExternalDriveRequirements()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"proton_binary":"proton-drive","sets":[{
        "id":"documents","name":"Documents","remote_root":"/my-files/backups",
        "source_directories":["/home/user/Documents"],"exclusions":["/home/user/Documents/cache"],
        "schedule":{"frequency":"daily","hour":9,"minute":30},"only_on_ac_power":true,
        "required_volumes":[{"mount_path":"/run/media/missing-drive","device_id":"646576696365"}]
    }]})");
    file.close();

    BackupConfigStore store(path);
    BackupConfig config;
    QVERIFY(store.load(&config));
    QCOMPARE(config.sets.size(), 1);
    QCOMPARE(config.sets.first().id, QStringLiteral("documents"));
    QCOMPARE(config.sets.first().name, QStringLiteral("Documents"));
    QCOMPARE(config.sets.first().exclusions, QStringList {QStringLiteral("/home/user/Documents/cache")});
    QCOMPARE(config.sets.first().schedule.frequency, QStringLiteral("daily"));
    QCOMPARE(config.sets.first().schedule.hour, 9);
    QCOMPARE(config.sets.first().schedule.minute, 30);
    QVERIFY(config.sets.first().onlyOnAcPower);

    QVERIFY(store.save(config));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonObject backup = QJsonDocument::fromJson(file.readAll()).object()
        .value(QStringLiteral("sets")).toArray().first().toObject();
    QVERIFY(!backup.contains(QStringLiteral("required_volumes")));
}

void BackupConfigTest::rejectsMalformedConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("not-json");
    file.close();

    BackupConfigStore store(path);
    BackupConfig config;
    QString error;
    QVERIFY(!store.load(&config, &error));
    QCOMPARE(error, QStringLiteral("The Custos backup configuration is malformed."));
}

void BackupConfigTest::rejectsIncompleteConfiguration()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("settings.json")));
    BackupConfig config;
    config.protonBinary.clear();
    QString error;

    QVERIFY(!store.save(config, &error));
    QCOMPARE(error, QStringLiteral("The Custos backup configuration is incomplete."));
}

void BackupConfigTest::rejectsDuplicateSetIdsAndInvalidSchedules()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("settings.json")));
    BackupConfig config;
    config.protonBinary = QStringLiteral("proton-drive");
    config.sets = {
        {QStringLiteral("same"), QStringLiteral("One"), QStringLiteral("backups/one"), {QStringLiteral("/tmp")}},
        {QStringLiteral("same"), QStringLiteral("Two"), QStringLiteral("backups/two"), {QStringLiteral("/tmp")}},
    };
    QString error;
    QVERIFY(!store.save(config, &error));
    QCOMPARE(error, QStringLiteral("The Custos backup configuration is incomplete."));

    config.sets.removeLast();
    config.sets.first().schedule.frequency = QStringLiteral("hourly");
    QVERIFY(!store.save(config, &error));
    QCOMPARE(error, QStringLiteral("The Custos backup configuration is incomplete."));
}

void BackupConfigTest::rejectsNullOutput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    BackupConfigStore store(directory.filePath(QStringLiteral("settings.json")));
    QString error;

    QVERIFY(!store.load(nullptr, &error));
    QCOMPARE(error, QStringLiteral("A destination for Custos backup configuration is required."));
}

QTEST_MAIN(BackupConfigTest)
#include "backupconfig_test.moc"
