#pragma once

#include <QString>
#include <QStringList>

struct ProcessOutput {
    int exitCode = -1;
    QString standardOutput;
    QString standardError;

    bool successful() const { return exitCode == 0; }
};

class ProcessRunner
{
public:
    virtual ~ProcessRunner() = default;
    virtual ProcessOutput run(const QStringList &arguments) = 0;
};
