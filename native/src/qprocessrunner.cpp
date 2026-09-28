#include "qprocessrunner.h"

#include <QProcess>

QProcessRunner::QProcessRunner(QString executable)
    : executable(std::move(executable))
{
}

ProcessOutput QProcessRunner::run(const QStringList &arguments)
{
    QProcess process;
    process.start(executable, arguments);

    if (!process.waitForFinished()) {
        return {-1, QString::fromLocal8Bit(process.readAllStandardOutput()), process.errorString()};
    }

    return {
        process.exitCode(),
        QString::fromLocal8Bit(process.readAllStandardOutput()),
        QString::fromLocal8Bit(process.readAllStandardError()),
    };
}
