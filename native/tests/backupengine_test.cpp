#include <QTemporaryDir>
#include <QTest>

#include "../src/backupengine.h"
#include "../src/localprovider.h"

class BackupEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingSource();
    void listsRegularFilesAndSkipsSymlinks();
    void backsUpVerifiesAndRestoresOneFile();
};

void BackupEngineTest::rejectsMissingSource()
{
    BackupEngine engine;
    QString error;

    QVERIFY(!engine.validateSelection(QStringLiteral("/tmp/praefectus-does-not-exist"), &error));
    QCOMPARE(error, QStringLiteral("The selected folder does not exist."));
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

QTEST_MAIN(BackupEngineTest)
#include "backupengine_test.moc"
