#include <QFile>
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
    void restoresOnlyTheSelectedFile();
};

void BackupManifestTest::loadsVersionedEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"copy/file.txt","size":12}]})");
    file.close();

    QVector<BackupEntry> entries;
    QVERIFY(BackupManifest::load(path, &entries));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().remotePath, QStringLiteral("copy/file.txt"));
    QCOMPARE(entries.first().size, qint64(12));
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
    QVERIFY(engine.restoreFile({QStringLiteral("/source/selected.txt"), QStringLiteral("copy/selected.txt"), 8}, destination.path(), provider, &error));
    QVERIFY(QFileInfo::exists(destination.filePath(QStringLiteral("selected.txt"))));
    QVERIFY(!QFileInfo::exists(destination.filePath(QStringLiteral("unrelated.txt"))));
}

void BackupManifestTest::rejectsTraversalPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("manifest.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"({"version":1,"entries":[{"source":"/home/user/file.txt","remote":"../outside","size":12}]})");
    file.close();

    QVector<BackupEntry> entries;
    QString error;
    QVERIFY(!BackupManifest::load(path, &entries, &error));
    QCOMPARE(error, QStringLiteral("The backup manifest contains an unsafe path."));
}

QTEST_MAIN(BackupManifestTest)
#include "backupmanifest_test.moc"
