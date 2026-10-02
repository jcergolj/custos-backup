#pragma once

#include <QtGlobal>

struct BackupProgress {
    qint64 totalBytes = 0;
    qint64 processedBytes = 0;
    int totalFiles = 0;
    int processedFiles = 0;
    bool finalizing = false;
};
