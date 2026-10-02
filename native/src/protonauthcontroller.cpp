#include "protonauthcontroller.h"

#include <QStandardPaths>

ProtonAuthController::ProtonAuthController(QString executable, QObject *parent)
    : QObject(parent), executable(std::move(executable))
{
    timeout.setSingleShot(true);
    timeout.setInterval(15000);
    connect(&timeout, &QTimer::timeout, this, [this] {
        timedOut = true;
        probe.kill();
    });
    connect(&probe, &QProcess::stateChanged, this, [this] { emit stateChanged(); });
    connect(&probe, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finishCheck(false, tr("Cannot start the Proton Drive CLI. Install proton-drive and try again."));
        }
    });
    connect(&probe, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const bool success = !timedOut && status == QProcess::NormalExit && exitCode == 0;
        QString message = QString::fromLocal8Bit(probe.readAllStandardError()).trimmed();
        if (message.isEmpty()) {
            message = QString::fromLocal8Bit(probe.readAllStandardOutput()).trimmed();
        }
        if (timedOut) {
            message = tr("Checking Proton Drive timed out. Check your connection and try again.");
        } else if (!success && message.isEmpty()) {
            message = tr("Unable to connect to Proton Drive. Sign in or check your connection.");
        }
        finishCheck(success, success ? QString() : message);
    });
    refreshTimer.setInterval(30000);
    connect(&refreshTimer, &QTimer::timeout, this, &ProtonAuthController::refresh);
    refreshTimer.start();
    QTimer::singleShot(0, this, &ProtonAuthController::refresh);
}

ProtonAuthController::~ProtonAuthController()
{
    disconnect(&probe, nullptr, this, nullptr);
    if (checking()) {
        probe.kill();
        probe.waitForFinished(1000);
    }
}

void ProtonAuthController::refresh()
{
    if (checking()) {
        return;
    }
    timedOut = false;
    timeout.start();
    probe.start(executable, {QStringLiteral("filesystem"), QStringLiteral("info"),
                            QStringLiteral("/my-files"), QStringLiteral("--json")});
}

void ProtonAuthController::finishCheck(bool success, const QString &error)
{
    timeout.stop();
    const bool newlyConnected = success && hasChecked && !connected;
    connected = success;
    hasChecked = true;
    lastError = error;
    emit stateChanged();
    if (newlyConnected) {
        emit statusChanged(tr("Signed in to Proton Drive."));
    }
}

void ProtonAuthController::signIn()
{
    if (connected || checking()) {
        return;
    }
    const QString binary = QStandardPaths::findExecutable(executable);
    if (binary.isEmpty()) {
        emit failed(tr("Install the Proton Drive CLI before signing in."));
        return;
    }
    const QString terminal = QStandardPaths::findExecutable(QStringLiteral("xdg-terminal-exec"));
    if (terminal.isEmpty() || !QProcess::startDetached(terminal, {binary, QStringLiteral("auth"), QStringLiteral("login")})) {
        emit failed(tr("Unable to open a terminal. Run proton-drive auth login in your terminal."));
        return;
    }
    emit statusChanged(tr("Complete Proton sign-in in your browser. Keep the terminal open until it finishes."));
    QTimer::singleShot(2000, this, &ProtonAuthController::refresh);
}
