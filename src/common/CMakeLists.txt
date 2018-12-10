# Generate cpp with Git revision from template
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

add_library(common STATIC
    x64/xbyak_abi.h
    x64/xbyak_util.h
    alignment.h
    assert.h
    bit_field.h
    bit_set.h
    cityhash.cpp
    cityhash.h
    color.h
    common_funcs.h
    common_paths.h
    common_types.h
    file_util.cpp
    file_util.h
    hash.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/log.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    math_util.h
    misc.cpp
    param_package.cpp
    param_package.h
    quaternion.h
    ring_buffer.h
    scm_rev.cpp
    scm_rev.h
    scope_exit.h
    string_util.cpp
    string_util.h
    swap.h
    event.h
    thread_pool.h
    thread_queue_list.h
    threadsafe_queue.h
    vector_math.h
    web_result.h
)

create_target_directory_groups(common)

target_link_libraries(common PUBLIC Boost::boost fmt PRIVATE xbyak)
