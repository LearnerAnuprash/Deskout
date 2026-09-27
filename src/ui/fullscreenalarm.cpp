#include "ui/fullscreenalarm.h"

#include "core/eyeexercises.h"
#include "ui/appicon.h"

#include <QCloseEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

namespace {

// Buttons ignore input briefly so a keystroke or click already in progress
// when the alarm appears can't dismiss it by accident.
constexpr int InputGraceMs = 1500;
constexpr int AutoCloseAfterExerciseMs = 5000;

// The alarm keeps its own calm dark look regardless of the app theme.
const char AlarmStyle[] = R"(
QWidget#AlarmRoot { background: #0F1B1A; }
QWidget#AlarmRoot QLabel { color: #E8F3F1; background: transparent; }
QLabel#AlarmTitle { font-size: 40px; font-weight: 600; }
QLabel#AlarmBody { font-size: 19px; color: #B5CFCB; }
QLabel#AlarmCaption { font-size: 14px; color: #8FB1AC; letter-spacing: 1px; }
QLabel#AlarmCountdown { font-size: 76px; font-weight: 300; }
QWidget#AlarmRoot QPushButton {
    font-size: 16px; padding: 12px 26px; border-radius: 10px;
    border: 1px solid #3C5A56; background: #1B2E2C; color: #E8F3F1;
}
QWidget#AlarmRoot QPushButton:hover { background: #24403C; }
QWidget#AlarmRoot QPushButton:disabled { color: #5E7773; border-color: #2A3F3C; }
QWidget#AlarmRoot QPushButton#AlarmPrimary {
    background: #2BB5A2; border-color: #2BB5A2; color: #0E1A18; font-weight: 600;
}
QWidget#AlarmRoot QPushButton#AlarmPrimary:hover { background: #35C9B4; }
QWidget#AlarmRoot QPushButton#AlarmPrimary:disabled { background: #1E5E56; color: #0E1A18; }
QWidget#AlarmRoot QPushButton#AlarmLink {
    background: transparent; border: none; color: #8FB1AC; font-size: 14px;
    padding: 6px; text-decoration: underline;
}
QWidget#AlarmRoot QPushButton#AlarmLink:hover { color: #E8F3F1; }
QWidget#AlarmRoot QPushButton#AlarmLink:disabled { color: #4A6360; }
)";

QLabel *label(const QString &text, const char *objectName)
{
    auto *l = new QLabel(text);
    l->setObjectName(QLatin1String(objectName));
    l->setAlignment(Qt::AlignCenter);
    l->setWordWrap(true);
    return l;
}

QPushButton *button(const QString &text, const char *objectName = nullptr)
{
    auto *b = new QPushButton(text);
    if (objectName)
        b->setObjectName(QLatin1String(objectName));
    b->setCursor(Qt::PointingHandCursor);
    // No default/auto-default: Enter must not confirm by accident.
    b->setAutoDefault(false);
    b->setDefault(false);
    return b;
}

QString formatSeconds(int seconds)
{
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

// Frameless, always-on-top, full-screen window that refuses to close
// unless the alarm closes it.
class AlarmWindow : public QWidget
{
public:
    AlarmWindow()
        : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setObjectName(QStringLiteral("AlarmRoot"));
        setAttribute(Qt::WA_StyledBackground);
        setStyleSheet(QLatin1String(AlarmStyle));
        setWindowTitle(QStringLiteral("Deskout"));
        setWindowIcon(AppIcon::icon());
    }

    void closeForReal()
    {
        m_allowClose = true;
        close();
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (m_allowClose)
            event->accept();
        else
            event->ignore();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        // Swallow Escape and friends; only the buttons end the alarm.
        event->accept();
    }

private:
    bool m_allowClose = false;
};

FullScreenAlarm::FullScreenAlarm(const Alert &alert, QObject *parent)
    : QObject(parent)
    , m_alert(alert)
{
    m_exerciseTimer.setInterval(1000);
    connect(&m_exerciseTimer, &QTimer::timeout, this, &FullScreenAlarm::tickExercise);
    m_autoClose.setSingleShot(true);
    m_autoClose.setInterval(AutoCloseAfterExerciseMs);
    connect(&m_autoClose, &QTimer::timeout, this, &FullScreenAlarm::finish);
}

FullScreenAlarm::~FullScreenAlarm()
{
    for (const QPointer<AlarmWindow> &window : std::as_const(m_windows)) {
        if (window) {
            window->closeForReal();
            delete window;
        }
    }
}

void FullScreenAlarm::show()
{
    QScreen *mainScreen = QGuiApplication::screenAt(QCursor::pos());
    if (!mainScreen)
        mainScreen = QGuiApplication::primaryScreen();

    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        auto *window = new AlarmWindow;
        auto *layout = new QVBoxLayout(window);
        layout->setContentsMargins(48, 48, 48, 32);
        if (screen == mainScreen) {
            m_stack = new QStackedWidget;
            m_stack->addWidget(buildPromptPage());
            m_stack->addWidget(buildExercisePage());
            layout->addWidget(m_stack);
        } else {
            layout->addWidget(buildSecondaryContent());
        }

        window->setGeometry(screen->geometry());
        window->winId(); // create the native window so we can pick its screen
        if (QWindow *handle = window->windowHandle())
            handle->setScreen(screen);
        window->showFullScreen();
        m_windows << window;
        if (screen == mainScreen) {
            window->raise();
            window->activateWindow();
        }
    }

    for (QPushButton *b : std::as_const(m_promptButtons))
        b->setEnabled(false);
    QTimer::singleShot(InputGraceMs, this, [this] {
        for (QPushButton *b : std::as_const(m_promptButtons))
            b->setEnabled(true);
    });
}

void FullScreenAlarm::dismiss()
{
    finish();
}

QWidget *FullScreenAlarm::buildPromptPage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(18);
    layout->addStretch(3);

    auto *icon = new QLabel;
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(AppIcon::icon().pixmap(96, 96));
    layout->addWidget(icon);
    layout->addWidget(label(m_alert.title, "AlarmTitle"));
    layout->addWidget(label(m_alert.body, "AlarmBody"));
    layout->addSpacing(20);

    auto *row = new QHBoxLayout;
    row->setSpacing(14);
    row->addStretch(1);
    auto *confirm = button(m_alert.confirmLabel, "AlarmPrimary");
    confirm->setProperty("actionKey", QLatin1String(Alert::ConfirmKey));
    connect(confirm, &QPushButton::clicked, this, &FullScreenAlarm::onConfirm);
    row->addWidget(confirm);
    m_promptButtons = {confirm};
    for (const AlertAction &action : std::as_const(m_alert.actions)) {
        auto *extra = button(action.label);
        extra->setProperty("actionKey", action.key);
        connect(extra, &QPushButton::clicked, this, [this, key = action.key] {
            m_answered = true;
            Q_EMIT responded(key);
            finish();
        });
        row->addWidget(extra);
        m_promptButtons << extra;
    }
    row->addStretch(1);
    layout->addLayout(row);
    layout->addStretch(4);

    auto *disable = button(m_alert.disableFullScreenLabel, "AlarmLink");
    disable->setToolTip(tr("Show this as a notification from now on"));
    auto *disableRow = new QHBoxLayout;
    disableRow->addStretch(1);
    disableRow->addWidget(disable);
    disableRow->addStretch(1);
    layout->addLayout(disableRow);
    m_promptButtons << disable;

    connect(disable, &QPushButton::clicked, this, [this] {
        m_answered = true;
        Q_EMIT fullScreenDisabled();
        finish();
    });
    return page;
}

QWidget *FullScreenAlarm::buildExercisePage()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(18);
    layout->addStretch(3);
    layout->addWidget(label(tr("EYE EXERCISE"), "AlarmCaption"));
    m_exerciseTitle = label(QString(), "AlarmTitle");
    layout->addWidget(m_exerciseTitle);

    m_exerciseText = label(QString(), "AlarmBody");
    m_exerciseText->setFixedWidth(720);
    auto *textRow = new QHBoxLayout;
    textRow->addStretch(1);
    textRow->addWidget(m_exerciseText, 0);
    textRow->addStretch(1);
    layout->addLayout(textRow);

    m_countdown = label(QString(), "AlarmCountdown");
    layout->addWidget(m_countdown);

    m_exerciseButton = button(tr("Skip"));
    auto *row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(m_exerciseButton);
    row->addStretch(1);
    layout->addLayout(row);
    layout->addStretch(4);

    connect(m_exerciseButton, &QPushButton::clicked, this, &FullScreenAlarm::finish);
    return page;
}

QWidget *FullScreenAlarm::buildSecondaryContent()
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->addStretch(1);
    layout->addWidget(label(m_alert.title, "AlarmTitle"));
    layout->addWidget(label(tr("Use the main screen to continue."), "AlarmBody"));
    layout->addStretch(1);
    return page;
}

void FullScreenAlarm::onConfirm()
{
    m_answered = true;
    Q_EMIT responded(QLatin1String(Alert::ConfirmKey));
    if (m_alert.eyeExercise)
        startExercise();
    else
        finish();
}

void FullScreenAlarm::startExercise()
{
    const EyeExercise exercise = EyeExercises::takeNext();
    m_exerciseTitle->setText(exercise.title);
    m_exerciseText->setText(exercise.instructions);
    m_secondsLeft = exercise.seconds;
    m_countdown->setText(formatSeconds(m_secondsLeft));
    m_stack->setCurrentIndex(1);
    m_exerciseTimer.start();
}

void FullScreenAlarm::tickExercise()
{
    if (--m_secondsLeft > 0) {
        m_countdown->setText(formatSeconds(m_secondsLeft));
        return;
    }
    m_exerciseTimer.stop();
    m_countdown->setText(tr("Well done"));
    m_exerciseButton->setText(tr("Done"));
    m_exerciseButton->setObjectName(QStringLiteral("AlarmPrimary"));
    // Re-polish so the new object name picks up the primary style.
    m_exerciseButton->style()->unpolish(m_exerciseButton);
    m_exerciseButton->style()->polish(m_exerciseButton);
    m_autoClose.start();
}

void FullScreenAlarm::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    m_exerciseTimer.stop();
    m_autoClose.stop();
    for (const QPointer<AlarmWindow> &window : std::as_const(m_windows)) {
        if (window)
            window->closeForReal();
    }
    Q_EMIT finished();
}
