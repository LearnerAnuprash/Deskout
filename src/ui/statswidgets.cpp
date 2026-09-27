#include "ui/statswidgets.h"

#include <QHelpEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

#include <algorithm>

namespace {

constexpr qreal RingWidth = 6.0;
constexpr int AxisWidth = 34;
constexpr int LabelHeight = 36;
constexpr int TopPadding = 16;

QColor mixed(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount, a.greenF() + (b.greenF() - a.greenF()) * amount,
                            a.blueF() + (b.blueF() - a.blueF()) * amount);
}

} // namespace

ProgressRing::ProgressRing(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(sizeHint());
}

void ProgressRing::setPercent(int percent)
{
    m_percent = std::min(percent, 100);
    update();
}

void ProgressRing::setGoalMet(bool met)
{
    m_goalMet = met;
    update();
}

void ProgressRing::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette &pal = palette();
    const QRectF ring = QRectF(rect()).adjusted(RingWidth / 2 + 1, RingWidth / 2 + 1, -RingWidth / 2 - 1,
                                                -RingWidth / 2 - 1);

    painter.setPen(QPen(pal.color(QPalette::Midlight), RingWidth));
    painter.drawEllipse(ring);
    if (m_percent > 0) {
        const QColor accent = pal.color(QPalette::Highlight);
        QPen arc(m_goalMet ? accent : mixed(accent, pal.color(QPalette::Base), 0.35), RingWidth);
        arc.setCapStyle(Qt::RoundCap);
        painter.setPen(arc);
        // Qt angles: 1/16 degree, counter-clockwise from 3 o'clock.
        painter.drawArc(ring, 90 * 16, -int(360 * 16 * m_percent / 100.0));
    }

    QFont font = this->font();
    font.setPixelSize(15);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(m_percent < 0 ? pal.color(QPalette::PlaceholderText) : pal.color(QPalette::Text));
    painter.drawText(rect(), Qt::AlignCenter,
                     m_percent < 0 ? QStringLiteral("–") : QStringLiteral("%1%").arg(m_percent));
}

WeekChart::WeekChart(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(sizeHint().height());
    setMouseTracking(true);
}

void WeekChart::setDays(const QList<Day> &days, int thresholdPercent, const QDate &today)
{
    m_days = days;
    m_threshold = thresholdPercent;
    m_today = today;
    update();
}

QRectF WeekChart::plotArea() const
{
    return QRectF(AxisWidth, TopPadding, width() - AxisWidth - 4, height() - TopPadding - LabelHeight);
}

int WeekChart::dayAt(const QPointF &pos) const
{
    const QRectF plot = plotArea();
    if (m_days.isEmpty() || pos.x() < plot.left() || pos.x() > plot.right())
        return -1;
    const int index = int((pos.x() - plot.left()) / (plot.width() / m_days.size()));
    return index >= 0 && index < m_days.size() ? index : -1;
}

bool WeekChart::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        const int index = dayAt(help->pos());
        if (index < 0) {
            QToolTip::hideText();
            return true;
        }
        const Day &day = m_days.at(index);
        QString text = QLocale().toString(day.date, QStringLiteral("dddd, d MMMM")) + QLatin1Char('\n');
        text += day.adherence.isActive()
                    ? tr("%1 of %2 breaks taken (%3%)")
                          .arg(day.adherence.taken)
                          .arg(day.adherence.shown)
                          .arg(day.adherence.percent())
                    : tr("No reminders");
        if (day.focusMinutes > 0)
            text += QLatin1Char('\n') + tr("%1 min of focus").arg(day.focusMinutes);
        QToolTip::showText(help->globalPos(), text, this);
        return true;
    }
    return QWidget::event(event);
}

void WeekChart::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette &pal = palette();
    const QColor text = pal.color(QPalette::Text);
    const QColor muted = pal.color(QPalette::PlaceholderText);
    const QColor accent = pal.color(QPalette::Highlight);
    const QColor grid = pal.color(QPalette::Midlight);
    const QRectF plot = plotArea();

    QFont small = font();
    small.setPixelSize(11);
    painter.setFont(small);

    // Horizontal grid at 0 / 50 / 100 %.
    for (int percent : {0, 50, 100}) {
        const qreal y = plot.bottom() - plot.height() * percent / 100.0;
        painter.setPen(QPen(grid, 1));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(muted);
        painter.drawText(QRectF(0, y - 8, AxisWidth - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("%1%").arg(percent));
    }

    if (m_days.isEmpty())
        return;
    const qreal column = plot.width() / m_days.size();
    const qreal barWidth = std::min<qreal>(34, column * 0.5);
    for (int i = 0; i < m_days.size(); ++i) {
        const Day &day = m_days.at(i);
        const qreal centre = plot.left() + column * (i + 0.5);
        const bool isToday = day.date == m_today;

        if (day.adherence.isActive()) {
            const int percent = day.adherence.percent();
            const qreal height = std::max<qreal>(3, plot.height() * percent / 100.0);
            const QRectF bar(centre - barWidth / 2, plot.bottom() - height, barWidth, height);
            QPainterPath path;
            path.addRoundedRect(bar, 4, 4);
            painter.fillPath(path, day.adherence.meets(m_threshold) ? accent : mixed(accent, pal.color(QPalette::Base), 0.6));
            painter.setPen(text);
            painter.drawText(QRectF(centre - column / 2, bar.top() - 16, column, 14), Qt::AlignCenter,
                             QStringLiteral("%1%").arg(percent));
        } else {
            // No reminders that day: a dot on the baseline.
            painter.setPen(Qt::NoPen);
            painter.setBrush(muted);
            painter.drawEllipse(QPointF(centre, plot.bottom() - 3), 3, 3);
        }

        QFont label = small;
        label.setWeight(isToday ? QFont::DemiBold : QFont::Normal);
        painter.setFont(label);
        painter.setPen(isToday ? text : muted);
        const QString name = isToday ? tr("Today") : QLocale().toString(day.date, QStringLiteral("ddd"));
        painter.drawText(QRectF(centre - column / 2, plot.bottom() + 4, column, 15), Qt::AlignCenter, name);
        painter.setFont(small);
        painter.setPen(muted);
        if (day.focusMinutes > 0)
            painter.drawText(QRectF(centre - column / 2, plot.bottom() + 19, column, 14), Qt::AlignCenter,
                             tr("%1m focus").arg(day.focusMinutes));
    }

    // The goal, dashed, over the bars.
    const qreal goalY = plot.bottom() - plot.height() * m_threshold / 100.0;
    QPen goal(text, 1, Qt::DashLine);
    goal.setColor(mixed(text, pal.color(QPalette::Base), 0.45));
    painter.setPen(goal);
    painter.drawLine(QPointF(plot.left(), goalY), QPointF(plot.right(), goalY));
}
