#pragma once

#include <QString>

#include <memory>
#include <optional>

// Native queries about what the user is doing. One implementation per OS
// (activity_linux.cpp / activity_win.cpp / activity_mac.cpp).
class ActivityBackend
{
public:
    virtual ~ActivityBackend() = default;

    // Milliseconds since the last keyboard/mouse input, if known.
    virtual std::optional<qint64> idleMs() = 0;
    virtual QString idleBackendName() const = 0;

    // True when another application is full-screen or the user is
    // presenting / in a call, i.e. a full-screen takeover would be rude.
    virtual bool isOtherAppFullscreen() = 0;
    virtual QString fullscreenBackendName() const = 0;
};

std::unique_ptr<ActivityBackend> createActivityBackend();
