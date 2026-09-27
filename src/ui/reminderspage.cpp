#include "ui/reminderspage.h"

#include "core/reminderengine.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString formatDuration(int seconds)
{
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

QLabel *makeLabel(const QString &text, const char *objectName = nullptr)
{
    auto *label = new QLabel(text);
    if (objectName)
        label->setObjectName(QLatin1String(objectName));
    label->setWordWrap(true);
    return label;
}

} // namespace

RemindersPage::RemindersPage(ReminderEngine *engine, QWidget *parent)
    : QWidget(parent)
    , m_engine(engine)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(makeLabel(tr("Reminders"), "PageTitle"));
    layout->addWidget(makeLabel(tr("Timers only run during active hours while you're at the "
                                   "computer. They stop while you're away or everything is paused."),
                                "Muted"));

    for (const QString &id : m_engine->ids())
        layout->addWidget(buildCard(id));
    layout->addStretch(1);

    m_refresh.setInterval(1000);
    connect(&m_refresh, &QTimer::timeout, this, &RemindersPage::refreshStatus);
    connect(m_engine, &ReminderEngine::configChanged, this, &RemindersPage::refreshConfig);
    connect(m_engine, &ReminderEngine::reminderDue, this, &RemindersPage::refreshStatus);
    connect(m_engine, &ReminderEngine::reminderResolved, this, &RemindersPage::refreshStatus);
    for (const Card &card : std::as_const(m_cards))
        refreshConfig(card.id);
}

QWidget *RemindersPage::buildCard(const QString &id)
{
    Card card;
    card.id = id;

    auto *frame = new QFrame;
    frame->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    auto *header = new QHBoxLayout;
    header->addWidget(makeLabel(Reminders::texts(id).name, "CardTitle"), 1);
    card.enabled = new QCheckBox(tr("On"));
    header->addWidget(card.enabled);
    layout->addLayout(header);

    card.summary = makeLabel(QString(), "Muted");
    layout->addWidget(card.summary);

    auto *footer = new QHBoxLayout;
    card.status = makeLabel(QString());
    footer->addWidget(card.status, 1);
    auto *test = new QPushButton(tr("Test now"));
    test->setToolTip(tr("Show this reminder right away"));
    auto *edit = new QPushButton(tr("Edit…"));
    footer->addWidget(test);
    footer->addWidget(edit);
    layout->addLayout(footer);

    connect(card.enabled, &QCheckBox::toggled, this, [this, id](bool on) {
        ReminderConfig config = m_engine->config(id);
        config.enabled = on;
        m_engine->setConfig(config);
        refreshStatus();
    });
    connect(test, &QPushButton::clicked, this, [this, id] { m_engine->triggerNow(id); });
    connect(edit, &QPushButton::clicked, this, &RemindersPage::editRequested);

    m_cards << card;
    return frame;
}

void RemindersPage::refreshConfig(const QString &id)
{
    for (const Card &card : std::as_const(m_cards)) {
        if (card.id != id)
            continue;
        const ReminderConfig config = m_engine->config(id);
        const QSignalBlocker blocker(card.enabled);
        card.enabled->setChecked(config.enabled);
        card.summary->setText(config.summary());
    }
    refreshStatus();
}

void RemindersPage::refreshStatus()
{
    for (const Card &card : std::as_const(m_cards)) {
        const ReminderStatus status = m_engine->status(card.id);
        const QString left = formatDuration(status.secondsRemaining);
        QString text;
        switch (status.state) {
        case ReminderState::Disabled: text = tr("Off"); break;
        case ReminderState::OutsideHours: text = tr("Outside active hours"); break;
        case ReminderState::Paused: text = tr("Paused · %1 left").arg(left); break;
        case ReminderState::Idle: text = tr("You're away · timer stopped at %1").arg(left); break;
        case ReminderState::Counting: text = tr("Next in %1").arg(left); break;
        case ReminderState::Due: text = tr("Due now"); break;
        }
        card.status->setText(text);
    }
}

void RemindersPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshStatus();
    m_refresh.start();
}

void RemindersPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_refresh.stop();
}
