#include "themecolors.h"

#include <QColor>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace {
QStringList themePaths()
{
    return {
        QDir(qEnvironmentVariable("XDG_STATE_HOME", QDir::home().filePath(QStringLiteral(".local/state"))))
            .filePath(QStringLiteral("omarchy/current/theme")),
        QDir(qEnvironmentVariable("XDG_CONFIG_HOME", QDir::home().filePath(QStringLiteral(".config"))))
            .filePath(QStringLiteral("omarchy/current/theme"))
    };
}

QColor blend(const QColor &background, const QColor &foreground, double amount)
{
    return QColor::fromRgbF(background.redF() * (1 - amount) + foreground.redF() * amount,
                           background.greenF() * (1 - amount) + foreground.greenF() * amount,
                           background.blueF() * (1 - amount) + foreground.blueF() * amount);
}

QColor contrastText(const QColor &background)
{
    const auto linear = [](double channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const double luminance = 0.2126 * linear(background.redF()) + 0.7152 * linear(background.greenF())
        + 0.0722 * linear(background.blueF());
    return luminance > 0.179 ? QColor(Qt::black) : QColor(Qt::white);
}

QVariantMap systemColors()
{
    const QPalette palette = QGuiApplication::palette();
    return {{"background", palette.color(QPalette::Window)},
            {"foreground", palette.color(QPalette::WindowText)},
            {"muted", palette.color(QPalette::Disabled, QPalette::Text)},
            {"surface", palette.color(QPalette::AlternateBase)},
            {"border", palette.color(QPalette::Mid)},
            {"accent", palette.color(QPalette::Highlight)},
            {"highlight", palette.color(QPalette::Highlight)},
            {"highlightedText", palette.color(QPalette::HighlightedText)},
            {"brightText", palette.color(QPalette::BrightText)}};
}
}

ThemeColors::ThemeColors(QObject *parent) : ThemeColors(themePaths(), parent) {}

ThemeColors::ThemeColors(QStringList directories, QObject *parent)
    : QObject(parent), themeDirectories(std::move(directories))
{
    debounce.setSingleShot(true);
    debounce.setInterval(100);
    connect(&watcher, &QFileSystemWatcher::fileChanged, &debounce, qOverload<>(&QTimer::start));
    connect(&watcher, &QFileSystemWatcher::directoryChanged, &debounce, qOverload<>(&QTimer::start));
    connect(&debounce, &QTimer::timeout, this, &ThemeColors::reload);
    qGuiApp->installEventFilter(this);
    reload();
}

bool ThemeColors::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        debounce.start();
    }
    return QObject::eventFilter(watched, event);
}

void ThemeColors::reload()
{
    QVariantMap colors = systemColors();
    QSet<QString> paths;
    const QRegularExpression assignment(QStringLiteral(R"(^\s*([a-z_0-9]+)\s*=\s*["'](#[0-9a-fA-F]{6})["']\s*(?:#.*)?$)"));
    bool loaded = false;
    for (const QString &directory : themeDirectories) {
        const QString filePath = QDir(directory).filePath(QStringLiteral("colors.toml"));
        if (QFileInfo::exists(filePath)) {
            paths.insert(filePath);
        }
        // Omarchy replaces the entire theme directory. Watch its ancestors as well
        // so atomic swaps and first-time theme creation can re-arm the file watch.
        QDir ancestor(directory);
        while (!ancestor.isRoot()) {
            if (ancestor.exists()) {
                paths.insert(ancestor.absolutePath());
            }
            ancestor = QDir(QFileInfo(ancestor.absolutePath()).absolutePath());
        }
        QFile file(filePath);
        if (loaded || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        QMap<QString, QColor> values;
        while (!file.atEnd()) {
            const auto match = assignment.match(QString::fromUtf8(file.readLine()).trimmed());
            if (match.hasMatch()) {
                values.insert(match.captured(1), QColor(match.captured(2)));
            }
        }
        if (!values.contains("background") || !values.contains("foreground") || !values.contains("accent")) {
            continue;
        }
        const QColor background = values.value("background");
        const QColor foreground = values.value("foreground");
        const QColor highlight = values.value("selection", values.value("accent"));
        colors = {{"background", background}, {"foreground", foreground},
                  {"muted", values.value("muted", values.value("dark_foreground", blend(background, foreground, 0.65)))},
                  {"surface", values.value("lighter_background", blend(background, foreground, 0.08))},
                  {"border", blend(background, foreground, 0.22)},
                  {"accent", values.value("accent")}, {"highlight", highlight},
                  {"highlightedText", values.value("selection_foreground", contrastText(highlight))},
                  {"brightText", values.value("bright_foreground", foreground)}};
        loaded = true;
    }
    const QStringList oldPaths = watcher.files() + watcher.directories();
    if (!oldPaths.isEmpty()) {
        watcher.removePaths(oldPaths);
    }
    if (!paths.isEmpty()) {
        watcher.addPaths(paths.values());
    }
    if (colors != currentColors) {
        currentColors = colors;
        emit colorsChanged();
    }
}
