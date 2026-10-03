#pragma once

#include "processrunner.h"

#include <QString>
#include <QUrl>

struct ProtonFolderLookup {
    QUrl url;
    QString error;
};

class ProtonFolderLink final
{
public:
    static QString cachePath(const QString &configPath);
    static bool validPath(const QString &remotePath);
    static QUrl cached(const QString &cachePath, const QString &remotePath);
    static ProtonFolderLookup resolve(ProcessRunner &runner, const QString &remotePath,
        const QString &cachePath = {});
};
