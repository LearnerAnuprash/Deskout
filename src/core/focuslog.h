#pragma once

#include "core/focustimer.h"

#include <QDate>
#include <QList>
#include <QObject>

struct FocusDaySummary
{
    int completed = 0;
    int stoppedEarly = 0;
    int focusedSeconds = 0;
};

// Focus session history in the app database. Feeds the Focus page now and
// the stats view later.
class FocusLog : public QObject
{
    Q_OBJECT

public:
    // Sessions stopped before this much focus aren't worth keeping
    // (mis-clicks, "wrong duration" restarts).
    static constexpr int MinAbandonedSeconds = 60;

    explicit FocusLog(QObject *parent = nullptr);

    // False if the session was skipped (too short) or the database is
    // unavailable.
    bool record(const FocusSession &session);

    // Most recent first.
    QList<FocusSession> recent(int limit) const;
    FocusDaySummary summaryFor(const QDate &day) const;

Q_SIGNALS:
    void changed();
};
