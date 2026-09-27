#pragma once

#include <QList>
#include <QString>

// Something that needs the user's attention: a due reminder, a finished
// focus session. AlertCenter decides whether it becomes a full-screen alarm
// or a notification; the source only describes it.
struct AlertAction
{
    QString key;
    QString label;
};

struct Alert
{
    // Key reported by the primary button.
    static constexpr char ConfirmKey[] = "confirm";

    // Identifies the source (e.g. "reminder/eye"); one alert per key at a time.
    QString key;
    QString title;
    QString body;
    QString confirmLabel;
    // Extra buttons after the primary one. A notification can only offer
    // the first of these.
    QList<AlertAction> actions;
    // Label of the link that switches this alert to notification-only.
    QString disableFullScreenLabel;
    // The user's preference; AlertCenter may still fall back to a
    // notification (another app is full-screen).
    bool fullScreen = true;
    // Show an eye exercise after the primary button.
    bool eyeExercise = false;
};
