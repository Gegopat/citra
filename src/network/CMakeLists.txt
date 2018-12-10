add_library(network STATIC
    packet.cpp
    packet.h
    room.cpp
    room.h
    room_member.cpp
    room_member.h
)

create_target_directory_groups(network)

target_link_libraries(network PRIVATE common enet json-headers httplib)

get_directory_property(OPENSSL_LIBS
        DIRECTORY ${PROJECT_SOURCE_DIR}/externals/libressl
        DEFINITION OPENSSL_LIBS)
target_compile_definitions(network PRIVATE -DCPPHTTPLIB_OPENSSL_SUPPORT)

