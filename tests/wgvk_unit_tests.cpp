#include <cstdint>
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>

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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
