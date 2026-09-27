#include "ui/theme.h"

#include "core/settingskeys.h"

#include <QApplication>
#include <QPalette>
#include <QProcess>
#include <QSettings>
#include <QStyleFactory>
#include <QStyleHints>

namespace {

bool g_dark = false;

struct Colors
{
    QColor window, base, alternateBase, text, mutedText, button, border, accent, accentText;
    QColor bannerBackground, bannerText;
};

const Colors LightColors = {
    QColor("#F6F7F9"), QColor("#FFFFFF"), QColor("#EDF0F3"), QColor("#1F2933"), QColor("#6B7785"),
    QColor("#FFFFFF"), QColor("#D9DEE4"), QColor("#1F9D8B"), QColor("#FFFFFF"),
    QColor("#FEF3C7"), QColor("#92400E"),
};

const Colors DarkColors = {
    QColor("#1C1F24"), QColor("#24282E"), QColor("#2A2F36"), QColor("#E6E9ED"), QColor("#8D98A5"),
    QColor("#2E333A"), QColor("#3A4048"), QColor("#2BB5A2"), QColor("#0E1A18"),
    QColor("#3A2E12"), QColor("#FCD34D"),
};

bool isDarkColor(const QColor &color)
{
    return color.lightness() < 128;
}

bool systemPrefersDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown)
        return scheme == Qt::ColorScheme::Dark;
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
    // Qt < 6.5 can't read the GNOME preference, so ask gsettings directly.
    for (const char *key : {"color-scheme", "gtk-theme"}) {
        QProcess process;
        process.start(QStringLiteral("gsettings"),
                      {QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"),
                       QLatin1String(key)});
        if (process.waitForFinished(1000) && process.exitCode() == 0) {
            const QString value = QString::fromUtf8(process.readAllStandardOutput()).toLower();
            if (value.contains(QLatin1String("dark")))
                return true;
            if (value.contains(QLatin1String("light")))
                return false;
        }
    }
#endif
    // Fall back to the platform's default palette.
    static const bool paletteDark = isDarkColor(QApplication::palette().color(QPalette::Window));
    return paletteDark;
}

QPalette paletteFor(const Colors &c)
{
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.alternateBase);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::PlaceholderText, c.mutedText);
    p.setColor(QPalette::Button, c.button);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::ToolTipBase, c.base);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.border.darker(130));
    p.setColor(QPalette::Light, c.base.lighter(110));
    p.setColor(QPalette::Midlight, c.alternateBase);
    p.setColor(QPalette::Shadow, QColor(0, 0, 0, 80));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, c.mutedText);
    return p;
}

QString styleSheetFor(const Colors &c)
{
    return QStringLiteral(R"(
QWidget#Sidebar { background: %1; border-right: 1px solid %2; }
QLabel#SidebarBrand { font-size: 18px; font-weight: 600; padding: 4px 8px 12px 8px; }
QListWidget#SidebarNav { background: transparent; border: none; outline: 0; font-size: 14px; }
QListWidget#SidebarNav::item { padding: 8px 10px; margin: 1px 0; border-radius: 6px; color: %3; }
QListWidget#SidebarNav::item:hover:!selected { background: %4; }
QListWidget#SidebarNav::item:selected { background: %5; color: %6; }
QLabel#PageTitle { font-size: 22px; font-weight: 600; }
QLabel#Muted { color: %7; }
QFrame#Card { background: %8; border: 1px solid %2; border-radius: 10px; }
QLabel#CardTitle { font-weight: 600; }
QPushButton#ReadingModeButton { background: %8; border: 1px solid %2; border-radius: 4px; padding: 5px; }
QPushButton#ReadingModeButton:hover { border-color: %5; }
QPushButton#ReadingModeButton:checked { background: %5; border-color: %5; color: %6; }
QToolButton#DayToggle { background: %8; border: 1px solid %2; border-radius: 6px; padding: 4px 8px; }
QToolButton#DayToggle:checked { background: %5; border-color: %5; color: %6; }
QFrame#PauseBanner { background: %9; border-radius: 8px; }
QFrame#PauseBanner QLabel { color: %10; font-weight: 600; }
)")
        .arg(c.alternateBase.name(), c.border.name(), c.text.name(), c.button.name(),
             c.accent.name(), c.accentText.name(), c.mutedText.name(), c.base.name(),
             c.bannerBackground.name())
        .arg(c.bannerText.name());
}

} // namespace

namespace Theme {

Mode savedMode()
{
    const QString value = QSettings().value(SettingsKeys::UiTheme).toString();
    if (value == QLatin1String("light"))
        return Mode::Light;
    if (value == QLatin1String("dark"))
        return Mode::Dark;
    return Mode::System;
}

void saveMode(Mode mode)
{
    const char *value = mode == Mode::Light ? "light" : mode == Mode::Dark ? "dark" : "system";
    QSettings().setValue(SettingsKeys::UiTheme, QLatin1String(value));
}

void apply(Mode mode)
{
    // Must read the platform palette before Fusion replaces it.
    const bool dark = mode == Mode::Dark || (mode == Mode::System && systemPrefersDark());
    const Colors &colors = dark ? DarkColors : LightColors;

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(paletteFor(colors));
    qApp->setStyleSheet(styleSheetFor(colors));
    g_dark = dark;
}

void applySaved()
{
    apply(savedMode());
}

bool isDark()
{
    return g_dark;
}

} // namespace Theme
