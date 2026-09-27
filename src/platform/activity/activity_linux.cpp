#include "platform/activity/activitybackend.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>

#include <dlfcn.h>
#include <unistd.h>

#include <vector>

// X11 headers last: their macros (None, Bool, Status...) clash with Qt.
#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace {

constexpr int DBusTimeoutMs = 500;

// Layout of XScreenSaverInfo from <X11/extensions/scrnsaver.h>. libXss is
// loaded at runtime so building doesn't need its dev package.
struct XssInfo
{
    Window window;
    int state;
    int kind;
    unsigned long tilOrSince;
    unsigned long idle;
    unsigned long eventMask;
};
using XssAllocFn = XssInfo *(*)();
using XssQueryFn = int (*)(Display *, Drawable, XssInfo *);

int ignoreXErrors(Display *, XErrorEvent *)
{
    return 0;
}

bool envContains(const char *name, const char *needle)
{
    return QString::fromLocal8Bit(qgetenv(name)).contains(QLatin1String(needle), Qt::CaseInsensitive);
}

QDBusMessage callSession(const char *service, const char *path, const char *interface,
                         const char *method, const QVariantList &args = {})
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String(service), QLatin1String(path),
                                                          QLatin1String(interface), QLatin1String(method));
    message.setArguments(args);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, DBusTimeoutMs);
}

class LinuxActivityBackend final : public ActivityBackend
{
public:
    LinuxActivityBackend()
    {
        m_waylandSession = envContains("XDG_SESSION_TYPE", "wayland");
        if (!qEnvironmentVariableIsEmpty("DISPLAY"))
            m_display = XOpenDisplay(nullptr);

        m_hasMutterIdle = mutterIdleMs().has_value();
        // On Wayland, XScreenSaver only sees input sent to X11 clients.
        if (!m_hasMutterIdle && m_display && !m_waylandSession)
            loadXss();

        const QDBusMessage reply = gnomeInhibitedReply();
        m_hasGnomeInhibit = reply.type() == QDBusMessage::ReplyMessage;
    }

    ~LinuxActivityBackend() override
    {
        if (m_xssInfo)
            XFree(m_xssInfo);
        if (m_display)
            XCloseDisplay(m_display);
        if (m_xssLibrary)
            dlclose(m_xssLibrary);
    }

    std::optional<qint64> idleMs() override
    {
        if (m_hasMutterIdle)
            return mutterIdleMs();
        if (m_xssInfo && m_xssQuery(m_display, DefaultRootWindow(m_display), m_xssInfo))
            return qint64(m_xssInfo->idle);
        return std::nullopt;
    }

    QString idleBackendName() const override
    {
        if (m_hasMutterIdle)
            return QCoreApplication::translate("Activity", "GNOME idle monitor");
        if (m_xssInfo)
            return QCoreApplication::translate("Activity", "X11 screensaver extension");
        return QCoreApplication::translate("Activity", "Unavailable on this desktop");
    }

    bool isOtherAppFullscreen() override
    {
        return x11ActiveWindowFullscreen() || gnomeIdleInhibited();
    }

    QString fullscreenBackendName() const override
    {
        QStringList parts;
        if (m_display)
            parts << (m_waylandSession ? QCoreApplication::translate("Activity", "X11 apps (XWayland)")
                                       : QCoreApplication::translate("Activity", "X11 active window"));
        if (m_hasGnomeInhibit)
            parts << QCoreApplication::translate("Activity", "GNOME idle inhibitors (calls, video, presentations)");
        return parts.isEmpty() ? QCoreApplication::translate("Activity", "Unavailable")
                               : parts.join(QLatin1String(" + "));
    }

private:
    static std::optional<qint64> mutterIdleMs()
    {
        const QDBusMessage reply = callSession("org.gnome.Mutter.IdleMonitor",
                                               "/org/gnome/Mutter/IdleMonitor/Core",
                                               "org.gnome.Mutter.IdleMonitor", "GetIdletime");
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
            return std::nullopt;
        return qint64(reply.arguments().constFirst().toULongLong());
    }

    static QDBusMessage gnomeInhibitedReply()
    {
        // Flag 8 = "inhibit the session being marked idle": set by video
        // players, browsers during calls/video, presentation tools.
        return callSession("org.gnome.SessionManager", "/org/gnome/SessionManager",
                           "org.gnome.SessionManager", "IsInhibited", {QVariant::fromValue(uint(8))});
    }

    bool gnomeIdleInhibited() const
    {
        if (!m_hasGnomeInhibit)
            return false;
        const QDBusMessage reply = gnomeInhibitedReply();
        return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()
               && reply.arguments().constFirst().toBool();
    }

    void loadXss()
    {
        m_xssLibrary = dlopen("libXss.so.1", RTLD_LAZY);
        if (!m_xssLibrary)
            return;
        auto alloc = reinterpret_cast<XssAllocFn>(dlsym(m_xssLibrary, "XScreenSaverAllocInfo"));
        m_xssQuery = reinterpret_cast<XssQueryFn>(dlsym(m_xssLibrary, "XScreenSaverQueryInfo"));
        if (alloc && m_xssQuery)
            m_xssInfo = alloc();
    }

    std::vector<unsigned long> property(Window window, const char *name, Atom type) const
    {
        std::vector<unsigned long> values;
        Atom actualType = 0;
        int format = 0;
        unsigned long count = 0;
        unsigned long bytesAfter = 0;
        unsigned char *data = nullptr;
        const Atom atom = XInternAtom(m_display, name, False);
        if (XGetWindowProperty(m_display, window, atom, 0, 1024, False, type, &actualType, &format,
                               &count, &bytesAfter, &data) == Success
            && data && format == 32) {
            // Xlib returns 32-bit properties as an array of long.
            const auto *longs = reinterpret_cast<const unsigned long *>(data);
            values.assign(longs, longs + count);
        }
        if (data)
            XFree(data);
        return values;
    }

    bool x11ActiveWindowFullscreen() const
    {
        if (!m_display)
            return false;
        // The active window can vanish mid-query; don't let Xlib's default
        // handler kill the process over a BadWindow.
        auto *previous = XSetErrorHandler(ignoreXErrors);
        bool fullscreen = false;
        const std::vector<unsigned long> active =
            property(DefaultRootWindow(m_display), "_NET_ACTIVE_WINDOW", XA_WINDOW);
        if (!active.empty() && active.front() != 0) {
            const Window window = Window(active.front());
            const std::vector<unsigned long> pid = property(window, "_NET_WM_PID", XA_CARDINAL);
            const bool ours = !pid.empty() && pid_t(pid.front()) == getpid();
            if (!ours) {
                const Atom fullscreenAtom = XInternAtom(m_display, "_NET_WM_STATE_FULLSCREEN", False);
                for (unsigned long state : property(window, "_NET_WM_STATE", XA_ATOM))
                    fullscreen = fullscreen || Atom(state) == fullscreenAtom;
            }
        }
        XSync(m_display, False);
        XSetErrorHandler(previous);
        return fullscreen;
    }

    Display *m_display = nullptr;
    bool m_waylandSession = false;
    bool m_hasMutterIdle = false;
    bool m_hasGnomeInhibit = false;
    void *m_xssLibrary = nullptr;
    XssQueryFn m_xssQuery = nullptr;
    XssInfo *m_xssInfo = nullptr;
};

} // namespace

std::unique_ptr<ActivityBackend> createActivityBackend()
{
    return std::make_unique<LinuxActivityBackend>();
}
