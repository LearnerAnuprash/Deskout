#include "platform/activity/activitybackend.h"

#include <QCoreApplication>

#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

namespace {

bool numberValue(CFDictionaryRef dict, CFStringRef key, long long *out)
{
    const auto number = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, key));
    return number && CFNumberGetValue(number, kCFNumberLongLongType, out);
}

class MacActivityBackend final : public ActivityBackend
{
public:
    std::optional<qint64> idleMs() override
    {
        const double seconds = CGEventSourceSecondsSinceLastEventType(
            kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
        return qint64(seconds * 1000.0);
    }

    QString idleBackendName() const override
    {
        return QCoreApplication::translate("Activity", "macOS input event source");
    }

    // The window list is ordered front to back: look at the frontmost normal
    // window and check whether it covers a whole display.
    bool isOtherAppFullscreen() override
    {
        CFArrayRef windows = CGWindowListCopyWindowInfo(
            kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        if (!windows)
            return false;

        bool fullscreen = false;
        for (CFIndex i = 0; i < CFArrayGetCount(windows); ++i) {
            const auto window = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));
            long long layer = -1;
            if (!numberValue(window, kCGWindowLayer, &layer) || layer != 0)
                continue;

            long long pid = 0;
            numberValue(window, kCGWindowOwnerPID, &pid);
            CGRect bounds = CGRectNull;
            const auto boundsDict = static_cast<CFDictionaryRef>(CFDictionaryGetValue(window, kCGWindowBounds));
            if (pid != getpid() && boundsDict && CGRectMakeWithDictionaryRepresentation(boundsDict, &bounds)) {
                CGDirectDisplayID displays[16];
                uint32_t count = 0;
                if (CGGetActiveDisplayList(16, displays, &count) == kCGErrorSuccess) {
                    for (uint32_t d = 0; d < count; ++d)
                        fullscreen = fullscreen || CGRectEqualToRect(bounds, CGDisplayBounds(displays[d]));
                }
            }
            break; // only the frontmost normal window matters
        }
        CFRelease(windows);
        return fullscreen;
    }

    QString fullscreenBackendName() const override
    {
        return QCoreApplication::translate("Activity", "macOS frontmost window");
    }
};

} // namespace

std::unique_ptr<ActivityBackend> createActivityBackend()
{
    return std::make_unique<MacActivityBackend>();
}
