#pragma once

#include "processrunner.h"

struct ProcessTimeouts {
    int metadataMilliseconds = 5 * 60 * 1000;
    int transferMilliseconds = 24 * 60 * 60 * 1000;

    static ProcessTimeouts fromEnvironment();
};

class QProcessRunner final : public ProcessRunner
{
public:
    explicit QProcessRunner(QString executable, ProcessTimeouts timeouts = ProcessTimeouts::fromEnvironment());
    ProcessOutput run(const QStringList &arguments) override;

private:
    QString executable;
    ProcessTimeouts timeouts;
};
