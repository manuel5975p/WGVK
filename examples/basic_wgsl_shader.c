#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
#endif

#include "common.h"
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


// The WGSL shader source remains the same.
const char wgslSource[] = 
"override brightness = 0.0;\n"
"struct VertexInput {\n"
"    @location(0) position: vec2f,\n"
"    @location(1) uv: vec2f\n"
"};\n"
"\n"
"struct VertexOutput {\n"
"    @builtin(position) position: vec4f,\n"
"    @location(0) uv: vec2f\n"
"};\n"
"\n"
"@vertex\n"
"fn vs_main(in: VertexInput) -> VertexOutput {\n"
"    var out: VertexOutput;\n"
"    out.position = vec4f(in.position.x, in.position.y, 0.0f, 1.0f);\n"
"    out.uv = in.uv;\n"
"    return out;\n"
"}\n"
"\n"
"@group(0) @binding(0) var colDiffuse: texture_2d<f32>;\n"
"@group(0) @binding(1) var grsampler: sampler;\n"
"\n"
"@fragment\n"
"fn fs_main(in: VertexOutput) -> @location(0) vec4f {\n"
"    return textureSample(colDiffuse, grsampler, in.uv) * brightness;\n"
"}\n";

typedef struct {
    wgpu_base base;
    WGPURenderPipeline pipeline;
    WGPUBindGroup bind_group;
    WGPUBuffer index_buffer;
    float t;
} Context;

uint64_t stamp;
uint64_t frameCount = 0;
uint64_t totalFrameCount = 0;
int resized = 0;

void main_loop(void* user_data) {
    Context* ctx = (Context*)user_data;
    WGPUDevice device = ctx->base.device;
    WGPUQueue queue = ctx->base.queue;

    ctx->t += 0.01f;
    glfwPollEvents();
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(ctx->base.surface, &surfaceTexture);

    // The surface must be valid to draw.
    
    //printf("Size: %d x %d\n", width, height);
    
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal || resized) {
        resized = 0;
        int width, height;
        glfwGetWindowSize(ctx->base.window, &width, &height);
        
        WGPUSurfaceCapabilities caps = {0};
        WGPUPresentMode desiredPresentMode = WGPUPresentMode_Immediate;

        WGPUSurfaceConfiguration surfaceConfig = {
            .device = device,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .usage = WGPUTextureUsage_RenderAttachment,
            .width =  (uint32_t)width,
            .height = (uint32_t)height,
            .alphaMode = WGPUCompositeAlphaMode_Opaque,
            .presentMode = desiredPresentMode,
        };
        
        wgpuSurfaceConfigure(ctx->base.surface, &surfaceConfig);
        return;
    }

    WGPUTextureView surfaceView = wgpuTextureCreateView(surfaceTexture.texture, &(const WGPUTextureViewDescriptor){
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .usage = WGPUTextureUsage_RenderAttachment,
        .aspect = WGPUTextureAspect_All,
    });

    WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(device, NULL);

    WGPURenderPassColorAttachment colorAttachment = {
        .view = surfaceView,
        .resolveTarget = NULL,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {0.5, 0.2, 0, 0.5},
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    WGPURenderPassEncoder rpenc = wgpuCommandEncoderBeginRenderPass(cenc, &(const WGPURenderPassDescriptor){
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    });

    const float scale = 0.5f;
    float adhocVertices[16] = {
        scale * 1.5f * cosf(ctx->t             ), sin(ctx->t) * 0.4f + scale * 1.5f * sinf(ctx->t             ), 0.0f, 0.0f,
        scale * 1.5f * cosf(ctx->t + M_PI_2    ), sin(ctx->t) * 0.4f + scale * 1.5f * sinf(ctx->t + M_PI_2    ), 0.0f, 1.0f,
        scale * 1.5f * cosf(ctx->t + 2 * M_PI_2), sin(ctx->t) * 0.4f + scale * 1.5f * sinf(ctx->t + 2 * M_PI_2), 1.0f, 1.0f,
        scale * 1.5f * cosf(ctx->t + 3 * M_PI_2), sin(ctx->t) * 0.4f + scale * 1.5f * sinf(ctx->t + 3 * M_PI_2), 1.0f, 0.0f,
    };
    WGPUBufferDescriptor adhocBufferDesc = {
        .size = sizeof(adhocVertices),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    };
    static WGPUBuffer adhocBuffer;
    if(totalFrameCount == 0){
        adhocBuffer = wgpuDeviceCreateBuffer(device, &adhocBufferDesc);
    }
    wgpuQueueWriteBuffer(queue, adhocBuffer, 0, adhocVertices, sizeof(adhocVertices));

    // Record render commands.
    wgpuRenderPassEncoderSetPipeline(rpenc, ctx->pipeline);
    wgpuRenderPassEncoderSetBindGroup(rpenc, 0, ctx->bind_group, 0, NULL);
    wgpuRenderPassEncoderSetIndexBuffer(rpenc, ctx->index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(rpenc, 0, adhocBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(rpenc, 6, 1, 0, 0, 0);

    wgpuRenderPassEncoderEnd(rpenc);
    WGPUCommandBuffer cbuffer = wgpuCommandEncoderFinish(cenc, NULL);

    // Submit and present.
    wgpuQueueSubmit(queue, 1, &cbuffer);
    #ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(ctx->base.surface);
    #endif
    // Release per-frame resources.
    // wgpuBufferRelease(adhocBuffer);
    wgpuRenderPassEncoderRelease(rpenc);
    wgpuTextureViewRelease(surfaceView);
    wgpuCommandBufferRelease(cbuffer);
    wgpuCommandEncoderRelease(cenc);
    ++frameCount;
    ++totalFrameCount;
    uint64_t nextStamp = nanoTime();
    if(nextStamp - stamp > ((uint64_t)1000000000ULL)){
        stamp = nextStamp;
        printf("FPS: %llu\n", (unsigned long long)frameCount);
        frameCount = 0;
    }
}
void resizeCallback(GLFWwindow* window, int width, int height){
    resized = 1;
}

int main() {
    Context* ctx = (Context*)calloc(1, sizeof(Context));
    stamp = nanoTime();
    ctx->base = wgpu_init();
    glfwSetWindowSizeCallback(ctx->base.window, resizeCallback);
    if (!ctx->base.device) {
        free(ctx);
        return 1;
    }
    WGPUDevice device = ctx->base.device;
    WGPUQueue queue = ctx->base.queue;

    WGPUShaderSourceWGSL shaderCodeDesc = {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = {
            .data = wgslSource,
            .length = WGPU_STRLEN
        }
    };
    WGPUShaderModuleDescriptor shaderDesc = { .nextInChain = &shaderCodeDesc.chain };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Vertex Layout
    WGPUVertexAttribute vertexAttributes[2] = {
        {
            .shaderLocation = 0,
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
        },
        {
            .shaderLocation = 1,
            .format = WGPUVertexFormat_Float32x2,
            .offset = 2 * sizeof(float),
            
        }
    };
    WGPUVertexBufferLayout vbLayout = {
        .arrayStride = sizeof(float) * 4,
        .attributeCount = 2,
        .attributes = vertexAttributes,
        .stepMode = WGPUVertexStepMode_Vertex
    };

    WGPUColorTargetState colorTargetState = {
        .format = WGPUTextureFormat_BGRA8Unorm,
        .writeMask = WGPUColorWriteMask_All,
    };
    WGPUConstantEntry brightnessConstant = {
        .nextInChain = NULL,
        .key = STRVIEW("brightness"),
        .value = 1.0
    };

    WGPUFragmentState fragmentState = {
        .entryPoint = {
            .data = "fs_main",
            .length = sizeof("fs_main") - 1
        },
        .module = shaderModule,
        .constantCount = 1,
        .constants = &brightnessConstant,
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    WGPUBindGroupLayoutEntry layoutEntries[2] = {
        {.binding = 0,
            .visibility = WGPUShaderStage_Fragment,
            .texture.sampleType = WGPUTextureSampleType_Float,
            .texture.viewDimension = WGPUTextureViewDimension_2D
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .sampler.type = WGPUSamplerBindingType_Filtering
        }
    };
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = { .entries = layoutEntries, .entryCount = 2 };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    WGPUPipelineLayoutDescriptor pldesc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindGroupLayout
    };
    WGPUPipelineLayout pllayout = wgpuDeviceCreatePipelineLayout(device, &pldesc);

    // Render Pipeline
    WGPURenderPipelineDescriptor rpdesc = {
        .vertex = { 
            .module = shaderModule, 
            .entryPoint = {
                .data = "vs_main",
                .length = sizeof("vs_main") - 1
            },
            .bufferCount = 1,
            .buffers = &vbLayout,
        },
        .fragment = &fragmentState,
        .primitive = { 
            .topology = WGPUPrimitiveTopology_TriangleList,
            .cullMode = WGPUCullMode_Back,
            .frontFace = WGPUFrontFace_CCW
        },
        .layout = pllayout,
        .multisample = {
            .count = 1,
            .mask = 0xffffffff
        },
    };
    ctx->pipeline = wgpuDeviceCreateRenderPipeline(device, &rpdesc);

    WGPUTextureDescriptor tdesc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {10, 10, 1},
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1
    };
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &tdesc);


    WGPUTextureViewDescriptor viewDesc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_TextureBinding
    };
    WGPUTextureView textureView = wgpuTextureCreateView(texture, &viewDesc);
    uint8_t* textureData = calloc(tdesc.size.width * tdesc.size.height, 4);
    for (size_t i = 0; i < tdesc.size.width * tdesc.size.height * 4; i++) {
        textureData[i] = (i) & 255;
    }
    wgpuQueueWriteTexture(
        queue,
        &(WGPUTexelCopyTextureInfo){
            .texture = texture,
            .aspect = WGPUTextureAspect_All,
        },
        textureData, tdesc.size.width * tdesc.size.height * 4,
        &(WGPUTexelCopyBufferLayout){.bytesPerRow = tdesc.size.width * 4, .rowsPerImage = tdesc.size.height},
        &(WGPUExtent3D){tdesc.size.width, tdesc.size.height, 1}
    );
    free(textureData);

    WGPUSamplerDescriptor samplerDesc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Linear,
        .minFilter = WGPUFilterMode_Nearest,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0,
        .lodMaxClamp = 1,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 1
    };
    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);

    WGPUBindGroupEntry bindGroupEntries[2] = {
        {.binding = 0, .textureView = textureView},
        {.binding = 1, .sampler = sampler}
    };
    WGPUBindGroupDescriptor bindGroupDesc = { .layout = bindGroupLayout, .entryCount = 2, .entries = bindGroupEntries };
    ctx->bind_group = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
    
    const uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };
    WGPUBufferDescriptor iboDesc = { .size = sizeof(indices), .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst };
    ctx->index_buffer = wgpuDeviceCreateBuffer(device, &iboDesc);
    wgpuQueueWriteBuffer(queue, ctx->index_buffer, 0, indices, sizeof(indices));
    
    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pllayout);
    wgpuTextureRelease(texture);
    wgpuTextureViewRelease(textureView);
    wgpuSamplerRelease(sampler);

    ctx->t = 0.0f;
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, ctx, 0, 1);
    #else
    while(!glfwWindowShouldClose(ctx->base.window)){
        main_loop(ctx);
    }
    #endif
    return 0;
}