#include <QSignalSpy>
#include <QTest>

#include "../src/protonfolderbrowser.h"

class FolderRunner final : public ProcessRunner
{
public:
    QVector<ProcessOutput> responses;
    QVector<QStringList> requests;

    ProcessOutput run(const QStringList &arguments) override
    {
        requests.append(arguments);
        if (responses.isEmpty()) {
            return {1, {}, QStringLiteral("Unexpected command")};
        }
        return responses.takeFirst();
    }
};

class ProtonFolderBrowserTest final : public QObject
{
    Q_OBJECT

private slots:
    void resolvesAncestorShareForTargetFolder();
    void reportsMetadataAndCommandFailures_data();
    void reportsMetadataAndCommandFailures();
    void rejectsUnsafePathsWithoutRunningCommands();
};

void ProtonFolderBrowserTest::resolvesAncestorShareForTargetFolder()
{
    FolderRunner runner;
    runner.responses = {
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~backup-node=="})"), {}},
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~root-node","deprecatedShareId":"root-share=="})"), {}},
    };
    ProtonFolderBrowser browser(runner);
    QSignalSpy resolvedSpy(&browser, &ProtonFolderBrowser::folderResolved);
    QSignalSpy failureSpy(&browser, &ProtonFolderBrowser::failed);

    browser.openFolder(QStringLiteral("/my-files/backups"));
    QVERIFY(browser.busy());
    browser.openFolder(QStringLiteral("/my-files/another-folder"));
    QTRY_COMPARE(resolvedSpy.count(), 1);
    QVERIFY(!browser.busy());
    QVERIFY(failureSpy.isEmpty());
    const QUrl url = resolvedSpy.first().first().toUrl();
    QCOMPARE(url.scheme(), QStringLiteral("https"));
    QCOMPARE(url.host(), QStringLiteral("drive.proton.me"));
    QCOMPARE(url.path(), QStringLiteral("/root-share==/folder/backup-node=="));
    QCOMPARE(runner.requests, (QVector<QStringList> {
        {QStringLiteral("filesystem"), QStringLiteral("info"), QStringLiteral("-j"), QStringLiteral("/my-files/backups")},
        {QStringLiteral("filesystem"), QStringLiteral("info"), QStringLiteral("-j"), QStringLiteral("/my-files")},
    }));
}

void ProtonFolderBrowserTest::reportsMetadataAndCommandFailures_data()
{
    QTest::addColumn<QString>("metadata");
    QTest::addColumn<QString>("rootMetadata");
    QTest::addColumn<QString>("commandError");
    QTest::addColumn<QString>("expectedError");
    const QString invalidMetadata = QStringLiteral("Proton Drive returned invalid folder metadata.");
    QTest::newRow("authentication error") << QString() << QString() << QStringLiteral("not authenticated") << QStringLiteral("not authenticated");
    QTest::newRow("invalid JSON") << QStringLiteral("{") << QString() << QString() << invalidMetadata;
    QTest::newRow("not a folder") << QStringLiteral(R"({"type":"file","uid":"volume~file"})") << QString() << QString() << invalidMetadata;
    QTest::newRow("missing UID") << QStringLiteral(R"({"type":"folder"})") << QString() << QString() << invalidMetadata;
    QTest::newRow("malformed UID") << QStringLiteral(R"({"type":"folder","uid":"volume~node~extra"})") << QString() << QString() << invalidMetadata;
    QTest::newRow("wrong root volume") << QStringLiteral(R"({"type":"folder","uid":"volume~node"})")
        << QStringLiteral(R"({"type":"folder","uid":"other-volume~root","deprecatedShareId":"share"})") << QString() << invalidMetadata;
    QTest::newRow("missing share ID") << QStringLiteral(R"({"type":"folder","uid":"volume~node"})")
        << QStringLiteral(R"({"type":"folder","uid":"volume~root"})") << QString()
        << QStringLiteral("The Proton Drive folder's browser link is unavailable.");
}

void ProtonFolderBrowserTest::reportsMetadataAndCommandFailures()
{
    QFETCH(QString, metadata);
    QFETCH(QString, rootMetadata);
    QFETCH(QString, commandError);
    QFETCH(QString, expectedError);
    FolderRunner runner;
    runner.responses.append({commandError.isEmpty() ? 0 : 1, metadata, commandError});
    if (!rootMetadata.isEmpty()) {
        runner.responses.append({0, rootMetadata, {}});
    }
    ProtonFolderBrowser browser(runner);
    QSignalSpy resolvedSpy(&browser, &ProtonFolderBrowser::folderResolved);
    QSignalSpy failureSpy(&browser, &ProtonFolderBrowser::failed);

    browser.openFolder(QStringLiteral("/my-files/backups"));
    QTRY_COMPARE(failureSpy.count(), 1);
    QCOMPARE(failureSpy.first().first().toString(), expectedError);
    QVERIFY(resolvedSpy.isEmpty());
    QVERIFY(!browser.busy());
}

void ProtonFolderBrowserTest::rejectsUnsafePathsWithoutRunningCommands()
{
    FolderRunner runner;
    ProtonFolderBrowser browser(runner);
    QSignalSpy failureSpy(&browser, &ProtonFolderBrowser::failed);
    browser.openFolder(QStringLiteral("/my-files/../outside"));
    QTRY_COMPARE(failureSpy.count(), 1);
    QVERIFY(runner.requests.isEmpty());
}

QTEST_GUILESS_MAIN(ProtonFolderBrowserTest)
#include "protonfolderbrowser_test.moc"
