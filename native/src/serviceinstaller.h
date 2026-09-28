#pragma once

#include <QString>

class ServiceInstaller
{
public:
    explicit ServiceInstaller(QString serviceDirectory);

    bool install(const QString &workerPath, QString *installedPath = nullptr, QString *error = nullptr) const;

private:
    QString serviceDirectory;
};
