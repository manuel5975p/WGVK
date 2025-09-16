#ifndef TINT_C_API_H
#define TINT_C_API_H

#define SUPPORT_VULKAN_BACKEND 1
#include "wgvk.h"
#include <wgvk_structs_impl.h>


RGAPI WGPUReflectionInfo reflectionInfo_wgsl_sync(WGPUStringView wgslSource);
#ifndef SPIRV_BLOB_STRUCTS
#define SPIRV_BLOB_STRUCTS
typedef struct tc_spirvSingleEntrypoint{
    char entryPointName[16];
    size_t codeSize;
    uint32_t* code;
}tc_spirvSingleEntrypoint;

typedef struct tc_SpirvBlob{
    tc_spirvSingleEntrypoint entryPoints[16];
}tc_SpirvBlob;
#endif
RGAPI tc_SpirvBlob wgslToSpirv(const WGPUShaderSourceWGSL* source, uint32_t constantCount, const WGPUConstantEntry* constants);
RGAPI void reflectionInfo_wgsl_free(WGPUReflectionInfo* reflectionInfo);

#endif