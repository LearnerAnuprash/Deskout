#include "ui/statspage.h"

#include "core/reminders.h"
#include "core/stats.h"
#include "ui/statswidgets.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

constexpr int ChartDays = 7;
// Picks up midnight and changes made while the page is open.
constexpr int RefreshIntervalMs = 60 * 1000;

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
        return QCoreApplication::translate("StatsPage", "%1 min").arg(minutes);
    return QCoreApplication::translate("StatsPage", "%1 h %2 min").arg(minutes / 60).arg(minutes % 60);
}

} // namespace

StatsPage::StatsPage(StatsStore *stats, QWidget *parent)
    : QWidget(parent)
    , m_stats(stats)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("PageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("PageScrollContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 8, 0);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->addWidget(makeLabel(tr("Stats"), "PageTitle"), 1);
    m_date = makeLabel(QString(), "Muted");
    m_date->setWordWrap(false);
    header->addWidget(m_date, 0, Qt::AlignBottom);
    layout->addLayout(header);
    layout->addWidget(makeLabel(tr("How today's breaks and focus are going, and how consistent you've been."),
                                "Muted"));

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->addWidget(buildTile(&m_streak, tr("Current streak"), false), 0, 0);
    grid->addWidget(buildTile(&m_breaks, tr("Breaks today"), true), 0, 1);
    grid->addWidget(buildTile(&m_focus, tr("Focus today"), false), 0, 2);
    const QStringList ids = Reminders::allIds();
    for (int i = 0; i < ids.size(); ++i) {
        Tile tile;
        QWidget *card = buildTile(&tile, Reminders::texts(ids.at(i)).name, true);
        m_reminders.insert(ids.at(i), tile);
        grid->addWidget(card, 1 + i / 3, i % 3);
    }
    for (int column = 0; column < 3; ++column)
        grid->setColumnStretch(column, 1);
    layout->addLayout(grid);
    layout->addWidget(buildChartCard());
    layout->addStretch(1);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    m_streak.value->setObjectName(QStringLiteral("StatValueAccent"));

    m_refreshTimer.setInterval(RefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &StatsPage::refresh);
    connect(m_stats, &StatsStore::changed, this, [this] {
        if (isVisible())
            refresh();
    });
    refresh();
}

QWidget *StatsPage::buildTile(Tile *tile, const QString &caption, bool withRing)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    card->setMinimumWidth(150);
    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(16, 12, 14, 12);
    layout->setSpacing(10);

    auto *texts = new QVBoxLayout;
    texts->setSpacing(2);
    QLabel *title = makeLabel(caption, "FieldLabel");
    title->setTextFormat(Qt::PlainText); // "Walk & stretch"
    texts->addWidget(title);
    tile->value = new QLabel;
    tile->value->setObjectName(QStringLiteral("StatValue"));
    texts->addWidget(tile->value);
    tile->detail = makeLabel(QString(), "Muted");
    texts->addWidget(tile->detail);
    texts->addStretch(1);
    layout->addLayout(texts, 1);

    if (withRing) {
        tile->ring = new ProgressRing;
        layout->addWidget(tile->ring, 0, Qt::AlignVCenter);
    }
    return card;
}

QWidget *StatsPage::buildChartCard()
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("Card"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 14);
    layout->setSpacing(8);

    layout->addWidget(makeLabel(tr("Last 7 days"), "CardTitle"));
    layout->addWidget(makeLabel(tr("Share of breaks taken each day. The dashed line is your streak goal."), "Muted"));

    m_chart = new WeekChart;
    layout->addWidget(m_chart);

    auto *goal = new QHBoxLayout;
    goal->setSpacing(6);
    goal->addWidget(new QLabel(tr("Streak goal: take at least")));
    m_threshold = new QSpinBox;
    m_threshold->setRange(10, 100);
    m_threshold->setSingleStep(5);
    m_threshold->setSuffix(QStringLiteral("%"));
    m_threshold->setValue(StatsStore::threshold());
    connect(m_threshold, qOverload<int>(&QSpinBox::valueChanged), this, [this](int percent) {
        StatsStore::setThreshold(percent);
        refresh();
    });
    goal->addWidget(m_threshold);
    goal->addWidget(makeLabel(tr("of your breaks on each day with reminders. Days off don't break it."), "Muted"),
                    1);
    layout->addLayout(goal);
    return card;
}

void StatsPage::refresh()
{
    const QDate today = QDate::currentDate();
    const int threshold = StatsStore::threshold();
    m_date->setText(QLocale().toString(today, QStringLiteral("dddd, d MMMM")));

    const Streak streak = m_stats->streak(today, threshold);
    m_streak.value->setText(QString::number(streak.current));
    m_streak.detail->setText((streak.current == 1 ? tr("day in a row") : tr("days in a row")) + QStringLiteral(" · ")
                             + tr("best %1").arg(streak.best));

    // Today's breaks, overall and per type.
    DayAdherence total;
    for (auto it = m_reminders.cbegin(); it != m_reminders.cend(); ++it) {
        DayAdherence day;
        day.shown = m_stats->value(today, StatsMetric::reminderShown(it.key()));
        day.taken = m_stats->value(today, StatsMetric::reminderTaken(it.key()));
        total.shown += day.shown;
        total.taken += day.taken;

        const Tile &tile = it.value();
        if (it.key() == QLatin1String(ReminderIds::Water)) {
            tile.value->setText(QString::number(day.taken));
            tile.detail->setText(day.taken == 1 ? tr("glass of water") : tr("glasses of water"));
        } else {
            tile.value->setText(day.isActive() ? QStringLiteral("%1/%2").arg(day.taken).arg(day.shown)
                                               : QStringLiteral("0"));
            tile.detail->setText(day.isActive() ? tr("taken") : tr("none due yet"));
        }
        tile.ring->setPercent(day.isActive() ? day.percent() : -1);
        tile.ring->setGoalMet(day.meets(threshold));
    }
    m_breaks.value->setText(total.isActive() ? QStringLiteral("%1/%2").arg(total.taken).arg(total.shown)
                                             : QStringLiteral("0"));
    if (!total.isActive())
        m_breaks.detail->setText(tr("no breaks due yet"));
    else if (total.meets(threshold))
        m_breaks.detail->setText(tr("goal met"));
    else
        m_breaks.detail->setText(tr("goal: %1%").arg(threshold));
    m_breaks.ring->setPercent(total.isActive() ? total.percent() : -1);
    m_breaks.ring->setGoalMet(total.meets(threshold));

    const int sessions = m_stats->value(today, QLatin1String(StatsMetric::FocusCompleted));
    const int focusSeconds = m_stats->value(today, QLatin1String(StatsMetric::FocusSeconds));
    m_focus.value->setText(QString::number(sessions));
    m_focus.detail->setText((sessions == 1 ? tr("session") : tr("sessions")) + QStringLiteral(" · ")
                            + formatMinutes(focusSeconds));

    // Last seven days, oldest first.
    const QDate first = today.addDays(-(ChartDays - 1));
    const QMap<QDate, DayAdherence> week = m_stats->adherence(first, today);
    QList<WeekChart::Day> days;
    for (QDate day = first; day <= today; day = day.addDays(1))
        days << WeekChart::Day{day, week.value(day),
                               (m_stats->value(day, QLatin1String(StatsMetric::FocusSeconds)) + 30) / 60};
    m_chart->setDays(days, threshold, today);
}

void StatsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
    m_refreshTimer.start();
}

void StatsPage::hideEvent(QHideEvent *event)
{
    m_refreshTimer.stop();
    QWidget::hideEvent(event);
}
