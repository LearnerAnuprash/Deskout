#pragma once

#include "core/stats.h"

#include <QDate>
#include <QList>
#include <QWidget>

// Circular progress with the percentage in the middle.
class ProgressRing : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressRing(QWidget *parent = nullptr);

    // 0-100, or negative for "nothing to measure yet" (empty ring, "–").
    void setPercent(int percent);
    int percent() const { return m_percent; }
    // Highlight the arc when the goal is met.
    void setGoalMet(bool met);

    QSize sizeHint() const override { return {64, 64}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_percent = -1;
    bool m_goalMet = false;
};

// Bar per day: share of breaks taken, with the streak goal as a dashed
// line. Days without reminders show as a dot.
class WeekChart : public QWidget
{
    Q_OBJECT

public:
    struct Day
    {
        QDate date;
        DayAdherence adherence;
        int focusMinutes = 0;
    };

    explicit WeekChart(QWidget *parent = nullptr);

    void setDays(const QList<Day> &days, int thresholdPercent, const QDate &today);

    QSize sizeHint() const override { return {520, 200}; }
    QSize minimumSizeHint() const override { return {300, 170}; }

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QRectF plotArea() const;
    // Index of the day column under `pos`, or -1.
    int dayAt(const QPointF &pos) const;

    QList<Day> m_days;
    int m_threshold = StatsStore::DefaultThreshold;
    QDate m_today;
};
