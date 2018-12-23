// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include "common/common_types.h"
#include "video_core/shader/check_sse4_1.h"

#ifdef _MSC_VER
#include <intrin.h>
#else

#if defined(__DragonFly__) || defined(__FreeBSD__)
// clang-format off
#include <sys/types.h>
#include <machine/cpufunc.h>
// clang-format on
#endif

static inline void __cpuidex(int info[4], int function_id, int subfunction_id) {
#if defined(__DragonFly__) || defined(__FreeBSD__)
    // Despite the name, this is just do_cpuid() with ECX as second input.
    cpuid_count((u_int)function_id, (u_int)subfunction_id, (u_int*)info);
#else
    info[0] = function_id;    // EAX
    info[2] = subfunction_id; // ECX
    __asm__("cpuid"
            : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
            : "a"(function_id), "c"(subfunction_id));
#endif
}

static inline void __cpuid(int info[4], int function_id) {
    return __cpuidex(info, function_id, 0);
}

#define _XCR_XFEATURE_ENABLED_MASK 0
static inline u64 _xgetbv(u32 index) {
    u32 eax, edx;
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((u64)edx << 32) | eax;
}

#endif // _MSC_VER

static const bool CheckSSE41Supported() {
    // Assumes the CPU supports the CPUID instruction. Those that don't would likely not support
    // Citra at all anyway
    int cpu_id[4];
    __cpuid(cpu_id, 0x00000000);
    int max_std_fn{cpu_id[0]}; // EAX
    if (max_std_fn >= 1) {
        __cpuid(cpu_id, 0x00000001);
        if ((cpu_id[2] >> 19) & 1)
            return true;
        else
            return false;
    }
    return false;
}

namespace Pica::Shader {

const bool IsSSE41Supported() {
    static const bool supported{CheckSSE41Supported()};
    return supported;
}

} // namespace Pica::Shader
