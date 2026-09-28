#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class BackupEngine final : public QObject
{
    Q_OBJECT

public:
    explicit BackupEngine(QObject *parent = nullptr);

    Q_INVOKABLE bool validateSelection(const QString &sourceDirectory, QString *error = nullptr) const;
    Q_INVOKABLE QStringList selectableFiles(const QString &sourceDirectory) const;
};
