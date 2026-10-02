#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

class ProtonAuthController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY stateChanged)
    Q_PROPERTY(bool checked READ checked NOTIFY stateChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)

public:
    explicit ProtonAuthController(QString executable, QObject *parent = nullptr);
    ~ProtonAuthController() override;
    bool authenticated() const { return connected; }
    bool checked() const { return hasChecked; }
    bool checking() const { return probe.state() != QProcess::NotRunning; }
    QString error() const { return lastError; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void signIn();

signals:
    void stateChanged();
    void statusChanged(const QString &message);
    void failed(const QString &error);

private:
    void finishCheck(bool success, const QString &error = {});
    QString executable;
    QProcess probe;
    QTimer timeout;
    QTimer refreshTimer;
    bool connected = false;
    bool hasChecked = false;
    bool timedOut = false;
    QString lastError;
};
