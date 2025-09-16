#ifndef GLSLANG_C_API_H
#define GLSLANG_C_API_H

#define SUPPORT_VULKAN_BACKEND 1
#include "wgvk.h"
#include <wgvk_structs_impl.h>

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
#ifdef __cplusplus
extern "C"{
#endif
    WGPUShaderModule wgpuDeviceCreateShaderModuleGLSL(WGPUDevice device, const WGPUShaderModuleDescriptor* shDesc);
#ifdef __cplusplus
}
#endif
#endif