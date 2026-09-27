#include "ui/focustimerwindow.h"

#include "core/focustimer.h"
#include "core/settingskeys.h"
#include "ui/appicon.h"
#include "ui/timedisplay.h"

#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

namespace {

// Grab area along the edges of the frameless mini view.
constexpr int ResizeMargin = 6;
constexpr QSize NormalDefaultSize(320, 210);
constexpr QSize NormalMinimumSize(230, 150);
constexpr QSize MiniDefaultSize(170, 64);
constexpr QSize MiniMinimumSize(90, 36);
constexpr int ScreenMargin = 24;

Qt::CursorShape cursorFor(Qt::Edges edges)
{
    const bool left = edges & Qt::LeftEdge;
    const bool right = edges & Qt::RightEdge;
    const bool top = edges & Qt::TopEdge;
    const bool bottom = edges & Qt::BottomEdge;
    if ((left && top) || (right && bottom))
        return Qt::SizeFDiagCursor;
    if ((right && top) || (left && bottom))
        return Qt::SizeBDiagCursor;
    if (left || right)
        return Qt::SizeHorCursor;
    if (top || bottom)
        return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

} // namespace

FocusTimerWindow::FocusTimerWindow(FocusTimer *timer, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_timer(timer)
{
    setWindowIcon(AppIcon::icon());
    // Deskout's lifetime follows the main window and tray, not this one.
    setAttribute(Qt::WA_QuitOnClose, false);
    setMouseTracking(true);

    QSettings settings;
    m_mini = settings.value(SettingsKeys::FocusMiniMode, false).toBool();
    m_alwaysOnTop = settings.value(SettingsKeys::FocusAlwaysOnTop, true).toBool();

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);

    m_caption = new QLabel;
    m_caption->setObjectName(QStringLiteral("Muted"));
    m_caption->setAlignment(Qt::AlignCenter);
    m_caption->setTextFormat(Qt::PlainText);
    m_caption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_caption);

    m_display = new TimeDisplay;
    // Clicks go to the window: drag, double-click and right-click anywhere.
    m_display->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_display, 1);

    m_normalChrome = new QWidget;
    auto *controls = new QHBoxLayout(m_normalChrome);
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(6);
    m_startPause = new QPushButton;
    m_startPause->setObjectName(QStringLiteral("PrimaryButton"));
    connect(m_startPause, &QPushButton::clicked, m_timer, &FocusTimer::toggle);
    controls->addWidget(m_startPause);
    m_stop = new QPushButton(tr("Stop"));
    connect(m_stop, &QPushButton::clicked, m_timer, &FocusTimer::stop);
    controls->addWidget(m_stop);
    controls->addStretch(1);

    m_pin = new QToolButton;
    m_pin->setObjectName(QStringLiteral("TimerToggle"));
    m_pin->setText(tr("On top"));
    m_pin->setToolTip(tr("Keep this timer above other windows"));
    m_pin->setCheckable(true);
    m_pin->setChecked(m_alwaysOnTop);
    connect(m_pin, &QToolButton::toggled, this, &FocusTimerWindow::setAlwaysOnTop);
    controls->addWidget(m_pin);

    m_miniButton = new QToolButton;
    m_miniButton->setObjectName(QStringLiteral("TimerToggle"));
    m_miniButton->setText(tr("Mini"));
    m_miniButton->setToolTip(tr("Shrink to just the time. Double-click it to come back."));
    connect(m_miniButton, &QToolButton::clicked, this, [this] { setMini(true); });
    controls->addWidget(m_miniButton);
    layout->addWidget(m_normalChrome);

    connect(m_timer, &FocusTimer::stateChanged, this, &FocusTimerWindow::refreshState);
    connect(m_timer, &FocusTimer::remainingChanged, this, &FocusTimerWindow::refreshTime);

    // Lay out the saved view without showing anything yet.
    m_caption->setVisible(!m_mini);
    m_normalChrome->setVisible(!m_mini);
    layout->setContentsMargins(m_mini ? QMargins(6, 4, 6, 6) : QMargins(14, 10, 14, 12));
    setMinimumSize(m_mini ? MiniMinimumSize : NormalMinimumSize);
    applyWindowFlags();
    refreshState();
}

void FocusTimerWindow::present()
{
    if (!isVisible())
        restoreGeometryFor(m_mini);
    show();
    raise();
    activateWindow();
}

void FocusTimerWindow::saveState() const
{
    if (!isVisible())
        return;
    QSettings settings;
    settings.setValue(m_mini ? SettingsKeys::FocusMiniGeometry : SettingsKeys::FocusWindowGeometry, saveGeometry());
    settings.setValue(SettingsKeys::FocusMiniMode, m_mini);
}

void FocusTimerWindow::setMini(bool mini)
{
    if (mini == m_mini)
        return;
    const bool visible = isVisible();
    saveState(); // geometry of the view we're leaving

    m_mini = mini;
    QSettings().setValue(SettingsKeys::FocusMiniMode, mini);
    m_caption->setVisible(!mini);
    m_normalChrome->setVisible(!mini);
    layout()->setContentsMargins(mini ? QMargins(6, 4, 6, 6) : QMargins(14, 10, 14, 12));
    setMinimumSize(mini ? MiniMinimumSize : NormalMinimumSize);
    unsetCursor();
    applyWindowFlags(); // hides the window
    restoreGeometryFor(mini);
    refreshTime();
    if (visible)
        present();
}

void FocusTimerWindow::setAlwaysOnTop(bool onTop)
{
    if (onTop == m_alwaysOnTop)
        return;
    m_alwaysOnTop = onTop;
    QSettings().setValue(SettingsKeys::FocusAlwaysOnTop, onTop);
    {
        const QSignalBlocker blocker(m_pin);
        m_pin->setChecked(onTop);
    }
    if (!isVisible()) {
        applyWindowFlags();
        return;
    }
    saveState();
    applyWindowFlags(); // hides the window
    restoreGeometryFor(m_mini);
    present();
}

void FocusTimerWindow::closeEvent(QCloseEvent *event)
{
    saveState();
    event->accept(); // just hides; the timer keeps running
}

void FocusTimerWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QString toggleText;
    switch (m_timer->state()) {
    case FocusTimer::State::Running:
        toggleText = tr("Pause");
        break;
    case FocusTimer::State::Paused:
        toggleText = tr("Resume");
        break;
    case FocusTimer::State::Idle:
        toggleText = tr("Start %1 min").arg(m_timer->lastMinutes());
        break;
    case FocusTimer::State::Finished:
        toggleText = tr("Start again (%1 min)").arg(m_timer->lastMinutes());
        break;
    }
    menu.addAction(toggleText, m_timer, &FocusTimer::toggle);
    menu.addAction(tr("Stop"), m_timer, &FocusTimer::stop)->setEnabled(m_timer->isActive());
    menu.addSeparator();
    // These rebuild or hide the native window, so they run after the menu
    // has closed rather than while it still belongs to this window.
    QAction *onTop = menu.addAction(tr("Keep on top"));
    onTop->setCheckable(true);
    onTop->setChecked(m_alwaysOnTop);
    connect(onTop, &QAction::toggled, this, &FocusTimerWindow::setAlwaysOnTop, Qt::QueuedConnection);
    QAction *mini = menu.addAction(tr("Mini view"));
    mini->setCheckable(true);
    mini->setChecked(m_mini);
    connect(mini, &QAction::toggled, this, &FocusTimerWindow::setMini, Qt::QueuedConnection);
    menu.addSeparator();
    menu.addAction(tr("Open Deskout"), this, &FocusTimerWindow::openAppRequested);
    QAction *hide = menu.addAction(tr("Hide timer"));
    connect(hide, &QAction::triggered, this, &QWidget::close, Qt::QueuedConnection);
    menu.exec(event->globalPos());
}

void FocusTimerWindow::keyPressEvent(QKeyEvent *event)
{
    // A focused button consumes Space itself, so this only sees it
    // otherwise (e.g. in the mini view).
    if (event->key() == Qt::Key_Space) {
        m_timer->toggle();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FocusTimerWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPending = false;
        setMini(!m_mini);
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void FocusTimerWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_mini) {
        const Qt::Edges edges = edgesAt(event->position().toPoint());
        if (edges && windowHandle() && windowHandle()->startSystemResize(edges))
            return;
        // Move only once the pointer actually drags, so a double-click
        // still reaches us.
        m_pressPos = event->position().toPoint();
        m_dragPending = true;
        return;
    }
    QWidget::mousePressEvent(event);
}

void FocusTimerWindow::mouseMoveEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    if (m_dragPending && (event->buttons() & Qt::LeftButton)
        && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragPending = false;
        if (windowHandle())
            windowHandle()->startSystemMove();
        return;
    }
    if (m_mini && event->buttons() == Qt::NoButton)
        setCursor(cursorFor(edgesAt(pos)));
    QWidget::mouseMoveEvent(event);
}

void FocusTimerWindow::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragPending = false;
    QWidget::mouseReleaseEvent(event);
}

void FocusTimerWindow::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    if (!m_mini)
        return;
    // Frameless: draw a hairline so the mini view doesn't melt into
    // whatever is behind it.
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

Qt::Edges FocusTimerWindow::edgesAt(const QPoint &pos) const
{
    Qt::Edges edges;
    if (!m_mini)
        return edges;
    if (pos.x() < ResizeMargin)
        edges |= Qt::LeftEdge;
    if (pos.x() >= width() - ResizeMargin)
        edges |= Qt::RightEdge;
    if (pos.y() < ResizeMargin)
        edges |= Qt::TopEdge;
    if (pos.y() >= height() - ResizeMargin)
        edges |= Qt::BottomEdge;
    return edges;
}

void FocusTimerWindow::applyWindowFlags()
{
    Qt::WindowFlags flags = Qt::Window;
    if (m_mini)
        flags |= Qt::FramelessWindowHint;
    if (m_alwaysOnTop)
        flags |= Qt::WindowStaysOnTopHint;
    if (windowFlags() != flags)
        setWindowFlags(flags);
}

void FocusTimerWindow::restoreGeometryFor(bool mini)
{
    const QByteArray saved =
        QSettings().value(mini ? SettingsKeys::FocusMiniGeometry : SettingsKeys::FocusWindowGeometry).toByteArray();
    if (restoreGeometry(saved))
        return;
    // First time: top-right corner of the screen the pointer is on.
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QSize size = mini ? MiniDefaultSize : NormalDefaultSize;
    resize(size);
    if (screen) {
        const QRect available = screen->availableGeometry();
        move(available.right() - size.width() - ScreenMargin, available.top() + ScreenMargin);
    }
}

void FocusTimerWindow::refreshState()
{
    switch (m_timer->state()) {
    case FocusTimer::State::Idle:
        m_startPause->setText(tr("Start"));
        m_caption->setText(m_timer->topic().isEmpty() ? tr("Ready") : m_timer->topic());
        break;
    case FocusTimer::State::Running:
        m_startPause->setText(tr("Pause"));
        m_caption->setText(m_timer->topic().isEmpty() ? tr("Focusing") : m_timer->topic());
        break;
    case FocusTimer::State::Paused:
        m_startPause->setText(tr("Resume"));
        m_caption->setText(tr("Paused"));
        break;
    case FocusTimer::State::Finished:
        m_startPause->setText(tr("Start again"));
        m_caption->setText(tr("Session complete"));
        break;
    }
    m_stop->setEnabled(m_timer->isActive());
    refreshTime();
}

void FocusTimerWindow::refreshTime()
{
    const FocusTimer::State state = m_timer->state();
    const QString time = FocusTimer::formatSeconds(m_timer->remainingSeconds());
    m_display->setText(time);
    m_display->setProgress(state == FocusTimer::State::Idle ? -1.0 : m_timer->progress());
    m_display->setFillRatio(m_mini ? 0.64 : 0.62);
    const QPalette pal = palette();
    if (state == FocusTimer::State::Running || state == FocusTimer::State::Finished)
        m_display->setColor(pal.color(QPalette::Highlight));
    else if (state == FocusTimer::State::Paused)
        m_display->setColor(pal.color(QPalette::PlaceholderText));
    else
        m_display->setColor(QColor());

    setWindowTitle(m_timer->isActive() ? tr("%1 · Focus timer").arg(time) : tr("Focus timer"));
    setToolTip(m_mini ? tr("Double-click for controls, right-click for the menu, drag to move.") : QString());
}
