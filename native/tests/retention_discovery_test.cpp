#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

#include "../src/backupcatalog.h"
#include "../src/backupengine.h"
#include "../src/backupecleanup.h"
#include "../src/localprovider.h"

class RecordingProvider final : public BackupProvider
{
public:
    QStringList calls;
    bool failPermanentDelete = false;

    bool upload(const QString &, const QString &, QString *) override { return true; }
    bool ensureDirectory(const QString &, QString *) override { return true; }
    bool download(const QString &, const QString &, QString *) override { return true; }
    bool inspect(const QString &, RemoteFile *, QString *) override { return true; }
    bool list(const QString &, QVector<RemoteItem> *, QString *) override { return true; }
    bool trash(const QString &path, QString *) override
    {
        calls.append(QStringLiteral("trash:%1").arg(path));
        return true;
    }
    bool permanentlyDelete(const QString &path, QString *error) override
    {
        calls.append(QStringLiteral("delete:%1").arg(path));
        if (failPermanentDelete) {
            if (error != nullptr) {
                *error = QStringLiteral("permanent delete failed");
            }
            failPermanentDelete = false;
            return false;
        }
        return true;
    }
};

class RetentionDiscoveryTest final : public QObject
{
    Q_OBJECT

private slots:
    void keepsNewestSuccessfulCopiesAndIncompleteCopiesAreEligible();
    void firstCleanupDecisionRemainsPendingUntilConfirmed();
    void cleanupUsesExactTargetsAndResumesAfterPermanentDeleteFailure();
    void discoversCompleteAndIncompleteCopiesWithoutLocalState();
};

void RetentionDiscoveryTest::keepsNewestSuccessfulCopiesAndIncompleteCopiesAreEligible()
{
    const QDateTime base(QDate(2026, 9, 1), QTime(12, 0), Qt::UTC);
    QVector<RemoteCopy> copies;
    for (int index = 0; index < 4; ++index) {
        copies.append({
            QStringLiteral("computer/set/copy-%1").arg(index), {}, QStringLiteral("computer"), QStringLiteral("set-id"),
            QStringLiteral("Set"), QStringLiteral("copy-%1").arg(index), QStringLiteral("complete"), base.addDays(index), {}, {}, {},
        });
    }
    copies.append({
        QStringLiteral("computer/set/incomplete"), {}, QStringLiteral("computer"), QStringLiteral("set-id"),
        QStringLiteral("Set"), QStringLiteral("incomplete"), QStringLiteral("incomplete"), base.addDays(5), {}, {}, {},
    });

    const QStringList expectedTargets {
        QStringLiteral("computer/set/copy-0"),
        QStringLiteral("computer/set/incomplete"),
    };
    QCOMPARE(BackupCleanup::eligibleTargets(copies, 3), expectedTargets);
}

void RetentionDiscoveryTest::cleanupUsesExactTargetsAndResumesAfterPermanentDeleteFailure()
{
    QTemporaryDir stateDirectory;
    QVERIFY(stateDirectory.isValid());
    CleanupStore store(stateDirectory.filePath(QStringLiteral("cleanup.json")));
    store.setPending(QStringLiteral("set-id"), {QStringLiteral("computer/set/old")});
    store.confirm(QStringLiteral("set-id"));
    QVERIFY(store.save());

    RecordingProvider provider;
    provider.failPermanentDelete = true;
    QString error;
    QVERIFY(!BackupCleanup::apply(provider, store, QStringLiteral("set-id"), &error));
    const QStringList firstCalls {
        QStringLiteral("trash:computer/set/old"),
        QStringLiteral("delete:computer/set/old"),
    };
    QCOMPARE(provider.calls, firstCalls);
    QVERIFY(error.contains(QStringLiteral("permanent delete failed")));

    CleanupStore reloaded(stateDirectory.filePath(QStringLiteral("cleanup.json")));
    QVERIFY(reloaded.load(&error));
    QVERIFY(BackupCleanup::apply(provider, reloaded, QStringLiteral("set-id"), &error));
    const QStringList allCalls {
        QStringLiteral("trash:computer/set/old"),
        QStringLiteral("delete:computer/set/old"),
        QStringLiteral("delete:computer/set/old"),
    };
    QCOMPARE(provider.calls, allCalls);
    QVERIFY(reloaded.state(QStringLiteral("set-id")).targets.isEmpty());
}

void RetentionDiscoveryTest::firstCleanupDecisionRemainsPendingUntilConfirmed()
{
    QTemporaryDir stateDirectory;
    QVERIFY(stateDirectory.isValid());
    const QString path = stateDirectory.filePath(QStringLiteral("cleanup.json"));
    CleanupStore store(path);
    store.setPending(QStringLiteral("set-id"), {QStringLiteral("computer/set/old")});
    QVERIFY(store.save());

    CleanupStore reloaded(path);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.state(QStringLiteral("set-id")).decision, QStringLiteral("pending"));
    const QStringList expectedTargets {QStringLiteral("computer/set/old")};
    QCOMPARE(reloaded.state(QStringLiteral("set-id")).targets, expectedTargets);
    reloaded.confirm(QStringLiteral("set-id"));
    QVERIFY(reloaded.save());

    CleanupStore confirmed(path);
    QVERIFY(confirmed.load());
    QCOMPARE(confirmed.state(QStringLiteral("set-id")).decision, QStringLiteral("confirmed"));
}

void RetentionDiscoveryTest::discoversCompleteAndIncompleteCopiesWithoutLocalState()
{
    QTemporaryDir source;
    QTemporaryDir remote;
    QVERIFY(source.isValid());
    QVERIFY(remote.isValid());
    QFile file(source.filePath(QStringLiteral("notes.txt")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("notes");
    file.close();

    BackupEngine engine;
    LocalProvider provider(remote.path());
    const BackupCopyMetadata completeMetadata {
        QStringLiteral("old-computer"), QStringLiteral("set-id"), QStringLiteral("Documents"),
        QStringLiteral("complete-copy"), QDateTime::currentDateTimeUtc().addDays(-1),
    };
    QString manifestPath;
    QString error;
    QVERIFY(engine.backup({source.path()}, QStringLiteral("backups/old-computer/Documents/complete-copy"), {}, completeMetadata, provider, &manifestPath, &error));

    const BackupCopyMetadata incompleteMetadata {
        QStringLiteral("old-computer"), QStringLiteral("set-id"), QStringLiteral("Documents"),
        QStringLiteral("incomplete-copy"), QDateTime::currentDateTimeUtc(),
    };
    QVERIFY(!engine.backup(
        {source.path(), source.filePath(QStringLiteral("missing.txt"))},
        QStringLiteral("backups/old-computer/Documents/incomplete-copy"), {}, incompleteMetadata, provider, &manifestPath, &error));

    const BackupCopyMetadata otherComputerMetadata {
        QStringLiteral("other-computer"), QStringLiteral("other-set-id"), QStringLiteral("Documents"),
        QStringLiteral("complete-copy"), QDateTime::currentDateTimeUtc().addDays(-2),
    };
    QVERIFY(engine.backup(
        {source.path()}, QStringLiteral("backups/other-computer/Documents/complete-copy"), {},
        otherComputerMetadata, provider, &manifestPath, &error));

    QVERIFY(QDir().mkpath(remote.filePath(QStringLiteral("backups/unrelated"))));
    QFile unrelated(remote.filePath(QStringLiteral("backups/unrelated/personal.txt")));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.write("do not delete");
    unrelated.close();

    QVector<RemoteCopy> copies;
    QVERIFY(BackupCatalog::discover(provider, QStringLiteral("backups"), &copies, &error));
    QCOMPARE(copies.size(), 3);
    QVERIFY(std::any_of(copies.cbegin(), copies.cend(), [](const RemoteCopy &copy) {
        return copy.computerName == QStringLiteral("other-computer") && copy.setId == QStringLiteral("other-set-id");
    }));
    const auto incomplete = std::find_if(copies.cbegin(), copies.cend(), [](const RemoteCopy &copy) {
        return copy.status == QStringLiteral("incomplete");
    });
    QVERIFY(incomplete != copies.cend());
    QCOMPARE(incomplete->setId, QStringLiteral("set-id"));
    QCOMPARE(incomplete->entries.size(), 1);
    QCOMPARE(incomplete->failedItems.size(), 1);
    QVERIFY(incomplete->failedItems.first().endsWith(QStringLiteral("/missing.txt")));
}

QTEST_MAIN(RetentionDiscoveryTest)
#include "retention_discovery_test.moc"
