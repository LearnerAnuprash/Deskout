#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <functional>

// One run of the focus timer, reported when it ends.
struct FocusSession
{
    QDateTime startedAt;
    int plannedSeconds = 0;
    // Time actually counted down; excludes pauses.
    int focusedSeconds = 0;
    // False when stopped early.
    bool completed = false;
    QString topic;
};

// Research/focus countdown. It counts real time the user spends in the
// session, not input activity: reading counts, but time the timer is
// paused or the computer is asleep does not. Showing and logging the
// session are other components' jobs.
class FocusTimer : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,     // nothing started yet
        Running,
        Paused,
        Finished, // last session ran to zero; like Idle, but says "done"
    };

    static constexpr int DefaultMinutes = 25;
    static constexpr int MinMinutes = 1;
    static constexpr int MaxMinutes = 240;

    using Clock = std::function<QDateTime()>;

    explicit FocusTimer(QObject *parent = nullptr, Clock clock = {});

    State state() const { return m_state; }
    // Running or paused.
    bool isActive() const { return m_state == State::Running || m_state == State::Paused; }

    // Duration and topic of the last session started (persisted), used to
    // pre-fill the next one.
    int lastMinutes() const;
    QString lastTopic() const;

    // The current or most recent session.
    QString topic() const { return m_topic; }
    qint64 plannedMs() const { return m_plannedMs; }
    qint64 focusedMs() const { return m_focusedMs; }
    qint64 remainingMs() const;
    // Rounded up, so "0:01" shows until the very end.
    int remainingSeconds() const;
    // 0.0 at the start, 1.0 when done.
    double progress() const;

    static QString formatSeconds(int seconds);

public Q_SLOTS:
    // Starts a new session, abandoning any active one. Minutes are clamped
    // to [MinMinutes, MaxMinutes].
    void start(int minutes, const QString &topic);
    // Starts with lastMinutes()/lastTopic().
    void startAgain();
    // Remembers the duration and topic for the next start(). While nothing
    // is running, the timer shows the new duration straight away.
    void setNext(int minutes, const QString &topic);
    void pause();
    void resume();
    // Start, pause or resume, whichever fits the current state.
    void toggle();
    // Abandons the active session.
    void stop();
    // Counts `elapsedMs` of session time. Called by the internal tick;
    // public so tests can drive time.
    void advance(qint64 elapsedMs);

Q_SIGNALS:
    void stateChanged(FocusTimer::State state);
    // Emitted when remainingSeconds() changes.
    void remainingChanged(int seconds);
    void sessionEnded(const FocusSession &session);

private:
    void onTick();
    static void remember(int minutes, const QString &topic);
    void setState(State state);
    void end(bool completed);

    Clock m_now;
    State m_state = State::Idle;
    QString m_topic;
    QDateTime m_startedAt;
    qint64 m_plannedMs = 0;
    qint64 m_focusedMs = 0;
    int m_lastEmittedSeconds = -1;
    QTimer m_tick;
    QElapsedTimer m_clock;
};
