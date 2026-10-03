#pragma once

#include <QtGlobal>
#include <QString>

struct BackupProgress {
    qint64 totalBytes = 0;
    qint64 processedBytes = 0;
    int totalFiles = 0;
    int processedFiles = 0;
    bool finalizing = false;
    int verifiedFiles = 0;
    qint64 verifiedBytes = 0;
    int failedItems = 0;
    QString currentFile;
    qint64 currentFileBytes = 0;
    QString phase;
};

// Only display samples are throttled. Durable run-status transitions are saved
// independently by the worker. The elapsed time is supplied by its monotonic clock.
class BackupProgressPersistence final
{
public:
    bool shouldSave(const BackupProgress &progress, qint64 elapsedMs)
    {
        const bool finalizing = progress.finalizing && !savedFinalizing;
        const bool finished = progress.totalFiles > 0
            && progress.processedFiles == progress.totalFiles && !savedFinished;
        if (lastSavedMs >= 0 && !finalizing && !finished && elapsedMs - lastSavedMs < 1000) {
            return false;
        }
        lastSavedMs = elapsedMs;
        savedFinalizing = savedFinalizing || progress.finalizing;
        savedFinished = savedFinished || finished;
        return true;
    }

private:
    qint64 lastSavedMs = -1;
    bool savedFinalizing = false;
    bool savedFinished = false;
};
