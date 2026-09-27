#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>

#include <functional>

// Global "pause / mute everything" state. Every reminder, alarm and timer
// alert must check isPaused() before firing. The state is persisted so a
// pause survives an app restart (e.g. "until tomorrow").
class PauseManager : public QObject
{
    Q_OBJECT

public:
    using Clock = std::function<QDateTime()>;

    explicit PauseManager(QObject *parent = nullptr, Clock clock = {});

    bool isPaused() const { return m_paused; }
    // True when paused with no end time ("until I resume").
    bool isIndefinite() const { return m_paused && !m_until.isValid(); }
    // End of the pause, or an invalid QDateTime when not paused / indefinite.
    QDateTime pausedUntil() const { return m_until; }

    // Human readable one-liner, e.g. "Paused until 14:30".
    QString statusText() const;

public Q_SLOTS:
    void pauseFor(int minutes);
    void pauseUntilTomorrow();
    void pauseIndefinitely();
    void resume();
    // Used by the global hotkey: resume when paused, else pause indefinitely.
    void toggle();
    // Resumes if the pause end time has passed. Called by an internal timer;
    // public so tests can drive it with a fake clock.
    void checkExpiry();

Q_SIGNALS:
    void pausedChanged(bool paused);

private:
    void setState(bool paused, const QDateTime &until);
    void load();
    void save() const;
    void scheduleCheck();

    Clock m_now;
    bool m_paused = false;
    QDateTime m_until;
    QTimer m_timer;
};
