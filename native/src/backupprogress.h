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
