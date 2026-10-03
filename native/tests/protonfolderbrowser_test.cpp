#include <QSignalSpy>
#include <QTest>
#include <QFile>
#include <QSemaphore>
#include <QTemporaryDir>
#include <atomic>

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

class BlockingFolderRunner final : public ProcessRunner
{
public:
    QString blockedPath;
    QSemaphore entered;
    QSemaphore proceed;
    std::atomic_int calls {0};

    ProcessOutput run(const QStringList &arguments) override
    {
        ++calls;
        if (arguments.last() == blockedPath) {
            entered.release();
            // Bound the fixture even if an assertion fails before releasing it.
            if (!proceed.tryAcquire(1, 5000)) return {1, {}, QStringLiteral("Fixture timeout")};
        }
        return {0, QStringLiteral(R"({"type":"folder","uid":"volume~node","deprecatedShareId":"share"})"), {}};
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
    void deepCopyUsesOnlyTargetAndRootMetadata();
    void persistsLinksAndOpensImmediatelyAfterRestart();
    void prefetchJoinsOpenWithoutDuplicateLookup();
    void backgroundLookupDoesNotDelayOtherLinks();
    void ignoresMalformedAndForeignCachedUrls();
    void fallsBackToNearerAncestorShare();
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

void ProtonFolderBrowserTest::deepCopyUsesOnlyTargetAndRootMetadata()
{
    FolderRunner runner;
    runner.responses = {
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~copy-node"})"), {}},
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~root","deprecatedShareId":"share"})"), {}},
    };
    const QString path = QStringLiteral("/my-files/backups/computer/documents/copy-id");
    const auto result = ProtonFolderLink::resolve(runner, path);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.url.path(), QStringLiteral("/share/folder/copy-node"));
    QCOMPARE(runner.requests.size(), 2);
    QCOMPARE(runner.requests.first().last(), path);
    QCOMPARE(runner.requests.last().last(), QStringLiteral("/my-files"));
}

void ProtonFolderBrowserTest::persistsLinksAndOpensImmediatelyAfterRestart()
{
    QTemporaryDir state;
    const QString cachePath = state.filePath(QStringLiteral("links.json"));
    const QString path = QStringLiteral("/my-files/backups/copy");
    FolderRunner runner;
    runner.responses = {{0, QStringLiteral(R"({"type":"folder","uid":"volume~copy==","deprecatedShareId":"share=="})"), {}}};
    {
        ProtonFolderBrowser browser(runner, cachePath);
        QSignalSpy opened(&browser, &ProtonFolderBrowser::folderResolved);
        browser.openFolder(path);
        QTRY_COMPARE(opened.count(), 1);
    }
    QCOMPARE(runner.requests.size(), 1);
    FolderRunner offlineRunner;
    ProtonFolderBrowser restarted(offlineRunner, cachePath);
    QSignalSpy opened(&restarted, &ProtonFolderBrowser::folderResolved);
    QSignalSpy busy(&restarted, &ProtonFolderBrowser::busyChanged);
    restarted.openFolder(path);
    // Synchronous delivery proves that no CLI task/event-loop turn is needed.
    QCOMPARE(opened.count(), 1);
    QCOMPARE(opened.first().first().toUrl().path(), QStringLiteral("/share==/folder/copy=="));
    QVERIFY(busy.isEmpty());
    QVERIFY(offlineRunner.requests.isEmpty());
}

void ProtonFolderBrowserTest::prefetchJoinsOpenWithoutDuplicateLookup()
{
    QTemporaryDir state;
    const QString cachePath = state.filePath(QStringLiteral("links.json"));
    BlockingFolderRunner runner;
    runner.blockedPath = QStringLiteral("/my-files/backups/copy");
    ProtonFolderBrowser browser(runner, cachePath);
    QSignalSpy opened(&browser, &ProtonFolderBrowser::folderResolved);
    browser.prefetchFolders({runner.blockedPath, runner.blockedPath});
    QTRY_VERIFY(runner.entered.available() > 0);
    QVERIFY(!browser.busy());
    QVERIFY(opened.isEmpty());
    browser.openFolder(runner.blockedPath);
    QVERIFY(browser.busy());
    runner.proceed.release();
    QTRY_COMPARE(opened.count(), 1);
    QVERIFY(!browser.busy());
    QCOMPARE(runner.calls.load(), 1);
    browser.openFolder(runner.blockedPath);
    QCOMPARE(opened.count(), 2);
    QCOMPARE(runner.calls.load(), 1);
}

void ProtonFolderBrowserTest::backgroundLookupDoesNotDelayOtherLinks()
{
    QTemporaryDir state;
    const QString cachePath = state.filePath(QStringLiteral("links.json"));
    BlockingFolderRunner runner;
    runner.blockedPath = QStringLiteral("/my-files/backups/slow-copy");
    ProtonFolderBrowser browser(runner, cachePath);
    QSignalSpy opened(&browser, &ProtonFolderBrowser::folderResolved);
    browser.prefetchFolders({runner.blockedPath});
    QTRY_VERIFY(runner.entered.available() > 0);
    browser.openFolder(QStringLiteral("/my-files/backups/other-copy"));
    QTRY_COMPARE(opened.count(), 1);
    QCOMPARE(runner.calls.load(), 2);
    browser.openFolder(QStringLiteral("/my-files/backups/other-copy"));
    QCOMPARE(opened.count(), 2);
    QCOMPARE(runner.calls.load(), 2);
    runner.proceed.release();
}

void ProtonFolderBrowserTest::ignoresMalformedAndForeignCachedUrls()
{
    QTemporaryDir state;
    const QString cachePath = state.filePath(QStringLiteral("links.json"));
    QFile cache(cachePath);
    QVERIFY(cache.open(QIODevice::WriteOnly));
    cache.write(R"({"/my-files/copy":"https://example.com/share/folder/node"})");
    cache.close();
    QVERIFY(ProtonFolderLink::cached(cachePath, QStringLiteral("/my-files/copy")).isEmpty());
    FolderRunner runner;
    runner.responses = {{0, QStringLiteral(R"({"type":"folder","uid":"volume~node","deprecatedShareId":"share"})"), {}}};
    auto resolved = ProtonFolderLink::resolve(runner, QStringLiteral("/my-files/copy"), cachePath);
    QVERIFY(resolved.error.isEmpty());
    QCOMPARE(resolved.url.host(), QStringLiteral("drive.proton.me"));
    QCOMPARE(runner.requests.size(), 1);
    QVERIFY(cache.open(QIODevice::WriteOnly | QIODevice::Truncate));
    cache.write("{");
    cache.close();
    QVERIFY(ProtonFolderLink::cached(cachePath, QStringLiteral("/my-files/copy")).isEmpty());
}

void ProtonFolderBrowserTest::fallsBackToNearerAncestorShare()
{
    FolderRunner runner;
    runner.responses = {
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~copy-node"})"), {}},
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~root"})"), {}},
        {0, QStringLiteral(R"({"type":"folder","uid":"volume~parent","deprecatedShareId":"parent-share"})"), {}},
    };
    const auto result = ProtonFolderLink::resolve(runner, QStringLiteral("/my-files/backups/copy"));
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.url.path(), QStringLiteral("/parent-share/folder/copy-node"));
    QCOMPARE(runner.requests.size(), 3);
    QCOMPARE(runner.requests.last().last(), QStringLiteral("/my-files/backups"));
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
