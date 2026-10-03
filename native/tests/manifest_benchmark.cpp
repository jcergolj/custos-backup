#include "backupmanifest.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>

// Opt-in loader timings, with fixture construction outside the measured region.
// No timing threshold: hardware and filesystem conditions are environment-specific.
int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    QJsonArray measurements;
    for (const int count : {1000, 10000}) {
        QJsonArray entries, expected, failed;
        for (int index = 0; index < count; ++index) {
            const QString path = QStringLiteral("projects/documents/verified-%1.txt").arg(index);
            const QString failure = QStringLiteral("projects/documents/failed-%1.txt").arg(index);
            expected.append(path);
            expected.append(failure);
            failed.append(failure);
            entries.append(QJsonObject {{"source", "/source/" + path}, {"remote", "/copy/" + path},
                {"restore", path}, {"size", 7}, {"sha256", QString(64, '0')}});
        }
        const QByteArray contents = QJsonDocument(QJsonObject {{"version", 2}, {"application", "omacustos"},
            {"computer", "computer"}, {"set_id", "documents"}, {"copy_id", "copy"},
            {"created_at", "2026-10-03T12:00:00.000Z"}, {"status", "incomplete"},
            {"expected", expected}, {"failed", failed}, {"entries", entries}}).toJson(QJsonDocument::Compact);
        QFile file(directory.filePath(QStringLiteral("manifest.json")));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) return 1;
        file.close();
        QVector<double> samples;
        for (int iteration = 0; iteration < 6; ++iteration) {
            QVector<BackupEntry> loaded;
            QString error;
            QElapsedTimer timer;
            timer.start();
            if (!BackupManifest::load(file.fileName(), &loaded, &error) || loaded.size() != count) {
                qCritical().noquote() << error;
                return 1;
            }
            const double elapsed = timer.nsecsElapsed() / 1000000.0;
            if (iteration > 0) samples.append(elapsed); // One warm-up, five samples.
        }
        std::sort(samples.begin(), samples.end());
        measurements.append(QJsonObject {{"verified_entries", count}, {"failed_items", count},
            {"manifest_bytes", contents.size()}, {"samples", samples.size()},
            {"median_ms", samples.at(samples.size() / 2)}, {"worst_ms", samples.last()}});
    }
    qInfo().noquote() << QJsonDocument(QJsonObject {{"manifest_load", measurements}}).toJson(QJsonDocument::Indented);
    return 0;
}
