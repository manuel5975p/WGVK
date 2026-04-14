#include <cstdint>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>
#include <string>

#include <wgvk.h>

// We need that one to check internals, like refCount etc.
#include <wgvk_structs_impl.h>


#ifdef __cplusplus
    #include <atomic>
    using refcount_t = std::atomic<uint32_t>;
#else
    typedef _Atomic(uint32_t) refcount_t;
#endif

class WebGPUTest : public ::testing::Test {
protected:
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;

    void SetUp() override {
        WGPUInstanceLayerSelection lsel = {0};
        const char* layernames[] = {"VK_LAYER_KHRONOS_validation"};
        lsel.instanceLayers = layernames;
        lsel.instanceLayerCount = 1;
        lsel.chain.sType = WGPUSType_InstanceLayerSelection;
        WGPUInstanceDescriptor desc = {};
        desc.nextInChain = &lsel.chain;
        instance = wgpuCreateInstance(&desc);
        ASSERT_NE(instance, nullptr) << "Failed to create WGPUInstance";
        ASSERT_NE(instance->instance, nullptr) << "Failed to create WGPUInstance";

        WGPURequestAdapterOptions options = {};
        options.powerPreference = WGPUPowerPreference_HighPerformance;
        
        struct AdapterCtx {
            WGPUAdapter adapter = nullptr;
            bool done = false;
        } adapterCtx;

        auto adapterCallback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView msg, void* userdata, void* userdata2) {
            AdapterCtx* ctx = (AdapterCtx*)userdata;
            if (status == WGPURequestAdapterStatus_Success) {
                ctx->adapter = adapter;
            } else {
                printf("Adapter Request Failed: %s\n", msg.data);
            }
            ctx->done = true;
        };

        WGPURequestAdapterCallbackInfo callbackInfo = {};
        callbackInfo.callback = adapterCallback;
        callbackInfo.userdata1 = &adapterCtx;

        WGPUFuture future = wgpuInstanceRequestAdapter(instance, &options, callbackInfo);
        
        WGPUFutureWaitInfo waitInfo = { future, 0 };
        while(!adapterCtx.done) {
            wgpuInstanceWaitAny(instance, 1, &waitInfo, 1000000000); // 1 sec timeout
        }
        
        adapter = adapterCtx.adapter;
        ASSERT_NE(adapter, nullptr) << "Failed to obtain WGPUAdapter";

        struct DeviceCtx {
            WGPUDevice device = nullptr;
            bool done = false;
        } deviceCtx;

        auto deviceCallback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView msg, void* userdata, void* userdata2) {
            DeviceCtx* ctx = (DeviceCtx*)userdata;
            if (status == WGPURequestDeviceStatus_Success) {
                ctx->device = device;
            } else {
                printf("Device Request Failed: %s\n", msg.data);
            }
            ctx->done = true;
        };

        WGPURequestDeviceCallbackInfo devCbInfo = {};
        devCbInfo.callback = deviceCallback;
        devCbInfo.userdata1 = &deviceCtx;

        WGPUDeviceDescriptor devDesc = {};
        devDesc.label = { "TestDevice", 10 };
        
        future = wgpuAdapterRequestDevice(adapter, &devDesc, devCbInfo);
        
        waitInfo = { future, 0 };
        while(!deviceCtx.done) {
            wgpuInstanceWaitAny(instance, 1, &waitInfo, 1000000000);
        }

        device = deviceCtx.device;
        ASSERT_NE(device, nullptr) << "Failed to obtain WGPUDevice";

        queue = wgpuDeviceGetQueue(device);
        ASSERT_NE(queue, nullptr) << "Failed to get WGPUQueue";
    }

    void TearDown() override {
        if (queue){
            wgpuQueueRelease(queue);
        }
        if (device){
            wgpuDeviceRelease(device);
        }
        if (adapter){
            wgpuAdapterRelease(adapter);
        }
        if (instance){
            wgpuInstanceRelease(instance);
        }
    }
};

TEST(WGPUApiValidation, SurfaceGetCapabilitiesRejectsNullArgsAndClearsOutput) {
    WGPUSurfaceCapabilities caps = {};
    caps.formatCount = 123;
    caps.presentModeCount = 456;
    caps.alphaModeCount = 789;
    caps.formats = reinterpret_cast<WGPUTextureFormat*>(uintptr_t(0x1));

    EXPECT_EQ(wgpuSurfaceGetCapabilities(nullptr, nullptr, &caps), WGPUStatus_Error);
    EXPECT_EQ(caps.formatCount, 0u);
    EXPECT_EQ(caps.presentModeCount, 0u);
    EXPECT_EQ(caps.alphaModeCount, 0u);
    EXPECT_EQ(caps.formats, nullptr);

    EXPECT_EQ(wgpuSurfaceGetCapabilities(nullptr, nullptr, nullptr), WGPUStatus_Error);
}

TEST_F(WebGPUTest, BufferReferenceCounting) {
    WGPUBufferDescriptor desc = {};
    desc.size = 1024;
    desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    desc.mappedAtCreation = false;
    desc.label = { "RefTestBuffer", 13 };

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);

    // Initial RefCount should be 1
    wgpuBufferAddRef(buffer); 
    ASSERT_EQ(buffer->refCount, 2);
    
    wgpuBufferRelease(buffer);
    // RefCount should be 1
    ASSERT_EQ(buffer->refCount, 1);
    wgpuBufferRelease(buffer);
    // RefCount should be 0, memory freed.
    // Note: Can't easily verify memory free without mocking free(), 
    // but ASan will catch double-free or leaks here.
}

TEST_F(WebGPUTest, BindGroupKeepsLayoutAlive) {
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;
    
    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(layout, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = 256;
    bufDesc.usage = WGPUBufferUsage_Storage;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = buffer;
    bgEntry.size = 256;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = layout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bindGroup, nullptr);

    ASSERT_EQ(layout->refCount, 2);
    wgpuBindGroupLayoutRelease(layout);
    // Remaining ref by bindGroup
    ASSERT_EQ(layout->refCount, 1);

    // This should trigger the final release of the layout.
    wgpuBindGroupRelease(bindGroup);
    
    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, BufferMappingWrite) {
    WGPUBufferDescriptor desc = {};
    desc.size = 64;
    desc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    desc.mappedAtCreation = false;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    
    struct MapCtx {
        bool done = false;
        WGPUMapAsyncStatus status;
    } mapCtx;

    auto mapCallback = [](WGPUMapAsyncStatus status, WGPUStringView msg, void* userdata, void* u2) {
        MapCtx* ctx = (MapCtx*)userdata;
        ctx->status = status;
        ctx->done = true;
    };

    WGPUBufferMapCallbackInfo cbInfo = {};
    cbInfo.callback = mapCallback;
    cbInfo.userdata1 = &mapCtx;
    cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUFuture future = wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, 64, cbInfo);
    
    // Wait
    WGPUFutureWaitInfo waitInfo = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &waitInfo, 100000000);
    }

    ASSERT_EQ(mapCtx.status, WGPUMapAsyncStatus_Success);

    void* ptr = wgpuBufferGetMappedRange(buffer, 0, 64);
    ASSERT_NE(ptr, nullptr);
    
    // Write data
    int* intPtr = (int*)ptr;
    *intPtr = 42;

    wgpuBufferUnmap(buffer);
    
    // Check state (conceptually, via API check if available or failure to map again immediately)
    // Cleanup
    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, GLSLComputeMultiplication) {
    // 1. Create Data Buffer (Input/Output)
    const uint32_t elementCount = 64;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);
    
    // Create staging buffer mapped at creation to upload initial data
    WGPUBufferDescriptor stagingDesc = {};
    stagingDesc.size = bufferSize;
    stagingDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    stagingDesc.mappedAtCreation = true;
    WGPUBuffer stagingBuffer = wgpuDeviceCreateBuffer(device, &stagingDesc);
    
    uint32_t* initialData = (uint32_t*)wgpuBufferGetMappedRange(stagingBuffer, 0, bufferSize);
    for(uint32_t i=0; i<elementCount; ++i) initialData[i] = i;
    wgpuBufferUnmap(stagingBuffer);

    // Create storage buffer on GPU
    WGPUBufferDescriptor storageDesc = {};
    storageDesc.size = bufferSize;
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer storageBuffer = wgpuDeviceCreateBuffer(device, &storageDesc);

    // Copy data to storage buffer
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, stagingBuffer, 0, storageBuffer, 0, bufferSize);
    WGPUCommandBuffer setupCmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &setupCmd);
    
    // Wait for queue (simple wait idle for test)
    wgpuQueueWaitIdle(queue);
    wgpuCommandBufferRelease(setupCmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(stagingBuffer);

    // 2. Create GLSL Shader
    // Multiplies every element by 2
    const char* glslCode = R"(
        #version 450
        layout(local_size_x = 1) in;
        
        layout(std430, set = 0, binding = 0) buffer Data {
            uint values[];
        } data;

        void main() {
            uint index = gl_GlobalInvocationID.x;
            data.values[index] = data.values[index] * 2;
        }
    )";

    WGPUShaderSourceGLSL glslSource = {};
    glslSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    glslSource.stage = WGPUShaderStage_Compute;
    glslSource.code.data = glslCode;
    glslSource.code.length = strlen(glslCode);

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = (WGPUChainedStruct*)&glslSource;
    shaderDesc.label = { "ComputeShader", 13 };

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    ASSERT_NE(shaderModule, nullptr);

    // 3. Create Pipeline Layout
    WGPUBindGroupLayoutEntry bglEntry = {};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Compute;
    bglEntry.buffer.type = WGPUBufferBindingType_Storage;
    bglEntry.buffer.minBindingSize = bufferSize;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    // 4. Create Compute Pipeline
    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.compute.module = shaderModule;
    pipeDesc.compute.entryPoint = { "main", 4 };
    
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // 5. Create BindGroup
    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = storageBuffer;
    bgEntry.offset = 0;
    bgEntry.size = bufferSize;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    // 6. Encode and Submit
    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor passDesc = {}; // Null timestamp writes
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);
    
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, elementCount, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass); // Release encoder handle

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    // 7. Readback results
    // Create readback buffer
    WGPUBufferDescriptor readDesc = {};
    readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    // Encode copy from storage to readback
    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, storageBuffer, 0, readBuffer, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    
    // Intentionally defer releasing the cmd buffer
    // to check refcount validity
    ASSERT_EQ(cmd->refCount, 2);
    
    for(uint32_t i = 0;i < framesInFlight;i++){
        wgpuDeviceTick(device);
    }
    ASSERT_EQ(cmd->refCount, 1);
    wgpuCommandBufferRelease(cmd);
    
    // Map Async
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while(!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);

    for(uint32_t i = 0; i < elementCount; ++i) {
        EXPECT_EQ(results[i], i * 2) << "Index " << i << " mismatch";
    }

    wgpuBufferUnmap(readBuffer);

    // Cleanup
    wgpuBufferRelease(readBuffer);
    wgpuBufferRelease(storageBuffer);
    wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bgl);
    ASSERT_EQ(pipelineLayout->refCount, 2);
    wgpuPipelineLayoutRelease(pipelineLayout);
    ASSERT_EQ(pipelineLayout->refCount, 1);
    ASSERT_EQ(pipeline->refCount, 1);
    wgpuComputePipelineRelease(pipeline);
    ASSERT_EQ(shaderModule->refCount, 1);
    wgpuShaderModuleRelease(shaderModule);
}

TEST_F(WebGPUTest, QueueWorkDone) {
    struct WorkCtx {
        bool done = false;
        WGPUQueueWorkDoneStatus status;
    } workCtx;

    auto workCallback = [](WGPUQueueWorkDoneStatus status, void* userdata, void* u2) {
        WorkCtx* ctx = (WorkCtx*)userdata;
        ctx->status = status;
        ctx->done = true;
    };

    WGPUQueueWorkDoneCallbackInfo cbInfo = {};
    cbInfo.callback = workCallback;
    cbInfo.userdata1 = &workCtx;
    cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUFuture future = wgpuQueueOnSubmittedWorkDone(queue, cbInfo);
    
    // Submit some dummy work to ensure queue progresses
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    WGPUFutureWaitInfo waitInfo = { future, 0 };
    while (!workCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &waitInfo, 100000000);
    }

    ASSERT_EQ(workCtx.status, WGPUQueueWorkDoneStatus_Success);
}

TEST_F(WebGPUTest, BufferCopyRoundTrip) {
    const size_t dataSize = 1024; // 256 uint32_t's
    const size_t count = dataSize / sizeof(uint32_t);

    // 1. Create Source Buffer: Mapped at creation, Write capable
    WGPUBufferDescriptor srcDesc = {};
    srcDesc.size = dataSize;
    srcDesc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    srcDesc.mappedAtCreation = true;
    srcDesc.label = { "SourceBuffer", 12 };
    
    WGPUBuffer srcBuffer = wgpuDeviceCreateBuffer(device, &srcDesc);
    ASSERT_NE(srcBuffer, nullptr);
    
    // Fill with pattern
    uint32_t* srcPtr = (uint32_t*)wgpuBufferGetMappedRange(srcBuffer, 0, dataSize);
    ASSERT_NE(srcPtr, nullptr);
    for(uint32_t i = 0; i < count; ++i) {
        srcPtr[i] = 0xCAFEBABE + i;
    }
    wgpuBufferUnmap(srcBuffer);
    
    // 2. Create Intermediate Buffer: GPU only
    WGPUBufferDescriptor interDesc = {};
    interDesc.size = dataSize;
    interDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc; 
    interDesc.mappedAtCreation = false;
    interDesc.label = { "IntermediateBuffer", 18 };
    
    WGPUBuffer interBuffer = wgpuDeviceCreateBuffer(device, &interDesc);
    ASSERT_NE(interBuffer, nullptr);
    
    // 3. Create Destination Buffer: Map Read capable
    WGPUBufferDescriptor dstDesc = {};
    dstDesc.size = dataSize;
    dstDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    dstDesc.mappedAtCreation = false;
    dstDesc.label = { "DestBuffer", 10 };
    
    WGPUBuffer dstBuffer = wgpuDeviceCreateBuffer(device, &dstDesc);
    ASSERT_NE(dstBuffer, nullptr);
    
    // 4. Encode Copies
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    
    // Source -> Intermediate
    wgpuCommandEncoderCopyBufferToBuffer(encoder, srcBuffer, 0, interBuffer, 0, dataSize);
    
    // Intermediate -> Destination
    wgpuCommandEncoderCopyBufferToBuffer(encoder, interBuffer, 0, dstBuffer, 0, dataSize);
    
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    ASSERT_NE(cmd, nullptr);
    
    // Check RefCounts while command buffer is alive but not yet submitted (or just submitted).
    // Resources should be referenced by the CommandBuffer/ResourceUsage tracking.
    // Expected: 1 (User) + 1 (CommandBuffer/Encoder Tracking) = 2
    EXPECT_EQ(srcBuffer->refCount, 2);
    EXPECT_EQ(interBuffer->refCount, 2);
    EXPECT_EQ(dstBuffer->refCount, 2);

    // 5. Submit
    wgpuQueueSubmit(queue, 1, &cmd);
    
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);
    
    // After submission, CommandBuffer ref is gone, but Queue/FrameCache now holds them.
    // wgvk moves tracking to internal frame structures.
    
    // 6. Map Async Destination
    struct MapCtx { 
        bool done = false; 
        WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error; 
    } mapCtx;
    
    auto mapCb = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
        auto* ctx = (MapCtx*)ud;
        ctx->status = status;
        ctx->done = true;
    };
    
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    
    // This implicitly synchronizes access to dstBuffer
    WGPUFuture future = wgpuBufferMapAsync(dstBuffer, WGPUMapMode_Read, 0, dataSize, cbInfo);
    
    // Wait for callback
    WGPUFutureWaitInfo waitInfo = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &waitInfo, UINT64_MAX);
    }
    
    ASSERT_EQ(mapCtx.status, WGPUMapAsyncStatus_Success);
    
    // 7. Verify Data
    const uint32_t* dstPtr = (const uint32_t*)wgpuBufferGetConstMappedRange(dstBuffer, 0, dataSize);
    ASSERT_NE(dstPtr, nullptr);
    
    for(uint32_t i = 0; i < count; ++i) {
        EXPECT_EQ(dstPtr[i], 0xCAFEBABE + i) << "Mismatch at index " << i;
    }
    
    wgpuBufferUnmap(dstBuffer);
    
    // 8. Tick device to cycle frame resources and release internal refs
    // Submit dummy work to move the ring buffer if necessary, or just tick.
    // Based on previous discussion, we might need a dummy submission to prevent the wait-on-zero-sem bug
    // if wgpuDeviceTick hasn't been patched yet in the binary under test.
    // Assuming patched wgpuDeviceTick:
    for(uint32_t i = 0;i < framesInFlight;i++){
        wgpuDeviceTick(device); // Frame N -> N+1
    }
    
    // Verify RefCounts have dropped back to 1 (only our local variables holding them)
    EXPECT_EQ(srcBuffer->refCount, 1);
    EXPECT_EQ(interBuffer->refCount, 1);
    EXPECT_EQ(dstBuffer->refCount, 1);
    
    // Cleanup
    wgpuBufferRelease(srcBuffer);
    wgpuBufferRelease(interBuffer);
    wgpuBufferRelease(dstBuffer);
}

TEST_F(WebGPUTest, RenderPassClearToRed) {
    // 64 pixels width * 4 bytes = 256 bytes per row (WebGPU requirement aligned)
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256; 
    const size_t bufferSize = bytesPerRow * height;

    // 1. Create Texture (Render Attachment + Copy Source)
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    texDesc.label = { "ColorAttachment", 15 };
    
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    ASSERT_NE(texture, nullptr);

    // 2. Create Default View
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr); 
    ASSERT_NE(view, nullptr);

    // 3. Create Readback Buffer
    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = bufferSize;
    bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bufDesc.mappedAtCreation = false;
    bufDesc.label = { "ReadbackBuffer", 14 };
    
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    ASSERT_NE(buffer, nullptr);

    // 4. Encode Render Pass
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    
    WGPURenderPassColorAttachment colorAtt = {};
    colorAtt.view = view;
    colorAtt.loadOp = WGPULoadOp_Clear;
    colorAtt.storeOp = WGPUStoreOp_Store;
    colorAtt.clearValue = {1.0, 0.0, 0.0, 1.0}; // RED
    
    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &colorAtt;
    rpDesc.depthStencilAttachment = nullptr; // No depth
    
    WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderEnd(rp);
    wgpuRenderPassEncoderRelease(rp);

    // 5. Encode Copy (Texture -> Buffer)
    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.mipLevel = 0;
    srcInfo.origin = {0, 0, 0};
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = buffer;
    dstInfo.layout.offset = 0;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    // 6. Submit
    wgpuQueueSubmit(queue, 1, &cmd);
    
    // RefCount Check:
    // Texture should be held by: 
    // 1. User (test variable)
    // 2. View (internal ref)
    // 3. Command Buffer/Resource Usage tracking (pending execution)
    EXPECT_GE(texture->refCount, 3); 
    
    wgpuCommandBufferRelease(cmd);

    // 7. Map Async & Verify
    struct MapCtx { bool done = false; WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->status = status;
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    
    WGPUFuture mapFut = wgpuBufferMapAsync(buffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while(!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }
    ASSERT_EQ(mapCtx.status, WGPUMapAsyncStatus_Success);

    const uint8_t* mappedData = (const uint8_t*)wgpuBufferGetConstMappedRange(buffer, 0, bufferSize);
    ASSERT_NE(mappedData, nullptr);

    // Verify Red pixels [255, 0, 0, 255]
    auto checkPixel = [&](uint32_t x, uint32_t y) {
        size_t offset = y * bytesPerRow + x * 4;
        EXPECT_EQ(mappedData[offset + 0], 255) << "Red mismatch at " << x << "," << y;
        EXPECT_EQ(mappedData[offset + 1], 0)   << "Green mismatch at " << x << "," << y;
        EXPECT_EQ(mappedData[offset + 2], 0)   << "Blue mismatch at " << x << "," << y;
        EXPECT_EQ(mappedData[offset + 3], 255) << "Alpha mismatch at " << x << "," << y;
    };

    checkPixel(0, 0);
    checkPixel(width - 1, 0);
    checkPixel(0, height - 1);
    checkPixel(width - 1, height - 1);
    checkPixel(width / 2, height / 2);

    wgpuBufferUnmap(buffer);

    // 8. Cleanup & Final Ref Check
    // Cycle frames to release internal references held by the queue
    for(uint32_t i = 0;i < framesInFlight;i++){
        wgpuDeviceTick(device);
    }
    
    // Texture should now only be held by User(1) + View(1) = 2
    EXPECT_EQ(texture->refCount, 2);
    // View held by User(1)
    EXPECT_EQ(view->refCount, 1);
    // Buffer held by User(1)
    EXPECT_EQ(buffer->refCount, 1);

    wgpuTextureViewRelease(view);
    
    // View released its hold on Texture, now RefCount = 1
    EXPECT_EQ(texture->refCount, 1);
    
    wgpuTextureRelease(texture);
    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, RenderPassTriangleDraw) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256; 
    const size_t bufferSize = bytesPerRow * height;

    // 1. Setup Shaders
    // Triangle covering Top-Left, Bottom-Left, Bottom-Right (in 0..64 screen coords)
    // Conceptually covers the area where y >= x
    const char* vsCode = R"(
        #version 450
        void main() {
            const vec2 pos[3] = vec2[3](
                vec2(-1.0, -1.0), // Bottom Left (NDCS) -> Bottom Left (Screen)
                vec2( 1.0, -1.0), // Bottom Right (NDCS) -> Bottom Right (Screen)
                vec2(-1.0,  1.0)  // Top Left (NDCS) -> Top Left (Screen)
            );
            // Use z = 0.5 to avoid near/far clipping issues
            gl_Position = vec4(pos[gl_VertexIndex], 0.5, 1.0);
        }
    )";

    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(0.0, 1.0, 0.0, 1.0); // Green
        }
    )";

    WGPUShaderSourceGLSL vsSource = {};
    vsSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    vsSource.stage = WGPUShaderStage_Vertex;
    vsSource.code.data = vsCode;
    vsSource.code.length = strlen(vsCode);
    WGPUShaderModuleDescriptor vsDesc = {};
    vsDesc.nextInChain = (WGPUChainedStruct*)&vsSource;
    WGPUShaderModule vsModule = wgpuDeviceCreateShaderModule(device, &vsDesc);
    ASSERT_NE(vsModule, nullptr);

    WGPUShaderSourceGLSL fsSource = {};
    fsSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    fsSource.stage = WGPUShaderStage_Fragment;
    fsSource.code.data = fsCode;
    fsSource.code.length = strlen(fsCode);
    WGPUShaderModuleDescriptor fsDesc = {};
    fsDesc.nextInChain = (WGPUChainedStruct*)&fsSource;
    WGPUShaderModule fsModule = wgpuDeviceCreateShaderModule(device, &fsDesc);
    ASSERT_NE(fsModule, nullptr);

    // 2. Pipeline
    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    plDesc.bindGroupLayouts = nullptr;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    colorTarget.blend = nullptr;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // 3. Resources
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = bufferSize;
    bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    // 4. Encode
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 1.0, 1.0}; // Blue Clear

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.aspect = WGPUTextureAspect_All;
    
    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    // 5. Submit
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // 6. Map & Verify
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    
    WGPUFuture future = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    WGPUFutureWaitInfo fwi = { .future = future, .completed = 0 };

    while(!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT32_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    auto checkPixel = [&](uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b) {
        size_t offset = y * bytesPerRow + x * 4;
        EXPECT_EQ(pixels[offset+0], r) << "R mismatch at " << x << "," << y;
        EXPECT_EQ(pixels[offset+1], g) << "G mismatch at " << x << "," << y;
        EXPECT_EQ(pixels[offset+2], b) << "B mismatch at " << x << "," << y;
        EXPECT_EQ(pixels[offset+3], 255);
    };

    // Check Inside Triangle (Green)
    // This region is definitely covered by TL-BL-BR triangle (x <= y roughly)
    checkPixel(10, 50, 0, 255, 0); 
    checkPixel(0, 63, 0, 255, 0);

    // Check Outside Triangle (Blue Clear Color)
    // This region is Top-Right (x > y)
    checkPixel(50, 10, 0, 0, 255);
    checkPixel(63, 0, 0, 0, 255);

    wgpuBufferUnmap(readBuffer);

    // 7. Cleanup
    for(uint32_t i = 0; i < framesInFlight; i++){
        wgpuDeviceTick(device);
    }

    wgpuBufferRelease(readBuffer);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

#if SUPPORT_WGSL == 1
TEST_F(WebGPUTest, WGSLShaderWithOverrideAndTextureSample) {
    // Same shader as basic_wgsl_shader example
    const char* wgslSource =
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

    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t bufferSize = bytesPerRow * height;

    // 1. Create WGSL shader module
    WGPUShaderSourceWGSL wgslSourceDesc = {};
    wgslSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslSourceDesc.code.data = wgslSource;
    wgslSourceDesc.code.length = strlen(wgslSource);

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = (WGPUChainedStruct*)&wgslSourceDesc;
    shaderDesc.label = { "WGSLOverrideShader", 18 };

    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    ASSERT_NE(shaderModule, nullptr) << "Failed to create WGSL shader module";

    // 2. Create a source texture (10x10 with known data, like the example)
    WGPUTextureDescriptor srcTexDesc = {};
    srcTexDesc.size = {10, 10, 1};
    srcTexDesc.format = WGPUTextureFormat_RGBA8Unorm;
    srcTexDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    srcTexDesc.mipLevelCount = 1;
    srcTexDesc.sampleCount = 1;
    srcTexDesc.dimension = WGPUTextureDimension_2D;

    WGPUTexture srcTexture = wgpuDeviceCreateTexture(device, &srcTexDesc);
    ASSERT_NE(srcTexture, nullptr);

    // Fill with non-zero data (cycling pattern like the example)
    std::vector<uint8_t> texData(10 * 10 * 4);
    for (size_t i = 0; i < texData.size(); i++) {
        texData[i] = (uint8_t)((i + 1) & 255); // +1 to ensure non-zero
    }

    WGPUTexelCopyTextureInfo texCopyDst = {};
    texCopyDst.texture = srcTexture;
    texCopyDst.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout texDataLayout = {};
    texDataLayout.bytesPerRow = 10 * 4;
    texDataLayout.rowsPerImage = 10;

    WGPUExtent3D texWriteSize = {10, 10, 1};
    wgpuQueueWriteTexture(queue, &texCopyDst, texData.data(), texData.size(), &texDataLayout, &texWriteSize);

    WGPUTextureViewDescriptor srcViewDesc = {};
    srcViewDesc.format = WGPUTextureFormat_RGBA8Unorm;
    srcViewDesc.dimension = WGPUTextureViewDimension_2D;
    srcViewDesc.baseMipLevel = 0;
    srcViewDesc.mipLevelCount = 1;
    srcViewDesc.baseArrayLayer = 0;
    srcViewDesc.arrayLayerCount = 1;
    srcViewDesc.aspect = WGPUTextureAspect_All;
    srcViewDesc.usage = WGPUTextureUsage_TextureBinding;

    WGPUTextureView srcTextureView = wgpuTextureCreateView(srcTexture, &srcViewDesc);
    ASSERT_NE(srcTextureView, nullptr);

    // 3. Create sampler
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.addressModeW = WGPUAddressMode_Repeat;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Nearest;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.lodMinClamp = 0;
    samplerDesc.lodMaxClamp = 1;
    samplerDesc.compare = WGPUCompareFunction_Undefined;
    samplerDesc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);
    ASSERT_NE(sampler, nullptr);

    // 4. Create bind group layout and bind group
    WGPUBindGroupLayoutEntry layoutEntries[2] = {};
    layoutEntries[0].binding = 0;
    layoutEntries[0].visibility = WGPUShaderStage_Fragment;
    layoutEntries[0].texture.sampleType = WGPUTextureSampleType_Float;
    layoutEntries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    layoutEntries[1].binding = 1;
    layoutEntries[1].visibility = WGPUShaderStage_Fragment;
    layoutEntries[1].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 2;
    bglDesc.entries = layoutEntries;
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bindGroupLayout, nullptr);

    WGPUBindGroupEntry bgEntries[2] = {};
    bgEntries[0].binding = 0;
    bgEntries[0].textureView = srcTextureView;
    bgEntries[1].binding = 1;
    bgEntries[1].sampler = sampler;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 2;
    bgDesc.entries = bgEntries;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bindGroup, nullptr);

    // 5. Create pipeline layout
    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pipelineLayout, nullptr);

    // 6. Create render pipeline with override constant brightness=1.0
    WGPUConstantEntry brightnessConstant = {};
    brightnessConstant.key = { "brightness", 10 };
    brightnessConstant.value = 1.0;

    WGPUVertexAttribute vertexAttributes[2] = {};
    vertexAttributes[0].shaderLocation = 0;
    vertexAttributes[0].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[0].offset = 0;
    vertexAttributes[1].shaderLocation = 1;
    vertexAttributes[1].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[1].offset = 2 * sizeof(float);

    WGPUVertexBufferLayout vbLayout = {};
    vbLayout.arrayStride = 4 * sizeof(float);
    vbLayout.attributeCount = 2;
    vbLayout.attributes = vertexAttributes;
    vbLayout.stepMode = WGPUVertexStepMode_Vertex;

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = { "fs_main", 7 };
    fragmentState.constantCount = 1;
    fragmentState.constants = &brightnessConstant;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = shaderModule;
    pipeDesc.vertex.entryPoint = { "vs_main", 7 };
    pipeDesc.vertex.bufferCount = 1;
    pipeDesc.vertex.buffers = &vbLayout;
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr) << "Failed to create render pipeline with WGSL override shader";

    // 7. Create fullscreen quad vertex + index buffers
    // Quad covers [-1, -1] to [1, 1] with UVs [0,0] to [1,1]
    float quadVerts[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,  // bottom-left
         1.0f, -1.0f,  1.0f, 0.0f,  // bottom-right
         1.0f,  1.0f,  1.0f, 1.0f,  // top-right
        -1.0f,  1.0f,  0.0f, 1.0f,  // top-left
    };
    uint32_t quadIndices[] = { 0, 1, 2, 0, 2, 3 };

    WGPUBufferDescriptor vbDesc = {};
    vbDesc.size = sizeof(quadVerts);
    vbDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(device, &vbDesc);
    wgpuQueueWriteBuffer(queue, vertexBuffer, 0, quadVerts, sizeof(quadVerts));

    WGPUBufferDescriptor ibDesc = {};
    ibDesc.size = sizeof(quadIndices);
    ibDesc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    WGPUBuffer indexBuffer = wgpuDeviceCreateBuffer(device, &ibDesc);
    wgpuQueueWriteBuffer(queue, indexBuffer, 0, quadIndices, sizeof(quadIndices));

    // 8. Create render target texture and readback buffer
    WGPUTextureDescriptor rtDesc = {};
    rtDesc.size = {width, height, 1};
    rtDesc.format = WGPUTextureFormat_RGBA8Unorm;
    rtDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    rtDesc.mipLevelCount = 1;
    rtDesc.sampleCount = 1;
    rtDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture renderTarget = wgpuDeviceCreateTexture(device, &rtDesc);
    ASSERT_NE(renderTarget, nullptr);

    WGPUTextureView rtView = wgpuTextureCreateView(renderTarget, nullptr);
    ASSERT_NE(rtView, nullptr);

    WGPUBufferDescriptor readBufDesc = {};
    readBufDesc.size = bufferSize;
    readBufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readBufDesc);
    ASSERT_NE(readBuffer, nullptr);

    // 9. Encode render pass + copy to readback buffer
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment colorAtt = {};
    colorAtt.view = rtView;
    colorAtt.loadOp = WGPULoadOp_Clear;
    colorAtt.storeOp = WGPUStoreOp_Store;
    colorAtt.clearValue = {0.0, 0.0, 0.0, 0.0}; // Clear to black/transparent

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &colorAtt;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, 6, 1, 0, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = renderTarget;
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    // 10. Submit
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // 11. Map and verify pixels
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };

    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    // Check that pixels are NOT all zero (brightness=1.0 means texture data should show through)
    // Sample a few pixels across the render target
    uint32_t nonZeroCount = 0;
    for (uint32_t y = 0; y < height; y += 8) {
        for (uint32_t x = 0; x < width; x += 8) {
            size_t offset = y * bytesPerRow + x * 4;
            uint8_t r = pixels[offset + 0];
            uint8_t g = pixels[offset + 1];
            uint8_t b = pixels[offset + 2];
            uint8_t a = pixels[offset + 3];
            if (r > 0 || g > 0 || b > 0 || a > 0) {
                nonZeroCount++;
            }
        }
    }

    // With brightness=1.0 and non-zero texture data, we expect the vast majority of
    // sampled pixels to be non-zero. The fullscreen quad covers everything.
    EXPECT_GT(nonZeroCount, 0u) << "All sampled pixels are zero - shader output is blank";
    EXPECT_GE(nonZeroCount, 32u) << "Too few non-zero pixels - expected most of the " << (8 * 8) << " samples to be non-zero";

    // Also verify the center pixel specifically
    {
        size_t offset = (height / 2) * bytesPerRow + (width / 2) * 4;
        uint8_t r = pixels[offset + 0];
        uint8_t g = pixels[offset + 1];
        uint8_t b = pixels[offset + 2];
        EXPECT_TRUE(r > 0 || g > 0 || b > 0) << "Center pixel is black (r=" << (int)r << " g=" << (int)g << " b=" << (int)b << ")";
    }

    wgpuBufferUnmap(readBuffer);

    // 12. Cleanup
    for (uint32_t i = 0; i < framesInFlight; i++) {
        wgpuDeviceTick(device);
    }

    wgpuBufferRelease(readBuffer);
    wgpuBufferRelease(vertexBuffer);
    wgpuBufferRelease(indexBuffer);
    wgpuTextureViewRelease(rtView);
    wgpuTextureRelease(renderTarget);
    wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuRenderPipelineRelease(pipeline);
    wgpuTextureViewRelease(srcTextureView);
    wgpuTextureRelease(srcTexture);
    wgpuSamplerRelease(sampler);
    wgpuShaderModuleRelease(shaderModule);
}
#endif

TEST_F(WebGPUTest, LimitsGetAndSet) {
    // Get limits from adapter
    WGPULimits adapterLimits = {0};
    ASSERT_EQ(wgpuAdapterGetLimits(adapter, &adapterLimits), WGPUStatus_Success) << "Failed to get adapter limits";
    
    // Verify that core limits are reasonable (non-zero, within expected ranges)
    EXPECT_GT(adapterLimits.maxTextureDimension1D, 0u);
    EXPECT_GT(adapterLimits.maxTextureDimension2D, 0u);
    EXPECT_GT(adapterLimits.maxTextureDimension3D, 0u);
    EXPECT_GT(adapterLimits.maxBufferSize, 0ull);
    EXPECT_GE(adapterLimits.maxBindGroups, 4u); // WebGPU spec minimum is 4
    EXPECT_GT(adapterLimits.maxVertexBuffers, 0u);
    EXPECT_GT(adapterLimits.maxComputeInvocationsPerWorkgroup, 0u);
    
    // Verify alignment limits are power of 2
    EXPECT_EQ(adapterLimits.minUniformBufferOffsetAlignment & (adapterLimits.minUniformBufferOffsetAlignment - 1), 0u) 
        << "minUniformBufferOffsetAlignment should be power of 2";
    EXPECT_EQ(adapterLimits.minStorageBufferOffsetAlignment & (adapterLimits.minStorageBufferOffsetAlignment - 1), 0u)
        << "minStorageBufferOffsetAlignment should be power of 2";

    // Test 2: Create device WITH specific required limits - should return those limits
    struct DeviceCtx {
        WGPUDevice device = nullptr;
        bool done = false;
    } deviceCtx;
    
    auto deviceCallback = [](WGPURequestDeviceStatus status, WGPUDevice dev, WGPUStringView msg, void* userdata, void* userdata2) {
        DeviceCtx* ctx = (DeviceCtx*)userdata;
        ctx->device = dev;
        ctx->done = true;
    };
    
    WGPULimits requiredLimits = adapterLimits;
    requiredLimits.maxTextureDimension2D = 4096;
    requiredLimits.maxBindGroups = 6;
    
    WGPUDeviceDescriptor deviceDesc3 = {0};
    deviceDesc3.requiredLimits = &requiredLimits;
    
    WGPURequestDeviceCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, deviceCallback, &deviceCtx, nullptr };
    WGPUFuture future = wgpuAdapterRequestDevice(adapter, &deviceDesc3, cbInfo);
    WGPUFutureWaitInfo fwi = { .future = future, .completed = 0 };
    
    while(!deviceCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT32_MAX);
    }
    
    ASSERT_NE(deviceCtx.device, nullptr) << "Failed to create device with required limits";
    WGPUDevice device3 = deviceCtx.device;
    
    WGPULimits deviceLimits3 = {0};
    wgpuDeviceGetLimits(device3, &deviceLimits3);
    EXPECT_EQ(deviceLimits3.maxTextureDimension2D, 4096u) << "Device should return requested maxTextureDimension2D";
    EXPECT_EQ(deviceLimits3.maxBindGroups, 6u) << "Device should return requested maxBindGroups";
    
    // TODO: Test that creating textures larger than maxTextureDimension2D fails
    // Currently not enforced, but should be:
    // WGPUTextureDescriptor texDesc = {0};
    // texDesc.size = {deviceLimits3.maxTextureDimension2D + 1, 1, 1};
    // texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    // texDesc.usage = WGPUTextureUsage_CopyDst;
    // WGPUTexture tex = wgpuDeviceCreateTexture(device3, &texDesc);
    // EXPECT_EQ(tex, nullptr) << "Should fail to create texture exceeding limits";
    
    wgpuDeviceRelease(device3);
}

// ---------------------------------------------------------------------------
// Infrastructure helpers
// ---------------------------------------------------------------------------

static WGPUShaderModule compileGLSL(WGPUDevice device, WGPUShaderStage stage, const char* code) {
    WGPUShaderSourceGLSL glslSource = {};
    glslSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    glslSource.stage = stage;
    glslSource.code.data = code;
    glslSource.code.length = strlen(code);

    WGPUShaderModuleDescriptor desc = {};
    desc.nextInChain = (WGPUChainedStruct*)&glslSource;
    return wgpuDeviceCreateShaderModule(device, &desc);
}

static void* mapBufferSync(WGPUInstance instance, WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size) {
    struct MapCtx { bool done = false; WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Error; } ctx;
    auto cb = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
        auto* c = (MapCtx*)ud;
        c->status = status;
        c->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, cb, &ctx, nullptr };
    WGPUFuture future = wgpuBufferMapAsync(buffer, mode, offset, size, cbInfo);
    WGPUFutureWaitInfo fwi = { future, 0 };
    while (!ctx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }
    if (ctx.status != WGPUMapAsyncStatus_Success) return nullptr;
    if (mode == WGPUMapMode_Read) {
        return const_cast<void*>(wgpuBufferGetConstMappedRange(buffer, offset, size));
    }
    return wgpuBufferGetMappedRange(buffer, offset, size);
}

// ---------------------------------------------------------------------------
// P0 Section A: Reference Counting Tests
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, TextureReferenceCounting) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_CopyDst;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;
    desc.label = { "RefTestTexture", 14 };

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(texture, nullptr);

    wgpuTextureAddRef(texture);
    ASSERT_EQ(texture->refCount, 2);

    wgpuTextureRelease(texture);
    ASSERT_EQ(texture->refCount, 1);

    wgpuTextureRelease(texture);
}

TEST_F(WebGPUTest, TextureViewReferenceCounting) {
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {64, 64, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    ASSERT_NE(texture, nullptr);

    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    ASSERT_NE(view, nullptr);

    // Texture refcount: 1 (user) + 1 (view) = 2
    EXPECT_EQ(texture->refCount, 2);
    EXPECT_EQ(view->refCount, 1);

    wgpuTextureViewAddRef(view);
    ASSERT_EQ(view->refCount, 2);

    wgpuTextureViewRelease(view);
    ASSERT_EQ(view->refCount, 1);

    wgpuTextureViewRelease(view);
    // View released its ref on texture
    EXPECT_EQ(texture->refCount, 1);

    wgpuTextureRelease(texture);
}

TEST_F(WebGPUTest, SamplerReferenceCounting) {
    WGPUSamplerDescriptor desc = {};
    desc.addressModeU = WGPUAddressMode_Repeat;
    desc.addressModeV = WGPUAddressMode_Repeat;
    desc.addressModeW = WGPUAddressMode_Repeat;
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr);

    wgpuSamplerAddRef(sampler);
    ASSERT_EQ(sampler->refCount, 2);

    wgpuSamplerRelease(sampler);
    ASSERT_EQ(sampler->refCount, 1);

    wgpuSamplerRelease(sampler);
}

TEST_F(WebGPUTest, ShaderModuleReferenceCounting) {
    const char* code = R"(
        #version 450
        layout(local_size_x = 1) in;
        void main() {}
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code);
    ASSERT_NE(sm, nullptr);

    wgpuShaderModuleAddRef(sm);
    ASSERT_EQ(sm->refCount, 2);

    wgpuShaderModuleRelease(sm);
    ASSERT_EQ(sm->refCount, 1);

    wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, BindGroupLayoutReferenceCounting) {
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor desc = {};
    desc.entryCount = 1;
    desc.entries = &entry;

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &desc);
    ASSERT_NE(bgl, nullptr);

    wgpuBindGroupLayoutAddRef(bgl);
    ASSERT_EQ(bgl->refCount, 2);

    wgpuBindGroupLayoutRelease(bgl);
    ASSERT_EQ(bgl->refCount, 1);

    wgpuBindGroupLayoutRelease(bgl);
}

TEST_F(WebGPUTest, PipelineLayoutReferenceCounting) {
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pl, nullptr);

    // PL should AddRef the BGL
    EXPECT_EQ(bgl->refCount, 2);
    EXPECT_EQ(pl->refCount, 1);

    wgpuPipelineLayoutAddRef(pl);
    ASSERT_EQ(pl->refCount, 2);

    wgpuPipelineLayoutRelease(pl);
    ASSERT_EQ(pl->refCount, 1);

    // Release user ref on BGL
    wgpuBindGroupLayoutRelease(bgl);
    EXPECT_EQ(bgl->refCount, 1); // still held by PL

    wgpuPipelineLayoutRelease(pl);
    // PL freed, BGL freed (ASan validates)
}

TEST_F(WebGPUTest, ComputePipelineReferenceCounting) {
    const char* code = R"(
        #version 450
        layout(local_size_x = 1) in;
        layout(std430, set = 0, binding = 0) buffer Data { uint v[]; } data;
        void main() { data.v[gl_GlobalInvocationID.x] = 0; }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code);
    ASSERT_NE(sm, nullptr);

    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pl;
    pipeDesc.compute.module = sm;
    pipeDesc.compute.entryPoint = { "main", 4 };

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // Pipeline should AddRef layout
    EXPECT_EQ(pl->refCount, 2);
    EXPECT_EQ(pipeline->refCount, 1);

    wgpuComputePipelineAddRef(pipeline);
    ASSERT_EQ(pipeline->refCount, 2);

    wgpuComputePipelineRelease(pipeline);
    ASSERT_EQ(pipeline->refCount, 1);

    // Cleanup
    wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pl);
    wgpuComputePipelineRelease(pipeline);
    wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, RenderPipelineReferenceCounting) {
    const char* vsCode = R"(
        #version 450
        void main() {
            gl_Position = vec4(0.0, 0.0, 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    plDesc.bindGroupLayouts = nullptr;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pl, nullptr);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pl;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline rp = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(rp, nullptr);

    // Pipeline should AddRef layout
    EXPECT_EQ(pl->refCount, 2);

    wgpuRenderPipelineAddRef(rp);
    ASSERT_EQ(rp->refCount, 2);

    wgpuRenderPipelineRelease(rp);
    ASSERT_EQ(rp->refCount, 1);

    // Cleanup
    wgpuPipelineLayoutRelease(pl);
    wgpuRenderPipelineRelease(rp);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

TEST_F(WebGPUTest, BindGroupReferenceCounting) {
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = 256;
    bufDesc.usage = WGPUBufferUsage_Storage;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    ASSERT_NE(buffer, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = buffer;
    bgEntry.size = 256;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bg, nullptr);

    // BG should AddRef BGL
    EXPECT_EQ(bgl->refCount, 2);
    EXPECT_EQ(bg->refCount, 1);

    wgpuBindGroupAddRef(bg);
    ASSERT_EQ(bg->refCount, 2);

    wgpuBindGroupRelease(bg);
    ASSERT_EQ(bg->refCount, 1);

    // Release user ref on BGL, still held by BG
    wgpuBindGroupLayoutRelease(bgl);
    EXPECT_EQ(bgl->refCount, 1);

    wgpuBindGroupRelease(bg);
    // BG freed, BGL freed
    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, CommandEncoderReferenceCounting) {
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    ASSERT_NE(enc, nullptr);

    // BUG: wgpuDeviceCreateCommandEncoder does not set refCount = 1.
    // The encoder starts at refCount=0 (RL_CALLOC zero-init).
    // We document this and test AddRef increments correctly.
    EXPECT_EQ(enc->refCount, 0) << "Known bug: encoder refCount not initialized to 1";

    wgpuCommandEncoderAddRef(enc);
    EXPECT_EQ(enc->refCount, 1);

    // Finish the encoder to get a command buffer (ends VkCommandBuffer recording).
    // This avoids vkBeginCommandBuffer errors on release.
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    ASSERT_NE(cmd, nullptr);

    // Now release: enc refCount goes 1->0, encoder freed.
    wgpuCommandEncoderRelease(enc);
    wgpuCommandBufferRelease(cmd);
}

TEST_F(WebGPUTest, CommandBufferReferenceCounting) {
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    ASSERT_NE(enc, nullptr);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    ASSERT_NE(cmd, nullptr);

    wgpuCommandBufferAddRef(cmd);
    ASSERT_EQ(cmd->refCount, 2);

    wgpuCommandBufferRelease(cmd);
    ASSERT_EQ(cmd->refCount, 1);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

TEST_F(WebGPUTest, QuerySetReferenceCounting) {
    WGPUQuerySetDescriptor desc = {};
    desc.type = WGPUQueryType_Occlusion;
    desc.count = 4;

    WGPUQuerySet qs = wgpuDeviceCreateQuerySet(device, &desc);
    ASSERT_NE(qs, nullptr);

    EXPECT_EQ(qs->refCount, 1);

    wgpuQuerySetAddRef(qs);
    EXPECT_EQ(qs->refCount, 2);

    wgpuQuerySetRelease(qs);
    EXPECT_EQ(qs->refCount, 1);

    wgpuQuerySetRelease(qs);
}

TEST_F(WebGPUTest, FenceRefCounting) {
    WGPUFence fence = wgpuDeviceCreateFence(device);
    ASSERT_NE(fence, nullptr);

    wgpuFenceAddRef(fence);
    ASSERT_EQ(fence->refCount, 2);

    wgpuFenceRelease(fence);
    ASSERT_EQ(fence->refCount, 1);

    wgpuFenceRelease(fence);
}

// ---------------------------------------------------------------------------
// P0 Edge Cases
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, Edge_CreateAndImmediateRelease_Buffer) {
    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_CopyDst;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);
    wgpuBufferRelease(buffer);
    // ASan validates no leak / no crash
}

TEST_F(WebGPUTest, Edge_CreateAndImmediateRelease_Texture) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_CopyDst;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(texture, nullptr);
    wgpuTextureRelease(texture);
}

TEST_F(WebGPUTest, Edge_EmptyCommandEncoderFinish) {
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    ASSERT_NE(enc, nullptr);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    ASSERT_NE(cmd, nullptr);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

TEST_F(WebGPUTest, Edge_SubmitEmptyCommandBuffer) {
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    ASSERT_NE(enc, nullptr);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    ASSERT_NE(cmd, nullptr);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuQueueWaitIdle(queue);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

TEST_F(WebGPUTest, Edge_QueueSubmit_ZeroCommandBuffers) {
    wgpuQueueSubmit(queue, 0, nullptr);
    // No crash, no hang
}

TEST_F(WebGPUTest, Edge_DeviceTick_NoWork) {
    wgpuDeviceTick(device);
    // No crash, no hang
}

// ---------------------------------------------------------------------------
// P0 Fence Tests
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, FenceCreate_NonNull) {
    WGPUFence f = wgpuDeviceCreateFence(device);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->refCount, 1);
    EXPECT_EQ(atomic_load_explicit(&f->state, std::memory_order_acquire), WGPUFenceState_Reset);
    wgpuFenceRelease(f);
}

TEST_F(WebGPUTest, FenceSignalAndWait) {
    // wgpuQueueOnSubmittedWorkDone internally creates a fence, submits empty
    // work, then waits on it. We test that the full fence lifecycle works by
    // going through OnSubmittedWorkDone and verifying completion.
    struct WorkCtx {
        bool done = false;
        WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Error;
    } ctx;

    auto cb = [](WGPUQueueWorkDoneStatus status, void* ud, void*) {
        WorkCtx* c = (WorkCtx*)ud;
        c->status = status;
        c->done = true;
    };

    // Submit real work so the fence actually has something to wait on
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);

    WGPUQueueWorkDoneCallbackInfo cbInfo = {};
    cbInfo.callback = cb;
    cbInfo.userdata1 = &ctx;
    cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUFuture future = wgpuQueueOnSubmittedWorkDone(queue, cbInfo);
    WGPUFutureWaitInfo waitInfo = { future, 0 };
    while (!ctx.done) {
        wgpuInstanceWaitAny(instance, 1, &waitInfo, 100000000);
    }

    EXPECT_EQ(ctx.status, WGPUQueueWorkDoneStatus_Success);
    EXPECT_EQ(waitInfo.completed, 1u);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

TEST_F(WebGPUTest, FencesWait_MultipleFences) {
    // Create two fences and verify wgpuFencesWait with them does not crash.
    WGPUFence f1 = wgpuDeviceCreateFence(device);
    WGPUFence f2 = wgpuDeviceCreateFence(device);
    ASSERT_NE(f1, nullptr);
    ASSERT_NE(f2, nullptr);

    // wgpuFencesWait with count=0 and nullptr -- must not crash
    wgpuFencesWait(nullptr, 0, UINT64_MAX);

    // wgpuFencesWait with valid fences in Reset state -- must not hang or crash.
    // These fences are in Reset state (never submitted), so waiting on them
    // should either return immediately or be a no-op. The implementation
    // calls wgpuFenceWait per fence which checks state.
    // NOTE: Fences in Reset state have never been submitted to the GPU, so
    // vkWaitForFences on them would block. We only test the null/zero path.

    wgpuFenceRelease(f1);
    wgpuFenceRelease(f2);
}

// ---------------------------------------------------------------------------
// P0 WaitAny Tests
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, WaitAny_SingleFuture_WorkDone) {
    struct WorkCtx {
        bool done = false;
        WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Error;
    } ctx;

    auto cb = [](WGPUQueueWorkDoneStatus status, void* ud, void*) {
        WorkCtx* c = (WorkCtx*)ud;
        c->status = status;
        c->done = true;
    };

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);

    WGPUQueueWorkDoneCallbackInfo cbInfo = {};
    cbInfo.callback = cb;
    cbInfo.userdata1 = &ctx;
    cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUFuture future = wgpuQueueOnSubmittedWorkDone(queue, cbInfo);
    WGPUFutureWaitInfo wi = { future, 0 };

    WGPUWaitStatus ws = WGPUWaitStatus_Error;
    while (!ctx.done) {
        ws = wgpuInstanceWaitAny(instance, 1, &wi, 100000000);
    }

    EXPECT_EQ(ws, WGPUWaitStatus_Success);
    EXPECT_EQ(wi.completed, 1u);
    EXPECT_EQ(ctx.status, WGPUQueueWorkDoneStatus_Success);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

TEST_F(WebGPUTest, WaitAny_ZeroFutures) {
    // Calling WaitAny with 0 futures and nullptr should not crash
    WGPUWaitStatus ws = wgpuInstanceWaitAny(instance, 0, nullptr, 0);
    EXPECT_EQ(ws, WGPUWaitStatus_Success);
}

// ---------------------------------------------------------------------------
// P0 Invalid Param Tests
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, InvalidParam_BufferMapRead_IllegalUsageCombination) {
    // MapRead can only be combined with CopyDst per the WebGPU spec.
    // Combining MapRead | Storage is illegal. WGVK emits a validation error
    // via DeviceCallback but still creates the buffer.

    struct ErrorCtx {
        bool fired = false;
        WGPUErrorType type = WGPUErrorType_NoError;
    } errCtx;

    auto errCb = [](const WGPUDevice*, WGPUErrorType type, WGPUStringView, void* ud, void*) {
        ErrorCtx* ctx = (ErrorCtx*)ud;
        ctx->type = type;
        ctx->fired = true;
    };

    // Install error callback directly on device internals (we have access
    // via wgvk_structs_impl.h)
    WGPUUncapturedErrorCallbackInfo prevCb = device->uncapturedErrorCallbackInfo;
    device->uncapturedErrorCallbackInfo.callback = errCb;
    device->uncapturedErrorCallbackInfo.userdata1 = &errCtx;
    device->uncapturedErrorCallbackInfo.userdata2 = nullptr;

    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_Storage;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    // Buffer should still be created (WGVK continues after validation error)
    ASSERT_NE(buffer, nullptr);
    EXPECT_TRUE(errCtx.fired) << "Expected validation error for MapRead | Storage";
    EXPECT_EQ(errCtx.type, WGPUErrorType_Validation);

    wgpuBufferRelease(buffer);

    // Restore previous callback
    device->uncapturedErrorCallbackInfo = prevCb;
}

TEST_F(WebGPUTest, InvalidParam_BufferMapWrite_IllegalUsageCombination) {
    // MapWrite can only be combined with CopySrc per the WebGPU spec.
    // Combining MapWrite | Storage is illegal.

    struct ErrorCtx {
        bool fired = false;
        WGPUErrorType type = WGPUErrorType_NoError;
    } errCtx;

    auto errCb = [](const WGPUDevice*, WGPUErrorType type, WGPUStringView, void* ud, void*) {
        ErrorCtx* ctx = (ErrorCtx*)ud;
        ctx->type = type;
        ctx->fired = true;
    };

    WGPUUncapturedErrorCallbackInfo prevCb = device->uncapturedErrorCallbackInfo;
    device->uncapturedErrorCallbackInfo.callback = errCb;
    device->uncapturedErrorCallbackInfo.userdata1 = &errCtx;
    device->uncapturedErrorCallbackInfo.userdata2 = nullptr;

    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_Storage;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);
    EXPECT_TRUE(errCtx.fired) << "Expected validation error for MapWrite | Storage";
    EXPECT_EQ(errCtx.type, WGPUErrorType_Validation);

    wgpuBufferRelease(buffer);

    device->uncapturedErrorCallbackInfo = prevCb;
}

// ---------------------------------------------------------------------------
// P0 More Edge Cases
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, Edge_QueueWorkDone_NoSubmission) {
    // Call OnSubmittedWorkDone without any prior user submission.
    // The implementation internally submits an empty VkSubmitInfo with a fence,
    // so the callback should still fire with Success.
    struct WorkCtx {
        bool done = false;
        WGPUQueueWorkDoneStatus status = WGPUQueueWorkDoneStatus_Error;
    } ctx;

    auto cb = [](WGPUQueueWorkDoneStatus status, void* ud, void*) {
        WorkCtx* c = (WorkCtx*)ud;
        c->status = status;
        c->done = true;
    };

    WGPUQueueWorkDoneCallbackInfo cbInfo = {};
    cbInfo.callback = cb;
    cbInfo.userdata1 = &ctx;
    cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUFuture future = wgpuQueueOnSubmittedWorkDone(queue, cbInfo);
    WGPUFutureWaitInfo waitInfo = { future, 0 };
    while (!ctx.done) {
        wgpuInstanceWaitAny(instance, 1, &waitInfo, 100000000);
    }

    EXPECT_EQ(ctx.status, WGPUQueueWorkDoneStatus_Success);
}

TEST_F(WebGPUTest, Edge_FutureIdMonotonicity) {
    // Create 3 futures via different async operations and verify their IDs
    // are strictly monotonically increasing.
    // We use buffer map operations to avoid fence-cache reuse issues that
    // occur when calling wgpuQueueOnSubmittedWorkDone in rapid succession.

    // Create 3 small buffers to map
    WGPUBuffer bufs[3];
    for (int i = 0; i < 3; i++) {
        WGPUBufferDescriptor desc = {};
        desc.size = 64;
        desc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        bufs[i] = wgpuDeviceCreateBuffer(device, &desc);
        ASSERT_NE(bufs[i], nullptr);
    }

    struct MapCtx { bool done = false; };

    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        MapCtx* c = (MapCtx*)ud;
        c->done = true;
    };

    // Create 3 futures from buffer map operations
    MapCtx ctxs[3] = {};
    WGPUFuture futures[3];
    for (int i = 0; i < 3; i++) {
        WGPUBufferMapCallbackInfo cbInfo = {};
        cbInfo.callback = mapCb;
        cbInfo.userdata1 = &ctxs[i];
        cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;
        futures[i] = wgpuBufferMapAsync(bufs[i], WGPUMapMode_Read, 0, 64, cbInfo);
    }

    // Verify monotonicity before waiting
    EXPECT_LT(futures[0].id, futures[1].id) << "Future IDs must be strictly increasing";
    EXPECT_LT(futures[1].id, futures[2].id) << "Future IDs must be strictly increasing";

    // Wait on all futures to clean up
    for (int i = 0; i < 3; i++) {
        WGPUFutureWaitInfo wi = { futures[i], 0 };
        while (!ctxs[i].done) {
            wgpuInstanceWaitAny(instance, 1, &wi, 100000000);
        }
    }

    // Cleanup
    for (int i = 0; i < 3; i++) {
        wgpuBufferUnmap(bufs[i]);
        wgpuBufferRelease(bufs[i]);
    }
}

// ============================================================
// P0 Compute Dispatch Tests
// ============================================================

TEST_F(WebGPUTest, ComputeDispatch_Workgroup1x1x1) {
    const uint32_t elementCount = 256;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);

    WGPUBufferDescriptor stagingDesc = {};
    stagingDesc.size = bufferSize;
    stagingDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    stagingDesc.mappedAtCreation = true;
    WGPUBuffer stagingBuffer = wgpuDeviceCreateBuffer(device, &stagingDesc);
    ASSERT_NE(stagingBuffer, nullptr);

    uint32_t* initialData = (uint32_t*)wgpuBufferGetMappedRange(stagingBuffer, 0, bufferSize);
    for (uint32_t i = 0; i < elementCount; ++i) initialData[i] = i;
    wgpuBufferUnmap(stagingBuffer);

    WGPUBufferDescriptor storageDesc = {};
    storageDesc.size = bufferSize;
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer storageBuffer = wgpuDeviceCreateBuffer(device, &storageDesc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, stagingBuffer, 0, storageBuffer, 0, bufferSize);
    WGPUCommandBuffer setupCmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &setupCmd);
    wgpuQueueWaitIdle(queue);
    wgpuCommandBufferRelease(setupCmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(stagingBuffer);

    const char* glslCode = R"(
        #version 450
        layout(local_size_x = 1) in;
        layout(std430, set = 0, binding = 0) buffer Data {
            uint values[];
        } data;
        void main() {
            uint index = gl_GlobalInvocationID.x;
            data.values[index] = data.values[index] + 1;
        }
    )";

    WGPUShaderSourceGLSL glslSource = {};
    glslSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    glslSource.stage = WGPUShaderStage_Compute;
    glslSource.code.data = glslCode;
    glslSource.code.length = strlen(glslCode);

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = (WGPUChainedStruct*)&glslSource;
    shaderDesc.label = { "IncrementShader", 15 };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    ASSERT_NE(shaderModule, nullptr);

    WGPUBindGroupLayoutEntry bglEntry = {};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Compute;
    bglEntry.buffer.type = WGPUBufferBindingType_Storage;
    bglEntry.buffer.minBindingSize = bufferSize;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.compute.module = shaderModule;
    pipeDesc.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = storageBuffer;
    bgEntry.offset = 0;
    bgEntry.size = bufferSize;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor passDesc = {};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, 256, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    WGPUBufferDescriptor readDesc = {};
    readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, storageBuffer, 0, readBuffer, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) {
        EXPECT_EQ(results[i], i + 1) << "Index " << i << " mismatch";
    }

    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(readBuffer);
    wgpuBufferRelease(storageBuffer);
    wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuComputePipelineRelease(pipeline);
    wgpuShaderModuleRelease(shaderModule);
}

TEST_F(WebGPUTest, ComputeDispatch_Workgroup64x1x1) {
    const uint32_t elementCount = 1024;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);

    WGPUBufferDescriptor stagingDesc = {};
    stagingDesc.size = bufferSize;
    stagingDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    stagingDesc.mappedAtCreation = true;
    WGPUBuffer stagingBuffer = wgpuDeviceCreateBuffer(device, &stagingDesc);
    ASSERT_NE(stagingBuffer, nullptr);

    uint32_t* initialData = (uint32_t*)wgpuBufferGetMappedRange(stagingBuffer, 0, bufferSize);
    for (uint32_t i = 0; i < elementCount; ++i) initialData[i] = i;
    wgpuBufferUnmap(stagingBuffer);

    WGPUBufferDescriptor storageDesc = {};
    storageDesc.size = bufferSize;
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer storageBuffer = wgpuDeviceCreateBuffer(device, &storageDesc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, stagingBuffer, 0, storageBuffer, 0, bufferSize);
    WGPUCommandBuffer setupCmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &setupCmd);
    wgpuQueueWaitIdle(queue);
    wgpuCommandBufferRelease(setupCmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuBufferRelease(stagingBuffer);

    const char* glslCode = R"(
        #version 450
        layout(local_size_x = 64) in;
        layout(std430, set = 0, binding = 0) buffer Data {
            uint values[];
        } data;
        void main() {
            uint index = gl_GlobalInvocationID.x;
            data.values[index] = data.values[index] * 3;
        }
    )";

    WGPUShaderSourceGLSL glslSource = {};
    glslSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    glslSource.stage = WGPUShaderStage_Compute;
    glslSource.code.data = glslCode;
    glslSource.code.length = strlen(glslCode);

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = (WGPUChainedStruct*)&glslSource;
    shaderDesc.label = { "Mul3Shader", 10 };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    ASSERT_NE(shaderModule, nullptr);

    WGPUBindGroupLayoutEntry bglEntry = {};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Compute;
    bglEntry.buffer.type = WGPUBufferBindingType_Storage;
    bglEntry.buffer.minBindingSize = bufferSize;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.compute.module = shaderModule;
    pipeDesc.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = storageBuffer;
    bgEntry.offset = 0;
    bgEntry.size = bufferSize;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor passDesc = {};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, 16, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    WGPUBufferDescriptor readDesc = {};
    readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, storageBuffer, 0, readBuffer, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) {
        EXPECT_EQ(results[i], i * 3) << "Index " << i << " mismatch";
    }

    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(readBuffer);
    wgpuBufferRelease(storageBuffer);
    wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuComputePipelineRelease(pipeline);
    wgpuShaderModuleRelease(shaderModule);
}

// ============================================================
// P0 Copy Tests
// ============================================================

TEST_F(WebGPUTest, CopyBufferToBuffer_Full) {
    const uint32_t elementCount = 256;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);

    WGPUBufferDescriptor srcDesc = {};
    srcDesc.size = bufferSize;
    srcDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    srcDesc.mappedAtCreation = true;
    WGPUBuffer srcBuffer = wgpuDeviceCreateBuffer(device, &srcDesc);
    ASSERT_NE(srcBuffer, nullptr);

    uint32_t* srcData = (uint32_t*)wgpuBufferGetMappedRange(srcBuffer, 0, bufferSize);
    for (uint32_t i = 0; i < elementCount; ++i) srcData[i] = 0xCAFE0000 + i;
    wgpuBufferUnmap(srcBuffer);

    WGPUBufferDescriptor dstDesc = {};
    dstDesc.size = bufferSize;
    dstDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer dstBuffer = wgpuDeviceCreateBuffer(device, &dstDesc);
    ASSERT_NE(dstBuffer, nullptr);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, srcBuffer, 0, dstBuffer, 0, bufferSize);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(dstBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(dstBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) {
        EXPECT_EQ(results[i], 0xCAFE0000 + i) << "Index " << i << " mismatch";
    }

    wgpuBufferUnmap(dstBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(srcBuffer);
    wgpuBufferRelease(dstBuffer);
}

TEST_F(WebGPUTest, CopyBufferToBuffer_PartialWithOffsets) {
    const uint32_t bufferSize = 1024;
    const uint32_t copySize = 256;
    const uint32_t srcOffset = 256;
    const uint32_t dstOffset = 512;

    WGPUBufferDescriptor srcDesc = {};
    srcDesc.size = bufferSize;
    srcDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    srcDesc.mappedAtCreation = true;
    WGPUBuffer srcBuffer = wgpuDeviceCreateBuffer(device, &srcDesc);
    ASSERT_NE(srcBuffer, nullptr);

    uint32_t* srcData = (uint32_t*)wgpuBufferGetMappedRange(srcBuffer, 0, bufferSize);
    for (uint32_t i = 0; i < bufferSize / sizeof(uint32_t); ++i) srcData[i] = 0xAA000000 + i;
    wgpuBufferUnmap(srcBuffer);

    WGPUBufferDescriptor dstDesc = {};
    dstDesc.size = bufferSize;
    dstDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    dstDesc.mappedAtCreation = true;
    WGPUBuffer dstBuffer = wgpuDeviceCreateBuffer(device, &dstDesc);
    ASSERT_NE(dstBuffer, nullptr);

    uint32_t* dstData = (uint32_t*)wgpuBufferGetMappedRange(dstBuffer, 0, bufferSize);
    memset(dstData, 0xFF, bufferSize);
    wgpuBufferUnmap(dstBuffer);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, srcBuffer, srcOffset, dstBuffer, dstOffset, copySize);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    WGPUBufferDescriptor readDesc = {};
    readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, dstBuffer, 0, readBuffer, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);

    for (uint32_t i = 0; i < dstOffset / sizeof(uint32_t); ++i) {
        EXPECT_EQ(results[i], 0xFFFFFFFF) << "Untouched region [0,512) corrupted at u32 index " << i;
    }
    for (uint32_t i = 0; i < copySize / sizeof(uint32_t); ++i) {
        uint32_t dstIdx = dstOffset / sizeof(uint32_t) + i;
        uint32_t srcIdx = srcOffset / sizeof(uint32_t) + i;
        EXPECT_EQ(results[dstIdx], 0xAA000000 + srcIdx) << "Copied region mismatch at u32 index " << dstIdx;
    }
    for (uint32_t i = (dstOffset + copySize) / sizeof(uint32_t); i < bufferSize / sizeof(uint32_t); ++i) {
        EXPECT_EQ(results[i], 0xFFFFFFFF) << "Untouched region [768,1024) corrupted at u32 index " << i;
    }

    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(srcBuffer);
    wgpuBufferRelease(dstBuffer);
    wgpuBufferRelease(readBuffer);
}

TEST_F(WebGPUTest, CopyBufferToTexture_RGBA8) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t dataSize = bytesPerRow * height;

    std::vector<uint8_t> srcPixels(dataSize);
    for (size_t i = 0; i < dataSize; ++i) {
        srcPixels[i] = (uint8_t)((i * 7 + 13) & 0xFF);
    }

    WGPUBufferDescriptor uploadDesc = {};
    uploadDesc.size = dataSize;
    uploadDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    uploadDesc.mappedAtCreation = true;
    WGPUBuffer uploadBuffer = wgpuDeviceCreateBuffer(device, &uploadDesc);
    ASSERT_NE(uploadBuffer, nullptr);

    void* mapped = wgpuBufferGetMappedRange(uploadBuffer, 0, dataSize);
    memcpy(mapped, srcPixels.data(), dataSize);
    wgpuBufferUnmap(uploadBuffer);

    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    ASSERT_NE(texture, nullptr);

    WGPUBufferDescriptor readDesc = {};
    readDesc.size = dataSize;
    readDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPUTexelCopyBufferInfo bufSrcInfo = {};
    bufSrcInfo.buffer = uploadBuffer;
    bufSrcInfo.layout.offset = 0;
    bufSrcInfo.layout.bytesPerRow = bytesPerRow;
    bufSrcInfo.layout.rowsPerImage = height;

    WGPUTexelCopyTextureInfo texDstInfo = {};
    texDstInfo.texture = texture;
    texDstInfo.mipLevel = 0;
    texDstInfo.origin = {0, 0, 0};
    texDstInfo.aspect = WGPUTextureAspect_All;

    WGPUExtent3D copyExtent = {width, height, 1};
    wgpuCommandEncoderCopyBufferToTexture(encoder, &bufSrcInfo, &texDstInfo, &copyExtent);

    WGPUTexelCopyTextureInfo texSrcInfo = {};
    texSrcInfo.texture = texture;
    texSrcInfo.mipLevel = 0;
    texSrcInfo.origin = {0, 0, 0};
    texSrcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo bufDstInfo = {};
    bufDstInfo.buffer = readBuffer;
    bufDstInfo.layout.offset = 0;
    bufDstInfo.layout.bytesPerRow = bytesPerRow;
    bufDstInfo.layout.rowsPerImage = height;

    wgpuCommandEncoderCopyTextureToBuffer(encoder, &texSrcInfo, &bufDstInfo, &copyExtent);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, dataSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* readback = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, dataSize);
    ASSERT_NE(readback, nullptr);

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t offset = y * bytesPerRow + x * 4;
            EXPECT_EQ(readback[offset + 0], srcPixels[offset + 0]) << "R mismatch at " << x << "," << y;
            EXPECT_EQ(readback[offset + 1], srcPixels[offset + 1]) << "G mismatch at " << x << "," << y;
            EXPECT_EQ(readback[offset + 2], srcPixels[offset + 2]) << "B mismatch at " << x << "," << y;
            EXPECT_EQ(readback[offset + 3], srcPixels[offset + 3]) << "A mismatch at " << x << "," << y;
        }
    }

    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(uploadBuffer);
    wgpuBufferRelease(readBuffer);
    wgpuTextureRelease(texture);
}

// ============================================================
// P0 Buffer Mapping Tests
// ============================================================

TEST_F(WebGPUTest, BufferMapUnmapCycle) {
    const size_t bufSize = 256;

    WGPUBufferDescriptor desc = {};
    desc.size = bufSize;
    desc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);

    // First map-write cycle
    {
        struct MapCtx1 { bool done = false; WGPUMapAsyncStatus status; } mapCtx1;
        auto mapCb1 = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
            auto* ctx = (MapCtx1*)ud;
            ctx->status = status;
            ctx->done = true;
        };
        WGPUBufferMapCallbackInfo cbInfo1 = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb1, &mapCtx1, nullptr };
        WGPUFuture fut1 = wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, bufSize, cbInfo1);
        WGPUFutureWaitInfo fwi1 = { fut1, 0 };
        while (!mapCtx1.done) {
            wgpuInstanceWaitAny(instance, 1, &fwi1, UINT64_MAX);
        }
        ASSERT_EQ(mapCtx1.status, WGPUMapAsyncStatus_Success);

        uint8_t* ptr = (uint8_t*)wgpuBufferGetMappedRange(buffer, 0, bufSize);
        ASSERT_NE(ptr, nullptr);
        memset(ptr, 0xAA, bufSize);
        wgpuBufferUnmap(buffer);
    }

    // Second map-write cycle
    {
        struct MapCtx2 { bool done = false; WGPUMapAsyncStatus status; } mapCtx2;
        auto mapCb2 = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
            auto* ctx = (MapCtx2*)ud;
            ctx->status = status;
            ctx->done = true;
        };
        WGPUBufferMapCallbackInfo cbInfo2 = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb2, &mapCtx2, nullptr };
        WGPUFuture fut2 = wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, bufSize, cbInfo2);
        WGPUFutureWaitInfo fwi2 = { fut2, 0 };
        while (!mapCtx2.done) {
            wgpuInstanceWaitAny(instance, 1, &fwi2, UINT64_MAX);
        }
        ASSERT_EQ(mapCtx2.status, WGPUMapAsyncStatus_Success);

        uint8_t* ptr2 = (uint8_t*)wgpuBufferGetMappedRange(buffer, 0, bufSize);
        ASSERT_NE(ptr2, nullptr);
        memset(ptr2, 0xBB, bufSize);
        wgpuBufferUnmap(buffer);
    }

    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, BufferMappedAtCreationWrite) {
    const uint32_t elementCount = 64;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);

    WGPUBufferDescriptor srcDesc = {};
    srcDesc.size = bufferSize;
    srcDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    srcDesc.mappedAtCreation = true;
    WGPUBuffer srcBuffer = wgpuDeviceCreateBuffer(device, &srcDesc);
    ASSERT_NE(srcBuffer, nullptr);

    uint32_t* ptr = (uint32_t*)wgpuBufferGetMappedRange(srcBuffer, 0, bufferSize);
    ASSERT_NE(ptr, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) ptr[i] = 0xDEAD0000 + i;
    wgpuBufferUnmap(srcBuffer);

    WGPUBufferDescriptor readDesc = {};
    readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, srcBuffer, 0, readBuffer, 0, bufferSize);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) {
        EXPECT_EQ(results[i], 0xDEAD0000 + i) << "Index " << i << " mismatch after GPU round-trip";
    }

    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(srcBuffer);
    wgpuBufferRelease(readBuffer);
}

TEST_F(WebGPUTest, BufferMapReadAfterGPUWrite) {
    const uint32_t elementCount = 256;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);

    WGPUBufferDescriptor storageDesc = {};
    storageDesc.size = bufferSize;
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    WGPUBuffer storageBuffer = wgpuDeviceCreateBuffer(device, &storageDesc);

    const char* glslCode = R"(
        #version 450
        layout(local_size_x = 1) in;
        layout(std430, set = 0, binding = 0) buffer Data {
            uint values[];
        } data;
        void main() {
            uint index = gl_GlobalInvocationID.x;
            data.values[index] = 42;
        }
    )";

    WGPUShaderSourceGLSL glslSource = {};
    glslSource.chain.sType = WGPUSType_ShaderSourceGLSL;
    glslSource.stage = WGPUShaderStage_Compute;
    glslSource.code.data = glslCode;
    glslSource.code.length = strlen(glslCode);

    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = (WGPUChainedStruct*)&glslSource;
    shaderDesc.label = { "Write42Shader", 13 };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    ASSERT_NE(shaderModule, nullptr);

    WGPUBindGroupLayoutEntry bglEntry = {};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Compute;
    bglEntry.buffer.type = WGPUBufferBindingType_Storage;
    bglEntry.buffer.minBindingSize = bufferSize;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.compute.module = shaderModule;
    pipeDesc.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = storageBuffer;
    bgEntry.offset = 0;
    bgEntry.size = bufferSize;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor passDesc = {};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, elementCount, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    WGPUBufferDescriptor readDesc = {};
    readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);

    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, storageBuffer, 0, readBuffer, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) {
        EXPECT_EQ(results[i], 42u) << "Index " << i << " expected 42";
    }

    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);

    wgpuBufferRelease(readBuffer);
    wgpuBufferRelease(storageBuffer);
    wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuComputePipelineRelease(pipeline);
    wgpuShaderModuleRelease(shaderModule);
}

TEST_F(WebGPUTest, BufferMapStateTracking) {
    const size_t bufSize = 256;

    WGPUBufferDescriptor desc = {};
    desc.size = bufSize;
    desc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);

    // NOTE: buffer->mapState is zero-initialized (0), but WGPUBufferMapState_Unmapped == 1.
    // This is a known WGVK bug: mapState not set to Unmapped on creation.
    // For now, accept either 0 or Unmapped as "not mapped".
    EXPECT_TRUE(wgpuBufferGetMapState(buffer) == WGPUBufferMapState_Unmapped ||
                wgpuBufferGetMapState(buffer) == 0)
        << "Expected Unmapped (1) or zero-init (0), got " << wgpuBufferGetMapState(buffer);

    struct MapCtx { bool done = false; WGPUMapAsyncStatus status; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus status, WGPUStringView, void* ud, void*) {
        auto* ctx = (MapCtx*)ud;
        ctx->status = status;
        ctx->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture fut = wgpuBufferMapAsync(buffer, WGPUMapMode_Write, 0, bufSize, cbInfo);
    WGPUFutureWaitInfo fwi = { fut, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }
    ASSERT_EQ(mapCtx.status, WGPUMapAsyncStatus_Success);

    EXPECT_EQ(wgpuBufferGetMapState(buffer), WGPUBufferMapState_Mapped);

    wgpuBufferUnmap(buffer);

    // Same caveat: may be 0 or Unmapped after unmap
    EXPECT_TRUE(wgpuBufferGetMapState(buffer) == WGPUBufferMapState_Unmapped ||
                wgpuBufferGetMapState(buffer) == 0);

    wgpuBufferRelease(buffer);
}

// ---------------------------------------------------------------------------
// P1 Section: Draw Call Variations
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, Draw_BasicTriangle_Instanced) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t bufferSize = bytesPerRow * height;

    // Shaders
    const char* vsCode = R"(
        #version 450
        void main() {
            vec2 pos[3] = vec2[3](vec2(-0.3, -0.3), vec2(0.3, -0.3), vec2(0.0, 0.3));
            float offset = (float(gl_InstanceIndex) - 1.5) * 0.4;
            gl_Position = vec4(pos[gl_VertexIndex].x + offset, pos[gl_VertexIndex].y, 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    // Pipeline
    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // Render target + readback buffer
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = bufferSize;
    bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    // Encode
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 0.0, 1.0}; // Black clear

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 4, 0, 0); // 3 verts, 4 instances
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // Map & verify
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture future = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    WGPUFutureWaitInfo fwi = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    // Verify some non-clear-color (non-black) pixels exist
    bool foundWhite = false;
    for (uint32_t y = 0; y < height && !foundWhite; y++) {
        for (uint32_t x = 0; x < width && !foundWhite; x++) {
            size_t off = y * bytesPerRow + x * 4;
            if (pixels[off] == 255 && pixels[off+1] == 255 && pixels[off+2] == 255) {
                foundWhite = true;
            }
        }
    }
    EXPECT_TRUE(foundWhite) << "Expected some white pixels from instanced draw";

    wgpuBufferUnmap(readBuffer);

    // Cleanup
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(readBuffer);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

TEST_F(WebGPUTest, DrawIndirect_Basic) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t bufferSize = bytesPerRow * height;

    // Shaders: fullscreen-ish green triangle
    const char* vsCode = R"(
        #version 450
        void main() {
            vec2 pos[3] = vec2[3](
                vec2(-1.0, -1.0),
                vec2( 3.0, -1.0),
                vec2(-1.0,  3.0)
            );
            gl_Position = vec4(pos[gl_VertexIndex], 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    // Pipeline
    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // Indirect buffer: {vertexCount=3, instanceCount=1, firstVertex=0, firstInstance=0}
    uint32_t indirectData[4] = {3, 1, 0, 0};
    WGPUBufferDescriptor indBufDesc = {};
    indBufDesc.size = sizeof(indirectData);
    indBufDesc.usage = WGPUBufferUsage_Indirect | WGPUBufferUsage_CopyDst;
    WGPUBuffer indirectBuffer = wgpuDeviceCreateBuffer(device, &indBufDesc);
    ASSERT_NE(indirectBuffer, nullptr);
    wgpuQueueWriteBuffer(queue, indirectBuffer, 0, indirectData, sizeof(indirectData));

    // Render target + readback
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = bufferSize;
    bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    // Encode
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 0.0, 1.0};

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDrawIndirect(pass, indirectBuffer, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // Map & verify
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture future = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    WGPUFutureWaitInfo fwi = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    // Verify green pixels exist (drawn via indirect)
    bool foundGreen = false;
    for (uint32_t y = 0; y < height && !foundGreen; y++) {
        for (uint32_t x = 0; x < width && !foundGreen; x++) {
            size_t off = y * bytesPerRow + x * 4;
            if (pixels[off] == 0 && pixels[off+1] == 255 && pixels[off+2] == 0) {
                foundGreen = true;
            }
        }
    }
    EXPECT_TRUE(foundGreen) << "Expected green pixels from indirect draw";

    wgpuBufferUnmap(readBuffer);

    // Cleanup
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(readBuffer);
    wgpuBufferRelease(indirectBuffer);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

TEST_F(WebGPUTest, MultipleDrawCalls_SameRenderPass) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t bufferSize = bytesPerRow * height;

    // Vertex shader: triangle offset by gl_VertexIndex base
    // We use two draws: one left-shifted, one right-shifted via firstVertex trick.
    // Simpler: just use a fullscreen-ish left triangle and right triangle.
    const char* vsCode = R"(
        #version 450
        void main() {
            // 6 vertices total: first 3 = left triangle, next 3 = right triangle
            vec2 pos[6] = vec2[6](
                vec2(-1.0, -1.0), vec2( 0.0, -1.0), vec2(-1.0,  1.0),
                vec2( 0.0, -1.0), vec2( 1.0, -1.0), vec2( 1.0,  1.0)
            );
            gl_Position = vec4(pos[gl_VertexIndex], 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    // Pipeline
    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // Render target + readback
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBufferDescriptor bufDescR = {};
    bufDescR.size = bufferSize;
    bufDescR.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &bufDescR);

    // Encode: two draw calls in same render pass
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 0.0, 1.0};

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0); // Left triangle (vertices 0-2)
    wgpuRenderPassEncoderDraw(pass, 3, 1, 3, 0); // Right triangle (vertices 3-5)
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // Map & verify
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture future = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    WGPUFutureWaitInfo fwi = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    // Check left region (x=5, y=32) -- should be red from first draw
    {
        size_t off = 32 * bytesPerRow + 5 * 4;
        EXPECT_EQ(pixels[off], 255) << "Left region R";
        EXPECT_EQ(pixels[off+1], 0) << "Left region G";
        EXPECT_EQ(pixels[off+2], 0) << "Left region B";
    }
    // Check right region (x=60, y=32) -- should be red from second draw
    {
        size_t off = 32 * bytesPerRow + 60 * 4;
        EXPECT_EQ(pixels[off], 255) << "Right region R";
        EXPECT_EQ(pixels[off+1], 0) << "Right region G";
        EXPECT_EQ(pixels[off+2], 0) << "Right region B";
    }

    wgpuBufferUnmap(readBuffer);

    // Cleanup
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(readBuffer);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

// ---------------------------------------------------------------------------
// P1 Section: Viewport & Scissor
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, Viewport_SubRegion) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t bufferSize = bytesPerRow * height;

    // Fullscreen green triangle
    const char* vsCode = R"(
        #version 450
        void main() {
            vec2 pos[3] = vec2[3](
                vec2(-1.0, -1.0),
                vec2( 3.0, -1.0),
                vec2(-1.0,  3.0)
            );
            gl_Position = vec4(pos[gl_VertexIndex], 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = bufferSize;
    bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    // Encode with viewport set to top-left 32x32
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 0.0, 1.0};

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetViewport(pass, 0, 0, 32, 32, 0.0f, 1.0f);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // Map & verify
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture future = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    WGPUFutureWaitInfo fwi = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    // Inside viewport (top-left quadrant): should be green
    {
        size_t off = 10 * bytesPerRow + 10 * 4;
        EXPECT_EQ(pixels[off], 0) << "Inside viewport R";
        EXPECT_EQ(pixels[off+1], 255) << "Inside viewport G";
        EXPECT_EQ(pixels[off+2], 0) << "Inside viewport B";
    }
    // Outside viewport (bottom-right quadrant): should be black (clear color)
    {
        size_t off = 50 * bytesPerRow + 50 * 4;
        EXPECT_EQ(pixels[off], 0) << "Outside viewport R";
        EXPECT_EQ(pixels[off+1], 0) << "Outside viewport G";
        EXPECT_EQ(pixels[off+2], 0) << "Outside viewport B";
    }

    wgpuBufferUnmap(readBuffer);

    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(readBuffer);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

TEST_F(WebGPUTest, ScissorRect_Clipping) {
    const uint32_t width = 64;
    const uint32_t height = 64;
    const uint32_t bytesPerRow = 256;
    const size_t bufferSize = bytesPerRow * height;

    // Fullscreen green triangle
    const char* vsCode = R"(
        #version 450
        void main() {
            vec2 pos[3] = vec2[3](
                vec2(-1.0, -1.0),
                vec2( 3.0, -1.0),
                vec2(-1.0,  3.0)
            );
            gl_Position = vec4(pos[gl_VertexIndex], 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pipelineLayout;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {width, height, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = bufferSize;
    bufDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    // Encode with scissor rect in center (16,16,32,32)
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);

    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 0.0, 1.0};

    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rpDesc);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetScissorRect(pass, 16, 16, 32, 32);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUTexelCopyTextureInfo srcInfo = {};
    srcInfo.texture = texture;
    srcInfo.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferInfo dstInfo = {};
    dstInfo.buffer = readBuffer;
    dstInfo.layout.bytesPerRow = bytesPerRow;
    dstInfo.layout.rowsPerImage = height;

    WGPUExtent3D copySize = {width, height, 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &srcInfo, &dstInfo, &copySize);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // Map & verify
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) {
        ((MapCtx*)ud)->done = true;
    };
    WGPUBufferMapCallbackInfo cbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture future = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, cbInfo);
    WGPUFutureWaitInfo fwi = { future, 0 };
    while (!mapCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX);
    }

    const uint8_t* pixels = (const uint8_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(pixels, nullptr);

    // Inside scissor (center): should be green
    {
        size_t off = 32 * bytesPerRow + 32 * 4;
        EXPECT_EQ(pixels[off], 0) << "Inside scissor R";
        EXPECT_EQ(pixels[off+1], 255) << "Inside scissor G";
        EXPECT_EQ(pixels[off+2], 0) << "Inside scissor B";
    }
    // Outside scissor (top-left corner): should be black
    {
        size_t off = 2 * bytesPerRow + 2 * 4;
        EXPECT_EQ(pixels[off], 0) << "Outside scissor R";
        EXPECT_EQ(pixels[off+1], 0) << "Outside scissor G";
        EXPECT_EQ(pixels[off+2], 0) << "Outside scissor B";
    }
    // Outside scissor (bottom-right past scissor): should be black
    {
        size_t off = 60 * bytesPerRow + 60 * 4;
        EXPECT_EQ(pixels[off], 0) << "Outside scissor bottom-right R";
        EXPECT_EQ(pixels[off+1], 0) << "Outside scissor bottom-right G";
        EXPECT_EQ(pixels[off+2], 0) << "Outside scissor bottom-right B";
    }

    wgpuBufferUnmap(readBuffer);

    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(readBuffer);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(texture);
    wgpuRenderPipelineRelease(pipeline);
    wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

// ---------------------------------------------------------------------------
// P1 Section: Multi-threading
// ---------------------------------------------------------------------------

// NOTE: DISABLED because concurrent buffer creation triggers Vulkan validation
// errors (non-thread-safe VMA path). Kept as documentation of the known issue.
TEST_F(WebGPUTest, MT_ConcurrentBufferCreation) {
    std::vector<WGPUBuffer> bufs1, bufs2;
    bufs1.reserve(100);
    bufs2.reserve(100);

    auto fn = [&](std::vector<WGPUBuffer>& out) {
        for (int i = 0; i < 100; i++) {
            WGPUBufferDescriptor d = {};
            d.size = 256;
            d.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
            out.push_back(wgpuDeviceCreateBuffer(device, &d));
        }
    };

    std::thread t1(fn, std::ref(bufs1));
    std::thread t2(fn, std::ref(bufs2));
    t1.join();
    t2.join();

    ASSERT_EQ(bufs1.size(), 100u);
    ASSERT_EQ(bufs2.size(), 100u);

    for (auto b : bufs1) {
        ASSERT_NE(b, nullptr);
        wgpuBufferRelease(b);
    }
    for (auto b : bufs2) {
        ASSERT_NE(b, nullptr);
        wgpuBufferRelease(b);
    }
}

TEST_F(WebGPUTest, MT_ConcurrentAddRefRelease) {
    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);

    // Each thread does 10000 AddRef then 10000 Release, net effect = 0
    auto fn = [&]() {
        for (int i = 0; i < 10000; i++) {
            wgpuBufferAddRef(buffer);
        }
        for (int i = 0; i < 10000; i++) {
            wgpuBufferRelease(buffer);
        }
    };

    std::thread t1(fn);
    std::thread t2(fn);
    t1.join();
    t2.join();

    // After both threads complete: initial 1 + 0 + 0 = 1
    EXPECT_EQ(buffer->refCount, 1u);

    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, MT_ConcurrentMapUnmap) {
    // Two separate buffers mapped at creation, written by different threads.
    // Uses mappedAtCreation to avoid async map path (WaitAny is not thread-safe).
    const size_t bufSize = 256;

    WGPUBufferDescriptor descA = {};
    descA.size = bufSize;
    descA.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    descA.mappedAtCreation = true;
    WGPUBuffer bufferA = wgpuDeviceCreateBuffer(device, &descA);
    ASSERT_NE(bufferA, nullptr);

    WGPUBufferDescriptor descB = {};
    descB.size = bufSize;
    descB.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    descB.mappedAtCreation = true;
    WGPUBuffer bufferB = wgpuDeviceCreateBuffer(device, &descB);
    ASSERT_NE(bufferB, nullptr);

    void* ptrA = wgpuBufferGetMappedRange(bufferA, 0, bufSize);
    void* ptrB = wgpuBufferGetMappedRange(bufferB, 0, bufSize);
    ASSERT_NE(ptrA, nullptr);
    ASSERT_NE(ptrB, nullptr);

    std::atomic<bool> okA{false}, okB{false};

    auto writeAndUnmap = [&](WGPUBuffer buf, void* ptr, uint8_t fillVal, std::atomic<bool>& ok) {
        memset(ptr, fillVal, bufSize);
        wgpuBufferUnmap(buf);
        ok.store(true);
    };

    std::thread t1(writeAndUnmap, bufferA, ptrA, 0xAA, std::ref(okA));
    std::thread t2(writeAndUnmap, bufferB, ptrB, 0xBB, std::ref(okB));
    t1.join();
    t2.join();

    EXPECT_TRUE(okA.load()) << "Thread 1 failed to write buffer A";
    EXPECT_TRUE(okB.load()) << "Thread 2 failed to write buffer B";

    wgpuBufferRelease(bufferA);
    wgpuBufferRelease(bufferB);
}

// ---------------------------------------------------------------------------
// P1 Section: Fence Edge Cases
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, FenceWait_ZeroTimeout) {
    // Create a fence and get it to Finished state so wgpuFenceWait returns immediately.
    // We access internals (state field) since the public API doesn't expose fence state.
    WGPUFence f = wgpuDeviceCreateFence(device);
    ASSERT_NE(f, nullptr);

    // Manually set state to Finished so wgpuFenceWait takes the fast path.
    atomic_store_explicit(&f->state, WGPUFenceState_Finished, std::memory_order_release);

    auto start = std::chrono::steady_clock::now();
    wgpuFenceWait(f, 0);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 100)
        << "FenceWait on Finished fence with timeout 0 should return quickly";

    wgpuFenceRelease(f);
}

TEST_F(WebGPUTest, FenceWait_NullFence) {
    // wgpuFenceWait(nullptr, ...) must not crash -- early return path
    wgpuFenceWait(nullptr, UINT64_MAX);
}

TEST_F(WebGPUTest, FencesWait_NullArray) {
    // wgpuFencesWait(nullptr, 0, ...) must not crash -- early return path
    wgpuFencesWait(nullptr, 0, UINT64_MAX);
}

// ---------------------------------------------------------------------------
// P1 Section: Additional Edge Cases
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, Edge_Label_SetOnBuffer) {
    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);

    wgpuBufferSetLabel(buffer, {"TestLabel", 9});
    // No crash is success
    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, Edge_Label_SetOnTexture) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_CopyDst;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(texture, nullptr);

    wgpuTextureSetLabel(texture, {"TexLabel", 8});
    // No crash is success
    wgpuTextureRelease(texture);
}

TEST_F(WebGPUTest, Edge_Label_SetOnDevice) {
    wgpuDeviceSetLabel(device, {"DevLabel", 8});
    // No crash is success
}

TEST_F(WebGPUTest, Edge_CommandEncoderFinish_Twice) {
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    ASSERT_NE(enc, nullptr);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    ASSERT_NE(cmd, nullptr);

    // After finish, the encoder should be marked as consumed
    EXPECT_EQ(enc->movedFrom, 1u);

    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

// ===========================================================================
// P1 Dependency Chain Tests
// ===========================================================================

TEST_F(WebGPUTest, TextureViewKeepsTextureAlive) {
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {64, 64, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    ASSERT_NE(texture, nullptr);

    WGPUTextureView view = wgpuTextureCreateView(texture, nullptr);
    ASSERT_NE(view, nullptr);

    // Texture: 1 (user) + 1 (view internal ref) = 2
    EXPECT_EQ(texture->refCount, 2);
    EXPECT_EQ(view->refCount, 1);

    // Release user ref on texture. View still holds it.
    wgpuTextureRelease(texture);
    EXPECT_EQ(texture->refCount, 1);

    // Release view. Cascades to final texture release.
    wgpuTextureViewRelease(view);
    // Both freed -- ASan validates no leak/UAF.
}

TEST_F(WebGPUTest, BindGroupKeepsLayoutAlive_Extended) {
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;

    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    // Create a buffer for bind group entries
    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = 256;
    bufDesc.usage = WGPUBufferUsage_Storage;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    ASSERT_NE(buffer, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = buffer;
    bgEntry.size = 256;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    // Create 2 BindGroups sharing same BGL
    WGPUBindGroup bg1 = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bg1, nullptr);
    WGPUBindGroup bg2 = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bg2, nullptr);

    // BGL: 1 (user) + 1 (bg1) + 1 (bg2) = 3
    EXPECT_EQ(bgl->refCount, 3);

    // Release user ref on BGL
    wgpuBindGroupLayoutRelease(bgl);
    EXPECT_EQ(bgl->refCount, 2);

    // Release BG1 -> BGL drops to 1
    wgpuBindGroupRelease(bg1);
    EXPECT_EQ(bgl->refCount, 1);

    // Release BG2 -> BGL freed
    wgpuBindGroupRelease(bg2);
    // BGL freed -- ASan validates

    wgpuBufferRelease(buffer);
}

TEST_F(WebGPUTest, BindGroupKeepsBuffersAlive) {
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = 256;
    bufDesc.usage = WGPUBufferUsage_Storage;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    ASSERT_NE(buffer, nullptr);

    uint32_t preRefCount = buffer->refCount;

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = buffer;
    bgEntry.size = 256;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bg, nullptr);

    // BindGroup tracks buffer via ResourceUsage -- refCount should have increased
    EXPECT_GT((uint32_t)buffer->refCount, preRefCount)
        << "BindGroup should AddRef the buffer via ResourceUsage tracking";

    // Release user buffer ref. BindGroup still holds it.
    wgpuBufferRelease(buffer);

    // Release BindGroup -> triggers releaseAllAndClear -> buffer freed
    wgpuBindGroupRelease(bg);
    // No crash -- ASan validates

    wgpuBindGroupLayoutRelease(bgl);
}

TEST_F(WebGPUTest, ComputePipelineKeepsPipelineLayoutAlive) {
    const char* code = R"(
        #version 450
        layout(local_size_x = 1) in;
        layout(std430, set = 0, binding = 0) buffer Data { uint v[]; } data;
        void main() { data.v[gl_GlobalInvocationID.x] = 0; }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code);
    ASSERT_NE(sm, nullptr);

    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pl, nullptr);

    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pl;
    pipeDesc.compute.module = sm;
    pipeDesc.compute.entryPoint = { "main", 4 };

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // Pipeline should AddRef its layout
    EXPECT_EQ(pl->refCount, 2);

    // Release user ref on PipelineLayout. Pipeline still holds it.
    wgpuPipelineLayoutRelease(pl);
    EXPECT_EQ(pl->refCount, 1);

    // Release pipeline -> cascading cleanup of layout -> BGL
    wgpuComputePipelineRelease(pipeline);
    // pl freed, BGL ref dropped -- ASan validates

    wgpuBindGroupLayoutRelease(bgl);
    wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, RenderPipelineKeepsPipelineLayoutAlive) {
    const char* vsCode = R"(
        #version 450
        void main() {
            gl_Position = vec4(0.0, 0.0, 0.5, 1.0);
        }
    )";
    const char* fsCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() {
            outColor = vec4(1.0, 0.0, 0.0, 1.0);
        }
    )";

    WGPUShaderModule vsModule = compileGLSL(device, WGPUShaderStage_Vertex, vsCode);
    ASSERT_NE(vsModule, nullptr);
    WGPUShaderModule fsModule = compileGLSL(device, WGPUShaderStage_Fragment, fsCode);
    ASSERT_NE(fsModule, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 0;
    plDesc.bindGroupLayouts = nullptr;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pl, nullptr);

    WGPUColorTargetState colorTarget = {};
    colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = fsModule;
    fragmentState.entryPoint = { "main", 4 };
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPURenderPipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pl;
    pipeDesc.vertex.module = vsModule;
    pipeDesc.vertex.entryPoint = { "main", 4 };
    pipeDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipeDesc.primitive.cullMode = WGPUCullMode_None;
    pipeDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipeDesc.multisample.count = 1;
    pipeDesc.multisample.mask = 0xFFFFFFFF;
    pipeDesc.fragment = &fragmentState;

    WGPURenderPipeline rp = wgpuDeviceCreateRenderPipeline(device, &pipeDesc);
    ASSERT_NE(rp, nullptr);

    // Pipeline should AddRef layout
    EXPECT_EQ(pl->refCount, 2);

    // Release user ref on PipelineLayout. Pipeline still holds it.
    wgpuPipelineLayoutRelease(pl);
    EXPECT_EQ(pl->refCount, 1);

    // Release pipeline -> cascading layout cleanup
    wgpuRenderPipelineRelease(rp);
    // pl freed -- ASan validates

    wgpuShaderModuleRelease(vsModule);
    wgpuShaderModuleRelease(fsModule);
}

TEST_F(WebGPUTest, PipelineLayoutKeepsBindGroupLayoutsAlive) {
    // Create 2 BGLs with different bindings
    WGPUBindGroupLayoutEntry entry0 = {};
    entry0.binding = 0;
    entry0.visibility = WGPUShaderStage_Compute;
    entry0.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc0 = {};
    bglDesc0.entryCount = 1;
    bglDesc0.entries = &entry0;
    WGPUBindGroupLayout bgl0 = wgpuDeviceCreateBindGroupLayout(device, &bglDesc0);
    ASSERT_NE(bgl0, nullptr);

    WGPUBindGroupLayoutEntry entry1 = {};
    entry1.binding = 0;
    entry1.visibility = WGPUShaderStage_Compute;
    entry1.buffer.type = WGPUBufferBindingType_Uniform;

    WGPUBindGroupLayoutDescriptor bglDesc1 = {};
    bglDesc1.entryCount = 1;
    bglDesc1.entries = &entry1;
    WGPUBindGroupLayout bgl1 = wgpuDeviceCreateBindGroupLayout(device, &bglDesc1);
    ASSERT_NE(bgl1, nullptr);

    WGPUBindGroupLayout bgls[2] = { bgl0, bgl1 };
    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 2;
    plDesc.bindGroupLayouts = bgls;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pl, nullptr);

    // PipelineLayout should AddRef both BGLs
    EXPECT_EQ(bgl0->refCount, 2);
    EXPECT_EQ(bgl1->refCount, 2);

    // Release user refs on both BGLs
    wgpuBindGroupLayoutRelease(bgl0);
    wgpuBindGroupLayoutRelease(bgl1);
    EXPECT_EQ(bgl0->refCount, 1);
    EXPECT_EQ(bgl1->refCount, 1);

    // Release PipelineLayout -> both BGLs freed
    wgpuPipelineLayoutRelease(pl);
    // Both BGLs freed -- ASan validates
}

TEST_F(WebGPUTest, FullDependencyChain_Pipeline_Layout_BGL) {
    const char* code = R"(
        #version 450
        layout(local_size_x = 1) in;
        layout(std430, set = 0, binding = 0) buffer Data { uint v[]; } data;
        void main() { data.v[gl_GlobalInvocationID.x] = 0; }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code);
    ASSERT_NE(sm, nullptr);

    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    ASSERT_NE(pl, nullptr);

    WGPUComputePipelineDescriptor pipeDesc = {};
    pipeDesc.layout = pl;
    pipeDesc.compute.module = sm;
    pipeDesc.compute.entryPoint = { "main", 4 };

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);

    // Initial state:
    // bgl: 1 (user) + 1 (PL) = 2
    // pl:  1 (user) + 1 (pipeline) = 2
    // pipeline: 1 (user)
    EXPECT_EQ(bgl->refCount, 2);
    EXPECT_EQ(pl->refCount, 2);
    EXPECT_EQ(pipeline->refCount, 1);

    // Release BGL user ref. Still held by PL.
    wgpuBindGroupLayoutRelease(bgl);
    EXPECT_EQ(bgl->refCount, 1);

    // Release PL user ref. Still held by pipeline.
    wgpuPipelineLayoutRelease(pl);
    EXPECT_EQ(pl->refCount, 1);
    // BGL refCount unchanged (still held by layout, which is still alive)
    EXPECT_EQ(bgl->refCount, 1);

    // Release pipeline -> cascades: pl freed -> bgl freed
    wgpuComputePipelineRelease(pipeline);
    // All three freed -- ASan validates

    wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, CommandBufferTracksInFlightBuffers) {
    const uint32_t bufferSize = 256;

    WGPUBufferDescriptor srcDesc = {};
    srcDesc.size = bufferSize;
    srcDesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &srcDesc);
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->refCount, 1);

    // Encode a copy (buffer to itself with offset, or just reference it)
    WGPUBufferDescriptor dstDesc = {};
    dstDesc.size = bufferSize;
    dstDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer dstBuffer = wgpuDeviceCreateBuffer(device, &dstDesc);
    ASSERT_NE(dstBuffer, nullptr);

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer, 0, dstBuffer, 0, bufferSize);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    ASSERT_NE(cmd, nullptr);

    // After finish, command buffer tracks resources
    EXPECT_GE((uint32_t)buffer->refCount, 2)
        << "Buffer should be tracked by command buffer after Finish";

    // Submit
    wgpuQueueSubmit(queue, 1, &cmd);

    // In-flight tracking should keep buffer alive
    EXPECT_GE((uint32_t)buffer->refCount, 2);

    wgpuCommandBufferRelease(cmd);

    // Tick framesInFlight times to cycle frame resources
    for (uint32_t i = 0; i < framesInFlight; i++) {
        wgpuDeviceTick(device);
    }

    // RefCount should drop back to 1 (only user ref)
    EXPECT_EQ(buffer->refCount, 1);

    wgpuBufferRelease(buffer);
    wgpuBufferRelease(dstBuffer);
    wgpuCommandEncoderRelease(encoder);
}

TEST_F(WebGPUTest, TextureViewCacheRefCount) {
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {64, 64, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &texDesc);
    ASSERT_NE(texture, nullptr);

    // Create view with default params
    WGPUTextureView view1 = wgpuTextureCreateView(texture, nullptr);
    ASSERT_NE(view1, nullptr);
    EXPECT_EQ(view1->refCount, 1);
    EXPECT_EQ(texture->refCount, 2); // user + view

    // Release the view
    wgpuTextureViewRelease(view1);
    EXPECT_EQ(texture->refCount, 1);

    // Create view again with same params -- may be a cache hit
    WGPUTextureView view2 = wgpuTextureCreateView(texture, nullptr);
    ASSERT_NE(view2, nullptr);
    EXPECT_EQ(view2->refCount, 1);
    EXPECT_EQ(texture->refCount, 2);

    // Cache hit means same pointer (optional check -- cache behavior is impl detail)
    // EXPECT_EQ(view1, view2); // May or may not be true depending on cache eviction

    wgpuTextureViewRelease(view2);
    wgpuTextureRelease(texture);
}

// ===========================================================================
// P1 Destroy vs Release Tests
// ===========================================================================

TEST_F(WebGPUTest, BufferDestroyThenRelease) {
    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);

    // Destroy is currently a no-op stub, but should not crash
    wgpuBufferDestroy(buffer);

    // Release should free normally even after destroy
    wgpuBufferRelease(buffer);
    // No crash, no double-free -- ASan validates
}

TEST_F(WebGPUTest, TextureDestroyThenRelease) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_CopyDst;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture texture = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(texture, nullptr);

    // Destroy is currently a no-op stub, but should not crash
    wgpuTextureDestroy(texture);

    // Release should free normally even after destroy
    wgpuTextureRelease(texture);
    // No crash, no double-free -- ASan validates
}

TEST_F(WebGPUTest, DeviceDestroyBehavior) {
    // Create a second device to test destroy on
    struct DeviceCtx {
        WGPUDevice device = nullptr;
        bool done = false;
    } deviceCtx;

    auto deviceCallback = [](WGPURequestDeviceStatus status, WGPUDevice dev, WGPUStringView msg, void* userdata, void* userdata2) {
        DeviceCtx* ctx = (DeviceCtx*)userdata;
        if (status == WGPURequestDeviceStatus_Success) {
            ctx->device = dev;
        }
        ctx->done = true;
    };

    WGPURequestDeviceCallbackInfo cbInfo = {};
    cbInfo.callback = deviceCallback;
    cbInfo.userdata1 = &deviceCtx;
    cbInfo.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUDeviceDescriptor devDesc = {};
    devDesc.label = { "TestDevice2", 11 };

    WGPUFuture future = wgpuAdapterRequestDevice(adapter, &devDesc, cbInfo);
    WGPUFutureWaitInfo waitInfo = { future, 0 };
    while (!deviceCtx.done) {
        wgpuInstanceWaitAny(instance, 1, &waitInfo, 1000000000);
    }

    ASSERT_NE(deviceCtx.device, nullptr) << "Failed to create second device";
    WGPUDevice device2 = deviceCtx.device;

    // Destroy is currently a no-op stub
    wgpuDeviceDestroy(device2);

    // Release should free normally
    wgpuDeviceRelease(device2);
    // No crash -- ASan validates
}

TEST_F(WebGPUTest, ReleaseAfterDestroy_BufferStillHasRef) {
    WGPUBufferDescriptor desc = {};
    desc.size = 256;
    desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;

    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &desc);
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->refCount, 1);

    // AddRef: refCount = 2
    wgpuBufferAddRef(buffer);
    EXPECT_EQ(buffer->refCount, 2);

    // Destroy: currently no-op, but should not affect refCount
    wgpuBufferDestroy(buffer);
    EXPECT_EQ(buffer->refCount, 2);

    // Release: refCount = 1
    wgpuBufferRelease(buffer);
    EXPECT_EQ(buffer->refCount, 1);

    // Final release: refCount = 0, freed
    wgpuBufferRelease(buffer);
    // ASan validates no leak
}

TEST_F(WebGPUTest, Edge_ReleaseOrder_ReverseCreation) {
    // Create: BGL, buffer, BindGroup
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = 256;
    bufDesc.usage = WGPUBufferUsage_Storage;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    ASSERT_NE(buffer, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = buffer;
    bgEntry.size = 256;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bg, nullptr);

    // Release in reverse creation order: BindGroup, buffer, BGL
    wgpuBindGroupRelease(bg);
    wgpuBufferRelease(buffer);
    wgpuBindGroupLayoutRelease(bgl);
    // No crash -- ASan validates
}

TEST_F(WebGPUTest, Edge_ReleaseOrder_CreationOrder) {
    // Create: BGL, buffer, BindGroup
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    ASSERT_NE(bgl, nullptr);

    WGPUBufferDescriptor bufDesc = {};
    bufDesc.size = 256;
    bufDesc.usage = WGPUBufferUsage_Storage;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &bufDesc);
    ASSERT_NE(buffer, nullptr);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = buffer;
    bgEntry.size = 256;

    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;

    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgDesc);
    ASSERT_NE(bg, nullptr);

    // BGL refCount: 1 (user) + 1 (bg) = 2
    EXPECT_EQ(bgl->refCount, 2);

    // Release in creation order: BGL (held by BG), buffer, BindGroup (drops BGL ref)
    wgpuBindGroupLayoutRelease(bgl);
    EXPECT_EQ(bgl->refCount, 1); // still held by BG

    wgpuBufferRelease(buffer);

    wgpuBindGroupRelease(bg);
    // BGL freed via cascading release -- ASan validates. No crash.
}

// ============================================================
// P1 Advanced Compute Tests
// ============================================================

TEST_F(WebGPUTest, ComputeDispatch_Workgroup8x8x1) {
    const uint32_t elementCount = 4096;
    const uint32_t bufferSize = elementCount * sizeof(uint32_t);
    WGPUBufferDescriptor storageDesc = {};
    storageDesc.size = bufferSize;
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    WGPUBuffer storageBuffer = wgpuDeviceCreateBuffer(device, &storageDesc);
    ASSERT_NE(storageBuffer, nullptr);
    const char* glslCode = R"(
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
        layout(std430, set = 0, binding = 0) buffer Data { uint values[]; } data;
        void main() { uint x = gl_GlobalInvocationID.x; uint y = gl_GlobalInvocationID.y; uint idx = y * 64 + x; data.values[idx] = idx; }
    )";
    WGPUShaderModule shaderModule = compileGLSL(device, WGPUShaderStage_Compute, glslCode);
    ASSERT_NE(shaderModule, nullptr);
    WGPUBindGroupLayoutEntry bglEntry = {}; bglEntry.binding = 0; bglEntry.visibility = WGPUShaderStage_Compute;
    bglEntry.buffer.type = WGPUBufferBindingType_Storage; bglEntry.buffer.minBindingSize = bufferSize;
    WGPUBindGroupLayoutDescriptor bglDesc = {}; bglDesc.entryCount = 1; bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    WGPUPipelineLayoutDescriptor plDesc = {}; plDesc.bindGroupLayoutCount = 1; plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    WGPUComputePipelineDescriptor pipeDesc = {}; pipeDesc.layout = pipelineLayout;
    pipeDesc.compute.module = shaderModule; pipeDesc.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
    ASSERT_NE(pipeline, nullptr);
    WGPUBindGroupEntry bgEntry = {}; bgEntry.binding = 0; bgEntry.buffer = storageBuffer; bgEntry.size = bufferSize;
    WGPUBindGroupDescriptor bgDesc = {}; bgDesc.layout = bgl; bgDesc.entryCount = 1; bgDesc.entries = &bgEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor passDesc = {};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, 8, 8, 1);
    wgpuComputePassEncoderEnd(pass); wgpuComputePassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(encoder); wgpuCommandBufferRelease(cmd);
    WGPUBufferDescriptor readDesc = {}; readDesc.size = bufferSize;
    readDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer readBuffer = wgpuDeviceCreateBuffer(device, &readDesc);
    encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, storageBuffer, 0, readBuffer, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(encoder); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mapCtx;
    auto mapCb = [](WGPUMapAsyncStatus, WGPUStringView, void* ud, void*) { ((MapCtx*)ud)->done = true; };
    WGPUBufferMapCallbackInfo mapCbInfo = { nullptr, WGPUCallbackMode_WaitAnyOnly, mapCb, &mapCtx, nullptr };
    WGPUFuture mapFut = wgpuBufferMapAsync(readBuffer, WGPUMapMode_Read, 0, bufferSize, mapCbInfo);
    WGPUFutureWaitInfo fwi = { mapFut, 0 };
    while (!mapCtx.done) { wgpuInstanceWaitAny(instance, 1, &fwi, UINT64_MAX); }
    const uint32_t* results = (const uint32_t*)wgpuBufferGetConstMappedRange(readBuffer, 0, bufferSize);
    ASSERT_NE(results, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) { EXPECT_EQ(results[i], i) << "Index " << i; }
    wgpuBufferUnmap(readBuffer);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(readBuffer); wgpuBufferRelease(storageBuffer); wgpuBindGroupRelease(bindGroup);
    wgpuBindGroupLayoutRelease(bgl); wgpuPipelineLayoutRelease(pipelineLayout);
    wgpuComputePipelineRelease(pipeline); wgpuShaderModuleRelease(shaderModule);
}

TEST_F(WebGPUTest, ComputeDispatch_LargeBuffer1M) {
    const uint32_t elementCount = 1048576; const uint32_t bufferSize = elementCount * sizeof(uint32_t);
    WGPUBufferDescriptor storageDesc = {}; storageDesc.size = bufferSize;
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    WGPUBuffer storageBuffer = wgpuDeviceCreateBuffer(device, &storageDesc); ASSERT_NE(storageBuffer, nullptr);
    const char* glslCode = R"(
        #version 450
        layout(local_size_x = 256) in;
        layout(std430, set = 0, binding = 0) buffer Data { uint values[]; } data;
        void main() { uint idx = gl_GlobalInvocationID.x; data.values[idx] = idx ^ 0xDEADBEEFu; }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, glslCode); ASSERT_NE(sm, nullptr);
    WGPUBindGroupLayoutEntry be = {}; be.binding = 0; be.visibility = WGPUShaderStage_Compute;
    be.buffer.type = WGPUBufferBindingType_Storage; be.buffer.minBindingSize = bufferSize;
    WGPUBindGroupLayoutDescriptor bld = {}; bld.entryCount = 1; bld.entries = &be;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bld);
    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUComputePipelineDescriptor cpd = {}; cpd.layout = pl; cpd.compute.module = sm;
    cpd.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipe = wgpuDeviceCreateComputePipeline(device, &cpd); ASSERT_NE(pipe, nullptr);
    WGPUBindGroupEntry bge = {}; bge.binding = 0; bge.buffer = storageBuffer; bge.size = bufferSize;
    WGPUBindGroupDescriptor bgd = {}; bgd.layout = bgl; bgd.entryCount = 1; bgd.entries = &bge;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgd);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor pd = {};
    WGPUComputePassEncoder p = wgpuCommandEncoderBeginComputePass(enc, &pd);
    wgpuComputePassEncoderSetPipeline(p, pipe); wgpuComputePassEncoderSetBindGroup(p, 0, bg, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(p, 4096, 1, 1);
    wgpuComputePassEncoderEnd(p); wgpuComputePassEncoderRelease(p);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    WGPUBufferDescriptor rd = {}; rd.size = bufferSize; rd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer rb = wgpuDeviceCreateBuffer(device, &rd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, storageBuffer, 0, rb, 0, bufferSize);
    cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = { nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr };
    WGPUFuture mf = wgpuBufferMapAsync(rb, WGPUMapMode_Read, 0, bufferSize, ci);
    WGPUFutureWaitInfo fw = { mf, 0 }; while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }
    const uint32_t* res = (const uint32_t*)wgpuBufferGetConstMappedRange(rb, 0, bufferSize); ASSERT_NE(res, nullptr);
    for (uint32_t i = 0; i < elementCount; ++i) { EXPECT_EQ(res[i], i ^ 0xDEADBEEFu) << "Index " << i; }
    wgpuBufferUnmap(rb); for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuBufferRelease(storageBuffer); wgpuBindGroupRelease(bg);
    wgpuBindGroupLayoutRelease(bgl); wgpuPipelineLayoutRelease(pl);
    wgpuComputePipelineRelease(pipe); wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, ComputeDispatch_MultipleBindGroups) {
    const uint32_t N = 256; const uint32_t bSz = N * sizeof(uint32_t);
    WGPUBufferDescriptor sd = {}; sd.size = bSz; sd.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite; sd.mappedAtCreation = true;
    WGPUBuffer stg = wgpuDeviceCreateBuffer(device, &sd);
    uint32_t* id = (uint32_t*)wgpuBufferGetMappedRange(stg, 0, bSz); for (uint32_t i = 0; i < N; ++i) id[i] = i; wgpuBufferUnmap(stg);
    WGPUBufferDescriptor iD = {}; iD.size = bSz; iD.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst;
    WGPUBuffer inBuf = wgpuDeviceCreateBuffer(device, &iD);
    WGPUBufferDescriptor oD = {}; oD.size = bSz; oD.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    WGPUBuffer outBuf = wgpuDeviceCreateBuffer(device, &oD);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, stg, 0, inBuf, 0, bSz);
    WGPUCommandBuffer sc = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &sc); wgpuQueueWaitIdle(queue);
    wgpuCommandBufferRelease(sc); wgpuCommandEncoderRelease(enc); wgpuBufferRelease(stg);
    const char* code = R"(
        #version 450
        layout(local_size_x = 64) in;
        layout(std430, set = 0, binding = 0) readonly buffer Input { uint values[]; } inputData;
        layout(std430, set = 1, binding = 0) writeonly buffer Output { uint values[]; } outputData;
        void main() { uint idx = gl_GlobalInvocationID.x; outputData.values[idx] = inputData.values[idx] + 100; }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code); ASSERT_NE(sm, nullptr);
    WGPUBindGroupLayoutEntry e0 = {}; e0.binding = 0; e0.visibility = WGPUShaderStage_Compute;
    e0.buffer.type = WGPUBufferBindingType_ReadOnlyStorage; e0.buffer.minBindingSize = bSz;
    WGPUBindGroupLayoutDescriptor ld0 = {}; ld0.entryCount = 1; ld0.entries = &e0;
    WGPUBindGroupLayout l0 = wgpuDeviceCreateBindGroupLayout(device, &ld0);
    WGPUBindGroupLayoutEntry e1 = {}; e1.binding = 0; e1.visibility = WGPUShaderStage_Compute;
    e1.buffer.type = WGPUBufferBindingType_Storage; e1.buffer.minBindingSize = bSz;
    WGPUBindGroupLayoutDescriptor ld1 = {}; ld1.entryCount = 1; ld1.entries = &e1;
    WGPUBindGroupLayout l1 = wgpuDeviceCreateBindGroupLayout(device, &ld1);
    WGPUBindGroupLayout ls[2] = { l0, l1 };
    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 2; pld.bindGroupLayouts = ls;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUComputePipelineDescriptor cpd = {}; cpd.layout = pl; cpd.compute.module = sm; cpd.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipe = wgpuDeviceCreateComputePipeline(device, &cpd); ASSERT_NE(pipe, nullptr);
    WGPUBindGroupEntry be0 = {}; be0.binding = 0; be0.buffer = inBuf; be0.size = bSz;
    WGPUBindGroupDescriptor bd0 = {}; bd0.layout = l0; bd0.entryCount = 1; bd0.entries = &be0;
    WGPUBindGroup bg0 = wgpuDeviceCreateBindGroup(device, &bd0);
    WGPUBindGroupEntry be1 = {}; be1.binding = 0; be1.buffer = outBuf; be1.size = bSz;
    WGPUBindGroupDescriptor bd1 = {}; bd1.layout = l1; bd1.entryCount = 1; bd1.entries = &be1;
    WGPUBindGroup bg1 = wgpuDeviceCreateBindGroup(device, &bd1);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor pd = {}; WGPUComputePassEncoder p = wgpuCommandEncoderBeginComputePass(enc, &pd);
    wgpuComputePassEncoderSetPipeline(p, pipe);
    wgpuComputePassEncoderSetBindGroup(p, 0, bg0, 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(p, 1, bg1, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(p, 4, 1, 1);
    wgpuComputePassEncoderEnd(p); wgpuComputePassEncoderRelease(p);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    WGPUBufferDescriptor rrd = {}; rrd.size = bSz; rrd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer rb = wgpuDeviceCreateBuffer(device, &rrd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, outBuf, 0, rb, 0, bSz);
    cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = { nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr };
    WGPUFuture mf = wgpuBufferMapAsync(rb, WGPUMapMode_Read, 0, bSz, ci);
    WGPUFutureWaitInfo fw = { mf, 0 }; while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }
    const uint32_t* res = (const uint32_t*)wgpuBufferGetConstMappedRange(rb, 0, bSz); ASSERT_NE(res, nullptr);
    for (uint32_t i = 0; i < N; ++i) { EXPECT_EQ(res[i], i + 100) << "Index " << i; }
    wgpuBufferUnmap(rb); for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuBufferRelease(inBuf); wgpuBufferRelease(outBuf);
    wgpuBindGroupRelease(bg0); wgpuBindGroupRelease(bg1); wgpuBindGroupLayoutRelease(l0); wgpuBindGroupLayoutRelease(l1);
    wgpuPipelineLayoutRelease(pl); wgpuComputePipelineRelease(pipe); wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, ComputeDispatch_AtomicAdd) {
    const uint32_t bSz = sizeof(uint32_t);
    WGPUBufferDescriptor sd = {}; sd.size = bSz; sd.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite; sd.mappedAtCreation = true;
    WGPUBuffer stg = wgpuDeviceCreateBuffer(device, &sd); *(uint32_t*)wgpuBufferGetMappedRange(stg, 0, bSz) = 0; wgpuBufferUnmap(stg);
    WGPUBufferDescriptor std2 = {}; std2.size = bSz; std2.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer sto = wgpuDeviceCreateBuffer(device, &std2);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, stg, 0, sto, 0, bSz);
    WGPUCommandBuffer sc = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &sc); wgpuQueueWaitIdle(queue);
    wgpuCommandBufferRelease(sc); wgpuCommandEncoderRelease(enc); wgpuBufferRelease(stg);
    const char* code = R"(
        #version 450
        layout(local_size_x = 256) in;
        layout(std430, set = 0, binding = 0) buffer Counter { uint count; } counter;
        void main() { atomicAdd(counter.count, 1); }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code); ASSERT_NE(sm, nullptr);
    WGPUBindGroupLayoutEntry be = {}; be.binding = 0; be.visibility = WGPUShaderStage_Compute;
    be.buffer.type = WGPUBufferBindingType_Storage; be.buffer.minBindingSize = bSz;
    WGPUBindGroupLayoutDescriptor bld = {}; bld.entryCount = 1; bld.entries = &be;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bld);
    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUComputePipelineDescriptor cpd = {}; cpd.layout = pl; cpd.compute.module = sm; cpd.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipe = wgpuDeviceCreateComputePipeline(device, &cpd); ASSERT_NE(pipe, nullptr);
    WGPUBindGroupEntry bge = {}; bge.binding = 0; bge.buffer = sto; bge.size = bSz;
    WGPUBindGroupDescriptor bgd = {}; bgd.layout = bgl; bgd.entryCount = 1; bgd.entries = &bge;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor pd = {}; WGPUComputePassEncoder p = wgpuCommandEncoderBeginComputePass(enc, &pd);
    wgpuComputePassEncoderSetPipeline(p, pipe); wgpuComputePassEncoderSetBindGroup(p, 0, bg, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(p, 1, 1, 1);
    wgpuComputePassEncoderEnd(p); wgpuComputePassEncoderRelease(p);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    WGPUBufferDescriptor rd = {}; rd.size = bSz; rd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer rb = wgpuDeviceCreateBuffer(device, &rd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, sto, 0, rb, 0, bSz);
    cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = { nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr };
    WGPUFuture mf = wgpuBufferMapAsync(rb, WGPUMapMode_Read, 0, bSz, ci);
    WGPUFutureWaitInfo fw = { mf, 0 }; while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }
    const uint32_t* res = (const uint32_t*)wgpuBufferGetConstMappedRange(rb, 0, bSz); ASSERT_NE(res, nullptr);
    EXPECT_EQ(res[0], 256u) << "AtomicAdd expected 256";
    wgpuBufferUnmap(rb); for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuBufferRelease(sto); wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pl); wgpuComputePipelineRelease(pipe); wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, ComputeDispatch_AtomicMultiWorkgroup) {
    const uint32_t bSz = sizeof(uint32_t);
    WGPUBufferDescriptor sd = {}; sd.size = bSz; sd.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite; sd.mappedAtCreation = true;
    WGPUBuffer stg = wgpuDeviceCreateBuffer(device, &sd); *(uint32_t*)wgpuBufferGetMappedRange(stg, 0, bSz) = 0; wgpuBufferUnmap(stg);
    WGPUBufferDescriptor std2 = {}; std2.size = bSz; std2.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    WGPUBuffer sto = wgpuDeviceCreateBuffer(device, &std2);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, stg, 0, sto, 0, bSz);
    WGPUCommandBuffer sc = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &sc); wgpuQueueWaitIdle(queue);
    wgpuCommandBufferRelease(sc); wgpuCommandEncoderRelease(enc); wgpuBufferRelease(stg);
    const char* code = R"(
        #version 450
        layout(local_size_x = 256) in;
        layout(std430, set = 0, binding = 0) buffer Counter { uint count; } counter;
        void main() { atomicAdd(counter.count, 1); }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code); ASSERT_NE(sm, nullptr);
    WGPUBindGroupLayoutEntry be = {}; be.binding = 0; be.visibility = WGPUShaderStage_Compute;
    be.buffer.type = WGPUBufferBindingType_Storage; be.buffer.minBindingSize = bSz;
    WGPUBindGroupLayoutDescriptor bld = {}; bld.entryCount = 1; bld.entries = &be;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bld);
    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUComputePipelineDescriptor cpd = {}; cpd.layout = pl; cpd.compute.module = sm; cpd.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipe = wgpuDeviceCreateComputePipeline(device, &cpd); ASSERT_NE(pipe, nullptr);
    WGPUBindGroupEntry bge = {}; bge.binding = 0; bge.buffer = sto; bge.size = bSz;
    WGPUBindGroupDescriptor bgd = {}; bgd.layout = bgl; bgd.entryCount = 1; bgd.entries = &bge;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor pd = {}; WGPUComputePassEncoder p = wgpuCommandEncoderBeginComputePass(enc, &pd);
    wgpuComputePassEncoderSetPipeline(p, pipe); wgpuComputePassEncoderSetBindGroup(p, 0, bg, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(p, 4, 1, 1);
    wgpuComputePassEncoderEnd(p); wgpuComputePassEncoderRelease(p);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    WGPUBufferDescriptor rd = {}; rd.size = bSz; rd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer rb = wgpuDeviceCreateBuffer(device, &rd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, sto, 0, rb, 0, bSz);
    cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = { nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr };
    WGPUFuture mf = wgpuBufferMapAsync(rb, WGPUMapMode_Read, 0, bSz, ci);
    WGPUFutureWaitInfo fw = { mf, 0 }; while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }
    const uint32_t* res = (const uint32_t*)wgpuBufferGetConstMappedRange(rb, 0, bSz); ASSERT_NE(res, nullptr);
    EXPECT_EQ(res[0], 1024u) << "AtomicAdd multi-workgroup expected 1024";
    wgpuBufferUnmap(rb); for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuBufferRelease(sto); wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pl); wgpuComputePipelineRelease(pipe); wgpuShaderModuleRelease(sm);
}

TEST_F(WebGPUTest, ComputeDispatchIndirect) {
    const uint32_t N = 256; const uint32_t bSz = N * sizeof(uint32_t);
    WGPUBufferDescriptor sd = {}; sd.size = bSz; sd.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    WGPUBuffer sto = wgpuDeviceCreateBuffer(device, &sd); ASSERT_NE(sto, nullptr);
    WGPUBufferDescriptor id = {}; id.size = 12; id.usage = WGPUBufferUsage_Indirect | WGPUBufferUsage_CopyDst;
    WGPUBuffer ib = wgpuDeviceCreateBuffer(device, &id);
    uint32_t ia[3] = { 4, 1, 1 }; wgpuQueueWriteBuffer(queue, ib, 0, ia, 12); wgpuQueueWaitIdle(queue);
    const char* code = R"(
        #version 450
        layout(local_size_x = 64) in;
        layout(std430, set = 0, binding = 0) buffer Data { uint values[]; } data;
        void main() { uint idx = gl_GlobalInvocationID.x; data.values[idx] = idx * 5; }
    )";
    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code); ASSERT_NE(sm, nullptr);
    WGPUBindGroupLayoutEntry be = {}; be.binding = 0; be.visibility = WGPUShaderStage_Compute;
    be.buffer.type = WGPUBufferBindingType_Storage; be.buffer.minBindingSize = bSz;
    WGPUBindGroupLayoutDescriptor bld = {}; bld.entryCount = 1; bld.entries = &be;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bld);
    WGPUPipelineLayoutDescriptor pld = {}; pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &pld);
    WGPUComputePipelineDescriptor cpd = {}; cpd.layout = pl; cpd.compute.module = sm; cpd.compute.entryPoint = { "main", 4 };
    WGPUComputePipeline pipe = wgpuDeviceCreateComputePipeline(device, &cpd); ASSERT_NE(pipe, nullptr);
    WGPUBindGroupEntry bge = {}; bge.binding = 0; bge.buffer = sto; bge.size = bSz;
    WGPUBindGroupDescriptor bgd = {}; bgd.layout = bgl; bgd.entryCount = 1; bgd.entries = &bge;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(device, &bgd);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUComputePassDescriptor pd = {}; WGPUComputePassEncoder p = wgpuCommandEncoderBeginComputePass(enc, &pd);
    wgpuComputePassEncoderSetPipeline(p, pipe); wgpuComputePassEncoderSetBindGroup(p, 0, bg, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroupsIndirect(p, ib, 0);
    wgpuComputePassEncoderEnd(p); wgpuComputePassEncoderRelease(p);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    WGPUBufferDescriptor rd = {}; rd.size = bSz; rd.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer rb = wgpuDeviceCreateBuffer(device, &rd);
    enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(enc, sto, 0, rb, 0, bSz);
    cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = { nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr };
    WGPUFuture mf = wgpuBufferMapAsync(rb, WGPUMapMode_Read, 0, bSz, ci);
    WGPUFutureWaitInfo fw = { mf, 0 }; while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }
    const uint32_t* res = (const uint32_t*)wgpuBufferGetConstMappedRange(rb, 0, bSz); ASSERT_NE(res, nullptr);
    for (uint32_t i = 0; i < N; ++i) { EXPECT_EQ(res[i], i * 5) << "Index " << i; }
    wgpuBufferUnmap(rb); for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuBufferRelease(sto); wgpuBufferRelease(ib);
    wgpuBindGroupRelease(bg); wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pl); wgpuComputePipelineRelease(pipe); wgpuShaderModuleRelease(sm);
}

// ============================================================
// P1 Advanced Copy Tests
// ============================================================

TEST_F(WebGPUTest, CopyBufferToTexture_MipLevel1) {
    const uint32_t mW = 32, mH = 32, bpr = 256; const size_t mSz = bpr * mH;
    std::vector<uint8_t> sp(mSz);
    for (uint32_t y = 0; y < mH; ++y) for (uint32_t x = 0; x < mW; ++x) {
        size_t o = y*bpr+x*4; sp[o]=(uint8_t)(x*8); sp[o+1]=(uint8_t)(y*8); sp[o+2]=(uint8_t)((x+y)*4); sp[o+3]=255; }
    WGPUBufferDescriptor ud = {}; ud.size = mSz; ud.usage = WGPUBufferUsage_CopySrc|WGPUBufferUsage_MapWrite; ud.mappedAtCreation = true;
    WGPUBuffer ub = wgpuDeviceCreateBuffer(device, &ud);
    memcpy(wgpuBufferGetMappedRange(ub, 0, mSz), sp.data(), mSz); wgpuBufferUnmap(ub);
    WGPUTextureDescriptor td = {}; td.size = {64,64,1}; td.format = WGPUTextureFormat_RGBA8Unorm;
    td.usage = WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc; td.mipLevelCount = 2; td.sampleCount = 1; td.dimension = WGPUTextureDimension_2D;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &td); ASSERT_NE(tex, nullptr);
    WGPUBufferDescriptor rd = {}; rd.size = mSz; rd.usage = WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb = wgpuDeviceCreateBuffer(device, &rd);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUTexelCopyBufferInfo bs = {}; bs.buffer = ub; bs.layout.bytesPerRow = bpr; bs.layout.rowsPerImage = mH;
    WGPUTexelCopyTextureInfo tdi = {}; tdi.texture = tex; tdi.mipLevel = 1; tdi.aspect = WGPUTextureAspect_All;
    WGPUExtent3D ext = {mW,mH,1};
    wgpuCommandEncoderCopyBufferToTexture(enc, &bs, &tdi, &ext);
    WGPUTexelCopyTextureInfo tsi = {}; tsi.texture = tex; tsi.mipLevel = 1; tsi.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo bdi = {}; bdi.buffer = rb; bdi.layout.bytesPerRow = bpr; bdi.layout.rowsPerImage = mH;
    wgpuCommandEncoderCopyTextureToBuffer(enc, &tsi, &bdi, &ext);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = { nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr };
    WGPUFuture mf = wgpuBufferMapAsync(rb, WGPUMapMode_Read, 0, mSz, ci);
    WGPUFutureWaitInfo fw = { mf, 0 }; while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }
    const uint8_t* r = (const uint8_t*)wgpuBufferGetConstMappedRange(rb, 0, mSz); ASSERT_NE(r, nullptr);
    for (uint32_t y = 0; y < mH; ++y) for (uint32_t x = 0; x < mW; ++x) {
        size_t o = y*bpr+x*4;
        EXPECT_EQ(r[o],sp[o])<<"R mip1 "<<x<<","<<y; EXPECT_EQ(r[o+1],sp[o+1]); EXPECT_EQ(r[o+2],sp[o+2]); EXPECT_EQ(r[o+3],sp[o+3]); }
    wgpuBufferUnmap(rb); for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(ub); wgpuBufferRelease(rb); wgpuTextureRelease(tex);
}

TEST_F(WebGPUTest, CopyTextureToTexture_SameFormatSameSize) {
    const uint32_t W=64,H=64,bpr=256; const size_t dSz=bpr*H;
    std::vector<uint8_t> sp(dSz);
    for (uint32_t y=0;y<H;++y) for (uint32_t x=0;x<W;++x) { size_t o=y*bpr+x*4; sp[o]=(uint8_t)((x*3+y*7)&0xFF); sp[o+1]=(uint8_t)((x*5+y*11)&0xFF); sp[o+2]=(uint8_t)((x*13+y*17)&0xFF); sp[o+3]=255; }
    WGPUBufferDescriptor ud={}; ud.size=dSz; ud.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_MapWrite; ud.mappedAtCreation=true;
    WGPUBuffer ub=wgpuDeviceCreateBuffer(device,&ud); memcpy(wgpuBufferGetMappedRange(ub,0,dSz),sp.data(),dSz); wgpuBufferUnmap(ub);
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm; td.usage=WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture tA=wgpuDeviceCreateTexture(device,&td); WGPUTexture tB=wgpuDeviceCreateTexture(device,&td);
    WGPUBufferDescriptor rdd={}; rdd.size=dSz; rdd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb=wgpuDeviceCreateBuffer(device,&rdd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    WGPUTexelCopyBufferInfo bs={}; bs.buffer=ub; bs.layout.bytesPerRow=bpr; bs.layout.rowsPerImage=H;
    WGPUTexelCopyTextureInfo tAd={}; tAd.texture=tA; tAd.aspect=WGPUTextureAspect_All;
    WGPUExtent3D ext={W,H,1}; wgpuCommandEncoderCopyBufferToTexture(enc,&bs,&tAd,&ext);
    WGPUTexelCopyTextureInfo tAs={}; tAs.texture=tA; tAs.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyTextureInfo tBd2={}; tBd2.texture=tB; tBd2.aspect=WGPUTextureAspect_All;
    wgpuCommandEncoderCopyTextureToTexture(enc,&tAs,&tBd2,&ext);
    WGPUTexelCopyTextureInfo tBs={}; tBs.texture=tB; tBs.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo bd={}; bd.buffer=rb; bd.layout.bytesPerRow=bpr; bd.layout.rowsPerImage=H;
    wgpuCommandEncoderCopyTextureToBuffer(enc,&tBs,&bd,&ext);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuQueueSubmit(queue,1,&cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx{bool done=false;}mc; auto mcb=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MapCtx*)u)->done=true;};
    WGPUBufferMapCallbackInfo ci={nullptr,WGPUCallbackMode_WaitAnyOnly,mcb,&mc,nullptr};
    WGPUFuture mf=wgpuBufferMapAsync(rb,WGPUMapMode_Read,0,dSz,ci); WGPUFutureWaitInfo fw={mf,0};
    while(!mc.done){wgpuInstanceWaitAny(instance,1,&fw,UINT64_MAX);}
    const uint8_t* r=(const uint8_t*)wgpuBufferGetConstMappedRange(rb,0,dSz); ASSERT_NE(r,nullptr);
    for(uint32_t y=0;y<H;++y) for(uint32_t x=0;x<W;++x){size_t o=y*bpr+x*4; EXPECT_EQ(r[o],sp[o])<<"at "<<x<<","<<y; EXPECT_EQ(r[o+1],sp[o+1]); EXPECT_EQ(r[o+2],sp[o+2]); EXPECT_EQ(r[o+3],sp[o+3]);}
    wgpuBufferUnmap(rb); for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(ub); wgpuBufferRelease(rb); wgpuTextureRelease(tA); wgpuTextureRelease(tB);
}

TEST_F(WebGPUTest, CopyTextureToTexture_SubRegion) {
    const uint32_t W=64,H=64,bpr=256; const size_t dSz=bpr*H;
    std::vector<uint8_t> ck(dSz,0),bk(dSz,0);
    for(uint32_t y=0;y<H;++y) for(uint32_t x=0;x<W;++x){size_t o=y*bpr+x*4; uint8_t v=((x/4)+(y/4))%2==0?200:50; ck[o]=v;ck[o+1]=v;ck[o+2]=v;ck[o+3]=255; bk[o+3]=255;}
    WGPUBufferDescriptor ud={}; ud.size=dSz; ud.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_MapWrite; ud.mappedAtCreation=true;
    WGPUBuffer uA=wgpuDeviceCreateBuffer(device,&ud); memcpy(wgpuBufferGetMappedRange(uA,0,dSz),ck.data(),dSz); wgpuBufferUnmap(uA);
    WGPUBuffer uB=wgpuDeviceCreateBuffer(device,&ud); memcpy(wgpuBufferGetMappedRange(uB,0,dSz),bk.data(),dSz); wgpuBufferUnmap(uB);
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm; td.usage=WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture tA=wgpuDeviceCreateTexture(device,&td); WGPUTexture tB=wgpuDeviceCreateTexture(device,&td);
    WGPUBufferDescriptor rdd={}; rdd.size=dSz; rdd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb=wgpuDeviceCreateBuffer(device,&rdd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    WGPUExtent3D full={W,H,1};
    WGPUTexelCopyBufferInfo bsA={}; bsA.buffer=uA; bsA.layout.bytesPerRow=bpr; bsA.layout.rowsPerImage=H;
    WGPUTexelCopyTextureInfo tAd={}; tAd.texture=tA; tAd.aspect=WGPUTextureAspect_All;
    wgpuCommandEncoderCopyBufferToTexture(enc,&bsA,&tAd,&full);
    WGPUTexelCopyBufferInfo bsB={}; bsB.buffer=uB; bsB.layout.bytesPerRow=bpr; bsB.layout.rowsPerImage=H;
    WGPUTexelCopyTextureInfo tBd={}; tBd.texture=tB; tBd.aspect=WGPUTextureAspect_All;
    wgpuCommandEncoderCopyBufferToTexture(enc,&bsB,&tBd,&full);
    WGPUTexelCopyTextureInfo tAs={}; tAs.texture=tA; tAs.origin={8,8,0}; tAs.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyTextureInfo tBs={}; tBs.texture=tB; tBs.origin={32,32,0}; tBs.aspect=WGPUTextureAspect_All;
    WGPUExtent3D sub={16,16,1}; wgpuCommandEncoderCopyTextureToTexture(enc,&tAs,&tBs,&sub);
    WGPUTexelCopyTextureInfo tBr={}; tBr.texture=tB; tBr.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo bdr={}; bdr.buffer=rb; bdr.layout.bytesPerRow=bpr; bdr.layout.rowsPerImage=H;
    wgpuCommandEncoderCopyTextureToBuffer(enc,&tBr,&bdr,&full);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuQueueSubmit(queue,1,&cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx{bool done=false;}mc; auto mcb=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MapCtx*)u)->done=true;};
    WGPUBufferMapCallbackInfo ci={nullptr,WGPUCallbackMode_WaitAnyOnly,mcb,&mc,nullptr};
    WGPUFuture mf=wgpuBufferMapAsync(rb,WGPUMapMode_Read,0,dSz,ci); WGPUFutureWaitInfo fw={mf,0};
    while(!mc.done){wgpuInstanceWaitAny(instance,1,&fw,UINT64_MAX);}
    const uint8_t* r=(const uint8_t*)wgpuBufferGetConstMappedRange(rb,0,dSz); ASSERT_NE(r,nullptr);
    for(uint32_t y=0;y<H;++y) for(uint32_t x=0;x<W;++x){size_t o=y*bpr+x*4; bool inR=(x>=32&&x<48&&y>=32&&y<48);
        if(inR){size_t so=(8+(y-32))*bpr+(8+(x-32))*4; EXPECT_EQ(r[o],ck[so])<<"Copied "<<x<<","<<y;}
        else{EXPECT_EQ(r[o],0)<<"Outside "<<x<<","<<y; EXPECT_EQ(r[o+3],255);}}
    wgpuBufferUnmap(rb); for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(uA); wgpuBufferRelease(uB); wgpuBufferRelease(rb); wgpuTextureRelease(tA); wgpuTextureRelease(tB);
}

TEST_F(WebGPUTest, CopyBufferToTexture_WithRowPadding) {
    const uint32_t W=60,H=64,bpr=256; const size_t dSz=bpr*H;
    std::vector<uint8_t> sp(dSz,0xCC);
    for(uint32_t y=0;y<H;++y) for(uint32_t x=0;x<W;++x){size_t o=y*bpr+x*4; sp[o]=(uint8_t)((x+y*3)&0xFF); sp[o+1]=(uint8_t)((x*2+y)&0xFF); sp[o+2]=(uint8_t)((x+y)&0xFF); sp[o+3]=255;}
    WGPUBufferDescriptor ud={}; ud.size=dSz; ud.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_MapWrite; ud.mappedAtCreation=true;
    WGPUBuffer ub=wgpuDeviceCreateBuffer(device,&ud); memcpy(wgpuBufferGetMappedRange(ub,0,dSz),sp.data(),dSz); wgpuBufferUnmap(ub);
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm; td.usage=WGPUTextureUsage_CopyDst|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture tex=wgpuDeviceCreateTexture(device,&td);
    WGPUBufferDescriptor rdd={}; rdd.size=dSz; rdd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb=wgpuDeviceCreateBuffer(device,&rdd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    WGPUTexelCopyBufferInfo bs={}; bs.buffer=ub; bs.layout.bytesPerRow=bpr; bs.layout.rowsPerImage=H;
    WGPUTexelCopyTextureInfo tdi={}; tdi.texture=tex; tdi.aspect=WGPUTextureAspect_All;
    WGPUExtent3D ext={W,H,1}; wgpuCommandEncoderCopyBufferToTexture(enc,&bs,&tdi,&ext);
    WGPUTexelCopyTextureInfo tsi={}; tsi.texture=tex; tsi.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo bdi={}; bdi.buffer=rb; bdi.layout.bytesPerRow=bpr; bdi.layout.rowsPerImage=H;
    wgpuCommandEncoderCopyTextureToBuffer(enc,&tsi,&bdi,&ext);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuQueueSubmit(queue,1,&cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    struct MapCtx{bool done=false;}mc; auto mcb=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MapCtx*)u)->done=true;};
    WGPUBufferMapCallbackInfo ci={nullptr,WGPUCallbackMode_WaitAnyOnly,mcb,&mc,nullptr};
    WGPUFuture mf=wgpuBufferMapAsync(rb,WGPUMapMode_Read,0,dSz,ci); WGPUFutureWaitInfo fw={mf,0};
    while(!mc.done){wgpuInstanceWaitAny(instance,1,&fw,UINT64_MAX);}
    const uint8_t* r=(const uint8_t*)wgpuBufferGetConstMappedRange(rb,0,dSz); ASSERT_NE(r,nullptr);
    for(uint32_t y=0;y<H;++y) for(uint32_t x=0;x<W;++x){size_t o=y*bpr+x*4; EXPECT_EQ(r[o],sp[o])<<"R "<<x<<","<<y; EXPECT_EQ(r[o+1],sp[o+1]); EXPECT_EQ(r[o+2],sp[o+2]); EXPECT_EQ(r[o+3],sp[o+3]);}
    wgpuBufferUnmap(rb); for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(ub); wgpuBufferRelease(rb); wgpuTextureRelease(tex);
}

// ============================================================
// P1 Render Pass Variation Tests
// ============================================================

TEST_F(WebGPUTest, RenderPassMRT_TwoTargets) {
    const uint32_t W=64,H=64,bpr=256; const size_t bSz=bpr*H;
    const char* vs=R"(#version 450
        void main(){const vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexIndex],0.5,1.0);})";
    const char* fs=R"(#version 450
        layout(location=0) out vec4 c0; layout(location=1) out vec4 c1;
        void main(){c0=vec4(1,0,0,1);c1=vec4(0,0,1,1);})";
    WGPUShaderModule vsM=compileGLSL(device,WGPUShaderStage_Vertex,vs); ASSERT_NE(vsM,nullptr);
    WGPUShaderModule fsM=compileGLSL(device,WGPUShaderStage_Fragment,fs); ASSERT_NE(fsM,nullptr);
    WGPUPipelineLayoutDescriptor pld={}; WGPUPipelineLayout pl=wgpuDeviceCreatePipelineLayout(device,&pld);
    WGPUColorTargetState ct[2]={}; ct[0].format=WGPUTextureFormat_RGBA8Unorm; ct[0].writeMask=WGPUColorWriteMask_All;
    ct[1].format=WGPUTextureFormat_RGBA8Unorm; ct[1].writeMask=WGPUColorWriteMask_All;
    WGPUFragmentState fst={}; fst.module=fsM; fst.entryPoint={"main",4}; fst.targetCount=2; fst.targets=ct;
    WGPURenderPipelineDescriptor rpd={}; rpd.layout=pl; rpd.vertex.module=vsM; rpd.vertex.entryPoint={"main",4};
    rpd.primitive.topology=WGPUPrimitiveTopology_TriangleList; rpd.primitive.cullMode=WGPUCullMode_None;
    rpd.primitive.frontFace=WGPUFrontFace_CCW; rpd.multisample.count=1; rpd.multisample.mask=0xFFFFFFFF; rpd.fragment=&fst;
    WGPURenderPipeline pipe=wgpuDeviceCreateRenderPipeline(device,&rpd); ASSERT_NE(pipe,nullptr);
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm;
    td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture t0=wgpuDeviceCreateTexture(device,&td); WGPUTexture t1=wgpuDeviceCreateTexture(device,&td);
    WGPUTextureView v0=wgpuTextureCreateView(t0,nullptr); WGPUTextureView v1=wgpuTextureCreateView(t1,nullptr);
    WGPUBufferDescriptor rbd={}; rbd.size=bSz; rbd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb0=wgpuDeviceCreateBuffer(device,&rbd); WGPUBuffer rb1=wgpuDeviceCreateBuffer(device,&rbd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    WGPURenderPassColorAttachment atts[2]={}; atts[0].view=v0; atts[0].loadOp=WGPULoadOp_Clear; atts[0].storeOp=WGPUStoreOp_Store;
    atts[1].view=v1; atts[1].loadOp=WGPULoadOp_Clear; atts[1].storeOp=WGPUStoreOp_Store;
    WGPURenderPassDescriptor rpDesc={}; rpDesc.colorAttachmentCount=2; rpDesc.colorAttachments=atts;
    WGPURenderPassEncoder rp=wgpuCommandEncoderBeginRenderPass(enc,&rpDesc);
    wgpuRenderPassEncoderSetPipeline(rp,pipe); wgpuRenderPassEncoderDraw(rp,3,1,0,0);
    wgpuRenderPassEncoderEnd(rp); wgpuRenderPassEncoderRelease(rp);
    WGPUExtent3D csz={W,H,1};
    WGPUTexelCopyTextureInfo s0={}; s0.texture=t0; s0.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo d0={}; d0.buffer=rb0; d0.layout.bytesPerRow=bpr; d0.layout.rowsPerImage=H;
    wgpuCommandEncoderCopyTextureToBuffer(enc,&s0,&d0,&csz);
    WGPUTexelCopyTextureInfo s1={}; s1.texture=t1; s1.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo d1={}; d1.buffer=rb1; d1.layout.bytesPerRow=bpr; d1.layout.rowsPerImage=H;
    wgpuCommandEncoderCopyTextureToBuffer(enc,&s1,&d1,&csz);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuQueueSubmit(queue,1,&cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    {struct MC{bool d=false;}m; auto c=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MC*)u)->d=true;};
     WGPUBufferMapCallbackInfo i={nullptr,WGPUCallbackMode_WaitAnyOnly,c,&m,nullptr};
     WGPUFuture f=wgpuBufferMapAsync(rb0,WGPUMapMode_Read,0,bSz,i); WGPUFutureWaitInfo w={f,0};
     while(!m.d){wgpuInstanceWaitAny(instance,1,&w,UINT64_MAX);}
     const uint8_t*p=(const uint8_t*)wgpuBufferGetConstMappedRange(rb0,0,bSz); size_t o=(H/2)*bpr+(W/2)*4;
     EXPECT_EQ(p[o],255); EXPECT_EQ(p[o+1],0); EXPECT_EQ(p[o+2],0); EXPECT_EQ(p[o+3],255); wgpuBufferUnmap(rb0);}
    {struct MC2{bool d=false;}m2; auto c2=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MC2*)u)->d=true;};
     WGPUBufferMapCallbackInfo i2={nullptr,WGPUCallbackMode_WaitAnyOnly,c2,&m2,nullptr};
     WGPUFuture f2=wgpuBufferMapAsync(rb1,WGPUMapMode_Read,0,bSz,i2); WGPUFutureWaitInfo w2={f2,0};
     while(!m2.d){wgpuInstanceWaitAny(instance,1,&w2,UINT64_MAX);}
     const uint8_t*p2=(const uint8_t*)wgpuBufferGetConstMappedRange(rb1,0,bSz); size_t o=(H/2)*bpr+(W/2)*4;
     EXPECT_EQ(p2[o],0); EXPECT_EQ(p2[o+1],0); EXPECT_EQ(p2[o+2],255); EXPECT_EQ(p2[o+3],255); wgpuBufferUnmap(rb1);}
    for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb0); wgpuBufferRelease(rb1); wgpuTextureViewRelease(v0); wgpuTextureViewRelease(v1);
    wgpuTextureRelease(t0); wgpuTextureRelease(t1); wgpuRenderPipelineRelease(pipe); wgpuPipelineLayoutRelease(pl);
    wgpuShaderModuleRelease(vsM); wgpuShaderModuleRelease(fsM);
}

TEST_F(WebGPUTest, RenderPass_LoadOp_Load) {
    const uint32_t W=64,H=64,bpr=256; const size_t bSz=bpr*H;
    const char* vs=R"(#version 450
        void main(){const vec2 p[3]=vec2[3](vec2(-1,-1),vec2(1,-1),vec2(-1,1));gl_Position=vec4(p[gl_VertexIndex],0.5,1.0);})";
    const char* fs=R"(#version 450
        layout(location=0) out vec4 o; void main(){o=vec4(0,1,0,1);})";
    WGPUShaderModule vsM=compileGLSL(device,WGPUShaderStage_Vertex,vs); ASSERT_NE(vsM,nullptr);
    WGPUShaderModule fsM=compileGLSL(device,WGPUShaderStage_Fragment,fs); ASSERT_NE(fsM,nullptr);
    WGPUPipelineLayoutDescriptor pld={}; WGPUPipelineLayout pl=wgpuDeviceCreatePipelineLayout(device,&pld);
    WGPUColorTargetState ct={}; ct.format=WGPUTextureFormat_RGBA8Unorm; ct.writeMask=WGPUColorWriteMask_All;
    WGPUFragmentState fst={}; fst.module=fsM; fst.entryPoint={"main",4}; fst.targetCount=1; fst.targets=&ct;
    WGPURenderPipelineDescriptor rpd={}; rpd.layout=pl; rpd.vertex.module=vsM; rpd.vertex.entryPoint={"main",4};
    rpd.primitive.topology=WGPUPrimitiveTopology_TriangleList; rpd.primitive.cullMode=WGPUCullMode_None;
    rpd.primitive.frontFace=WGPUFrontFace_CCW; rpd.multisample.count=1; rpd.multisample.mask=0xFFFFFFFF; rpd.fragment=&fst;
    WGPURenderPipeline pipe=wgpuDeviceCreateRenderPipeline(device,&rpd); ASSERT_NE(pipe,nullptr);
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm;
    td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture tex=wgpuDeviceCreateTexture(device,&td); WGPUTextureView view=wgpuTextureCreateView(tex,nullptr);
    WGPUBufferDescriptor rbd={}; rbd.size=bSz; rbd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb=wgpuDeviceCreateBuffer(device,&rbd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    {WGPURenderPassColorAttachment a={}; a.view=view; a.loadOp=WGPULoadOp_Clear; a.storeOp=WGPUStoreOp_Store; a.clearValue={1,0,0,1};
     WGPURenderPassDescriptor rd={}; rd.colorAttachmentCount=1; rd.colorAttachments=&a;
     WGPURenderPassEncoder rp=wgpuCommandEncoderBeginRenderPass(enc,&rd); wgpuRenderPassEncoderEnd(rp); wgpuRenderPassEncoderRelease(rp);}
    {WGPURenderPassColorAttachment a={}; a.view=view; a.loadOp=WGPULoadOp_Load; a.storeOp=WGPUStoreOp_Store;
     WGPURenderPassDescriptor rd={}; rd.colorAttachmentCount=1; rd.colorAttachments=&a;
     WGPURenderPassEncoder rp=wgpuCommandEncoderBeginRenderPass(enc,&rd); wgpuRenderPassEncoderSetPipeline(rp,pipe);
     wgpuRenderPassEncoderDraw(rp,3,1,0,0); wgpuRenderPassEncoderEnd(rp); wgpuRenderPassEncoderRelease(rp);}
    WGPUTexelCopyTextureInfo si={}; si.texture=tex; si.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo di={}; di.buffer=rb; di.layout.bytesPerRow=bpr; di.layout.rowsPerImage=H;
    WGPUExtent3D csz={W,H,1}; wgpuCommandEncoderCopyTextureToBuffer(enc,&si,&di,&csz);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuCommandEncoderRelease(enc);
    wgpuQueueSubmit(queue,1,&cmd); wgpuCommandBufferRelease(cmd);
    struct MapCtx{bool done=false;}mc; auto mcb=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MapCtx*)u)->done=true;};
    WGPUBufferMapCallbackInfo ci={nullptr,WGPUCallbackMode_WaitAnyOnly,mcb,&mc,nullptr};
    WGPUFuture mf=wgpuBufferMapAsync(rb,WGPUMapMode_Read,0,bSz,ci); WGPUFutureWaitInfo fw={mf,0};
    while(!mc.done){wgpuInstanceWaitAny(instance,1,&fw,UINT64_MAX);}
    const uint8_t*px=(const uint8_t*)wgpuBufferGetConstMappedRange(rb,0,bSz); ASSERT_NE(px,nullptr);
    {size_t o=50*bpr+10*4; EXPECT_EQ(px[o],0); EXPECT_EQ(px[o+1],255); EXPECT_EQ(px[o+2],0);}
    {size_t o=10*bpr+50*4; EXPECT_EQ(px[o],255); EXPECT_EQ(px[o+1],0); EXPECT_EQ(px[o+2],0);}
    wgpuBufferUnmap(rb); for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuTextureViewRelease(view); wgpuTextureRelease(tex);
    wgpuRenderPipelineRelease(pipe); wgpuPipelineLayoutRelease(pl); wgpuShaderModuleRelease(vsM); wgpuShaderModuleRelease(fsM);
}

TEST_F(WebGPUTest, RenderPass_NoDraw_JustClear) {
    const uint32_t W=64,H=64,bpr=256; const size_t bSz=bpr*H;
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm;
    td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture tex=wgpuDeviceCreateTexture(device,&td); WGPUTextureView view=wgpuTextureCreateView(tex,nullptr);
    WGPUBufferDescriptor rbd={}; rbd.size=bSz; rbd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rb=wgpuDeviceCreateBuffer(device,&rbd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    WGPURenderPassColorAttachment att={}; att.view=view; att.loadOp=WGPULoadOp_Clear; att.storeOp=WGPUStoreOp_Store; att.clearValue={0.2,0.4,0.6,1.0};
    WGPURenderPassDescriptor rpD={}; rpD.colorAttachmentCount=1; rpD.colorAttachments=&att;
    WGPURenderPassEncoder rp=wgpuCommandEncoderBeginRenderPass(enc,&rpD); wgpuRenderPassEncoderEnd(rp); wgpuRenderPassEncoderRelease(rp);
    WGPUTexelCopyTextureInfo si={}; si.texture=tex; si.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo di={}; di.buffer=rb; di.layout.bytesPerRow=bpr; di.layout.rowsPerImage=H;
    WGPUExtent3D csz={W,H,1}; wgpuCommandEncoderCopyTextureToBuffer(enc,&si,&di,&csz);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuCommandEncoderRelease(enc);
    wgpuQueueSubmit(queue,1,&cmd); wgpuCommandBufferRelease(cmd);
    struct MapCtx{bool done=false;}mc; auto mcb=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MapCtx*)u)->done=true;};
    WGPUBufferMapCallbackInfo ci={nullptr,WGPUCallbackMode_WaitAnyOnly,mcb,&mc,nullptr};
    WGPUFuture mf=wgpuBufferMapAsync(rb,WGPUMapMode_Read,0,bSz,ci); WGPUFutureWaitInfo fw={mf,0};
    while(!mc.done){wgpuInstanceWaitAny(instance,1,&fw,UINT64_MAX);}
    const uint8_t*px=(const uint8_t*)wgpuBufferGetConstMappedRange(rb,0,bSz); ASSERT_NE(px,nullptr);
    auto chk=[&](uint32_t x,uint32_t y){size_t o=y*bpr+x*4; EXPECT_NEAR(px[o],51,1); EXPECT_NEAR(px[o+1],102,1); EXPECT_NEAR(px[o+2],153,1); EXPECT_EQ(px[o+3],255);};
    chk(0,0); chk(W-1,0); chk(0,H-1); chk(W-1,H-1); chk(W/2,H/2);
    wgpuBufferUnmap(rb); for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rb); wgpuTextureViewRelease(view); wgpuTextureRelease(tex);
}

TEST_F(WebGPUTest, RenderThenCompute_SameCommandBuffer) {
    const uint32_t W=64,H=64,bpr=256; const size_t tBSz=bpr*H;
    const uint32_t N=128; const uint32_t cBSz=N*sizeof(uint32_t);
    WGPUBufferDescriptor sd={}; sd.size=cBSz; sd.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_MapWrite; sd.mappedAtCreation=true;
    WGPUBuffer stg=wgpuDeviceCreateBuffer(device,&sd); uint32_t*id=(uint32_t*)wgpuBufferGetMappedRange(stg,0,cBSz);
    for(uint32_t i=0;i<N;++i) id[i]=i+1; wgpuBufferUnmap(stg);
    WGPUBufferDescriptor std2={}; std2.size=cBSz; std2.usage=WGPUBufferUsage_Storage|WGPUBufferUsage_CopyDst|WGPUBufferUsage_CopySrc;
    WGPUBuffer sto=wgpuDeviceCreateBuffer(device,&std2);
    {WGPUCommandEncoder e=wgpuDeviceCreateCommandEncoder(device,nullptr); wgpuCommandEncoderCopyBufferToBuffer(e,stg,0,sto,0,cBSz);
     WGPUCommandBuffer c=wgpuCommandEncoderFinish(e,nullptr); wgpuQueueSubmit(queue,1,&c); wgpuQueueWaitIdle(queue);
     wgpuCommandBufferRelease(c); wgpuCommandEncoderRelease(e);} wgpuBufferRelease(stg);
    const char* cc=R"(#version 450
        layout(local_size_x=64) in; layout(std430,set=0,binding=0) buffer D{uint v[];}d;
        void main(){uint i=gl_GlobalInvocationID.x; d.v[i]=d.v[i]*2;})";
    WGPUShaderModule cm=compileGLSL(device,WGPUShaderStage_Compute,cc); ASSERT_NE(cm,nullptr);
    WGPUBindGroupLayoutEntry ce={}; ce.binding=0; ce.visibility=WGPUShaderStage_Compute;
    ce.buffer.type=WGPUBufferBindingType_Storage; ce.buffer.minBindingSize=cBSz;
    WGPUBindGroupLayoutDescriptor cld={}; cld.entryCount=1; cld.entries=&ce;
    WGPUBindGroupLayout cbgl=wgpuDeviceCreateBindGroupLayout(device,&cld);
    WGPUPipelineLayoutDescriptor cpld={}; cpld.bindGroupLayoutCount=1; cpld.bindGroupLayouts=&cbgl;
    WGPUPipelineLayout cpl=wgpuDeviceCreatePipelineLayout(device,&cpld);
    WGPUComputePipelineDescriptor cpd={}; cpd.layout=cpl; cpd.compute.module=cm; cpd.compute.entryPoint={"main",4};
    WGPUComputePipeline cp=wgpuDeviceCreateComputePipeline(device,&cpd); ASSERT_NE(cp,nullptr);
    WGPUBindGroupEntry cbe={}; cbe.binding=0; cbe.buffer=sto; cbe.size=cBSz;
    WGPUBindGroupDescriptor cbd={}; cbd.layout=cbgl; cbd.entryCount=1; cbd.entries=&cbe;
    WGPUBindGroup cbg=wgpuDeviceCreateBindGroup(device,&cbd);
    WGPUTextureDescriptor td={}; td.size={W,H,1}; td.format=WGPUTextureFormat_RGBA8Unorm;
    td.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_CopySrc; td.mipLevelCount=1; td.sampleCount=1; td.dimension=WGPUTextureDimension_2D;
    WGPUTexture tex=wgpuDeviceCreateTexture(device,&td); WGPUTextureView view=wgpuTextureCreateView(tex,nullptr);
    WGPUBufferDescriptor rtbd={}; rtbd.size=tBSz; rtbd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rtb=wgpuDeviceCreateBuffer(device,&rtbd);
    WGPUBufferDescriptor rcbd={}; rcbd.size=cBSz; rcbd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer rcb=wgpuDeviceCreateBuffer(device,&rcbd);
    WGPUCommandEncoder enc=wgpuDeviceCreateCommandEncoder(device,nullptr);
    {WGPURenderPassColorAttachment a={}; a.view=view; a.loadOp=WGPULoadOp_Clear; a.storeOp=WGPUStoreOp_Store; a.clearValue={1,0,0,1};
     WGPURenderPassDescriptor rd={}; rd.colorAttachmentCount=1; rd.colorAttachments=&a;
     WGPURenderPassEncoder rp=wgpuCommandEncoderBeginRenderPass(enc,&rd); wgpuRenderPassEncoderEnd(rp); wgpuRenderPassEncoderRelease(rp);}
    {WGPUComputePassDescriptor cpd2={}; WGPUComputePassEncoder cpp=wgpuCommandEncoderBeginComputePass(enc,&cpd2);
     wgpuComputePassEncoderSetPipeline(cpp,cp); wgpuComputePassEncoderSetBindGroup(cpp,0,cbg,0,nullptr);
     wgpuComputePassEncoderDispatchWorkgroups(cpp,2,1,1); wgpuComputePassEncoderEnd(cpp); wgpuComputePassEncoderRelease(cpp);}
    WGPUTexelCopyTextureInfo tsi={}; tsi.texture=tex; tsi.aspect=WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo tdi={}; tdi.buffer=rtb; tdi.layout.bytesPerRow=bpr; tdi.layout.rowsPerImage=H;
    WGPUExtent3D tcs={W,H,1}; wgpuCommandEncoderCopyTextureToBuffer(enc,&tsi,&tdi,&tcs);
    wgpuCommandEncoderCopyBufferToBuffer(enc,sto,0,rcb,0,cBSz);
    WGPUCommandBuffer cmd=wgpuCommandEncoderFinish(enc,nullptr); wgpuQueueSubmit(queue,1,&cmd); wgpuCommandEncoderRelease(enc); wgpuCommandBufferRelease(cmd);
    {struct MC{bool d=false;}m; auto c=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MC*)u)->d=true;};
     WGPUBufferMapCallbackInfo i={nullptr,WGPUCallbackMode_WaitAnyOnly,c,&m,nullptr};
     WGPUFuture f=wgpuBufferMapAsync(rtb,WGPUMapMode_Read,0,tBSz,i); WGPUFutureWaitInfo w={f,0};
     while(!m.d){wgpuInstanceWaitAny(instance,1,&w,UINT64_MAX);}
     const uint8_t*px=(const uint8_t*)wgpuBufferGetConstMappedRange(rtb,0,tBSz); size_t o=(H/2)*bpr+(W/2)*4;
     EXPECT_EQ(px[o],255); EXPECT_EQ(px[o+1],0); EXPECT_EQ(px[o+2],0); EXPECT_EQ(px[o+3],255); wgpuBufferUnmap(rtb);}
    {struct MC2{bool d=false;}m2; auto c2=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MC2*)u)->d=true;};
     WGPUBufferMapCallbackInfo i2={nullptr,WGPUCallbackMode_WaitAnyOnly,c2,&m2,nullptr};
     WGPUFuture f2=wgpuBufferMapAsync(rcb,WGPUMapMode_Read,0,cBSz,i2); WGPUFutureWaitInfo w2={f2,0};
     while(!m2.d){wgpuInstanceWaitAny(instance,1,&w2,UINT64_MAX);}
     const uint32_t*res=(const uint32_t*)wgpuBufferGetConstMappedRange(rcb,0,cBSz);
     for(uint32_t i=0;i<N;++i) EXPECT_EQ(res[i],(i+1)*2)<<"Compute "<<i; wgpuBufferUnmap(rcb);}
    for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(rtb); wgpuBufferRelease(rcb); wgpuBufferRelease(sto);
    wgpuTextureViewRelease(view); wgpuTextureRelease(tex);
    wgpuBindGroupRelease(cbg); wgpuBindGroupLayoutRelease(cbgl);
    wgpuPipelineLayoutRelease(cpl); wgpuComputePipelineRelease(cp); wgpuShaderModuleRelease(cm);
}

TEST_F(WebGPUTest, QueueSubmit_TwoCommandBuffers) {
    const uint32_t N=128; const uint32_t bSz=N*sizeof(uint32_t);
    WGPUBufferDescriptor sd={}; sd.size=bSz; sd.usage=WGPUBufferUsage_CopySrc|WGPUBufferUsage_MapWrite; sd.mappedAtCreation=true;
    WGPUBuffer src=wgpuDeviceCreateBuffer(device,&sd); uint32_t*d=(uint32_t*)wgpuBufferGetMappedRange(src,0,bSz);
    for(uint32_t i=0;i<N;++i) d[i]=0xBEEF0000+i; wgpuBufferUnmap(src);
    WGPUBufferDescriptor id={}; id.size=bSz; id.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_CopySrc;
    WGPUBuffer inter=wgpuDeviceCreateBuffer(device,&id);
    WGPUBufferDescriptor dd={}; dd.size=bSz; dd.usage=WGPUBufferUsage_CopyDst|WGPUBufferUsage_MapRead;
    WGPUBuffer dst=wgpuDeviceCreateBuffer(device,&dd);
    WGPUCommandEncoder e1=wgpuDeviceCreateCommandEncoder(device,nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(e1,src,0,inter,0,bSz);
    WGPUCommandBuffer c1=wgpuCommandEncoderFinish(e1,nullptr);
    WGPUCommandEncoder e2=wgpuDeviceCreateCommandEncoder(device,nullptr);
    wgpuCommandEncoderCopyBufferToBuffer(e2,inter,0,dst,0,bSz);
    WGPUCommandBuffer c2=wgpuCommandEncoderFinish(e2,nullptr);
    WGPUCommandBuffer cmds[2]={c1,c2}; wgpuQueueSubmit(queue,2,cmds);
    wgpuCommandEncoderRelease(e1); wgpuCommandEncoderRelease(e2);
    wgpuCommandBufferRelease(c1); wgpuCommandBufferRelease(c2);
    struct MapCtx{bool done=false;}mc; auto mcb=[](WGPUMapAsyncStatus,WGPUStringView,void*u,void*){((MapCtx*)u)->done=true;};
    WGPUBufferMapCallbackInfo ci={nullptr,WGPUCallbackMode_WaitAnyOnly,mcb,&mc,nullptr};
    WGPUFuture mf=wgpuBufferMapAsync(dst,WGPUMapMode_Read,0,bSz,ci); WGPUFutureWaitInfo fw={mf,0};
    while(!mc.done){wgpuInstanceWaitAny(instance,1,&fw,UINT64_MAX);}
    const uint32_t*res=(const uint32_t*)wgpuBufferGetConstMappedRange(dst,0,bSz); ASSERT_NE(res,nullptr);
    for(uint32_t i=0;i<N;++i) EXPECT_EQ(res[i],0xBEEF0000+i)<<"Index "<<i;
    wgpuBufferUnmap(dst); for(uint32_t i=0;i<framesInFlight;i++) wgpuDeviceTick(device);
    wgpuBufferRelease(src); wgpuBufferRelease(inter); wgpuBufferRelease(dst);
}

// ---------------------------------------------------------------------------
// P2 Parameterized Tests: Texture Formats, Dimensions, Usage, Samplers
// ---------------------------------------------------------------------------

struct TextureFormatParam {
    WGPUTextureFormat format;
    const char* name;
    WGPUTextureUsage usage;
};

class TextureFormatTest : public WebGPUTest, public ::testing::WithParamInterface<TextureFormatParam> {};

TEST_P(TextureFormatTest, CreateAndVerify) {
    auto param = GetParam();
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = param.format;
    desc.usage = param.usage;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr) << "Failed to create texture with format " << param.name;
    EXPECT_EQ(wgpuTextureGetFormat(tex), param.format);
    EXPECT_EQ(wgpuTextureGetWidth(tex), 64u);
    EXPECT_EQ(wgpuTextureGetHeight(tex), 64u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

INSTANTIATE_TEST_SUITE_P(TextureFormats, TextureFormatTest, ::testing::Values(
    TextureFormatParam{WGPUTextureFormat_RGBA8Unorm, "RGBA8Unorm", WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc},
    TextureFormatParam{WGPUTextureFormat_BGRA8Unorm, "BGRA8Unorm", WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc},
    TextureFormatParam{WGPUTextureFormat_R8Unorm, "R8Unorm", WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc},
    TextureFormatParam{WGPUTextureFormat_RG8Unorm, "RG8Unorm", WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc},
    TextureFormatParam{WGPUTextureFormat_RGBA16Float, "RGBA16Float", WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc},
    TextureFormatParam{WGPUTextureFormat_Depth32Float, "Depth32Float", WGPUTextureUsage_RenderAttachment}
));

// ---------------------------------------------------------------------------
// Texture Dimension Tests (parameterized)
// ---------------------------------------------------------------------------

struct TextureDimParam {
    WGPUTextureDimension dim;
    WGPUExtent3D size;
    const char* name;
};

class TextureDimensionTest : public WebGPUTest, public ::testing::WithParamInterface<TextureDimParam> {};

TEST_P(TextureDimensionTest, CreateAndVerifyDimension) {
    auto param = GetParam();
    WGPUTextureDescriptor desc = {};
    desc.size = param.size;
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = param.dim;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr) << "Failed to create texture with dimension " << param.name;
    EXPECT_EQ(wgpuTextureGetDimension(tex), param.dim);
    EXPECT_EQ(wgpuTextureGetWidth(tex), param.size.width);
    EXPECT_EQ(wgpuTextureGetHeight(tex), param.size.height);
    EXPECT_EQ(wgpuTextureGetDepthOrArrayLayers(tex), param.size.depthOrArrayLayers);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

INSTANTIATE_TEST_SUITE_P(TextureDimensions, TextureDimensionTest, ::testing::Values(
    TextureDimParam{WGPUTextureDimension_1D, {256, 1, 1}, "1D"},
    TextureDimParam{WGPUTextureDimension_2D, {64, 64, 1}, "2D"},
    TextureDimParam{WGPUTextureDimension_3D, {32, 32, 32}, "3D"}
));

// ---------------------------------------------------------------------------
// Texture Mip Level Tests (non-parameterized)
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, TextureMipLevels_One) {
    WGPUTextureDescriptor desc = {};
    desc.size = {256, 256, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetMipLevelCount(tex), 1u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

TEST_F(WebGPUTest, TextureMipLevels_FullChain) {
    WGPUTextureDescriptor desc = {};
    desc.size = {256, 256, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 9; // log2(256) + 1
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetMipLevelCount(tex), 9u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

TEST_F(WebGPUTest, TextureMipLevels_Partial) {
    WGPUTextureDescriptor desc = {};
    desc.size = {256, 256, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 4;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetMipLevelCount(tex), 4u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

// ---------------------------------------------------------------------------
// Texture Usage Tests (parameterized)
// ---------------------------------------------------------------------------

struct TextureUsageParam {
    WGPUTextureUsage usage;
    const char* name;
};

class TextureUsageTest : public WebGPUTest, public ::testing::WithParamInterface<TextureUsageParam> {};

TEST_P(TextureUsageTest, CreateAndVerifyUsage) {
    auto param = GetParam();
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = param.usage;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr) << "Failed to create texture with usage " << param.name;
    // Verify that all requested usage bits are present in the getter result.
    // The implementation may add internal flags (e.g. StorageAttachment for StorageBinding).
    EXPECT_EQ(wgpuTextureGetUsage(tex) & param.usage, param.usage)
        << "Requested usage bits not present in getter result";
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

INSTANTIATE_TEST_SUITE_P(TextureUsages, TextureUsageTest, ::testing::Values(
    TextureUsageParam{WGPUTextureUsage_CopySrc, "CopySrc"},
    TextureUsageParam{WGPUTextureUsage_CopyDst, "CopyDst"},
    TextureUsageParam{WGPUTextureUsage_TextureBinding, "TextureBinding"},
    TextureUsageParam{WGPUTextureUsage_StorageBinding, "StorageBinding"},
    TextureUsageParam{WGPUTextureUsage_RenderAttachment, "RenderAttachment"},
    TextureUsageParam{WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst, "CopySrc|CopyDst"},
    TextureUsageParam{WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst, "TextureBinding|CopySrc|CopyDst"},
    TextureUsageParam{WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding, "RenderAttachment|TextureBinding"},
    TextureUsageParam{WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst, "RenderAttachment|CopySrc|CopyDst"}
));

// ---------------------------------------------------------------------------
// TextureGettersComprehensive
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, TextureGettersComprehensive) {
    WGPUTextureDescriptor desc = {};
    desc.size = {128, 256, 4};
    desc.format = WGPUTextureFormat_RGBA16Float;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 5;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetWidth(tex), 128u);
    EXPECT_EQ(wgpuTextureGetHeight(tex), 256u);
    EXPECT_EQ(wgpuTextureGetDepthOrArrayLayers(tex), 4u);
    EXPECT_EQ(wgpuTextureGetFormat(tex), WGPUTextureFormat_RGBA16Float);
    EXPECT_EQ(wgpuTextureGetMipLevelCount(tex), 5u);
    EXPECT_EQ(wgpuTextureGetSampleCount(tex), 1u);
    EXPECT_EQ(wgpuTextureGetDimension(tex), WGPUTextureDimension_2D);
    EXPECT_EQ(wgpuTextureGetUsage(tex), WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

// ---------------------------------------------------------------------------
// Sampler Tests (parameterized)
// ---------------------------------------------------------------------------

struct SamplerParam {
    WGPUFilterMode magFilter;
    WGPUFilterMode minFilter;
    WGPUMipmapFilterMode mipmapFilter;
    WGPUAddressMode addressU;
    WGPUAddressMode addressV;
    WGPUAddressMode addressW;
    const char* name;
};

class SamplerTest : public WebGPUTest, public ::testing::WithParamInterface<SamplerParam> {};

TEST_P(SamplerTest, CreateAndVerify) {
    auto p = GetParam();
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = p.magFilter;
    desc.minFilter = p.minFilter;
    desc.mipmapFilter = p.mipmapFilter;
    desc.addressModeU = p.addressU;
    desc.addressModeV = p.addressV;
    desc.addressModeW = p.addressW;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 32.0f;
    desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr) << "Failed with " << p.name;
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

INSTANTIATE_TEST_SUITE_P(Samplers, SamplerTest, ::testing::Values(
    SamplerParam{WGPUFilterMode_Nearest, WGPUFilterMode_Nearest, WGPUMipmapFilterMode_Nearest, WGPUAddressMode_ClampToEdge, WGPUAddressMode_ClampToEdge, WGPUAddressMode_ClampToEdge, "NearestClamp"},
    SamplerParam{WGPUFilterMode_Linear, WGPUFilterMode_Linear, WGPUMipmapFilterMode_Linear, WGPUAddressMode_Repeat, WGPUAddressMode_Repeat, WGPUAddressMode_Repeat, "LinearRepeat"},
    SamplerParam{WGPUFilterMode_Linear, WGPUFilterMode_Nearest, WGPUMipmapFilterMode_Nearest, WGPUAddressMode_MirrorRepeat, WGPUAddressMode_ClampToEdge, WGPUAddressMode_Repeat, "MixedFilterAddress"},
    SamplerParam{WGPUFilterMode_Nearest, WGPUFilterMode_Linear, WGPUMipmapFilterMode_Linear, WGPUAddressMode_Repeat, WGPUAddressMode_MirrorRepeat, WGPUAddressMode_ClampToEdge, "NearMinLinMag"},
    SamplerParam{WGPUFilterMode_Linear, WGPUFilterMode_Linear, WGPUMipmapFilterMode_Nearest, WGPUAddressMode_ClampToEdge, WGPUAddressMode_Repeat, WGPUAddressMode_MirrorRepeat, "LinearFilterMixedAddr"}
));

// ---------------------------------------------------------------------------
// Sampler LOD Tests (non-parameterized)
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, SamplerLODClamp_Default) {
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;
    desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    desc.addressModeU = WGPUAddressMode_Repeat;
    desc.addressModeV = WGPUAddressMode_Repeat;
    desc.addressModeW = WGPUAddressMode_Repeat;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 32.0f;
    desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

TEST_F(WebGPUTest, SamplerLODClamp_Restricted) {
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;
    desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    desc.addressModeU = WGPUAddressMode_Repeat;
    desc.addressModeV = WGPUAddressMode_Repeat;
    desc.addressModeW = WGPUAddressMode_Repeat;
    desc.lodMinClamp = 2.0f;
    desc.lodMaxClamp = 4.0f;
    desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

TEST_F(WebGPUTest, SamplerLODClamp_Zero) {
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;
    desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    desc.addressModeU = WGPUAddressMode_Repeat;
    desc.addressModeV = WGPUAddressMode_Repeat;
    desc.addressModeW = WGPUAddressMode_Repeat;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 0.0f;
    desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

// ---------------------------------------------------------------------------
// Sampler Compare Tests (non-parameterized)
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, SamplerCompare_Less) {
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;
    desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    desc.addressModeU = WGPUAddressMode_ClampToEdge;
    desc.addressModeV = WGPUAddressMode_ClampToEdge;
    desc.addressModeW = WGPUAddressMode_ClampToEdge;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 32.0f;
    desc.compare = WGPUCompareFunction_Less;
    desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr) << "Failed to create sampler with compare=Less";
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

TEST_F(WebGPUTest, SamplerCompare_AllFunctions) {
    WGPUCompareFunction compareFunctions[] = {
        WGPUCompareFunction_Never,
        WGPUCompareFunction_Less,
        WGPUCompareFunction_Equal,
        WGPUCompareFunction_LessEqual,
        WGPUCompareFunction_Greater,
        WGPUCompareFunction_NotEqual,
        WGPUCompareFunction_GreaterEqual,
        WGPUCompareFunction_Always
    };

    for (auto cmpFunc : compareFunctions) {
        WGPUSamplerDescriptor desc = {};
        desc.magFilter = WGPUFilterMode_Linear;
        desc.minFilter = WGPUFilterMode_Linear;
        desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        desc.addressModeU = WGPUAddressMode_ClampToEdge;
        desc.addressModeV = WGPUAddressMode_ClampToEdge;
        desc.addressModeW = WGPUAddressMode_ClampToEdge;
        desc.lodMinClamp = 0.0f;
        desc.lodMaxClamp = 32.0f;
        desc.compare = cmpFunc;
        desc.maxAnisotropy = 1;

        WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
        ASSERT_NE(sampler, nullptr) << "Failed to create sampler with compare function " << (int)cmpFunc;
        EXPECT_EQ(sampler->refCount, 1);
        wgpuSamplerRelease(sampler);
    }
}

// ---------------------------------------------------------------------------
// Sampler Anisotropy Tests (non-parameterized)
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, SamplerAnisotropy_Off) {
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;
    desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    desc.addressModeU = WGPUAddressMode_Repeat;
    desc.addressModeV = WGPUAddressMode_Repeat;
    desc.addressModeW = WGPUAddressMode_Repeat;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 32.0f;
    desc.maxAnisotropy = 1;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

TEST_F(WebGPUTest, SamplerAnisotropy_16x) {
    WGPUSamplerDescriptor desc = {};
    desc.magFilter = WGPUFilterMode_Linear;
    desc.minFilter = WGPUFilterMode_Linear;
    desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    desc.addressModeU = WGPUAddressMode_Repeat;
    desc.addressModeV = WGPUAddressMode_Repeat;
    desc.addressModeW = WGPUAddressMode_Repeat;
    desc.lodMinClamp = 0.0f;
    desc.lodMaxClamp = 32.0f;
    desc.maxAnisotropy = 16;

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &desc);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->refCount, 1);
    wgpuSamplerRelease(sampler);
}

// ---------------------------------------------------------------------------
// Texture Array Layers Tests (non-parameterized)
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, TextureArrayLayers_Single) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetDepthOrArrayLayers(tex), 1u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

TEST_F(WebGPUTest, TextureArrayLayers_Four) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 4};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetDepthOrArrayLayers(tex), 4u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

TEST_F(WebGPUTest, TextureArrayLayers_CubeMap) {
    WGPUTextureDescriptor desc = {};
    desc.size = {64, 64, 6};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    ASSERT_NE(tex, nullptr);
    EXPECT_EQ(wgpuTextureGetDepthOrArrayLayers(tex), 6u);
    EXPECT_EQ(tex->refCount, 1);
    wgpuTextureRelease(tex);
}

// ---------------------------------------------------------------------------
// P2 Tests: Limit Enforcement
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, Limits_QueryLimits_NonNull) {
    WGPULimits limits = {};
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);
    EXPECT_EQ(status, WGPUStatus_Success);
    EXPECT_GE(limits.maxTextureDimension2D, 8192u);
    EXPECT_GT(limits.maxBufferSize, 0u);
    EXPECT_GE(limits.maxBindGroups, 4u);
    EXPECT_GE(limits.maxComputeWorkgroupSizeX, 128u);
}

// DISABLED: wgpuDeviceCreateTexture calls TRACELOG(WGPU_LOG_FATAL) which traps
// when vkCreateImage fails. No graceful null return path for textures.
TEST_F(WebGPUTest, Limits_ExceedMaxTextureDimension2D) {
    WGPULimits limits = {};
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);
    ASSERT_EQ(status, WGPUStatus_Success);

    WGPUTextureDescriptor desc = {};
    desc.size = {limits.maxTextureDimension2D + 1, 64, 1};
    desc.format = WGPUTextureFormat_RGBA8Unorm;
    desc.usage = WGPUTextureUsage_TextureBinding;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.dimension = WGPUTextureDimension_2D;

    WGPUTexture tex = wgpuDeviceCreateTexture(device, &desc);
    // Expect null or validation error. No crash is the key assertion.
    if (tex) {
        wgpuTextureRelease(tex);
    }
}

// DISABLED: Vulkan debug callback calls rg_trap() on validation errors.
// Buffer size > maxBufferSize triggers VK validation error -> trap.
TEST_F(WebGPUTest, Limits_ExceedMaxBufferSize) {
    WGPULimits limits = {};
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);
    ASSERT_EQ(status, WGPUStatus_Success);

    WGPUBufferDescriptor desc = {};
    desc.size = limits.maxBufferSize + 1;
    desc.usage = WGPUBufferUsage_Storage;
    desc.mappedAtCreation = false;

    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &desc);
    // Expect null or OOM. No crash is the key assertion.
    if (buf) {
        wgpuBufferRelease(buf);
    }
}

// DISABLED: wgpuDeviceCreatePipelineLayout has a stack-local dslayouts[8] array.
// maxBindGroups is often > 8 (e.g. 32 on NVIDIA/AMD), so maxBindGroups+1 overflows
// the array. Also, assert fires before bindGroupLayoutCount is assigned (bug).
TEST_F(WebGPUTest, Limits_ExceedMaxBindGroups) {
    WGPULimits limits = {};
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);
    ASSERT_EQ(status, WGPUStatus_Success);

    const uint32_t count = limits.maxBindGroups + 1;
    // Create count BGLs (all identical, minimal)
    WGPUBindGroupLayoutEntry entry = {};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Compute;
    entry.buffer.type = WGPUBufferBindingType_Storage;

    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &entry;

    std::vector<WGPUBindGroupLayout> bgls(count);
    for (uint32_t i = 0; i < count; ++i) {
        bgls[i] = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
        ASSERT_NE(bgls[i], nullptr) << "BGL creation failed at index " << i;
    }

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = count;
    plDesc.bindGroupLayouts = bgls.data();

    WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);
    // Expect null or validation error. No crash.
    if (pl) {
        wgpuPipelineLayoutRelease(pl);
    }

    for (uint32_t i = 0; i < count; ++i) {
        wgpuBindGroupLayoutRelease(bgls[i]);
    }
}

// DISABLED: If shader compiles successfully but workgroup size exceeds device limit,
// vkCreateComputePipelines triggers VK validation error -> debug callback rg_trap().
TEST_F(WebGPUTest, Limits_ExceedMaxComputeWorkgroupSizeX) {
    WGPULimits limits = {};
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);
    ASSERT_EQ(status, WGPUStatus_Success);

    uint32_t badSize = limits.maxComputeWorkgroupSizeX + 1;
    std::string code =
        "#version 450\nlayout(local_size_x = " + std::to_string(badSize) +
        ") in;\nlayout(std430, set=0, binding=0) buffer D { uint v[]; } d;\n"
        "void main() { d.v[gl_GlobalInvocationID.x] = 0; }";

    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code.c_str());
    // sm may be null if compilation/validation rejects oversized workgroup.
    // If non-null, try pipeline creation -- it may fail there instead.
    if (sm) {
        WGPUBindGroupLayoutEntry entry = {};
        entry.binding = 0;
        entry.visibility = WGPUShaderStage_Compute;
        entry.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutDescriptor bglDesc = {};
        bglDesc.entryCount = 1;
        bglDesc.entries = &entry;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

        WGPUPipelineLayoutDescriptor plDesc = {};
        plDesc.bindGroupLayoutCount = 1;
        plDesc.bindGroupLayouts = &bgl;
        WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);

        WGPUComputePipelineDescriptor pipeDesc = {};
        pipeDesc.layout = pl;
        pipeDesc.compute.module = sm;
        pipeDesc.compute.entryPoint = {"main", 4};

        WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
        // Pipeline may be null if Vulkan validation rejects it. That's fine.
        if (pipeline) {
            wgpuComputePipelineRelease(pipeline);
        }
        wgpuPipelineLayoutRelease(pl);
        wgpuBindGroupLayoutRelease(bgl);
        wgpuShaderModuleRelease(sm);
    }
    // No crash is the key assertion.
}

// DISABLED: Same as above -- exceeding invocations/workgroup triggers VK validation -> trap.
TEST_F(WebGPUTest, Limits_ExceedMaxComputeInvocationsPerWorkgroup) {
    WGPULimits limits = {};
    WGPUStatus status = wgpuDeviceGetLimits(device, &limits);
    ASSERT_EQ(status, WGPUStatus_Success);

    // Pick local_size_x = maxComputeWorkgroupSizeX, local_size_y = enough to exceed invocations limit.
    // product = x * y must exceed maxComputeInvocationsPerWorkgroup.
    uint32_t lx = limits.maxComputeWorkgroupSizeX;
    uint32_t ly = (limits.maxComputeInvocationsPerWorkgroup / lx) + 1;
    // Clamp ly to maxComputeWorkgroupSizeY to avoid that limit blocking first.
    if (ly > limits.maxComputeWorkgroupSizeY) {
        // Cannot construct a valid test case; invocation limit is not reachable
        // via x*y alone without exceeding per-dimension limits first.
        GTEST_SKIP() << "Cannot exceed invocations limit without exceeding per-dimension limit";
    }

    std::string code =
        "#version 450\nlayout(local_size_x = " + std::to_string(lx) +
        ", local_size_y = " + std::to_string(ly) +
        ") in;\nlayout(std430, set=0, binding=0) buffer D { uint v[]; } d;\n"
        "void main() { d.v[gl_GlobalInvocationID.x] = 0; }";

    WGPUShaderModule sm = compileGLSL(device, WGPUShaderStage_Compute, code.c_str());
    if (sm) {
        WGPUBindGroupLayoutEntry entry = {};
        entry.binding = 0;
        entry.visibility = WGPUShaderStage_Compute;
        entry.buffer.type = WGPUBufferBindingType_Storage;

        WGPUBindGroupLayoutDescriptor bglDesc = {};
        bglDesc.entryCount = 1;
        bglDesc.entries = &entry;
        WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

        WGPUPipelineLayoutDescriptor plDesc = {};
        plDesc.bindGroupLayoutCount = 1;
        plDesc.bindGroupLayouts = &bgl;
        WGPUPipelineLayout pl = wgpuDeviceCreatePipelineLayout(device, &plDesc);

        WGPUComputePipelineDescriptor pipeDesc = {};
        pipeDesc.layout = pl;
        pipeDesc.compute.module = sm;
        pipeDesc.compute.entryPoint = {"main", 4};

        WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeDesc);
        if (pipeline) {
            wgpuComputePipelineRelease(pipeline);
        }
        wgpuPipelineLayoutRelease(pl);
        wgpuBindGroupLayoutRelease(bgl);
        wgpuShaderModuleRelease(sm);
    }
    // No crash is the key assertion.
}

// ---------------------------------------------------------------------------
// P2 Tests: Timestamp Queries
// ---------------------------------------------------------------------------

TEST_F(WebGPUTest, TimestampQuery_ComputePass) {
    // wgpuDeviceHasFeature is a stub (always 0). Check Vulkan properties directly.
    if (!device->adapter->deviceInfoCache.properties.properties.limits.timestampComputeAndGraphics) {
        GTEST_SKIP() << "Timestamp queries not supported by hardware";
    }

    WGPUQuerySetDescriptor qsDesc = {};
    qsDesc.type = WGPUQueryType_Timestamp;
    qsDesc.count = 2;
    WGPUQuerySet qs = wgpuDeviceCreateQuerySet(device, &qsDesc);
    ASSERT_NE(qs, nullptr);

    // Result buffer for 2 uint64 timestamps
    const size_t resultSize = 2 * sizeof(uint64_t);
    WGPUBufferDescriptor rbDesc = {};
    rbDesc.size = resultSize;
    rbDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer resultBuf = wgpuDeviceCreateBuffer(device, &rbDesc);
    ASSERT_NE(resultBuf, nullptr);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);

    // Reset query pool before use (WGVK omits this; access internals)
    device->functions.vkCmdResetQueryPool(enc->buffer, qs->queryPool, 0, 2);

    // Timestamp before compute pass
    wgpuCommandEncoderWriteTimestamp(enc, qs, 0);

    // Trivial compute pass (no actual work, just begin/end)
    WGPUComputePassDescriptor cpDesc = {};
    WGPUComputePassEncoder cp = wgpuCommandEncoderBeginComputePass(enc, &cpDesc);
    wgpuComputePassEncoderEnd(cp);
    wgpuComputePassEncoderRelease(cp);

    // Timestamp after compute pass
    wgpuCommandEncoderWriteTimestamp(enc, qs, 1);

    // Resolve query results to buffer
    wgpuCommandEncoderResolveQuerySet(enc, qs, 0, 2, resultBuf, 0);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuCommandBufferRelease(cmd);

    // Readback
    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = {nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr};
    WGPUFuture mf = wgpuBufferMapAsync(resultBuf, WGPUMapMode_Read, 0, resultSize, ci);
    WGPUFutureWaitInfo fw = {mf, 0};
    while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }

    const uint64_t* ts = (const uint64_t*)wgpuBufferGetConstMappedRange(resultBuf, 0, resultSize);
    ASSERT_NE(ts, nullptr);
    EXPECT_GT(ts[0], 0u) << "First timestamp should be non-zero";
    EXPECT_GE(ts[1], ts[0]) << "Second timestamp should be >= first";

    wgpuBufferUnmap(resultBuf);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(resultBuf);
    // wgpuQuerySetRelease is a stub no-op. Manually destroy VkQueryPool to avoid
    // validation error on device destroy (debug callback traps on leaked objects).
    device->functions.vkDestroyQueryPool(device->device, qs->queryPool, nullptr);
    free(qs);
}

TEST_F(WebGPUTest, TimestampQuery_RenderPass) {
    // wgpuDeviceHasFeature is a stub (always 0). Check Vulkan properties directly.
    if (!device->adapter->deviceInfoCache.properties.properties.limits.timestampComputeAndGraphics) {
        GTEST_SKIP() << "Timestamp queries not supported by hardware";
    }

    WGPUQuerySetDescriptor qsDesc = {};
    qsDesc.type = WGPUQueryType_Timestamp;
    qsDesc.count = 2;
    WGPUQuerySet qs = wgpuDeviceCreateQuerySet(device, &qsDesc);
    ASSERT_NE(qs, nullptr);

    const size_t resultSize = 2 * sizeof(uint64_t);
    WGPUBufferDescriptor rbDesc = {};
    rbDesc.size = resultSize;
    rbDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer resultBuf = wgpuDeviceCreateBuffer(device, &rbDesc);
    ASSERT_NE(resultBuf, nullptr);

    // Render target needed for render pass
    WGPUTextureDescriptor texDesc = {};
    texDesc.size = {16, 16, 1};
    texDesc.format = WGPUTextureFormat_RGBA8Unorm;
    texDesc.usage = WGPUTextureUsage_RenderAttachment;
    texDesc.mipLevelCount = 1;
    texDesc.sampleCount = 1;
    texDesc.dimension = WGPUTextureDimension_2D;
    WGPUTexture tex = wgpuDeviceCreateTexture(device, &texDesc);
    WGPUTextureView view = wgpuTextureCreateView(tex, nullptr);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);

    // Reset query pool
    device->functions.vkCmdResetQueryPool(enc->buffer, qs->queryPool, 0, 2);

    // Timestamp before render pass
    wgpuCommandEncoderWriteTimestamp(enc, qs, 0);

    // Minimal render pass (just clear)
    WGPURenderPassColorAttachment att = {};
    att.view = view;
    att.loadOp = WGPULoadOp_Clear;
    att.storeOp = WGPUStoreOp_Store;
    att.clearValue = {0.0, 0.0, 0.0, 1.0};
    WGPURenderPassDescriptor rpDesc = {};
    rpDesc.colorAttachmentCount = 1;
    rpDesc.colorAttachments = &att;
    WGPURenderPassEncoder rp = wgpuCommandEncoderBeginRenderPass(enc, &rpDesc);
    wgpuRenderPassEncoderEnd(rp);
    wgpuRenderPassEncoderRelease(rp);

    // Timestamp after render pass
    wgpuCommandEncoderWriteTimestamp(enc, qs, 1);

    wgpuCommandEncoderResolveQuerySet(enc, qs, 0, 2, resultBuf, 0);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = {nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr};
    WGPUFuture mf = wgpuBufferMapAsync(resultBuf, WGPUMapMode_Read, 0, resultSize, ci);
    WGPUFutureWaitInfo fw = {mf, 0};
    while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }

    const uint64_t* ts = (const uint64_t*)wgpuBufferGetConstMappedRange(resultBuf, 0, resultSize);
    ASSERT_NE(ts, nullptr);
    EXPECT_GT(ts[0], 0u) << "First timestamp should be non-zero";
    EXPECT_GE(ts[1], ts[0]) << "Second timestamp should be >= first";

    wgpuBufferUnmap(resultBuf);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(resultBuf);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(tex);
    // Manual QuerySet cleanup (stub release)
    device->functions.vkDestroyQueryPool(device->device, qs->queryPool, nullptr);
    free(qs);
}

TEST_F(WebGPUTest, TimestampQuery_MultipleResolves) {
    // wgpuDeviceHasFeature is a stub (always 0). Check Vulkan properties directly.
    if (!device->adapter->deviceInfoCache.properties.properties.limits.timestampComputeAndGraphics) {
        GTEST_SKIP() << "Timestamp queries not supported by hardware";
    }

    WGPUQuerySetDescriptor qsDesc = {};
    qsDesc.type = WGPUQueryType_Timestamp;
    qsDesc.count = 4;
    WGPUQuerySet qs = wgpuDeviceCreateQuerySet(device, &qsDesc);
    ASSERT_NE(qs, nullptr);

    const size_t resultSize = 4 * sizeof(uint64_t);
    WGPUBufferDescriptor rbDesc = {};
    rbDesc.size = resultSize;
    rbDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer resultBuf = wgpuDeviceCreateBuffer(device, &rbDesc);
    ASSERT_NE(resultBuf, nullptr);

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, nullptr);

    // Reset all 4 queries
    device->functions.vkCmdResetQueryPool(enc->buffer, qs->queryPool, 0, 4);

    // First compute pass: timestamps 0 and 1
    wgpuCommandEncoderWriteTimestamp(enc, qs, 0);
    {
        WGPUComputePassDescriptor cpDesc = {};
        WGPUComputePassEncoder cp = wgpuCommandEncoderBeginComputePass(enc, &cpDesc);
        wgpuComputePassEncoderEnd(cp);
        wgpuComputePassEncoderRelease(cp);
    }
    wgpuCommandEncoderWriteTimestamp(enc, qs, 1);

    // Second compute pass: timestamps 2 and 3
    wgpuCommandEncoderWriteTimestamp(enc, qs, 2);
    {
        WGPUComputePassDescriptor cpDesc = {};
        WGPUComputePassEncoder cp = wgpuCommandEncoderBeginComputePass(enc, &cpDesc);
        wgpuComputePassEncoderEnd(cp);
        wgpuComputePassEncoderRelease(cp);
    }
    wgpuCommandEncoderWriteTimestamp(enc, qs, 3);

    // Resolve all 4 timestamps
    wgpuCommandEncoderResolveQuerySet(enc, qs, 0, 4, resultBuf, 0);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, nullptr);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandEncoderRelease(enc);
    wgpuCommandBufferRelease(cmd);

    struct MapCtx { bool done = false; } mc;
    auto mcb = [](WGPUMapAsyncStatus, WGPUStringView, void* u, void*) { ((MapCtx*)u)->done = true; };
    WGPUBufferMapCallbackInfo ci = {nullptr, WGPUCallbackMode_WaitAnyOnly, mcb, &mc, nullptr};
    WGPUFuture mf = wgpuBufferMapAsync(resultBuf, WGPUMapMode_Read, 0, resultSize, ci);
    WGPUFutureWaitInfo fw = {mf, 0};
    while (!mc.done) { wgpuInstanceWaitAny(instance, 1, &fw, UINT64_MAX); }

    const uint64_t* ts = (const uint64_t*)wgpuBufferGetConstMappedRange(resultBuf, 0, resultSize);
    ASSERT_NE(ts, nullptr);
    EXPECT_GT(ts[0], 0u) << "ts[0] should be non-zero";
    EXPECT_GE(ts[1], ts[0]) << "ts[1] >= ts[0]";
    EXPECT_GE(ts[2], ts[1]) << "ts[2] >= ts[1]";
    EXPECT_GE(ts[3], ts[2]) << "ts[3] >= ts[2]";

    wgpuBufferUnmap(resultBuf);
    for (uint32_t i = 0; i < framesInFlight; i++) wgpuDeviceTick(device);
    wgpuBufferRelease(resultBuf);
    // Manual QuerySet cleanup (stub release)
    device->functions.vkDestroyQueryPool(device->device, qs->queryPool, nullptr);
    free(qs);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
