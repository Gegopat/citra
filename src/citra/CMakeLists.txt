set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
cmake_policy(SET CMP0071 NEW)

add_executable(citra
    Info.plist
    issues_map.h
    aboutdialog.cpp
    aboutdialog.h
    bootmanager.cpp
    bootmanager.h
    cheats.cpp
    cheats.h
    camera/camera_util.cpp
    camera/camera_util.h
    camera/still_image_camera.cpp
    camera/still_image_camera.h
    camera/qt_camera_base.cpp
    camera/qt_camera_base.h
    camera/qt_multimedia_camera.cpp
    camera/qt_multimedia_camera.h
    citra.rc
    camera/still_image_camera.cpp
    camera/still_image_camera.h
    configuration/config.cpp
    configuration/config.h
    configuration/configure_audio.cpp
    configuration/configure_audio.h
    configuration/configure_camera.cpp
    configuration/configure_camera.h
    configuration/configure_hacks.cpp
    configuration/configure_hacks.h
    configuration/configure_lle.cpp
    configuration/configure_lle.h
    configuration/configure_dialog.cpp
    configuration/configure_dialog.h
    configuration/configure_general.cpp
    configuration/configure_general.h
    configuration/configure_graphics.cpp
    configuration/configure_graphics.h
    configuration/configure_input.cpp
    configuration/configure_input.h
    configuration/configure_motion_touch.cpp
    configuration/configure_motion_touch.h
    configuration/configure_system.cpp
    configuration/configure_system.h
    configuration/configure_ui.cpp
    configuration/configure_ui.h
    multiplayer/chat_room.cpp
    multiplayer/chat_room.h
    multiplayer/client_room.cpp
    multiplayer/client_room.h
    multiplayer/direct_connect.cpp
    multiplayer/direct_connect.h
    multiplayer/host_room.cpp
    multiplayer/host_room.h
    multiplayer/lobby.cpp
    multiplayer/lobby.h
    multiplayer/lobby_p.h
    multiplayer/message.cpp
    multiplayer/message.h
    multiplayer/moderation_dialog.cpp
    multiplayer/moderation_dialog.h
    multiplayer/state.cpp
    multiplayer/state.h
    multiplayer/validation.h
    program_list.cpp
    program_list.h
    program_list_p.h
    program_list_worker.cpp
    program_list_worker.h
    hotkeys.cpp
    hotkeys.h
    main.cpp
    main.h
    ui_settings.cpp
    ui_settings.h
    util/clickable_label.cpp
    util/clickable_label.h
    util/util.cpp
    util/util.h
    swkbd.cpp
    swkbd.h
    mii_selector.cpp
    mii_selector.h
    control_panel.cpp
    control_panel.h
)

set(UIS
    configuration/configure.ui
    configuration/configure_audio.ui
    configuration/configure_camera.ui
    configuration/configure_hacks.ui
    configuration/configure_lle.ui
    configuration/configure_general.ui
    configuration/configure_graphics.ui
    configuration/configure_input.ui
    configuration/configure_motion_touch.ui
    configuration/configure_system.ui
    configuration/configure_ui.ui
    swkbd.ui
    mii_selector.ui
    aboutdialog.ui
    hotkeys.ui
    cheats.ui
    main.ui
    control_panel.ui
    multiplayer/chat_room.ui
    multiplayer/client_room.ui
    multiplayer/direct_connect.ui
    multiplayer/host_room.ui
    multiplayer/lobby.ui
    multiplayer/moderation_dialog.ui
)

file(GLOB_RECURSE ICONS ${PROJECT_SOURCE_DIR}/dist/icons/*)
file(GLOB_RECURSE THEMES ${PROJECT_SOURCE_DIR}/dist/themes/*)

qt5_wrap_ui(UI_HDRS ${UIS})

target_sources(citra
    PRIVATE
        ${ICONS}
        ${THEMES}
        ${UI_HDRS}
        ${UIS}
)

if (APPLE)
    set(MACOSX_ICON "../../dist/citra.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(citra PRIVATE ${MACOSX_ICON})
    set_target_properties(citra PROPERTIES MACOSX_BUNDLE TRUE)
    set_target_properties(citra PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
elseif(WIN32)
    # compile as a win32 gui application instead of a console application
    target_link_libraries(citra PRIVATE Qt5::WinMain)
    if(MSVC)
        set_target_properties(citra PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(citra PROPERTIES LINK_FLAGS_RELEASE "-mwindows")
    endif()
endif()

create_target_directory_groups(citra)

target_link_libraries(citra PRIVATE audio_core common core input_common network video_core)
target_link_libraries(citra PRIVATE Boost::boost Qt5::OpenGL Qt5::Widgets Qt5::Multimedia)
if (ENABLE_DISCORD_RPC)
    target_link_libraries(citra PRIVATE discord-rpc)
    target_compile_definitions(citra PRIVATE -DENABLE_DISCORD_RPC)
endif()
target_link_libraries(citra PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)

if (MSVC)
    include(CopyCitraQt5Deps)
    include(CopyCitraSDLDeps)
    copy_citra_Qt5_deps(citra)
    copy_citra_SDL_deps(citra)
endif()
