#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QObject>

#include <functional>
#include <optional>

// What the user wrote for one calendar day.
struct DailyUpdate
{
    QDate day;
    QString done; // what I did
    QString todo; // todos for the next day
    QDateTime createdAt;
    QDateTime updatedAt;
};

// Daily updates in the app database (one per local date), plus the
// "here's what you noted last time" recap shown once each new day.
class DailyUpdatesStore : public QObject
{
    Q_OBJECT

public:
    // Loaded per "Show older" click in the history.
    static constexpr int HistoryPageSize = 30;

    using Clock = std::function<QDateTime()>;

    explicit DailyUpdatesStore(QObject *parent = nullptr, Clock clock = {});

    std::optional<DailyUpdate> get(const QDate &day) const;
    // Creates or updates the day's entry. Saving two blank fields removes
    // the entry, so empty days don't show up in the history.
    bool save(const QDate &day, const QString &done, const QString &todo);
    // Most recent first, only days before `before` (all when invalid).
    QList<DailyUpdate> history(int limit, const QDate &before = QDate()) const;
    // The latest entry before `day`: yesterday's, or the last working
    // day's after a weekend.
    std::optional<DailyUpdate> latestBefore(const QDate &day) const;
    int count() const;

    // The recap to show on `today`: the latest earlier entry, unless the
    // recap is switched off or was already shown today.
    std::optional<DailyUpdate> recapDue(const QDate &today) const;
    static void markRecapShown(const QDate &today);
    static bool recapEnabled();
    static void setRecapEnabled(bool enabled);

Q_SIGNALS:
    void changed(const QDate &day);

private:
    Clock m_now;
};
