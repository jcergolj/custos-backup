#include <QTemporaryDir>
#include <QTest>

#include "../src/backupengine.h"
#include "../src/localprovider.h"

class BackupEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingSource();
    void rejectsUnsafeRemoteRoot();
    void listsRegularFilesAndSkipsSymlinks();
    void backsUpVerifiesAndRestoresOneFile();
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
