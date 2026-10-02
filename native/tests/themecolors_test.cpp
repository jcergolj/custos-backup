#include "../src/themecolors.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPalette>
#include <QTemporaryDir>
#include <QWindow>
#include <QtTest>

class ThemeColorsTest final : public QObject
{
    Q_OBJECT

    static void writePalette(const QString &directory, const QByteArray &background,
                             const QByteArray &extra = {})
    {
        QVERIFY(QDir().mkpath(directory));
        QFile file(QDir(directory).filePath("colors.toml"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("background = '" + background + "'\nforeground = '#eeeeee'\naccent = '#8877ee'\n" + extra);
    }

private slots:
    void followsAtomicDirectoryReplacementAndSubsequentEdits()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString themePath = home.filePath("current/theme");
        writePalette(themePath, "#112233");
        ThemeColors theme(QStringList{themePath});
        QCOMPARE(theme.colors().value("background").value<QColor>(), QColor("#112233"));
        QSignalSpy changes(&theme, &ThemeColors::colorsChanged);
        writePalette(home.filePath("next-theme"), "#445566",
                     "selection = '#eeeeee'\nselection_foreground = '#101010'\nmuted = '#999999'\n");
        QVERIFY(QDir(themePath).removeRecursively());
        QVERIFY(QDir().rename(home.filePath("next-theme"), themePath));
        QTRY_COMPARE(theme.colors().value("background").value<QColor>(), QColor("#445566"));
        QCOMPARE(theme.colors().value("highlightedText").value<QColor>(), QColor("#101010"));
        QCOMPARE(theme.colors().value("muted").value<QColor>(), QColor("#999999"));
        writePalette(themePath, "#667788");
        QTRY_COMPARE(theme.colors().value("background").value<QColor>(), QColor("#667788"));
        QVERIFY(changes.count() >= 2);
    }

    void discoversNewThemeAndSupportsLegacyPath()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        const QString modern = home.filePath("state/omarchy/current/theme");
        const QString legacy = home.filePath("config/omarchy/current/theme");
        writePalette(legacy, "#223344");
        ThemeColors theme(QStringList{modern, legacy});
        QCOMPARE(theme.colors().value("background").value<QColor>(), QColor("#223344"));
        writePalette(modern, "#334455");
        QTRY_COMPARE(theme.colors().value("background").value<QColor>(), QColor("#334455"));
    }

    void invalidOrMissingThemeUsesLiveSystemPalette()
    {
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QFile invalid(home.filePath("colors.toml"));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("background = '#invalid'\nforeground = '#eeeeee'\naccent = '#8877ee'\n");
        invalid.close();
        ThemeColors theme(QStringList{home.path()});
        QWindow window;
        QCOMPARE(theme.colors().value("background").value<QColor>(), QGuiApplication::palette().color(QPalette::Window));
        const QPalette original = QGuiApplication::palette();
        QPalette changed = original;
        changed.setColor(QPalette::Window, QColor("#123456"));
        QGuiApplication::setPalette(changed);
        QTRY_COMPARE(theme.colors().value("background").value<QColor>(), QColor("#123456"));
        QGuiApplication::setPalette(original);
    }
};

QTEST_MAIN(ThemeColorsTest)
#include "themecolors_test.moc"
