#include "core/pausemanager.h"

#include "core/settingskeys.h"

#include <QLocale>
#include <QSettings>

#include <algorithm>

namespace {
// Re-check at least this often so a pause still ends on time after system
// sleep or a wall-clock change (QTimer alone can drift in both cases).
constexpr qint64 MaxCheckIntervalMs = 30 * 1000;
} // namespace

PauseManager::PauseManager(QObject *parent, Clock clock)
    : QObject(parent)
    , m_now(clock ? std::move(clock) : [] { return QDateTime::currentDateTime(); })
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &PauseManager::checkExpiry);
    load();
}

QString PauseManager::statusText() const
{
    if (!m_paused)
        return tr("Reminders active");
    if (!m_until.isValid())
        return tr("Paused until you resume");

    const QDateTime now = m_now();
    const QString time = QLocale().toString(m_until.time(), QLocale::ShortFormat);
    if (m_until.date() == now.date())
        return tr("Paused until %1").arg(time);
    if (m_until.date() == now.date().addDays(1) && m_until.time() == QTime(0, 0))
        return tr("Paused until tomorrow");
    return tr("Paused until %1").arg(QLocale().toString(m_until, QLocale::ShortFormat));
}

void PauseManager::pauseFor(int minutes)
{
    setState(true, m_now().addSecs(qint64(std::max(1, minutes)) * 60));
}

void PauseManager::pauseUntilTomorrow()
{
    setState(true, QDateTime(m_now().date().addDays(1), QTime(0, 0)));
}

void PauseManager::pauseIndefinitely()
{
    setState(true, QDateTime());
}

void PauseManager::resume()
{
    setState(false, QDateTime());
}

void PauseManager::toggle()
{
    if (m_paused)
        resume();
    else
        pauseIndefinitely();
}

void PauseManager::checkExpiry()
{
    if (m_paused && m_until.isValid() && m_now() >= m_until)
        resume();
    else
        scheduleCheck();
}

void PauseManager::setState(bool paused, const QDateTime &until)
{
    const bool changed = paused != m_paused || until != m_until;
    m_paused = paused;
    m_until = paused ? until : QDateTime();
    save();
    scheduleCheck();
    if (changed)
        Q_EMIT pausedChanged(m_paused);
}

void PauseManager::load()
{
    QSettings settings;
    m_paused = settings.value(SettingsKeys::PausePaused, false).toBool();
    m_until = QDateTime::fromString(settings.value(SettingsKeys::PauseUntil).toString(),
                                    Qt::ISODate);
    if (!m_paused)
        m_until = QDateTime();
    // A timed pause that ended while the app was closed is simply over.
    if (m_paused && m_until.isValid() && m_now() >= m_until) {
        m_paused = false;
        m_until = QDateTime();
        save();
    }
    scheduleCheck();
}

void PauseManager::save() const
{
    QSettings settings;
    settings.setValue(SettingsKeys::PausePaused, m_paused);
    if (m_until.isValid())
        settings.setValue(SettingsKeys::PauseUntil, m_until.toString(Qt::ISODate));
    else
        settings.remove(SettingsKeys::PauseUntil);
}

void PauseManager::scheduleCheck()
{
    if (!m_paused || !m_until.isValid()) {
        m_timer.stop();
        return;
    }
    const qint64 remaining = std::max<qint64>(0, m_now().msecsTo(m_until));
    m_timer.start(int(std::min(remaining + 50, MaxCheckIntervalMs)));
}
