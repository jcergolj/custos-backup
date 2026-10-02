#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

// Persists global worker limits in a user-systemd drop-in, shared by manual and
// scheduled runs. A running worker keeps its current limits until its next start.
class ResourceUsage final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList names READ names CONSTANT)
    Q_PROPERTY(QStringList descriptions READ descriptions CONSTANT)
    Q_PROPERTY(int presetIndex READ presetIndex NOTIFY presetChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit ResourceUsage(QObject *parent = nullptr);
    ResourceUsage(QString serviceDirectory, QString systemctl, QObject *parent = nullptr);
    ~ResourceUsage() override;
    QStringList names() const;
    QStringList descriptions() const;
    int presetIndex() const { return savedIndex; }
    bool busy() const { return applying; }
    Q_INVOKABLE void save(int index);

signals:
    void presetChanged();
    void busyChanged();
    void statusChanged(const QString &message);
    void failed(const QString &error);

private:
    static QByteArray contents(int index);
    bool write(const QByteArray &data);
    void finish(bool success, const QString &error = {});
    QString dropInPath;
    QString systemctl;
    QProcess reload;
    QTimer timeout;
    int savedIndex = 0;
    int pendingIndex = 0;
    bool applying = false;
    bool hadPreviousFile = false;
    bool timedOut = false;
    QByteArray previousContents;
};
