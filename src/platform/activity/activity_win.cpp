#include "platform/activity/activitybackend.h"

#include <QCoreApplication>

#include <windows.h>
#include <shellapi.h>

namespace {

class WinActivityBackend final : public ActivityBackend
{
public:
    std::optional<qint64> idleMs() override
    {
        LASTINPUTINFO info = {};
        info.cbSize = sizeof(info);
        if (!GetLastInputInfo(&info))
            return std::nullopt;
        // Both are 32-bit tick counts; unsigned subtraction handles wrap-around.
        return qint64(DWORD(GetTickCount() - info.dwTime));
    }

    QString idleBackendName() const override
    {
        return QCoreApplication::translate("Activity", "Windows last-input time");
    }

    bool isOtherAppFullscreen() override
    {
        QUERY_USER_NOTIFICATION_STATE state = {};
        if (FAILED(SHQueryUserNotificationState(&state)))
            return false;
        return state == QUNS_BUSY || state == QUNS_RUNNING_D3D_FULL_SCREEN
               || state == QUNS_PRESENTATION_MODE;
    }

    QString fullscreenBackendName() const override
    {
        return QCoreApplication::translate("Activity", "Windows notification state (full-screen, presentation)");
    }
};

} // namespace

std::unique_ptr<ActivityBackend> createActivityBackend()
{
    return std::make_unique<WinActivityBackend>();
}
