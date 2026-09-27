cmake_minimum_required(VERSION 3.21)

project(Deskout
    VERSION 1.0.0
    DESCRIPTION "Desktop wellness and focus companion"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DESKOUT_BUILD_TESTS "Build Deskout unit tests" ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Network Sql Svg)

# Everything except main() lives in a static library so the unit tests can
# link against the same code the app runs.
add_library(deskout_lib STATIC
    src/app/alertcenter.cpp
    src/app/alertcenter.h
    src/app/deskoutapp.cpp
    src/app/deskoutapp.h
    src/app/focusalerts.cpp
    src/app/focusalerts.h
    src/app/readingmode.cpp
    src/app/readingmode.h
    src/app/reminderalerts.cpp
    src/app/reminderalerts.h
    src/app/statsrecorder.cpp
    src/app/statsrecorder.h
    src/core/alert.h
    src/core/commands.h
    src/core/dailyupdates.cpp
    src/core/dailyupdates.h
    src/core/database.cpp
    src/core/database.h
    src/core/docsstore.cpp
    src/core/docsstore.h
    src/core/eyeexercises.cpp
    src/core/eyeexercises.h
    src/core/focuslog.cpp
    src/core/focuslog.h
    src/core/focustimer.cpp
    src/core/focustimer.h
    src/core/notesstore.cpp
    src/core/notesstore.h
    src/core/pausemanager.cpp
    src/core/pausemanager.h
    src/core/reminderengine.cpp
    src/core/reminderengine.h
    src/core/reminders.cpp
    src/core/reminders.h
    src/core/settingskeys.h
    src/core/stats.cpp
    src/core/stats.h
    src/core/singleinstance.cpp
    src/core/singleinstance.h
    src/platform/activity/activitybackend.h
    src/platform/activity/activitymonitor.cpp
    src/platform/activity/activitymonitor.h
    src/platform/autostart.cpp
    src/platform/autostart.h
    src/platform/grayscale/grayscalebackend.h
    src/platform/hotkey/globalhotkey.cpp
    src/platform/hotkey/globalhotkey.h
    src/platform/hotkey/hotkeybackend.h
    src/platform/hotkey/keyformat.cpp
    src/platform/hotkey/keyformat.h
    src/ui/appicon.cpp
    src/ui/appicon.h
    src/ui/doceditor.cpp
    src/ui/doceditor.h
    src/ui/docspage.cpp
    src/ui/docspage.h
    src/ui/focuspage.cpp
    src/ui/focuspage.h
    src/ui/focustimerwindow.cpp
    src/ui/focustimerwindow.h
    src/ui/fullscreenalarm.cpp
    src/ui/fullscreenalarm.h
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/notespage.cpp
    src/ui/notespage.h
    src/ui/notifier.cpp
    src/ui/notifier.h
    src/ui/recapdialog.cpp
    src/ui/recapdialog.h
    src/ui/reminderspage.cpp
    src/ui/richtextedit.cpp
    src/ui/richtextedit.h
    src/ui/reminderspage.h
    src/ui/remindersettingspage.cpp
    src/ui/remindersettingspage.h
    src/ui/settingsdialog.cpp
    src/ui/settingsdialog.h
    src/ui/statspage.cpp
    src/ui/statspage.h
    src/ui/statswidgets.cpp
    src/ui/statswidgets.h
    src/ui/theme.cpp
    src/ui/theme.h
    src/ui/timedisplay.cpp
    src/ui/timedisplay.h
    src/ui/timeformat.cpp
    src/ui/timeformat.h
    src/ui/traycontroller.cpp
    src/ui/traycontroller.h
    src/ui/updatespage.cpp
    src/ui/updatespage.h
)

target_include_directories(deskout_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(deskout_lib PUBLIC DESKOUT_VERSION="${PROJECT_VERSION}")
target_link_libraries(deskout_lib PUBLIC Qt6::Widgets Qt6::Network Qt6::Sql Qt6::Svg)
if(MSVC)
    target_compile_options(deskout_lib PRIVATE /W4)
else()
    target_compile_options(deskout_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_sources(deskout_lib PRIVATE
        src/platform/activity/activity_win.cpp
        src/platform/grayscale/grayscale_win.cpp
        src/platform/hotkey/hotkeybackend_win.cpp)
    target_link_libraries(deskout_lib PUBLIC user32 shell32 magnification)
elseif(APPLE)
    target_sources(deskout_lib PRIVATE
        src/platform/activity/activity_mac.cpp
        src/platform/grayscale/grayscale_mac.cpp
        src/platform/hotkey/hotkeybackend_mac.cpp)
    target_link_libraries(deskout_lib PUBLIC "-framework Carbon" "-framework ApplicationServices")
else()
    find_package(X11 REQUIRED)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_sources(deskout_lib PRIVATE
        src/platform/activity/activity_linux.cpp
        src/platform/grayscale/grayscale_linux.cpp
        src/platform/linux/gsettings.cpp
        src/platform/linux/gsettings.h
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
        resources/icons/deskout-paused.svg
        resources/gnome-extension/metadata.json
        resources/gnome-extension/extension.js)
target_link_libraries(deskout PRIVATE deskout_lib)

# Icon shown by Explorer and Finder (the tray and window icons come from the SVG).
if(WIN32)
    target_sources(deskout PRIVATE packaging/windows/deskout.rc)
elseif(APPLE)
    set(deskout_icns packaging/macos/deskout.icns)
    target_sources(deskout PRIVATE ${deskout_icns})
    set_source_files_properties(${deskout_icns} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(deskout PROPERTIES OUTPUT_NAME Deskout)
endif()
set_target_properties(deskout PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER app.deskout.Deskout
    MACOSX_BUNDLE_BUNDLE_NAME Deskout
    MACOSX_BUNDLE_ICON_FILE deskout.icns
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

include(GNUInstallDirs)
install(TARGETS deskout
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Release packages: `cpack` builds a Windows installer (.exe) or a macOS disk
# image (.dmg). Linux packages are made by packaging/linux/package.sh.
if(WIN32 OR APPLE)
    if(WIN32)
        set(deskout_deploy_options --no-compiler-runtime --no-translations --no-opengl-sw
                                   --no-system-d3d-compiler)
    endif()
    # Copy the Qt libraries and plugins next to the app (windeployqt / macdeployqt).
    qt_generate_deploy_app_script(
        TARGET deskout
        OUTPUT_SCRIPT deskout_deploy_script
        DEPLOY_TOOL_OPTIONS ${deskout_deploy_options})
    install(SCRIPT ${deskout_deploy_script})
endif()

if(WIN32)
    # Ship the MSVC runtime DLLs with the app so no separate redistributable is needed.
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR INNOSETUP)
    set(CPACK_PACKAGE_EXECUTABLES deskout Deskout)
    set(CPACK_CREATE_DESKTOP_LINKS deskout)
    set(CPACK_INNOSETUP_RUN_EXECUTABLES deskout)
    set(CPACK_INNOSETUP_ICON_FILE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/deskout.ico)
    set(CPACK_INNOSETUP_USE_MODERN_WIZARD ON)
    set(CPACK_INNOSETUP_IGNORE_LICENSE_PAGE ON)
    # A fixed id lets a newer installer replace the installed version.
    set(CPACK_INNOSETUP_SETUP_AppId Deskout)
    set(CPACK_PACKAGE_FILE_NAME Deskout-${PROJECT_VERSION}-windows-x64-setup)
elseif(APPLE)
    # Deploying rewrites the binaries, so sign the bundle again (ad hoc, no
    # developer certificate). Apple Silicon refuses to run unsigned code.
    install(CODE [[
        execute_process(
            COMMAND codesign --force --deep --sign - "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Deskout.app"
            COMMAND_ERROR_IS_FATAL ANY)
    ]])

    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME Deskout)
    set(CPACK_PACKAGE_FILE_NAME Deskout-${PROJECT_VERSION}-macos)
endif()

if(WIN32 OR APPLE)
    set(CPACK_PACKAGE_NAME Deskout)
    set(CPACK_PACKAGE_VENDOR Deskout)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY Deskout)
    include(CPack)
endif()

if(DESKOUT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
