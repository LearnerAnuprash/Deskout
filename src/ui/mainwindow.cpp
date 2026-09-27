#include "ui/mainwindow.h"

#include "core/pausemanager.h"
#include "core/settingskeys.h"
#include "ui/appicon.h"

#include <QCloseEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

} // namespace

MainWindow::MainWindow(PauseManager *pause, QWidget *parent)
    : QMainWindow(parent)
    , m_pause(pause)
{
    setWindowTitle(QStringLiteral("Deskout"));
    setWindowIcon(AppIcon::icon());
    setMinimumSize(760, 500);

    auto *central = new QWidget;
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildSidebar());

    auto *content = new QWidget;
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(28, 20, 28, 20);
    contentLayout->setSpacing(16);
    contentLayout->addWidget(buildPauseBanner());

    m_pages = new QStackedWidget;
    m_pages->addWidget(buildHomePage());
    m_pages->addWidget(buildPlaceholderPage(tr("Reminders"),
        tr("Eye break, water and walk reminders with custom intervals, days and "
           "working hours, as full-screen alarms or notifications."), 1));
    m_pages->addWidget(buildPlaceholderPage(tr("Focus Timer"),
        tr("A 25-minute research timer you can pin on top, resize and shrink to a mini view."), 3));
    m_pages->addWidget(buildPlaceholderPage(tr("Notes"),
        tr("Quick memos you can create, edit and delete."), 4));
    m_pages->addWidget(buildPlaceholderPage(tr("Topic Docs"),
        tr("A lightweight document editor for longer write-ups, one doc per topic."), 5));
    m_pages->addWidget(buildPlaceholderPage(tr("Daily Updates"),
        tr("Write what you did today; see it first thing tomorrow."), 6));
    m_pages->addWidget(buildPlaceholderPage(tr("Stats"),
        tr("Breaks taken, hydration and focus sessions, plus your current streak."), 7));
    contentLayout->addWidget(m_pages, 1);
    layout->addWidget(content, 1);
    setCentralWidget(central);

    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    connect(m_pause, &PauseManager::pausedChanged, this, &MainWindow::refreshPauseState);
    refreshPauseState();

    if (!restoreGeometry(QSettings().value(SettingsKeys::UiWindowGeometry).toByteArray()))
        resize(960, 620);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings().setValue(SettingsKeys::UiWindowGeometry, saveGeometry());
    if (m_hideOnClose) {
        event->ignore();
        hide();
        Q_EMIT hiddenToTray();
        return;
    }
    QMainWindow::closeEvent(event);
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setAttribute(Qt::WA_StyledBackground);
    sidebar->setFixedWidth(210);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(8);

    auto *brand = new QLabel(QStringLiteral("Deskout"));
    brand->setObjectName(QStringLiteral("SidebarBrand"));
    layout->addWidget(brand);

    m_nav = new QListWidget;
    m_nav->setObjectName(QStringLiteral("SidebarNav"));
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->addItems({tr("Home"), tr("Reminders"), tr("Focus Timer"), tr("Notes"), tr("Topic Docs"),
                     tr("Daily Updates"), tr("Stats")});
    layout->addWidget(m_nav, 1);

    auto *readingMode = new QPushButton(tr("Reading mode"));
    readingMode->setEnabled(false);
    readingMode->setToolTip(tr("Arrives in Phase 2"));
    layout->addWidget(readingMode);

    auto *settings = new QPushButton(tr("Settings"));
    connect(settings, &QPushButton::clicked, this, &MainWindow::settingsRequested);
    layout->addWidget(settings);
    return sidebar;
}

QFrame *MainWindow::buildPauseBanner()
{
    m_pauseBanner = new QFrame;
    m_pauseBanner->setObjectName(QStringLiteral("PauseBanner"));
    auto *layout = new QHBoxLayout(m_pauseBanner);
    layout->setContentsMargins(14, 10, 10, 10);
    m_pauseBannerText = new QLabel;
    layout->addWidget(m_pauseBannerText, 1);
    auto *resume = new QPushButton(tr("Resume"));
    connect(resume, &QPushButton::clicked, m_pause, &PauseManager::resume);
    layout->addWidget(resume);
    return m_pauseBanner;
}

QWidget *MainWindow::buildHomePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(tr("Welcome to Deskout"), "PageTitle"));
    layout->addWidget(makeLabel(tr("Deskout lives in your system tray. Closing this window keeps "
                                   "it running; use Quit from the tray menu to exit."),
                                "Muted"));

    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 14, 18, 16);
    cardLayout->addWidget(makeLabel(tr("Status"), "CardTitle"));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(8);
    m_reminderStatus = makeLabel(QString());
    m_hotkeyStatus = makeLabel(QString());
    m_autoStartStatus = makeLabel(QString());
    m_trayStatus = makeLabel(QString());
    form->addRow(tr("Reminders"), m_reminderStatus);
    form->addRow(tr("Pause shortcut"), m_hotkeyStatus);
    form->addRow(tr("Launch at login"), m_autoStartStatus);
    form->addRow(tr("Tray icon"), m_trayStatus);
    cardLayout->addLayout(form);
    layout->addWidget(card);
    layout->addStretch(1);
    return page;
}

QWidget *MainWindow::buildPlaceholderPage(const QString &title, const QString &description, int phase)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(makeLabel(title, "PageTitle"));
    layout->addWidget(makeLabel(description));
    layout->addWidget(makeLabel(tr("Coming in Phase %1.").arg(phase), "Muted"));
    layout->addStretch(1);
    return page;
}

void MainWindow::refreshPauseState()
{
    const QString status = m_pause->statusText();
    m_pauseBanner->setVisible(m_pause->isPaused());
    m_pauseBannerText->setText(tr("All reminders are muted. %1.").arg(status));
    m_reminderStatus->setText(status);
}

void MainWindow::setHotkeyStatus(const QString &text)
{
    m_hotkeyStatus->setText(text);
}

void MainWindow::setAutoStartStatus(bool enabled)
{
    m_autoStartStatus->setText(enabled ? tr("On") : tr("Off"));
}

void MainWindow::setTrayStatus(const QString &text)
{
    m_trayStatus->setText(text);
}
