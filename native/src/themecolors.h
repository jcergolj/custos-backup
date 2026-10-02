#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class ThemeColors final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY colorsChanged)

public:
    explicit ThemeColors(QObject *parent = nullptr);
    explicit ThemeColors(QStringList themeDirectories, QObject *parent = nullptr);
    QVariantMap colors() const { return currentColors; }

signals:
    void colorsChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void reload();
    QStringList themeDirectories;
    QFileSystemWatcher watcher;
    QTimer debounce;
    QVariantMap currentColors;
};
