set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)

add_executable(citra-room
    citra-room.cpp
    citra-room.rc
)

create_target_directory_groups(citra-room)

target_link_libraries(citra-room PRIVATE common network)
if (MSVC)
    target_link_libraries(citra-room PRIVATE getopt)
endif()
target_link_libraries(citra-room PRIVATE ${PLATFORM_LIBRARIES} Threads::Threads)
