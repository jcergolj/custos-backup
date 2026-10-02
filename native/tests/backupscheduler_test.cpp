#include "../src/backupscheduler.h"
#include "../src/backupconfig.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class BackupSchedulerTest final : public QObject
{
    Q_OBJECT

    static bool write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
    }

    static QString cli(const QTemporaryDir &home)
    {
        const QString path = home.filePath("fake systemctl");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};
        file.write(R"(#!/bin/sh
base=${0%/*}
printf '%s\n' "$*" >> "$base/calls"
case "$*" in
  '--user daemon-reload')
    if [ -f "$base/reload-fails" ]; then printf 'Manager unavailable' >&2; exit 1; fi
    ;;
  '--user enable --now custos.timer')
    if [ -f "$base/enable-fails" ]; then printf 'Unit custos.timer is masked' >&2; exit 1; fi
    if [ ! -f "$base/not-active" ]; then printf yes > "$base/active"; fi
    ;;
  '--user show custos.timer --property=LoadState --property=ActiveState --property=UnitFileState')
    if [ -f "$base/query-fails" ]; then printf 'User bus unavailable' >&2; exit 1; fi
    printf 'LoadState=loaded\n'
    if [ -f "$base/active" ]; then
      printf 'ActiveState=active\n'
      if [ -f "$base/session-only" ]; then printf 'UnitFileState=disabled\n'; else printf 'UnitFileState=enabled\n'; fi
    else
      printf 'ActiveState=inactive\nUnitFileState=disabled\n'
    fi
    ;;
  *) exit 2 ;;
esac
exit 0
)");
        if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)) return {};
        return path;
    }

    static bool config(const QTemporaryDir &home, const QString &frequency)
    {
        BackupConfig settings;
        BackupSet set {"documents", "Documents", "/my-files/backups", {"/safe/documents"}, {}};
        set.schedule.frequency = frequency;
        settings.sets = {set};
        return BackupConfigStore(home.filePath("config.json")).save(settings);
    }

    static QByteArray calls(const QTemporaryDir &home)
    {
        QFile file(home.filePath("calls"));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

private slots:
    void savingAnEnabledScheduleActivatesAndVerifiesTimer_data()
    {
        QTest::addColumn<QString>("frequency");
        QTest::newRow("daily") << "daily";
        QTest::newRow("weekly") << "weekly";
        QTest::newRow("monthly") << "monthly";
    }

    void savingAnEnabledScheduleActivatesAndVerifiesTimer()
    {
        QFETCH(QString, frequency);
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "disabled"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        QTRY_COMPARE(scheduler.status(), QString("No scheduled backups"));
        QSignalSpy errors(&scheduler, &BackupScheduler::failed);
        QSignalSpy messages(&scheduler, &BackupScheduler::messageChanged);
        QVERIFY(config(home, frequency));
        scheduler.applySavedSchedules();
        QTRY_VERIFY(!scheduler.busy());
        QVERIFY(scheduler.ready());
        QVERIFY(scheduler.error().isEmpty());
        QVERIFY(scheduler.hasSchedules());
        QVERIFY(errors.isEmpty());
        QCOMPARE(messages.count(), 1);
        const QByteArray commands = calls(home);
        const int reload = commands.indexOf("--user daemon-reload\n");
        const int enable = commands.indexOf("--user enable --now custos.timer\n");
        QVERIFY(reload >= 0 && enable > reload);
        QVERIFY(commands.indexOf("--user show", enable) > enable);
        QVERIFY(!commands.contains("start custos.service"));
    }

    void openingAPausedScheduleOnlyReadsTimerState()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "daily"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        QTRY_COMPARE(scheduler.status(), QString("Scheduling paused"));
        QVERIFY(!scheduler.ready());
        QVERIFY(scheduler.error().contains("Enable scheduling"));
        QVERIFY(scheduler.error().contains("automatically at login"));
        QVERIFY(!calls(home).contains("enable"));
        scheduler.enable();
        QTRY_VERIFY(scheduler.ready() && !scheduler.busy());
    }

    void manualOnlySchedulesNeverEnableTimer()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "disabled"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        QTRY_COMPARE(scheduler.status(), QString("No scheduled backups"));
        QVERIFY(scheduler.error().isEmpty());
        scheduler.applySavedSchedules();
        QTRY_VERIFY(!scheduler.busy());
        QVERIFY(!calls(home).contains("enable"));
        QSignalSpy errors(&scheduler, &BackupScheduler::failed);
        scheduler.enable();
        QCOMPARE(errors.count(), 1);
        QVERIFY(!calls(home).contains("enable"));
    }

    void activationFailuresKeepScheduleSavedAndExposeRetry_data()
    {
        QTest::addColumn<QString>("failureFile");
        QTest::newRow("reload") << "reload-fails";
        QTest::newRow("enable") << "enable-fails";
        QTest::newRow("not active after enable") << "not-active";
    }

    void activationFailuresKeepScheduleSavedAndExposeRetry()
    {
        QFETCH(QString, failureFile);
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "daily"));
        QVERIFY(write(home.filePath(failureFile), "yes"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        QTRY_COMPARE(scheduler.status(), QString("Scheduling paused"));
        QSignalSpy errors(&scheduler, &BackupScheduler::failed);
        scheduler.applySavedSchedules();
        QTRY_VERIFY(!scheduler.busy());
        QVERIFY(!scheduler.ready());
        QVERIFY(!scheduler.error().isEmpty());
        if (failureFile == "enable-fails") {
            QVERIFY(scheduler.error().contains("systemctl --user unmask custos.timer"));
        }
        QCOMPARE(errors.count(), 1);
        const QString activationError = scheduler.error();
        scheduler.refresh();
        QTRY_VERIFY(!scheduler.busy());
        QCOMPARE(scheduler.error(), activationError);
        BackupConfig saved;
        QVERIFY(BackupConfigStore(home.filePath("config.json")).load(&saved));
        QCOMPARE(saved.sets.first().schedule.frequency, QString("daily"));
        QVERIFY(QFile::remove(home.filePath(failureFile)));
        scheduler.enable();
        QTRY_VERIFY(scheduler.ready() && !scheduler.busy());
        QVERIFY(scheduler.error().isEmpty());
    }

    void saveWhileCheckingStillActivatesTimer()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "disabled"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        scheduler.refresh();
        QVERIFY(scheduler.busy());
        QVERIFY(config(home, "daily"));
        scheduler.applySavedSchedules();
        QTRY_VERIFY(scheduler.ready() && !scheduler.busy());
    }

    void missingSystemctlIsReportedWithoutLosingTheSchedule()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "daily"));
        BackupScheduler scheduler(home.filePath("config.json"), home.filePath("missing"));
        QTRY_COMPARE(scheduler.status(), QString("Scheduling needs attention"));
        QSignalSpy errors(&scheduler, &BackupScheduler::failed);
        scheduler.applySavedSchedules();
        QTRY_COMPARE(errors.count(), 1);
        QVERIFY(!scheduler.busy());
        QVERIFY(scheduler.hasSchedules());
        QVERIFY(!scheduler.error().isEmpty());
    }

    void closingAfterSaveFinishesActivation()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "daily"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        {
            BackupScheduler scheduler(home.filePath("config.json"), systemctl);
            scheduler.applySavedSchedules();
            QVERIFY(scheduler.busy());
        }
        QVERIFY(QFile::exists(home.filePath("active")));
    }

    void laterChecksReflectExternalTimerChanges()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "daily"));
        QVERIFY(write(home.filePath("active"), "yes"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        QTRY_VERIFY(scheduler.ready() && !scheduler.busy());
        QVERIFY(scheduler.error().isEmpty());
        QVERIFY(QFile::remove(home.filePath("active")));
        scheduler.refresh();
        QTRY_COMPARE(scheduler.status(), QString("Scheduling paused"));
        QVERIFY(!scheduler.ready());
        QVERIFY(scheduler.error().contains("Enable scheduling"));
    }

    void sessionOnlyTimerExplainsHowToStartAtLogin()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QVERIFY(config(home, "daily"));
        QVERIFY(write(home.filePath("active"), "yes"));
        QVERIFY(write(home.filePath("session-only"), "yes"));
        const QString systemctl = cli(home);
        QVERIFY(!systemctl.isEmpty());
        BackupScheduler scheduler(home.filePath("config.json"), systemctl);
        QTRY_COMPARE(scheduler.status(), QString("Scheduling active for this session only"));
        QVERIFY(!scheduler.ready());
        QVERIFY(scheduler.error().contains("Enable scheduling"));
        QVERIFY(scheduler.error().contains("automatically at login"));
        QVERIFY(!calls(home).contains("enable"));
    }
};

QTEST_GUILESS_MAIN(BackupSchedulerTest)
#include "backupscheduler_test.moc"
