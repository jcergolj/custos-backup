#pragma once

#include "backupprovider.h"
#include "processrunner.h"

class ProtonProvider final : public BackupProvider
{
public:
    explicit ProtonProvider(ProcessRunner &runner);

    bool upload(const QString &localPath, const QString &remotePath, QString *error = nullptr) override;
    bool download(const QString &remotePath, const QString &localPath, QString *error = nullptr) override;
    bool inspect(const QString &remotePath, RemoteFile *file, QString *error = nullptr) override;

private:
    bool run(const QStringList &arguments, QString *error) const;
    ProcessRunner &runner;
};
