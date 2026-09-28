#pragma once

#include "processrunner.h"

class SystemdLauncher
{
public:
    explicit SystemdLauncher(ProcessRunner &runner);

    bool startUserService(const QString &serviceName, QString *error = nullptr);
    bool enableUserTimer(const QString &timerName, QString *error = nullptr);

private:
    ProcessRunner &runner;
};
