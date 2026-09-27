#include "core/focustimer.h"

#include "core/settingskeys.h"

#include <QSettings>

#include <algorithm>

namespace {
// Several ticks per second keep the display from skipping a second.
constexpr int TickMs = 250;
// A tick that arrives much later than expected (system suspend, debugger)
// must not count as focus time.
constexpr qint64 MaxTickMs = 5000;
} // namespace

FocusTimer::FocusTimer(QObject *parent, Clock clock)
    : QObject(parent)
    , m_now(std::move(clock))
{
    if (!m_now)
        m_now = [] { return QDateTime::currentDateTime(); };
    m_plannedMs = qint64(lastMinutes()) * 60 * 1000;
    m_topic = lastTopic();
    m_tick.setInterval(TickMs);
    connect(&m_tick, &QTimer::timeout, this, &FocusTimer::onTick);
}

int FocusTimer::lastMinutes() const
{
    return std::clamp(QSettings().value(SettingsKeys::FocusMinutes, DefaultMinutes).toInt(), MinMinutes,
                      MaxMinutes);
}

QString FocusTimer::lastTopic() const
{
    return QSettings().value(SettingsKeys::FocusTopic).toString();
}

qint64 FocusTimer::remainingMs() const
{
    return std::max<qint64>(0, m_plannedMs - m_focusedMs);
}

int FocusTimer::remainingSeconds() const
{
    return int((remainingMs() + 999) / 1000);
}

double FocusTimer::progress() const
{
    return m_plannedMs > 0 ? std::min(1.0, double(m_focusedMs) / double(m_plannedMs)) : 0.0;
}

QString FocusTimer::formatSeconds(int seconds)
{
    const int h = seconds / 3600;
    const int m = (seconds % 3600) / 60;
    const int s = seconds % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

void FocusTimer::start(int minutes, const QString &topic)
{
    if (isActive())
        end(false);

    minutes = std::clamp(minutes, MinMinutes, MaxMinutes);
    m_topic = topic.simplified();
    remember(minutes, m_topic);

    m_startedAt = m_now();
    m_plannedMs = qint64(minutes) * 60 * 1000;
    m_focusedMs = 0;
    m_clock.start();
    m_tick.start();
    setState(State::Running);
    m_lastEmittedSeconds = remainingSeconds();
    Q_EMIT remainingChanged(m_lastEmittedSeconds);
}

void FocusTimer::startAgain()
{
    start(lastMinutes(), lastTopic());
}

void FocusTimer::setNext(int minutes, const QString &topic)
{
    minutes = std::clamp(minutes, MinMinutes, MaxMinutes);
    remember(minutes, topic.simplified());
    if (isActive())
        return;
    m_topic = topic.simplified();
    m_plannedMs = qint64(minutes) * 60 * 1000;
    m_focusedMs = 0;
    setState(State::Idle);
    if (remainingSeconds() != m_lastEmittedSeconds) {
        m_lastEmittedSeconds = remainingSeconds();
        Q_EMIT remainingChanged(m_lastEmittedSeconds);
    }
}

void FocusTimer::pause()
{
    if (m_state != State::Running)
        return;
    // Count the time since the last tick before stopping the clock.
    onTick();
    if (m_state != State::Running)
        return; // that tick finished the session
    m_tick.stop();
    setState(State::Paused);
}

void FocusTimer::resume()
{
    if (m_state != State::Paused)
        return;
    m_clock.start();
    m_tick.start();
    setState(State::Running);
}

void FocusTimer::toggle()
{
    switch (m_state) {
    case State::Running:
        pause();
        break;
    case State::Paused:
        resume();
        break;
    case State::Idle:
    case State::Finished:
        startAgain();
        break;
    }
}

void FocusTimer::stop()
{
    if (!isActive())
        return;
    if (m_state == State::Running)
        onTick();
    if (isActive())
        end(false);
}

void FocusTimer::advance(qint64 elapsedMs)
{
    if (m_state != State::Running || elapsedMs <= 0)
        return;
    m_focusedMs = std::min(m_plannedMs, m_focusedMs + elapsedMs);

    const int seconds = remainingSeconds();
    if (seconds != m_lastEmittedSeconds) {
        m_lastEmittedSeconds = seconds;
        Q_EMIT remainingChanged(seconds);
    }
    if (m_focusedMs >= m_plannedMs)
        end(true);
}

void FocusTimer::onTick()
{
    advance(std::min(m_clock.restart(), MaxTickMs));
}

void FocusTimer::remember(int minutes, const QString &topic)
{
    QSettings settings;
    settings.setValue(SettingsKeys::FocusMinutes, minutes);
    settings.setValue(SettingsKeys::FocusTopic, topic);
}

void FocusTimer::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT stateChanged(state);
}

void FocusTimer::end(bool completed)
{
    m_tick.stop();
    FocusSession session;
    session.startedAt = m_startedAt;
    session.plannedSeconds = int(m_plannedMs / 1000);
    session.focusedSeconds = int(m_focusedMs / 1000);
    session.completed = completed;
    session.topic = m_topic;

    if (completed) {
        setState(State::Finished);
    } else {
        // Back to showing the full duration for the next session.
        m_focusedMs = 0;
        setState(State::Idle);
        m_lastEmittedSeconds = remainingSeconds();
        Q_EMIT remainingChanged(m_lastEmittedSeconds);
    }
    Q_EMIT sessionEnded(session);
}
