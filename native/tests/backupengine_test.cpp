#include <QTemporaryDir>
#include <QTest>

#include "../src/backupengine.h"

class BackupEngineTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsMissingSource();
    void listsRegularFilesAndSkipsSymlinks();
};

void BackupEngineTest::rejectsMissingSource()
{
    BackupEngine engine;
    QString error;

    QVERIFY(!engine.validateSelection(QStringLiteral("/tmp/praefectus-does-not-exist"), &error));
    QCOMPARE(error, QStringLiteral("The selected folder does not exist."));
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
