#pragma once

#include "processrunner.h"

class QProcessRunner final : public ProcessRunner
{
public:
    explicit QProcessRunner(QString executable);
    ProcessOutput run(const QStringList &arguments) override;

private:
    QString executable;
};
