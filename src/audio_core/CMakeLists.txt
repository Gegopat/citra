add_library(audio_core STATIC
    audio_types.h
    codec.cpp
    codec.h
    dsp_interface.cpp
    dsp_interface.h
    hle/decoder.cpp
    hle/decoder.h
    hle/filter.cpp
    hle/filter.h
    hle/hle.cpp
    hle/hle.h
    hle/mixers.cpp
    hle/mixers.h
    hle/shared_memory.h
    hle/source.cpp
    hle/source.h
    lle/lle.cpp
    lle/lle.h
    interpolate.cpp
    interpolate.h
    sink.cpp
    sink.h
    time_stretch.cpp
    time_stretch.h
    $<$<BOOL:${FFMPEG_FOUND}>:hle/ffmpeg_decoder.cpp hle/ffmpeg_decoder.h hle/ffmpeg_dl.cpp hle/ffmpeg_dl.h>
)

create_target_directory_groups(audio_core)

target_link_libraries(audio_core PUBLIC common core)
target_link_libraries(audio_core PRIVATE SoundTouch teakra cubeb)

if(FFMPEG_FOUND)
    if(UNIX)
        target_link_libraries(audio_core PRIVATE FFmpeg::avcodec)
    else()
        target_include_directories(audio_core PRIVATE ${FFMPEG_DIR}/include)
    endif()
    target_compile_definitions(audio_core PRIVATE HAVE_FFMPEG)
endif()
