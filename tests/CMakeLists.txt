find_package(Qt6 REQUIRED COMPONENTS Test)

function(deskout_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE deskout_lib Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    # No display needed for these tests.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

deskout_add_test(tst_pausemanager)
deskout_add_test(tst_keyformat)
deskout_add_test(tst_autostart)
deskout_add_test(tst_reminders)
deskout_add_test(tst_reminderengine)
deskout_add_test(tst_readingmode)
if(UNIX AND NOT APPLE)
    deskout_add_test(tst_gsettings)
endif()
