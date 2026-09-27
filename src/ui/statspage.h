#pragma once

#include <QDate>
#include <QMap>
#include <QTimer>
#include <QWidget>

class ProgressRing;
class QLabel;
class QSpinBox;
class StatsStore;
class WeekChart;

// Stats: today's breaks by type, focus, the current streak and the last
// seven days, laid out to read well at a glance (and in a screenshot).
class StatsPage : public QWidget
{
    Q_OBJECT

public:
    explicit StatsPage(StatsStore *stats, QWidget *parent = nullptr);

    void refresh();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Tile
    {
        QLabel *value = nullptr;
        QLabel *detail = nullptr;
        ProgressRing *ring = nullptr;
    };

    QWidget *buildTile(Tile *tile, const QString &caption, bool withRing);
    QWidget *buildChartCard();

    StatsStore *m_stats;
    QLabel *m_date = nullptr;
    Tile m_streak;
    Tile m_breaks;
    Tile m_focus;
    QMap<QString, Tile> m_reminders; // reminder id -> tile
    WeekChart *m_chart = nullptr;
    QSpinBox *m_threshold = nullptr;
    QTimer m_refreshTimer;
};
