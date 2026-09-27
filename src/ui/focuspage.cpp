#include "ui/focuspage.h"

#include "core/focuslog.h"
#include "core/focustimer.h"
#include "core/settingskeys.h"
#include "ui/timedisplay.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int Presets[] = {15, 25, 45, 60};
constexpr int RecentCount = 5;

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

QString formatMinutes(int seconds)
{
    const int minutes = (seconds + 30) / 60;
    if (minutes < 60)
        return QCoreApplication::translate("FocusPage", "%1 min").arg(minutes);
    return QCoreApplication::translate("FocusPage", "%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

QString formatWhen(const QDateTime &when)
{
    const QLocale locale;
    const QString time = locale.toString(when.time(), QLocale::ShortFormat);
    const QDate today = QDate::currentDate();
    if (when.date() == today)
        return time;
    if (when.date() == today.addDays(-1))
        return QCoreApplication::translate("FocusPage", "Yesterday %1").arg(time);
    return locale.toString(when.date(), QStringLiteral("ddd d MMM")) + QLatin1Char(' ') + time;
}

} // namespace

FocusPage::FocusPage(FocusTimer *timer, FocusLog *log, QWidget *parent)
    : QWidget(parent)
    , m_timer(timer)
    , m_log(log)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(tr("Focus Timer"), "PageTitle"));
    layout->addWidget(makeLabel(tr("A countdown for research and deep work. Pop it out to keep a small "
                                   "timer on top of the windows you're reading."),
                                "Muted"));
    layout->addWidget(buildTimerCard());
    layout->addWidget(buildHistoryCard());
    layout->addStretch(1);

    connect(m_timer, &FocusTimer::stateChanged, this, &FocusPage::refreshState);
    connect(m_timer, &FocusTimer::remainingChanged, this, &FocusPage::refreshTime);
    connect(m_log, &FocusLog::changed, this, &FocusPage::refreshHistory);
    refreshState();
    refreshHistory();
}

QWidget *FocusPage::buildTimerCard()
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 16);
    layout->setSpacing(10);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setHorizontalSpacing(16);
    m_topic = new QLineEdit(m_timer->lastTopic());
    m_topic->setPlaceholderText(tr("What are you researching? (optional)"));
    m_topic->setMaxLength(200);
    form->addRow(tr("Topic"), m_topic);

    auto *durationRow = new QHBoxLayout;
    durationRow->setSpacing(6);
    for (int minutes : Presets) {
        auto *preset = new QToolButton;
        preset->setObjectName(QStringLiteral("PresetButton"));
        preset->setText(tr("%1 min").arg(minutes));
        preset->setCursor(Qt::PointingHandCursor);
        connect(preset, &QToolButton::clicked, this, [this, minutes] { m_minutes->setValue(minutes); });
        durationRow->addWidget(preset);
        m_presets << preset;
    }
    durationRow->addSpacing(8);
    m_minutes = new QSpinBox;
    m_minutes->setRange(FocusTimer::MinMinutes, FocusTimer::MaxMinutes);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setValue(m_timer->lastMinutes());
    m_minutes->setAccessibleName(tr("Custom duration"));
    durationRow->addWidget(m_minutes);
    durationRow->addStretch(1);
    form->addRow(tr("Duration"), durationRow);
    layout->addLayout(form);

    connect(m_topic, &QLineEdit::textEdited, this, &FocusPage::onInputsEdited);
    connect(m_minutes, qOverload<int>(&QSpinBox::valueChanged), this, &FocusPage::onInputsEdited);

    m_display = new TimeDisplay;
    m_display->setFixedHeight(120);
    m_display->setFillRatio(0.62);
    layout->addWidget(m_display);

    m_caption = makeLabel(QString(), "Muted");
    m_caption->setAlignment(Qt::AlignCenter);
    m_caption->setTextFormat(Qt::PlainText); // shows the user's topic
    layout->addWidget(m_caption);

    auto *buttons = new QHBoxLayout;
    m_start = new QPushButton;
    m_start->setObjectName(QStringLiteral("PrimaryButton"));
    m_start->setMinimumWidth(110);
    connect(m_start, &QPushButton::clicked, this, &FocusPage::onStartClicked);
    buttons->addWidget(m_start);
    m_stop = new QPushButton(tr("Stop"));
    m_stop->setToolTip(tr("End this session early"));
    connect(m_stop, &QPushButton::clicked, m_timer, &FocusTimer::stop);
    buttons->addWidget(m_stop);
    buttons->addStretch(1);
    auto *popOut = new QPushButton(tr("Pop out timer"));
    popOut->setToolTip(tr("A small, resizable timer window that can stay on top of other windows"));
    connect(popOut, &QPushButton::clicked, this, &FocusPage::popOutRequested);
    buttons->addWidget(popOut);
    layout->addLayout(buttons);

    m_fullScreenAlert = new QCheckBox(tr("Full-screen alert when a session ends"));
    m_fullScreenAlert->setToolTip(tr("Off: a notification instead. Either way, nothing is shown while "
                                     "Deskout is paused, and a notification is used while another "
                                     "app is full-screen."));
    connect(m_fullScreenAlert, &QCheckBox::toggled, this,
            [](bool on) { QSettings().setValue(SettingsKeys::FocusFullScreenAlert, on); });
    layout->addWidget(m_fullScreenAlert);
    return card;
}

QWidget *FocusPage::buildHistoryCard()
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 16);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(tr("Today"), "CardTitle"));
    m_todaySummary = makeLabel(QString());
    layout->addWidget(m_todaySummary);
    m_recentLayout = new QVBoxLayout;
    m_recentLayout->setSpacing(4);
    layout->addLayout(m_recentLayout);
    return card;
}

void FocusPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // May have been changed from the alarm's "Disable full-screen alert".
    const QSignalBlocker blocker(m_fullScreenAlert);
    m_fullScreenAlert->setChecked(QSettings().value(SettingsKeys::FocusFullScreenAlert,
                                                    SettingsKeys::FocusFullScreenAlertDefault).toBool());
    // "Today" may have become yesterday since the last refresh.
    refreshHistory();
}

void FocusPage::onStartClicked()
{
    switch (m_timer->state()) {
    case FocusTimer::State::Running:
        m_timer->pause();
        break;
    case FocusTimer::State::Paused:
        m_timer->resume();
        break;
    case FocusTimer::State::Idle:
    case FocusTimer::State::Finished:
        m_timer->start(m_minutes->value(), m_topic->text());
        break;
    }
}

void FocusPage::onInputsEdited()
{
    if (!m_timer->isActive())
        m_timer->setNext(m_minutes->value(), m_topic->text());
}

void FocusPage::refreshState()
{
    const FocusTimer::State state = m_timer->state();
    const bool active = m_timer->isActive();

    m_topic->setEnabled(!active);
    m_minutes->setEnabled(!active);
    for (QWidget *preset : std::as_const(m_presets))
        preset->setEnabled(!active);
    if (active) {
        // Show what is actually running, e.g. when started from the tray.
        const QSignalBlocker topicBlocker(m_topic);
        const QSignalBlocker minutesBlocker(m_minutes);
        m_topic->setText(m_timer->topic());
        m_minutes->setValue(int(m_timer->plannedMs() / 60000));
    }
    m_stop->setEnabled(active);

    QString caption;
    switch (state) {
    case FocusTimer::State::Idle:
        m_start->setText(tr("Start"));
        caption = tr("Ready when you are.");
        break;
    case FocusTimer::State::Running:
        m_start->setText(tr("Pause"));
        caption = m_timer->topic().isEmpty() ? tr("Focusing…") : tr("Focusing on “%1”").arg(m_timer->topic());
        break;
    case FocusTimer::State::Paused:
        m_start->setText(tr("Resume"));
        caption = tr("Paused");
        break;
    case FocusTimer::State::Finished:
        m_start->setText(tr("Start again"));
        caption = tr("Session complete. Nice work!");
        break;
    }
    m_caption->setText(caption);
    refreshTime();
}

void FocusPage::refreshTime()
{
    const FocusTimer::State state = m_timer->state();
    m_display->setText(FocusTimer::formatSeconds(m_timer->remainingSeconds()));
    m_display->setProgress(state == FocusTimer::State::Idle ? -1.0 : m_timer->progress());
    const QPalette pal = palette();
    if (state == FocusTimer::State::Running || state == FocusTimer::State::Finished)
        m_display->setColor(pal.color(QPalette::Highlight));
    else if (state == FocusTimer::State::Paused)
        m_display->setColor(pal.color(QPalette::PlaceholderText));
    else
        m_display->setColor(QColor());
}

void FocusPage::refreshHistory()
{
    const FocusDaySummary today = m_log->summaryFor(QDate::currentDate());
    if (today.completed == 0 && today.stoppedEarly == 0) {
        m_todaySummary->setText(tr("No focus sessions yet today."));
    } else {
        QString text = today.completed == 1 ? tr("1 session completed")
                                            : tr("%1 sessions completed").arg(today.completed);
        text += QStringLiteral(" · ") + tr("%1 focused").arg(formatMinutes(today.focusedSeconds));
        if (today.stoppedEarly > 0)
            text += QStringLiteral(" · ") + tr("%1 stopped early").arg(today.stoppedEarly);
        m_todaySummary->setText(text);
    }

    while (QLayoutItem *item = m_recentLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QList<FocusSession> recent = m_log->recent(RecentCount);
    if (recent.isEmpty())
        return;
    m_recentLayout->addWidget(makeLabel(tr("Recent sessions"), "Muted"));
    for (const FocusSession &session : recent) {
        auto *row = new QWidget;
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(16);

        auto *when = new QLabel(formatWhen(session.startedAt));
        when->setObjectName(QStringLiteral("Muted"));
        when->setMinimumWidth(120);
        rowLayout->addWidget(when);

        const QString length = session.completed
                                   ? formatMinutes(session.focusedSeconds)
                                   : tr("%1 of %2").arg(formatMinutes(session.focusedSeconds),
                                                        formatMinutes(session.plannedSeconds));
        auto *duration = new QLabel(length);
        duration->setMinimumWidth(110);
        rowLayout->addWidget(duration);

        auto *topic = new QLabel(session.topic.isEmpty() ? tr("(no topic)") : session.topic);
        topic->setTextFormat(Qt::PlainText);
        if (session.topic.isEmpty())
            topic->setObjectName(QStringLiteral("Muted"));
        rowLayout->addWidget(topic, 1);

        auto *outcome = new QLabel(session.completed ? tr("Completed") : tr("Stopped early"));
        outcome->setObjectName(session.completed ? QStringLiteral("SessionCompleted") : QStringLiteral("Muted"));
        rowLayout->addWidget(outcome);
        m_recentLayout->addWidget(row);
    }
}
