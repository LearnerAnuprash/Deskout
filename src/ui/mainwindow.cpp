#include "ui/mainwindow.h"

#include "core/pausemanager.h"
#include "core/settingskeys.h"
#include "ui/appicon.h"
#include "ui/docspage.h"
#include "ui/focuspage.h"
#include "ui/notespage.h"
#include "ui/reminderspage.h"
#include "ui/updatespage.h"

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

MainWindow::MainWindow(const Context &context, QWidget *parent)
    : QMainWindow(parent)
    , m_pause(context.pause)
    , m_reminders(context.reminders)
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
    auto *remindersPage = new RemindersPage(m_reminders);
    connect(remindersPage, &RemindersPage::editRequested, this, &MainWindow::reminderSettingsRequested);
    m_pages->addWidget(remindersPage);
    auto *focusPage = new FocusPage(context.focus, context.focusLog);
    connect(focusPage, &FocusPage::popOutRequested, this, &MainWindow::focusWindowRequested);
    m_pages->addWidget(focusPage);
    m_pages->addWidget(new NotesPage(context.notes));
    m_pages->addWidget(new DocsPage(context.docs));
    m_updatesPage = new UpdatesPage(context.updates);
    m_pages->addWidget(m_updatesPage);
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

    m_readingModeButton = new QPushButton(tr("Reading mode"));
    m_readingModeButton->setObjectName(QStringLiteral("ReadingModeButton"));
    m_readingModeButton->setCheckable(true);
    connect(m_readingModeButton, &QPushButton::clicked, this, &MainWindow::readingModeToggled);
    layout->addWidget(m_readingModeButton);

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
    form->addRow(tr("Reminders"), m_reminderStatus);
    const QList<std::pair<StatusRow, QString>> rows = {
        {StatusRow::Hotkey, tr("Pause shortcut")},
        {StatusRow::AutoStart, tr("Launch at login")},
        {StatusRow::Tray, tr("Tray icon")},
        {StatusRow::Notifications, tr("Notifications")},
        {StatusRow::IdleDetection, tr("Away detection")},
        {StatusRow::FullscreenDetection, tr("Meeting detection")},
        {StatusRow::ReadingMode, tr("Reading mode")},
    };
    for (const auto &[row, title] : rows) {
        QLabel *value = makeLabel(QString());
        m_statusRows.insert(row, value);
        form->addRow(title, value);
    }
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

void MainWindow::setReadingMode(bool checked, const QString &toolTip)
{
    const QSignalBlocker blocker(m_readingModeButton);
    m_readingModeButton->setChecked(checked);
    m_readingModeButton->setToolTip(toolTip);
}

void MainWindow::showPage(Page page)
{
    m_nav->setCurrentRow(int(page));
    if (page == Page::Updates)
        m_updatesPage->focusToday();
}

void MainWindow::setStatus(StatusRow row, const QString &text)
{
    if (QLabel *label = m_statusRows.value(row))
        label->setText(text);
}
