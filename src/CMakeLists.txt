# Enable modules to include each other's files
include_directories(.)

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(video_core)
add_subdirectory(audio_core)
add_subdirectory(network)
add_subdirectory(input_common)
add_subdirectory(citra)
add_subdirectory(dedicated_room)
