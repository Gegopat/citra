// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include "common/common_types.h"
#include "video_core/renderer/resource_manager.h"

class StreamBuffer : private NonCopyable {
public:
    explicit StreamBuffer(GLenum target, GLsizeiptr size, bool array_buffer_for_amd,
                          bool prefer_coherent = false);
    ~StreamBuffer();

    GLuint GetHandle() const;
    GLsizeiptr GetSize() const;

    /*
     * Allocates a linear chunk of memory in the GPU buffer with at least "size" bytes
     * and the optional alignment requirement.
     * If the buffer is full, the whole buffer is reallocated which invalidates old chunks.
     * The return values are the pointer to the new chunk, the offset within the buffer,
     * and the invalidation flag for previous chunks.
     * The actual used size must be specified on unmapping the chunk.
     */
    std::tuple<u8*, GLintptr, bool> Map(GLsizeiptr size, GLintptr alignment = 0);

    void Unmap(GLsizeiptr size);

private:
    Buffer gl_buffer;
    GLenum gl_target;

    bool coherent = false;
    bool persistent = false;

    GLintptr buffer_pos{};
    GLsizeiptr buffer_size{};
    GLintptr mapped_offset{};
    GLsizeiptr mapped_size{};
    u8* mapped_ptr{};
};
