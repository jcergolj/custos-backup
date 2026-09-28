#pragma once

#include <QString>

struct BackupConfig {
    QString sourceDirectory;
    QString remoteRoot;
    QString protonBinary = QStringLiteral("proton-drive");
};

class BackupConfigStore
{
public:
    explicit BackupConfigStore(QString path);

    bool load(BackupConfig *config, QString *error = nullptr) const;
    bool save(const BackupConfig &config, QString *error = nullptr) const;

private:
    QString path;
};
