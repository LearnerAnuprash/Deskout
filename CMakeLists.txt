cmake_minimum_required(VERSION 3.21)

project(Deskout
    VERSION 0.1.0
    DESCRIPTION "Desktop wellness and focus companion"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DESKOUT_BUILD_TESTS "Build Deskout unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network Svg)

# Everything except main() lives in a static library so the unit tests can
# link against the same code the app runs.
add_library(deskout_lib STATIC
    src/app/deskoutapp.cpp
    src/app/deskoutapp.h
    src/app/reminderalerts.cpp
    src/app/reminderalerts.h
    src/core/commands.h
    src/core/eyeexercises.cpp
    src/core/eyeexercises.h
    src/core/pausemanager.cpp
    src/core/pausemanager.h
    src/core/reminderengine.cpp
    src/core/reminderengine.h
    src/core/reminders.cpp
    src/core/reminders.h
    src/core/settingskeys.h
    src/core/singleinstance.cpp
    src/core/singleinstance.h
    src/platform/activity/activitybackend.h
    src/platform/activity/activitymonitor.cpp
    src/platform/activity/activitymonitor.h
    src/platform/autostart.cpp
    src/platform/autostart.h
    src/platform/hotkey/globalhotkey.cpp
    src/platform/hotkey/globalhotkey.h
    src/platform/hotkey/hotkeybackend.h
    src/platform/hotkey/keyformat.cpp
    src/platform/hotkey/keyformat.h
    src/ui/appicon.cpp
    src/ui/appicon.h
    src/ui/fullscreenalarm.cpp
    src/ui/fullscreenalarm.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/notifier.cpp
    src/ui/notifier.h
    src/ui/reminderspage.cpp
    src/ui/reminderspage.h
    src/ui/remindersettingspage.cpp
    src/ui/remindersettingspage.h
    src/ui/settingsdialog.cpp
    src/ui/settingsdialog.h
    src/ui/theme.cpp
    src/ui/theme.h
    src/ui/traycontroller.cpp
    src/ui/traycontroller.h
)

target_include_directories(deskout_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(deskout_lib PUBLIC DESKOUT_VERSION="${PROJECT_VERSION}")
target_link_libraries(deskout_lib PUBLIC Qt6::Widgets Qt6::Network Qt6::Svg)
if(MSVC)
    target_compile_options(deskout_lib PRIVATE /W4)
else()
    target_compile_options(deskout_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_sources(deskout_lib PRIVATE
        src/platform/activity/activity_win.cpp
        src/platform/hotkey/hotkeybackend_win.cpp)
    target_link_libraries(deskout_lib PUBLIC user32 shell32)
elseif(APPLE)
    target_sources(deskout_lib PRIVATE
        src/platform/activity/activity_mac.cpp
        src/platform/hotkey/hotkeybackend_mac.cpp)
    target_link_libraries(deskout_lib PUBLIC "-framework Carbon" "-framework ApplicationServices")
else()
    find_package(X11 REQUIRED)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_sources(deskout_lib PRIVATE
        src/platform/activity/activity_linux.cpp
        src/platform/hotkey/hotkeybackend_linux.cpp
        src/platform/hotkey/hotkeybackend_gnome.cpp
        src/platform/hotkey/hotkeybackend_x11.cpp
        src/platform/hotkey/linuxbackends.h)
    target_link_libraries(deskout_lib PUBLIC X11::X11 Qt6::DBus ${CMAKE_DL_LIBS})
endif()

qt_add_executable(deskout WIN32 MACOSX_BUNDLE src/main.cpp)
qt_add_resources(deskout "deskout_resources"
    PREFIX "/"
    BASE resources
    FILES
        resources/icons/deskout.svg
        resources/icons/deskout-paused.svg)
target_link_libraries(deskout PRIVATE deskout_lib)

if(APPLE)
    set_target_properties(deskout PROPERTIES OUTPUT_NAME Deskout)
endif()
set_target_properties(deskout PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER app.deskout.Deskout
    MACOSX_BUNDLE_BUNDLE_NAME Deskout
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

include(GNUInstallDirs)
install(TARGETS deskout
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(DESKOUT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
