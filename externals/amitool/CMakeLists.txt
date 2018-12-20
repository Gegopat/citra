add_library(amitool STATIC
    amiibo.c
    amiitool.c
    drbg.c
    keygen.c
    util.c
    version.c
    gitversion.h
    util.h
    include/nfc3d/amiibo.h
    include/nfc3d/amitool.h
    include/nfc3d/drbg.h
    include/nfc3d/keygen.h
    include/nfc3d/version.h
)

target_include_directories(amitool PUBLIC "include")
target_link_libraries(amitool PRIVATE mbedtls)
