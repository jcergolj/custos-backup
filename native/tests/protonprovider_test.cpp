#include <QTest>

#include "../src/protonprovider.h"

class FakeRunner final : public ProcessRunner
{
public:
    QStringList arguments;
    ProcessOutput response;

    ProcessOutput run(const QStringList &requestedArguments) override
    {
        arguments = requestedArguments;
        return response;
    }
};

class ProtonProviderTest final : public QObject
{
    Q_OBJECT

private slots:
    void uploadUsesJsonCliArguments();
    void inspectParsesVerifiedMetadata();
    void commandErrorsAreActionable();
    void rejectsNullMetadataOutput();
};

void ProtonProviderTest::uploadUsesJsonCliArguments()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    ProtonProvider provider(runner);

    QVERIFY(provider.upload(QStringLiteral("/tmp/file.txt"), QStringLiteral("/backups/copy")));
    const QStringList expected {
        QStringLiteral("filesystem"), QStringLiteral("upload"), QStringLiteral("-j"),
        QStringLiteral("/tmp/file.txt"), QStringLiteral("/backups/copy"),
    };

    QCOMPARE(runner.arguments, expected);
}

void ProtonProviderTest::inspectParsesVerifiedMetadata()
{
    FakeRunner runner;
    runner.response.exitCode = 0;
    runner.response.standardOutput = QStringLiteral(R"({"size":17,"sha256":"abc123"})");
    ProtonProvider provider(runner);
    RemoteFile file;

    QVERIFY(provider.inspect(QStringLiteral("/backups/file.txt"), &file));
    QCOMPARE(file.path, QStringLiteral("/backups/file.txt"));
    QCOMPARE(file.size, qint64(17));
    QCOMPARE(file.checksum, QByteArray("abc123"));
}

void ProtonProviderTest::commandErrorsAreActionable()
{
    FakeRunner runner;
    runner.response.exitCode = 1;
    runner.response.standardError = QStringLiteral("not authenticated");
    ProtonProvider provider(runner);
    QString error;

    QVERIFY(!provider.download(QStringLiteral("/remote/file"), QStringLiteral("/tmp/file"), &error));
    QCOMPARE(error, QStringLiteral("not authenticated"));
}

void ProtonProviderTest::rejectsNullMetadataOutput()
{
    FakeRunner runner;
    ProtonProvider provider(runner);
    QString error;

    QVERIFY(!provider.inspect(QStringLiteral("/remote/file"), nullptr, &error));
    QCOMPARE(error, QStringLiteral("A destination for remote file metadata is required."));
    QVERIFY(runner.arguments.isEmpty());
}

QTEST_MAIN(ProtonProviderTest)
#include "protonprovider_test.moc"
