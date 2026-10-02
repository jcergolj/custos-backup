#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

class BackupScheduler final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool hasSchedules READ hasSchedules NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    explicit BackupScheduler(QString configPath, QString systemctl = QStringLiteral("systemctl"), QObject *parent = nullptr);
    ~BackupScheduler() override;
    bool busy() const { return working; }
    bool ready() const { return active && enabled && lastError.isEmpty(); }
    bool hasSchedules() const { return scheduled; }
    QString status() const;
    QString error() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void enable();
    void applySavedSchedules();

signals:
    void stateChanged();
    void messageChanged(const QString &message);
    void failed(const QString &error);

private:
    enum class Phase { Check, Reload, Enable, Verify };
    bool readSchedules();
    void start(Phase phase);
    void completed(bool success, const QString &output, const QString &error);
    void finish();
    QString configPath;
    QString systemctl;
    QProcess process;
    QTimer timeout;
    QTimer poll;
    Phase phase = Phase::Check;
    bool working = false;
    bool checked = false;
    bool active = false;
    bool enabled = false;
    bool scheduled = false;
    bool activationPending = false;
    bool timedOut = false;
    QString lastError;
    QString activationError;
};
