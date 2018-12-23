set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
if (POLICY CMP0071)
    cmake_policy(SET CMP0071 NEW)
endif()

add_executable(citra
    Info.plist
    issues_map.h
	qt_backend.h
	logging_window.cpp
	logging_window.h
    aboutdialog.cpp
    aboutdialog.h
    applets/swkbd.cpp
    applets/swkbd.h
    applets/mii_selector.cpp
    applets/mii_selector.h
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
    control_panel.cpp
    control_panel.h
    camera/still_image_camera.cpp
    camera/still_image_camera.h
    configuration/config.cpp
    configuration/config.h
    configuration/audio.cpp
    configuration/audio.h
    configuration/camera.cpp
    configuration/camera.h
    configuration/hacks.cpp
    configuration/hacks.h
    configuration/lle.cpp
    configuration/lle.h
    configuration/dialog.cpp
    configuration/dialog.h
    configuration/general.cpp
    configuration/general.h
    configuration/graphics.cpp
    configuration/hotkeys.cpp
    configuration/hotkeys.h
    configuration/input.cpp
    configuration/input.h
    configuration/motion_touch.cpp
    configuration/motion_touch.h
    configuration/system.cpp
    configuration/system.h
    configuration/ui.cpp
    configuration/ui.h
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
    util/sequence_dialog/sequence_dialog.cpp
    util/sequence_dialog/sequence_dialog.h
    util/util.cpp
    util/util.h
)

set(UIS
	logging_window.ui
    applets/swkbd.ui
    applets/mii_selector.ui
    control_panel.ui
    configuration/dialog.ui
    configuration/audio.ui
    configuration/camera.ui
    configuration/hacks.ui
    configuration/lle.ui
    configuration/general.ui
    configuration/graphics.ui
    configuration/hotkeys.ui
    configuration/input.ui
    configuration/motion_touch.ui
    configuration/system.ui
    configuration/ui.ui
    aboutdialog.ui
    cheats.ui
    main.ui
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
    # Compile as a win32 gui application instead of a console application
    target_link_libraries(citra PRIVATE Qt5::WinMain)
    if(MSVC)
        set_target_properties(citra PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
    elseif(MINGW)
        set_target_properties(citra PROPERTIES LINK_FLAGS_RELEASE "-mwindows")
    endif()
endif()

create_target_directory_groups(citra)

target_link_libraries(citra PRIVATE audio_core common core input_common network video_core)
target_link_libraries(citra PRIVATE asls Boost::boost Qt5::OpenGL Qt5::Widgets Qt5::Multimedia)
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
