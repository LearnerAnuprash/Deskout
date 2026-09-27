#include "platform/activity/activitymonitor.h"

#include "core/settingskeys.h"
#include "platform/activity/activitybackend.h"

#include <QSettings>

namespace {
constexpr int PollIntervalMs = 2000;
} // namespace

ActivityMonitor::ActivityMonitor(QObject *parent)
    : QObject(parent)
    , m_backend(createActivityBackend())
{
    m_poll.setInterval(PollIntervalMs);
    connect(&m_poll, &QTimer::timeout, this, &ActivityMonitor::poll);
    reload();
    m_poll.start();
}

ActivityMonitor::~ActivityMonitor() = default;

void ActivityMonitor::reload()
{
    QSettings settings;
    m_idleEnabled = settings.value(SettingsKeys::IdleDetectionEnabled, true).toBool();
    m_thresholdMinutes = qBound(1,
                                settings.value(SettingsKeys::IdleThresholdMinutes,
                                               SettingsKeys::IdleThresholdDefault).toInt(),
                                120);
    m_fullscreenEnabled = settings.value(SettingsKeys::FullscreenDetectionEnabled, true).toBool();
    poll();
}

QString ActivityMonitor::idleBackendName() const
{
    return m_backend ? m_backend->idleBackendName() : tr("Unavailable");
}

bool ActivityMonitor::shouldAvoidFullscreen()
{
    return m_fullscreenEnabled && m_backend && m_backend->isOtherAppFullscreen();
}

QString ActivityMonitor::fullscreenBackendName() const
{
    return m_backend ? m_backend->fullscreenBackendName() : tr("Unavailable");
}

void ActivityMonitor::poll()
{
    const std::optional<qint64> idle = m_backend ? m_backend->idleMs() : std::nullopt;
    m_idleSupported = idle.has_value();
    m_lastIdleMs = idle.value_or(0);

    const bool nowIdle = m_idleEnabled && idle
                         && *idle >= qint64(m_thresholdMinutes) * 60 * 1000;
    if (nowIdle != m_idle) {
        m_idle = nowIdle;
        Q_EMIT idleChanged(m_idle);
    }
}
