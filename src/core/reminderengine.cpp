#include "core/reminderengine.h"

#include <algorithm>

namespace {
constexpr int TickMs = 1000;
// A tick that arrives much later than expected (system suspend, debugger)
// must not count as screen time.
constexpr qint64 MaxTickMs = 5 * TickMs;
} // namespace

ReminderEngine::ReminderEngine(Hooks hooks, QObject *parent)
    : QObject(parent)
    , m_hooks(std::move(hooks))
{
    if (!m_hooks.isPaused)
        m_hooks.isPaused = [] { return false; };
    if (!m_hooks.isIdle)
        m_hooks.isIdle = [] { return false; };
    if (!m_hooks.now)
        m_hooks.now = [] { return QDateTime::currentDateTime(); };

    m_tick.setInterval(TickMs);
    connect(&m_tick, &QTimer::timeout, this, &ReminderEngine::onTick);
}

void ReminderEngine::start()
{
    reload();
    m_clock.start();
    m_tick.start();
}

void ReminderEngine::reload()
{
    QList<Entry> entries;
    for (const QString &id : Reminders::allIds()) {
        Entry entry;
        if (const Entry *existing = find(id))
            entry = *existing;
        entry.config = Reminders::load(id);
        entries << entry;
    }
    m_entries = entries;
    for (const Entry &entry : std::as_const(m_entries))
        Q_EMIT configChanged(entry.config.id);
}

QStringList ReminderEngine::ids() const
{
    QStringList result;
    for (const Entry &entry : m_entries)
        result << entry.config.id;
    return result;
}

ReminderConfig ReminderEngine::config(const QString &id) const
{
    const Entry *entry = find(id);
    return entry ? entry->config : Reminders::defaults(id);
}

void ReminderEngine::setConfig(const ReminderConfig &config)
{
    Entry *entry = find(config.id);
    if (!entry || entry->config == config)
        return;
    entry->config = config;
    Reminders::save(config);
    Q_EMIT configChanged(config.id);
}

ReminderStatus ReminderEngine::status(const QString &id) const
{
    const Entry *entry = find(id);
    if (!entry || !entry->config.enabled)
        return {ReminderState::Disabled, 0};

    const int remaining = int(std::max<qint64>(0, entry->config.intervalMs() - entry->elapsedMs + 999) / 1000);
    if (entry->due)
        return {ReminderState::Due, 0};
    if (!entry->config.isActiveAt(m_hooks.now()))
        return {ReminderState::OutsideHours, remaining};
    if (m_hooks.isPaused())
        return {ReminderState::Paused, remaining};
    if (m_hooks.isIdle())
        return {ReminderState::Idle, remaining};
    return {ReminderState::Counting, remaining};
}

void ReminderEngine::advance(qint64 elapsedMs)
{
    const QDateTime now = m_hooks.now();
    const bool paused = m_hooks.isPaused();
    const bool idle = m_hooks.isIdle();

    // Collect first, emit after: slots may call back into the engine.
    QStringList becameDue;
    for (Entry &entry : m_entries) {
        const bool active = entry.config.isActiveAt(now);
        // Each active period (e.g. every morning at 9:00) starts fresh.
        if (active && !entry.wasActive)
            entry.elapsedMs = 0;
        entry.wasActive = active;

        if (!active || paused || idle || entry.due)
            continue;
        entry.elapsedMs += elapsedMs;
        if (entry.elapsedMs >= entry.config.intervalMs()) {
            entry.due = true;
            becameDue << entry.config.id;
        }
    }
    for (const QString &id : std::as_const(becameDue))
        Q_EMIT reminderDue(id);
}

void ReminderEngine::triggerNow(const QString &id)
{
    Entry *entry = find(id);
    if (!entry || entry->due)
        return;
    entry->due = true;
    Q_EMIT reminderDue(id);
}

void ReminderEngine::confirm(const QString &id)
{
    resolve(find(id), 0, ReminderOutcome::Confirmed);
}

void ReminderEngine::snooze(const QString &id, int minutes)
{
    Entry *entry = find(id);
    if (!entry)
        return;
    // Due again after `minutes` of active time.
    const qint64 remaining = qint64(std::max(1, minutes)) * 60 * 1000;
    resolve(entry, std::max<qint64>(0, entry->config.intervalMs() - remaining), ReminderOutcome::Snoozed);
}

void ReminderEngine::acknowledge(const QString &id)
{
    resolve(find(id), 0, ReminderOutcome::Notified);
}

ReminderEngine::Entry *ReminderEngine::find(const QString &id)
{
    for (Entry &entry : m_entries) {
        if (entry.config.id == id)
            return &entry;
    }
    return nullptr;
}

const ReminderEngine::Entry *ReminderEngine::find(const QString &id) const
{
    return const_cast<ReminderEngine *>(this)->find(id);
}

void ReminderEngine::onTick()
{
    advance(std::min(m_clock.restart(), MaxTickMs));
}

void ReminderEngine::resolve(Entry *entry, qint64 elapsedMs, ReminderOutcome outcome)
{
    if (!entry)
        return;
    entry->due = false;
    entry->elapsedMs = elapsedMs;
    Q_EMIT reminderResolved(entry->config.id, outcome);
}
