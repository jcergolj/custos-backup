#pragma once

#include "processrunner.h"

class SystemdLauncher
{
public:
    explicit SystemdLauncher(ProcessRunner &runner);

    bool startUserService(const QString &serviceName, QString *error = nullptr);

private:
    ProcessRunner &runner;
};
