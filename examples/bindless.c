// bindless.c - minimal usage example for bindless descriptor arrays
//
// Two independent demos:
//
// 1. A bindless array of storage buffers (WGPUBindGroupLayoutDescriptorBindless,
//    binding 1) alongside a plain storage buffer (binding 0) the shader writes
//    results into. SLOT_COUNT buffers are attached, a compute pass reads each
//    slot with a non-uniform index, then one slot is cleared and reattached to
//    a different buffer to show slots can be freed and reused at runtime.
//
// 2. A bindless array of sampled images (binding 3: two small procedural
//    textures A and B) plus a plain sampler (binding 4), sampled bindlessly
//    into three plain storage-image outputs (bindings 0-2: A, B, and A+B).
//    Written out as outputA.png / outputB.png / outputAB.png.

#include "common.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define SLOT_COUNT 4
#define IMG_SIZE 64

static const char bufferArraySource[] =
"#version 460\n"
"#extension GL_EXT_nonuniform_qualifier : require\n"
"\n"
"layout(local_size_x = 4, local_size_y = 1, local_size_z = 1) in;\n"
"\n"
"layout(std430, binding = 0) writeonly buffer ResultsBuffer {\n"
"    uint results[];\n"
"};\n"
"\n"
"layout(std430, binding = 1) readonly buffer InputBlock {\n"
"    uint value;\n"
"} inputs[4];\n"
"\n"
"void main() {\n"
"    uint i = gl_GlobalInvocationID.x;\n"
"    results[i] = inputs[nonuniformEXT(i)].value;\n"
"}\n"
;

static const char textureArraySource[] =
"#version 460\n"
"#extension GL_EXT_nonuniform_qualifier : require\n"
"\n"
"layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;\n"
"\n"
"layout(binding = 0, rgba32f) uniform writeonly image2D outputA;\n"
"layout(binding = 1, rgba32f) uniform writeonly image2D outputB;\n"
"layout(binding = 2, rgba32f) uniform writeonly image2D outputAB;\n"
"\n"
"layout(binding = 4) uniform sampler samp;\n"
"layout(binding = 3) uniform texture2D textures[2];\n"
"\n"
"void main() {\n"
"    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);\n"
"    vec2 uv = (vec2(coord) + 0.5) / vec2(imageSize(outputA));\n"
"\n"
"    vec4 colorA = texture(sampler2D(textures[nonuniformEXT(0)], samp), uv);\n"
"    vec4 colorB = texture(sampler2D(textures[nonuniformEXT(1)], samp), uv);\n"
"\n"
"    imageStore(outputA, coord, colorA);\n"
"    imageStore(outputB, coord, colorB);\n"
"    imageStore(outputAB, coord, min(colorA + colorB, vec4(1.0)));\n"
"}\n"
;

static WGPUShaderModule compileGLSLModule(WGPUDevice device, const char *source, WGPUShaderStage stage) {
    WGPUShaderSourceGLSL glslSource = {
        .chain.sType = WGPUSType_ShaderSourceGLSL,
        .code = { source, WGPU_STRLEN },
        .stage = stage
    };
    WGPUShaderModuleDescriptor md = {
        .nextInChain = &glslSource.chain
    };
    return wgpuDeviceCreateShaderModule(device, &md);
}

// -----------------------------------------------------------------------------
// Demo 1: bindless storage buffer array
// -----------------------------------------------------------------------------

static void dispatchAndPrintBuffers(WGPUDevice device, WGPUQueue queue, WGPUComputePipeline pipeline, WGPUBindGroup bindGroup, WGPUBuffer resultsBuffer, WGPUBuffer readback) {
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, NULL);
    WGPUComputePassEncoder cpenc = wgpuCommandEncoderBeginComputePass(enc, NULL);
    wgpuComputePassEncoderSetPipeline(cpenc, pipeline);
    wgpuComputePassEncoderSetBindGroup(cpenc, 0, bindGroup, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(cpenc, 1, 1, 1);
    wgpuComputePassEncoderEnd(cpenc);
    wgpuComputePassEncoderRelease(cpenc);

    wgpuCommandEncoderCopyBufferToBuffer(enc, resultsBuffer, 0, readback, 0, sizeof(uint32_t) * SLOT_COUNT);
    WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(enc, NULL);
    wgpuCommandEncoderRelease(enc);
    wgpuQueueSubmit(queue, 1, &cbuf);
    wgpuCommandBufferRelease(cbuf);
    wgpuQueueWaitIdle(queue);

    uint32_t *mapped = NULL;
    wgpuBufferMap(readback, WGPUMapMode_Read, 0, sizeof(uint32_t) * SLOT_COUNT, (void **)&mapped);
    for (uint32_t i = 0; i < SLOT_COUNT; i++) {
        printf("results[%u] = %u\n", i, mapped[i]);
    }
    wgpuBufferUnmap(readback);
}

static void runBufferArrayDemo(WGPUDevice device, WGPUQueue queue) {
    WGPUShaderModule computeModule = compileGLSLModule(device, bufferArraySource, WGPUShaderStage_Compute);

    WGPUBindGroupLayoutDescriptorBindless bindlessMarker = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_BindGroupLayoutDescriptorBindless
        }
    };

    // Binding 0 (results) comes first so it lines up positionally with the single
    // entry passed to wgpuDeviceCreateBindGroup below; the bindless array (binding 1)
    // is populated afterwards via wgpuBindGroupUpdateEntry.
    WGPUBindGroupLayoutEntry bglEntries[2] = {
        [0] = {
            .binding = 0,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type = WGPUBufferBindingType_Storage,
                .minBindingSize = sizeof(uint32_t) * SLOT_COUNT
            }
        },
        [1] = {
            .binding = 1,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type = WGPUBufferBindingType_Storage,
                .minBindingSize = sizeof(uint32_t)
            },
            .bindingArraySize = SLOT_COUNT
        }
    };

    WGPUBindGroupLayoutDescriptor bglDesc = {
        .nextInChain = &bindlessMarker.chain,
        .entries = bglEntries,
        .entryCount = 2
    };

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayout plLayout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor){
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl
    });

    WGPUComputePipelineDescriptor cplDesc = {
        .label = STRVIEW("bindless buffer array compute"),
        .layout = plLayout,
        .compute = {
            .module = computeModule,
            .entryPoint = STRVIEW("main")
        }
    };
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &cplDesc);

    WGPUBuffer resultsBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = sizeof(uint32_t) * SLOT_COUNT,
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc
    });

    WGPUBuffer readback = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = sizeof(uint32_t) * SLOT_COUNT,
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead
    });

    WGPUBuffer inputBuffers[SLOT_COUNT];
    for (uint32_t i = 0; i < SLOT_COUNT; i++) {
        inputBuffers[i] = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
            .size = sizeof(uint32_t),
            .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst
        });
        uint32_t value = (i + 1) * 111;
        wgpuQueueWriteBuffer(queue, inputBuffers[i], 0, &value, sizeof(value));
    }

    WGPUBindGroupEntry bgEntries[1] = {
        {
            .binding = 0,
            .buffer = resultsBuffer,
            .size = sizeof(uint32_t) * SLOT_COUNT
        }
    };
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
        .layout = bgl,
        .entries = bgEntries,
        .entryCount = 1
    });

    for (uint32_t i = 0; i < SLOT_COUNT; i++) {
        WGPUBindGroupEntry entry = {
            .binding = 1,
            .buffer = inputBuffers[i],
            .size = sizeof(uint32_t)
        };
        wgpuBindGroupUpdateEntry(bindGroup, 1, i, &entry);
    }

    printf("-- buffer array: initial slots --\n");
    dispatchAndPrintBuffers(device, queue, pipeline, bindGroup, resultsBuffer, readback);

    // Detach slot 2 and reattach it to a different buffer to show slots can be
    // freed and reused at runtime.
    wgpuBindGroupClearEntry(bindGroup, 1, 2);

    WGPUBuffer replacementBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = sizeof(uint32_t),
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst
    });
    uint32_t replacementValue = 9999;
    wgpuQueueWriteBuffer(queue, replacementBuffer, 0, &replacementValue, sizeof(replacementValue));

    WGPUBindGroupEntry replacementEntry = {
        .binding = 1,
        .buffer = replacementBuffer,
        .size = sizeof(uint32_t)
    };
    wgpuBindGroupUpdateEntry(bindGroup, 1, 2, &replacementEntry);

    printf("-- buffer array: after clearing + reattaching slot 2 --\n");
    dispatchAndPrintBuffers(device, queue, pipeline, bindGroup, resultsBuffer, readback);
}

// -----------------------------------------------------------------------------
// Demo 2: bindless sampled image array
// -----------------------------------------------------------------------------

// Small procedural RGBA32Float texture: a horizontal red gradient over a dim base.
static void fillPatternA(float *pixels) {
    for (uint32_t y = 0; y < IMG_SIZE; y++) {
        for (uint32_t x = 0; x < IMG_SIZE; x++) {
            float *p = pixels + (y * IMG_SIZE + x) * 4;
            p[0] = (float)x / (float)(IMG_SIZE - 1);
            p[1] = 0.15f;
            p[2] = 0.15f;
            p[3] = 1.0f;
        }
    }
}

// Small procedural RGBA32Float texture: an 8x8 green/blue checkerboard.
static void fillPatternB(float *pixels) {
    for (uint32_t y = 0; y < IMG_SIZE; y++) {
        for (uint32_t x = 0; x < IMG_SIZE; x++) {
            float *p = pixels + (y * IMG_SIZE + x) * 4;
            int checker = ((x / 8) + (y / 8)) % 2;
            p[0] = 0.1f;
            p[1] = checker ? 0.8f : 0.2f;
            p[2] = checker ? 0.2f : 0.9f;
            p[3] = 1.0f;
        }
    }
}

static WGPUTextureView createSampledTexture(WGPUDevice device, WGPUQueue queue, const float *pixels, WGPUTexture *outTexture) {
    const WGPUTextureFormat format = WGPUTextureFormat_RGBA32Float;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &(WGPUTextureDescriptor){
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = { .width = IMG_SIZE, .height = IMG_SIZE, .depthOrArrayLayers = 1 },
        .format = format,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = &format
    });

    WGPUTexelCopyTextureInfo dst = {
        .texture = texture,
        .mipLevel = 0,
        .origin = { 0, 0, 0 },
        .aspect = WGPUTextureAspect_All
    };
    WGPUTexelCopyBufferLayout layout = {
        .offset = 0,
        .bytesPerRow = IMG_SIZE * sizeof(float) * 4,
        .rowsPerImage = IMG_SIZE
    };
    WGPUExtent3D writeSize = { IMG_SIZE, IMG_SIZE, 1 };
    wgpuQueueWriteTexture(queue, &dst, pixels, IMG_SIZE * IMG_SIZE * sizeof(float) * 4, &layout, &writeSize);

    WGPUTextureView view = wgpuTextureCreateView(texture, &(WGPUTextureViewDescriptor){
        .format = format,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_TextureBinding
    });

    *outTexture = texture;
    return view;
}

static WGPUTexture createOutputImage(WGPUDevice device) {
    const WGPUTextureFormat format = WGPUTextureFormat_RGBA32Float;
    return wgpuDeviceCreateTexture(device, &(WGPUTextureDescriptor){
        .usage = WGPUTextureUsage_StorageBinding | WGPUTextureUsage_CopySrc,
        .dimension = WGPUTextureDimension_2D,
        .size = { .width = IMG_SIZE, .height = IMG_SIZE, .depthOrArrayLayers = 1 },
        .format = format,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = &format
    });
}

static void writeImageToPNG(WGPUDevice device, WGPUQueue queue, WGPUTexture texture, const char *filename) {
    size_t pixelCount = (size_t)IMG_SIZE * (size_t)IMG_SIZE;
    size_t floatPixelSize = 4 * sizeof(float);
    size_t bufferSize = pixelCount * floatPixelSize;

    WGPUBuffer readback = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = bufferSize,
        .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst
    });

    WGPUTexelCopyTextureInfo src = {
        .texture = texture,
        .mipLevel = 0,
        .origin = { 0, 0, 0 },
        .aspect = WGPUTextureAspect_All
    };
    WGPUTexelCopyBufferInfo dst = {
        .buffer = readback,
        .layout = {
            .bytesPerRow = IMG_SIZE * floatPixelSize,
            .rowsPerImage = IMG_SIZE,
            .offset = 0
        }
    };
    WGPUExtent3D copySize = { IMG_SIZE, IMG_SIZE, 1 };

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, NULL);
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &copySize);
    WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(enc, NULL);
    wgpuCommandEncoderRelease(enc);
    wgpuQueueSubmit(queue, 1, &cbuf);
    wgpuCommandBufferRelease(cbuf);
    wgpuQueueWaitIdle(queue);

    struct Float4 { float x, y, z, w; } *mapped = NULL;
    wgpuBufferMap(readback, WGPUMapMode_Read, 0, bufferSize, (void **)&mapped);

    struct RGBA8 { uint8_t r, g, b, a; } *img = calloc(pixelCount, sizeof(struct RGBA8));
    for (size_t i = 0; i < pixelCount; i++) {
        float r = mapped[i].x, g = mapped[i].y, b = mapped[i].z;
        if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
        if (g < 0.0f) g = 0.0f; if (g > 1.0f) g = 1.0f;
        if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;
        img[i].r = (uint8_t)(255.0f * r);
        img[i].g = (uint8_t)(255.0f * g);
        img[i].b = (uint8_t)(255.0f * b);
        img[i].a = 255;
    }
    stbi_write_png(filename, IMG_SIZE, IMG_SIZE, 4, img, IMG_SIZE * 4);
    free(img);
    wgpuBufferUnmap(readback);
}

static void runSampledImageDemo(WGPUDevice device, WGPUQueue queue) {
    WGPUShaderModule computeModule = compileGLSLModule(device, textureArraySource, WGPUShaderStage_Compute);

    WGPUBindGroupLayoutDescriptorBindless bindlessMarker = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_BindGroupLayoutDescriptorBindless
        }
    };

    // Non-bindless entries (the three outputs and the sampler) come first so they line
    // up positionally with the entries passed to wgpuDeviceCreateBindGroup below; the
    // bindless texture array (binding 3) is populated afterwards via wgpuBindGroupUpdateEntry.
    WGPUBindGroupLayoutEntry bglEntries[5] = {
        [0] = {
            .binding = 0,
            .visibility = WGPUShaderStage_Compute,
            .storageTexture = {
                .viewDimension = WGPUTextureViewDimension_2D,
                .access = WGPUStorageTextureAccess_WriteOnly,
                .format = WGPUTextureFormat_RGBA32Float
            }
        },
        [1] = {
            .binding = 1,
            .visibility = WGPUShaderStage_Compute,
            .storageTexture = {
                .viewDimension = WGPUTextureViewDimension_2D,
                .access = WGPUStorageTextureAccess_WriteOnly,
                .format = WGPUTextureFormat_RGBA32Float
            }
        },
        [2] = {
            .binding = 2,
            .visibility = WGPUShaderStage_Compute,
            .storageTexture = {
                .viewDimension = WGPUTextureViewDimension_2D,
                .access = WGPUStorageTextureAccess_WriteOnly,
                .format = WGPUTextureFormat_RGBA32Float
            }
        },
        [3] = {
            .binding = 4,
            .visibility = WGPUShaderStage_Compute,
            .sampler = {
                .type = WGPUSamplerBindingType_NonFiltering
            }
        },
        [4] = {
            .binding = 3,
            .visibility = WGPUShaderStage_Compute,
            .texture = {
                .sampleType = WGPUTextureSampleType_UnfilterableFloat,
                .viewDimension = WGPUTextureViewDimension_2D
            },
            .bindingArraySize = 2
        }
    };

    WGPUBindGroupLayoutDescriptor bglDesc = {
        .nextInChain = &bindlessMarker.chain,
        .entries = bglEntries,
        .entryCount = 5
    };

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayout plLayout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor){
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl
    });

    WGPUComputePipelineDescriptor cplDesc = {
        .label = STRVIEW("bindless texture array compute"),
        .layout = plLayout,
        .compute = {
            .module = computeModule,
            .entryPoint = STRVIEW("main")
        }
    };
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &cplDesc);

    WGPUTexture outputA = createOutputImage(device);
    WGPUTexture outputB = createOutputImage(device);
    WGPUTexture outputAB = createOutputImage(device);

    const WGPUTextureFormat outputFormat = WGPUTextureFormat_RGBA32Float;
    WGPUTextureViewDescriptor outputViewDesc = {
        .format = outputFormat,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_StorageBinding
    };
    WGPUTextureView outputAView = wgpuTextureCreateView(outputA, &outputViewDesc);
    WGPUTextureView outputBView = wgpuTextureCreateView(outputB, &outputViewDesc);
    WGPUTextureView outputABView = wgpuTextureCreateView(outputAB, &outputViewDesc);

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &(WGPUSamplerDescriptor){
        .addressModeU = WGPUAddressMode_ClampToEdge,
        .addressModeV = WGPUAddressMode_ClampToEdge,
        .addressModeW = WGPUAddressMode_ClampToEdge,
        .magFilter = WGPUFilterMode_Nearest,
        .minFilter = WGPUFilterMode_Nearest,
        .mipmapFilter = WGPUMipmapFilterMode_Nearest,
        .lodMinClamp = 0.0f,
        .lodMaxClamp = 0.0f,
        .maxAnisotropy = 1
    });

    float *patternA = malloc(IMG_SIZE * IMG_SIZE * 4 * sizeof(float));
    float *patternB = malloc(IMG_SIZE * IMG_SIZE * 4 * sizeof(float));
    fillPatternA(patternA);
    fillPatternB(patternB);

    WGPUTexture textureA, textureB;
    WGPUTextureView textureAView = createSampledTexture(device, queue, patternA, &textureA);
    WGPUTextureView textureBView = createSampledTexture(device, queue, patternB, &textureB);
    free(patternA);
    free(patternB);

    WGPUBindGroupEntry bgEntries[4] = {
        { .binding = 0, .textureView = outputAView },
        { .binding = 1, .textureView = outputBView },
        { .binding = 2, .textureView = outputABView },
        { .binding = 4, .sampler = sampler }
    };
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
        .layout = bgl,
        .entries = bgEntries,
        .entryCount = 4
    });

    wgpuBindGroupUpdateEntry(bindGroup, 3, 0, &(WGPUBindGroupEntry){ .binding = 3, .textureView = textureAView });
    wgpuBindGroupUpdateEntry(bindGroup, 3, 1, &(WGPUBindGroupEntry){ .binding = 3, .textureView = textureBView });

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, NULL);
    WGPUComputePassEncoder cpenc = wgpuCommandEncoderBeginComputePass(enc, NULL);
    wgpuComputePassEncoderSetPipeline(cpenc, pipeline);
    wgpuComputePassEncoderSetBindGroup(cpenc, 0, bindGroup, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(cpenc, IMG_SIZE / 8, IMG_SIZE / 8, 1);
    wgpuComputePassEncoderEnd(cpenc);
    wgpuComputePassEncoderRelease(cpenc);
    WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(enc, NULL);
    wgpuCommandEncoderRelease(enc);
    wgpuQueueSubmit(queue, 1, &cbuf);
    wgpuCommandBufferRelease(cbuf);
    wgpuQueueWaitIdle(queue);

    writeImageToPNG(device, queue, outputA, "bindless_a.png");
    writeImageToPNG(device, queue, outputB, "bindless_b.png");
    writeImageToPNG(device, queue, outputAB, "bindless_ab.png");
    printf("wrote bindless_a.png, bindless_b.png, bindless_ab.png\n");
}

int main(void) {
    wgpu_base base = wgpu_init();
    WGPUDevice device = base.device;
    WGPUQueue queue = base.queue;

    runBufferArrayDemo(device, queue);
    runSampledImageDemo(device, queue);

    return 0;
}
