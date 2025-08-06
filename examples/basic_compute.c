#include <wgvk.h>
#include <external/volk.h>
#include <stdio.h>
#include <external/incbin.h>
#include <string.h>
#ifndef STRVIEW
    #define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif
const uint32_t binary_data[] = {
    0x07230203, 0x00010300, 0x00170001, 0x0000002e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000017, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000005, 0x00000029, 0x706d6f63, 0x5f657475, 0x6e69616d, 0x00000000, 0x00000006,
    0x00060010, 0x00000029, 0x00000011, 0x00000001, 0x00000001, 0x00000001, 0x00050006, 0x00000003,
    0x00000000, 0x656e6e69, 0x00000072, 0x000a0005, 0x00000003, 0x61746164, 0x6f6c625f, 0x745f6b63,
    0x5f746e69, 0x6c707865, 0x74696369, 0x79616c5f, 0x0074756f, 0x000c0005, 0x00000006, 0x706d6f63,
    0x5f657475, 0x6e69616d, 0x6f6c675f, 0x5f6c6162, 0x6f766e69, 0x69746163, 0x695f6e6f, 0x6e495f64,
    0x00747570, 0x00070005, 0x0000000a, 0x706d6f63, 0x5f657475, 0x6e69616d, 0x6e6e695f, 0x00007265,
    0x00030005, 0x0000000c, 0x00006469, 0x00060005, 0x00000029, 0x706d6f63, 0x5f657475, 0x6e69616d,
    0x00000000, 0x00040047, 0x00000004, 0x00000006, 0x00000004, 0x00050048, 0x00000003, 0x00000000,
    0x00000023, 0x00000000, 0x00030047, 0x00000003, 0x00000002, 0x00040047, 0x00000001, 0x00000022,
    0x00000000, 0x00040047, 0x00000001, 0x00000021, 0x00000000, 0x00030047, 0x00000001, 0x00000017,
    0x00040047, 0x00000006, 0x0000000b, 0x0000001c, 0x00030016, 0x00000005, 0x00000020, 0x0003001d,
    0x00000004, 0x00000005, 0x0003001e, 0x00000003, 0x00000004, 0x00040020, 0x00000002, 0x0000000c,
    0x00000003, 0x0004003b, 0x00000002, 0x00000001, 0x0000000c, 0x00040015, 0x00000009, 0x00000020,
    0x00000000, 0x00040017, 0x00000008, 0x00000009, 0x00000003, 0x00040020, 0x00000007, 0x00000001,
    0x00000008, 0x0004003b, 0x00000007, 0x00000006, 0x00000001, 0x00020013, 0x0000000b, 0x00040021,
    0x0000000d, 0x0000000b, 0x00000008, 0x00040020, 0x00000011, 0x0000000c, 0x00000004, 0x0004002b,
    0x00000009, 0x00000012, 0x00000000, 0x0004002b, 0x00000009, 0x00000015, 0x00000001, 0x00040020,
    0x00000019, 0x0000000c, 0x00000005, 0x00030021, 0x0000002a, 0x0000000b, 0x00050036, 0x0000000b,
    0x0000000a, 0x00000000, 0x0000000d, 0x00030037, 0x00000008, 0x0000000c, 0x000200f8, 0x0000000e,
    0x00050051, 0x00000009, 0x0000000f, 0x0000000c, 0x00000000, 0x00050041, 0x00000011, 0x00000010,
    0x00000001, 0x00000012, 0x00050044, 0x00000009, 0x00000013, 0x00000001, 0x00000000, 0x00050082,
    0x00000009, 0x00000014, 0x00000013, 0x00000015, 0x0007000c, 0x00000009, 0x00000016, 0x00000017,
    0x00000026, 0x0000000f, 0x00000014, 0x00060041, 0x00000019, 0x00000018, 0x00000001, 0x00000012,
    0x00000016, 0x00050051, 0x00000009, 0x0000001a, 0x0000000c, 0x00000000, 0x00050041, 0x00000011,
    0x0000001b, 0x00000001, 0x00000012, 0x00050044, 0x00000009, 0x0000001c, 0x00000001, 0x00000000,
    0x00050082, 0x00000009, 0x0000001d, 0x0000001c, 0x00000015, 0x0007000c, 0x00000009, 0x0000001e,
    0x00000017, 0x00000026, 0x0000001a, 0x0000001d, 0x00060041, 0x00000019, 0x0000001f, 0x00000001,
    0x00000012, 0x0000001e, 0x0005003d, 0x00000005, 0x00000020, 0x0000001f, 0x00000000, 0x00050051,
    0x00000009, 0x00000021, 0x0000000c, 0x00000000, 0x00050041, 0x00000011, 0x00000022, 0x00000001,
    0x00000012, 0x00050044, 0x00000009, 0x00000023, 0x00000001, 0x00000000, 0x00050082, 0x00000009,
    0x00000024, 0x00000023, 0x00000015, 0x0007000c, 0x00000009, 0x00000025, 0x00000017, 0x00000026,
    0x00000021, 0x00000024, 0x00060041, 0x00000019, 0x00000026, 0x00000001, 0x00000012, 0x00000025,
    0x0005003d, 0x00000005, 0x00000027, 0x00000026, 0x00000000, 0x00050085, 0x00000005, 0x00000028,
    0x00000020, 0x00000027, 0x0004003e, 0x00000018, 0x00000028, 0x00000000, 0x000100fd, 0x00010038,
    0x00050036, 0x0000000b, 0x00000029, 0x00000000, 0x0000002a, 0x000200f8, 0x0000002b, 0x0005003d,
    0x00000008, 0x0000002c, 0x00000006, 0x00000000, 0x00050039, 0x0000000b, 0x0000002d, 0x0000000a,
    0x0000002c, 0x000100fd, 0x00010038
};

void adapterCallbackFunction(
        enum WGPURequestAdapterStatus status,
        WGPUAdapter adapter,
        struct WGPUStringView label,
        void* userdata1,
        void* userdata2
    ){
    *((WGPUAdapter*)userdata1) = adapter;
}
void deviceCallbackFunction(
        WGPURequestDeviceStatus status,
        WGPUDevice device,
        WGPUStringView message,
        void* userdata1,
        void* userdata2
    ){
    *((WGPUDevice*)userdata1) = device;
}

void reflectionCallback(WGPUReflectionInfoRequestStatus status, const WGPUReflectionInfo* reflectionInfo, void* userdata1, void* userdata2){
    for(uint32_t i = 0;i < reflectionInfo->globalCount;i++){

        const char* typedesc = NULL;

        if(reflectionInfo->globals[i].buffer.type != WGPUBufferBindingType_BindingNotUsed){
            typedesc = "buffer";
        }
        if(reflectionInfo->globals[i].texture.sampleType != WGPUTextureSampleType_BindingNotUsed){
            typedesc = "texture";
        }
        if(reflectionInfo->globals[i].sampler.type != WGPUSamplerBindingType_BindingNotUsed){
            typedesc = "sampler";
        }
        

        char namebuffer[256] = {0};
        
        const WGPUGlobalReflectionInfo* toBePrinted = reflectionInfo->globals + i;

        memcpy(namebuffer, toBePrinted->name.data, toBePrinted->name.length);
        printf("Name: %s, location: %u, type: %s", namebuffer, toBePrinted->binding, typedesc);
        if(reflectionInfo->globals[i].buffer.type != WGPUBufferBindingType_BindingNotUsed){
            printf(", minBindingSize = %d", (int)toBePrinted->buffer.minBindingSize);
        }
        printf("\n");
    }
}

int main(){
    WGPUInstanceLayerSelection lsel = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_InstanceLayerSelection
        }
    };
    const char* layernames[] = {"VK_LAYER_KHRONOS_validation"};
    lsel.instanceLayers = layernames;
    lsel.instanceLayerCount = 1;
    
    WGPUInstanceDescriptor instanceDescriptor = {
        .nextInChain = 
        #ifdef NDEBUG
        NULL
        #else
        &lsel.chain
        #endif
        ,
        .capabilities = {0}
    };

    WGPUInstance instance = wgpuCreateInstance(&instanceDescriptor);

    WGPURequestAdapterOptions adapterOptions = {0};
    adapterOptions.featureLevel = WGPUFeatureLevel_Core;

    WGPURequestAdapterCallbackInfo adapterCallback = {0};
    adapterCallback.callback = adapterCallbackFunction;
    WGPUAdapter requestedAdapter;
    adapterCallback.userdata1 = (void*)&requestedAdapter;
    

    WGPUFuture aFuture = wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallback);
    WGPUFutureWaitInfo winfo = {
        .future = aFuture,
        .completed = 0
    };

    wgpuInstanceWaitAny(instance, 1, &winfo, ~0ull);
    WGPUStringView deviceLabel = {"WGPU Device", sizeof("WGPU Device") - 1};

    WGPUDeviceDescriptor deviceDescriptor = {
        .nextInChain = 0,
        .label = deviceLabel,
        .requiredFeatureCount = 0,
        .requiredFeatures = NULL,
        .requiredLimits = NULL,
        .defaultQueue = {0},
        .deviceLostCallbackInfo = {0},
        .uncapturedErrorCallbackInfo = {0},
    };
    
    WGPUDevice device = NULL;
    WGPURequestDeviceCallbackInfo requestDeviceCallbackInfo = {
        .callback = deviceCallbackFunction,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .userdata1 = &device
    };
    WGPUFuture requestDeviceFuture = wgpuAdapterRequestDevice(requestedAdapter, &deviceDescriptor, requestDeviceCallbackInfo);
    WGPUFutureWaitInfo requestDeviceFutureWaitInfo = {
        .future = requestDeviceFuture,
        .completed = 0
    };
    wgpuInstanceWaitAny(instance, 1, &requestDeviceFutureWaitInfo, ~0ull);
    

    WGPUShaderSourceSPIRV computeSourceSpirv = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_ShaderSourceSPIRV
        },
        .code = binary_data,
        .codeSize = sizeof(binary_data)
    };
    WGPUShaderModuleDescriptor computeModuleDesc = {
        .nextInChain = &computeSourceSpirv.chain,
        .label = {
            .data   = "Compute Modul",
            .length = sizeof("Compute Modul"),
        }
    };

    WGPUShaderModule computeModule = wgpuDeviceCreateShaderModule(device, &computeModuleDesc);
    
    WGPUReflectionInfoCallbackInfo reflectionCallbackInfo = {
        .nextInChain = NULL,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = reflectionCallback,
        .userdata1 = NULL,
        .userdata2 = NULL
    };

    WGPUFuture computeReflectionFuture = wgpuShaderModuleGetReflectionInfo(computeModule, reflectionCallbackInfo);

    WGPUFutureWaitInfo reflectionWaitInfo[1] = {
        {
            .future =   computeReflectionFuture,
            .completed = 0
        },
    };
    wgpuInstanceWaitAny(instance, 1, reflectionWaitInfo, 1 << 30);
    WGPUComputePipelineDescriptor cplDesc = {
        .nextInChain = NULL,
        .label = STRVIEW("Kopmute paipline"),
        .compute = {
            .constantCount = 0,
            .constants = NULL,
            .entryPoint = STRVIEW("compute_main"),
            .module = computeModule,
            .nextInChain = NULL
        }
    };
    WGPUBindGroupLayoutEntry bglEntries[1] = {
        [0] = {
            .binding = 0,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type = WGPUBufferBindingType_Storage,
                .hasDynamicOffset = 0,
                .minBindingSize = 4
            }
        }
    };
    WGPUBindGroupLayoutDescriptor bglDesc = {
        .entries = bglEntries,
        .entryCount = 1
    };
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    WGPUPipelineLayout pllayout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor){
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &layout,
    });

    cplDesc.layout = pllayout;
    WGPUComputePipeline cpl = wgpuDeviceCreateComputePipeline(device, &cplDesc);
    
    WGPUBuffer stbuf = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = 64,
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_MapWrite
    });
    
    WGPUBuffer readableBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = 64,
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead
    });

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    float floatData[16] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    };

    WGPUBindGroupEntry entries[1] = {
        (WGPUBindGroupEntry){
            .binding = 0,
            .buffer = stbuf,
            .size = 64,
        }
    };

    wgpuQueueWriteBuffer(queue, stbuf, 0, floatData, sizeof(floatData));
    WGPUBindGroup group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
        .entries = entries,
        .entryCount = 1,
        .layout = layout
    });

    WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(device, NULL);
    WGPUComputePassEncoder cpenc = wgpuCommandEncoderBeginComputePass(cenc, NULL);
    wgpuComputePassEncoderSetPipeline(cpenc, cpl);
    wgpuComputePassEncoderSetBindGroup(cpenc, 0, group, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(cpenc, 16, 1, 1);
    wgpuComputePassEncoderEnd(cpenc);
    wgpuComputePassEncoderRelease(cpenc);
    wgpuCommandEncoderCopyBufferToBuffer(cenc, stbuf, 0, readableBuffer, 0, 64);
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cenc, NULL);
    wgpuCommandEncoderRelease(cenc);
    wgpuQueueSubmit(queue, 1, &cmdBuffer);
    wgpuCommandBufferRelease(cmdBuffer);

    float* floatRead = NULL;
    wgpuBufferMap(readableBuffer, WGPUMapMode_Read, 0, 64, (void**)&floatRead);
    for(int i = 0;i < 16;i++){
        printf("output value[%d] = %f\n", i, floatRead[i]);
    }
    wgpuBufferUnmap(readableBuffer);
    wgpuBufferRelease(readableBuffer);
    wgpuBufferRelease(stbuf);
    
    wgpuShaderModuleRelease(computeModule);
    wgpuComputePipelineRelease(cpl);
    wgpuBindGroupRelease(group);
    wgpuBindGroupLayoutRelease(layout);
    wgpuPipelineLayoutRelease(pllayout);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(requestedAdapter);
    wgpuInstanceRelease(instance);

    return 0;
}