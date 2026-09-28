#include "backupschedule.h"

#include <QtGlobal>

namespace {

QTime scheduleTime(const BackupSchedule &schedule)
{
    return QTime(qBound(0, schedule.hour, 23), qBound(0, schedule.minute, 59));
}

QDateTime localDateTime(const QDate &date, const BackupSchedule &schedule)
{
    return QDateTime(date, scheduleTime(schedule));
}

}

QDateTime BackupScheduleCalculator::nextRun(const BackupSchedule &schedule, const QDateTime &after)
{
    if (!schedule.enabled() || !after.isValid()) {
        return {};
    }

    if (schedule.frequency == QStringLiteral("daily")) {
        QDate date = after.date();
        QDateTime candidate = localDateTime(date, schedule);
        if (candidate <= after) {
            candidate = localDateTime(date.addDays(1), schedule);
        }
        return candidate;
    }

    if (schedule.frequency == QStringLiteral("weekly")) {
        const int weekday = qBound(1, schedule.weekday, 7);
        for (int offset = 0; offset <= 7; ++offset) {
            const QDate date = after.date().addDays(offset);
            const QDateTime candidate = localDateTime(date, schedule);
            if (date.dayOfWeek() == weekday && candidate > after) {
                return candidate;
            }
        }
        return {};
    }

    if (schedule.frequency == QStringLiteral("monthly")) {
        QDate month = QDate(after.date().year(), after.date().month(), 1);
        for (int offset = 0; offset < 24; ++offset) {
            const QDate monthStart = month.addMonths(offset);
            const int day = qBound(1, schedule.dayOfMonth, monthStart.daysInMonth());
            const QDateTime candidate = localDateTime(QDate(monthStart.year(), monthStart.month(), day), schedule);
            if (candidate > after) {
                return candidate;
            }
        }
    }

    return {};
}

QDateTime BackupScheduleCalculator::dueRun(const BackupSchedule &schedule, const QDateTime &now)
{
    if (!schedule.enabled() || !now.isValid()) {
        return {};
    }

    if (schedule.frequency == QStringLiteral("daily")) {
        QDateTime candidate = localDateTime(now.date(), schedule);
        if (candidate > now) {
            candidate = localDateTime(now.date().addDays(-1), schedule);
        }
        return candidate;
    }

    if (schedule.frequency == QStringLiteral("weekly")) {
        const int weekday = qBound(1, schedule.weekday, 7);
        for (int offset = 0; offset <= 7; ++offset) {
            const QDate date = now.date().addDays(-offset);
            const QDateTime candidate = localDateTime(date, schedule);
            if (date.dayOfWeek() == weekday && candidate <= now) {
                return candidate;
            }
        }
        return {};
    }

    if (schedule.frequency == QStringLiteral("monthly")) {
        for (int offset = 0; offset < 24; ++offset) {
            const QDate month = now.date().addMonths(-offset);
            const int day = qBound(1, schedule.dayOfMonth, month.daysInMonth());
            const QDateTime candidate = localDateTime(QDate(month.year(), month.month(), day), schedule);
            if (candidate <= now) {
                return candidate;
            }
        }
    }

    return {};
}

bool BackupScheduleCalculator::isDue(const BackupSchedule &schedule, const QDateTime &lastScheduled, const QDateTime &now)
{
    if (!schedule.enabled() || !now.isValid()) {
        return false;
    }

    const QDateTime due = lastScheduled.isValid()
        ? nextRun(schedule, lastScheduled)
        : dueRun(schedule, now);

    return due.isValid() && due <= now;
}
