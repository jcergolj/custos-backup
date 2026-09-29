#include "qprocessrunner.h"

#include <QProcess>

namespace {

constexpr int processTimeoutMilliseconds = 5 * 60 * 1000;

}

QProcessRunner::QProcessRunner(QString executable)
    : executable(std::move(executable))
{
}

ProcessOutput QProcessRunner::run(const QStringList &arguments)
{
    QProcess process;
    process.start(executable, arguments);

    if (!process.waitForFinished(processTimeoutMilliseconds)) {
        process.kill();
        process.waitForFinished();
        return {
            -1,
            QString::fromLocal8Bit(process.readAllStandardOutput()),
            QStringLiteral("The Proton Drive command timed out."),
        };
    }

    return {
        process.exitCode(),
        QString::fromLocal8Bit(process.readAllStandardOutput()),
        QString::fromLocal8Bit(process.readAllStandardError()),
    };
}
