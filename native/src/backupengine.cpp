#include "backupengine.h"

#include <QDir>
#include <QFileInfo>
#include <QDirIterator>

BackupEngine::BackupEngine(QObject *parent)
    : QObject(parent)
{
}

bool BackupEngine::validateSelection(const QString &sourceDirectory, QString *error) const
{
    const QFileInfo source(sourceDirectory);

    if (!source.exists()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected folder does not exist.");
        }

        return false;
    }

    if (!source.isDir()) {
        if (error != nullptr) {
            *error = QStringLiteral("The selected path is not a folder.");
        }

        return false;
    }

    if (source.isSymLink()) {
        if (error != nullptr) {
            *error = QStringLiteral("Symbolic links are not valid backup roots.");
        }

        return false;
    }

    return true;
}

QStringList BackupEngine::selectableFiles(const QString &sourceDirectory) const
{
    if (!validateSelection(sourceDirectory)) {
        return {};
    }

    QStringList files;
    QDirIterator iterator(
        sourceDirectory,
        QDir::Files | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories
    );

    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo file = iterator.fileInfo();

        if (!file.isSymLink()) {
            files.append(file.absoluteFilePath());
        }
    }

    files.sort();

    return files;
}
