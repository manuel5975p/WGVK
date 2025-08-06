#include <wgvk.h>
#include <external/volk.h>
#include <stdio.h>
#include <external/incbin.h>
#include <string.h>
#include <stdlib.h>
#ifndef STRVIEW
    #define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif
const uint32_t computesquare_spv_data[] = {
    0x07230203, 0x00010000, 0x000d000b, 0x00000028, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0006000f, 0x00000005, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000b, 0x00060010, 0x00000004,
    0x00000011, 0x00000020, 0x00000001, 0x00000001, 0x00030003, 0x00000002, 0x000001c2, 0x000a0004,
    0x475f4c47, 0x4c474f4f, 0x70635f45, 0x74735f70, 0x5f656c79, 0x656e696c, 0x7269645f, 0x69746365,
    0x00006576, 0x00080004, 0x475f4c47, 0x4c474f4f, 0x6e695f45, 0x64756c63, 0x69645f65, 0x74636572,
    0x00657669, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00040005, 0x00000008, 0x65646e69,
    0x00000078, 0x00080005, 0x0000000b, 0x475f6c67, 0x61626f6c, 0x766e496c, 0x7461636f, 0x496e6f69,
    0x00000044, 0x00030005, 0x00000012, 0x0000424f, 0x00060006, 0x00000012, 0x00000000, 0x7074756f,
    0x61447475, 0x00006174, 0x00030005, 0x00000014, 0x00000000, 0x00030005, 0x00000019, 0x00004249,
    0x00060006, 0x00000019, 0x00000000, 0x75706e69, 0x74614474, 0x00000061, 0x00030005, 0x0000001b,
    0x00000000, 0x00040047, 0x0000000b, 0x0000000b, 0x0000001c, 0x00040047, 0x00000011, 0x00000006,
    0x00000004, 0x00030047, 0x00000012, 0x00000003, 0x00040048, 0x00000012, 0x00000000, 0x00000019,
    0x00050048, 0x00000012, 0x00000000, 0x00000023, 0x00000000, 0x00030047, 0x00000014, 0x00000019,
    0x00040047, 0x00000014, 0x00000021, 0x00000001, 0x00040047, 0x00000014, 0x00000022, 0x00000000,
    0x00040047, 0x00000018, 0x00000006, 0x00000004, 0x00030047, 0x00000019, 0x00000003, 0x00040048,
    0x00000019, 0x00000000, 0x00000018, 0x00050048, 0x00000019, 0x00000000, 0x00000023, 0x00000000,
    0x00030047, 0x0000001b, 0x00000018, 0x00040047, 0x0000001b, 0x00000021, 0x00000000, 0x00040047,
    0x0000001b, 0x00000022, 0x00000000, 0x00040047, 0x00000027, 0x0000000b, 0x00000019, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00040015, 0x00000006, 0x00000020, 0x00000000,
    0x00040020, 0x00000007, 0x00000007, 0x00000006, 0x00040017, 0x00000009, 0x00000006, 0x00000003,
    0x00040020, 0x0000000a, 0x00000001, 0x00000009, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000001,
    0x0004002b, 0x00000006, 0x0000000c, 0x00000000, 0x00040020, 0x0000000d, 0x00000001, 0x00000006,
    0x00030016, 0x00000010, 0x00000020, 0x0003001d, 0x00000011, 0x00000010, 0x0003001e, 0x00000012,
    0x00000011, 0x00040020, 0x00000013, 0x00000002, 0x00000012, 0x0004003b, 0x00000013, 0x00000014,
    0x00000002, 0x00040015, 0x00000015, 0x00000020, 0x00000001, 0x0004002b, 0x00000015, 0x00000016,
    0x00000000, 0x0003001d, 0x00000018, 0x00000010, 0x0003001e, 0x00000019, 0x00000018, 0x00040020,
    0x0000001a, 0x00000002, 0x00000019, 0x0004003b, 0x0000001a, 0x0000001b, 0x00000002, 0x00040020,
    0x0000001d, 0x00000002, 0x00000010, 0x0004002b, 0x00000006, 0x00000025, 0x00000020, 0x0004002b,
    0x00000006, 0x00000026, 0x00000001, 0x0006002c, 0x00000009, 0x00000027, 0x00000025, 0x00000026,
    0x00000026, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005,
    0x0004003b, 0x00000007, 0x00000008, 0x00000007, 0x00050041, 0x0000000d, 0x0000000e, 0x0000000b,
    0x0000000c, 0x0004003d, 0x00000006, 0x0000000f, 0x0000000e, 0x0003003e, 0x00000008, 0x0000000f,
    0x0004003d, 0x00000006, 0x00000017, 0x00000008, 0x0004003d, 0x00000006, 0x0000001c, 0x00000008,
    0x00060041, 0x0000001d, 0x0000001e, 0x0000001b, 0x00000016, 0x0000001c, 0x0004003d, 0x00000010,
    0x0000001f, 0x0000001e, 0x0004003d, 0x00000006, 0x00000020, 0x00000008, 0x00060041, 0x0000001d,
    0x00000021, 0x0000001b, 0x00000016, 0x00000020, 0x0004003d, 0x00000010, 0x00000022, 0x00000021,
    0x00050085, 0x00000010, 0x00000023, 0x0000001f, 0x00000022, 0x00060041, 0x0000001d, 0x00000024,
    0x00000014, 0x00000016, 0x00000017, 0x0003003e, 0x00000024, 0x00000023, 0x000100fd, 0x00010038
};
const size_t computesquare_spv_data_len = 1248;


const uint32_t computeroot_spv_data[] = {
    0x07230203, 0x00010000, 0x000d000b, 0x00000025, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0006000f, 0x00000005, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000b, 0x00060010, 0x00000004,
    0x00000011, 0x00000020, 0x00000001, 0x00000001, 0x00030003, 0x00000002, 0x000001c2, 0x000a0004,
    0x475f4c47, 0x4c474f4f, 0x70635f45, 0x74735f70, 0x5f656c79, 0x656e696c, 0x7269645f, 0x69746365,
    0x00006576, 0x00080004, 0x475f4c47, 0x4c474f4f, 0x6e695f45, 0x64756c63, 0x69645f65, 0x74636572,
    0x00657669, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00040005, 0x00000008, 0x65646e69,
    0x00000078, 0x00080005, 0x0000000b, 0x475f6c67, 0x61626f6c, 0x766e496c, 0x7461636f, 0x496e6f69,
    0x00000044, 0x00030005, 0x00000012, 0x0000424f, 0x00060006, 0x00000012, 0x00000000, 0x7074756f,
    0x61447475, 0x00006174, 0x00030005, 0x00000014, 0x00000000, 0x00030005, 0x00000019, 0x00004249,
    0x00060006, 0x00000019, 0x00000000, 0x75706e69, 0x74614474, 0x00000061, 0x00030005, 0x0000001b,
    0x00000000, 0x00040047, 0x0000000b, 0x0000000b, 0x0000001c, 0x00040047, 0x00000011, 0x00000006,
    0x00000004, 0x00030047, 0x00000012, 0x00000003, 0x00040048, 0x00000012, 0x00000000, 0x00000019,
    0x00050048, 0x00000012, 0x00000000, 0x00000023, 0x00000000, 0x00030047, 0x00000014, 0x00000019,
    0x00040047, 0x00000014, 0x00000021, 0x00000001, 0x00040047, 0x00000014, 0x00000022, 0x00000000,
    0x00040047, 0x00000018, 0x00000006, 0x00000004, 0x00030047, 0x00000019, 0x00000003, 0x00040048,
    0x00000019, 0x00000000, 0x00000018, 0x00050048, 0x00000019, 0x00000000, 0x00000023, 0x00000000,
    0x00030047, 0x0000001b, 0x00000018, 0x00040047, 0x0000001b, 0x00000021, 0x00000000, 0x00040047,
    0x0000001b, 0x00000022, 0x00000000, 0x00040047, 0x00000024, 0x0000000b, 0x00000019, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00040015, 0x00000006, 0x00000020, 0x00000000,
    0x00040020, 0x00000007, 0x00000007, 0x00000006, 0x00040017, 0x00000009, 0x00000006, 0x00000003,
    0x00040020, 0x0000000a, 0x00000001, 0x00000009, 0x0004003b, 0x0000000a, 0x0000000b, 0x00000001,
    0x0004002b, 0x00000006, 0x0000000c, 0x00000000, 0x00040020, 0x0000000d, 0x00000001, 0x00000006,
    0x00030016, 0x00000010, 0x00000020, 0x0003001d, 0x00000011, 0x00000010, 0x0003001e, 0x00000012,
    0x00000011, 0x00040020, 0x00000013, 0x00000002, 0x00000012, 0x0004003b, 0x00000013, 0x00000014,
    0x00000002, 0x00040015, 0x00000015, 0x00000020, 0x00000001, 0x0004002b, 0x00000015, 0x00000016,
    0x00000000, 0x0003001d, 0x00000018, 0x00000010, 0x0003001e, 0x00000019, 0x00000018, 0x00040020,
    0x0000001a, 0x00000002, 0x00000019, 0x0004003b, 0x0000001a, 0x0000001b, 0x00000002, 0x00040020,
    0x0000001d, 0x00000002, 0x00000010, 0x0004002b, 0x00000006, 0x00000022, 0x00000020, 0x0004002b,
    0x00000006, 0x00000023, 0x00000001, 0x0006002c, 0x00000009, 0x00000024, 0x00000022, 0x00000023,
    0x00000023, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005,
    0x0004003b, 0x00000007, 0x00000008, 0x00000007, 0x00050041, 0x0000000d, 0x0000000e, 0x0000000b,
    0x0000000c, 0x0004003d, 0x00000006, 0x0000000f, 0x0000000e, 0x0003003e, 0x00000008, 0x0000000f,
    0x0004003d, 0x00000006, 0x00000017, 0x00000008, 0x0004003d, 0x00000006, 0x0000001c, 0x00000008,
    0x00060041, 0x0000001d, 0x0000001e, 0x0000001b, 0x00000016, 0x0000001c, 0x0004003d, 0x00000010,
    0x0000001f, 0x0000001e, 0x0006000c, 0x00000010, 0x00000020, 0x00000001, 0x0000001f, 0x0000001f,
    0x00060041, 0x0000001d, 0x00000021, 0x00000014, 0x00000016, 0x00000017, 0x0003003e, 0x00000021,
    0x00000020, 0x000100fd, 0x00010038
};
const size_t computeroot_spv_data_len = 1196;

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
WGPUShaderModule compileComputeModule(WGPUDevice device, const uint32_t* spirvCode, size_t sizeInBytes){
        WGPUShaderSourceSPIRV squareSpirv = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_ShaderSourceSPIRV
        },
        .code = spirvCode,
        .codeSize = sizeInBytes
    };
    
    WGPUShaderModuleDescriptor computeModuleDesc = {
        .nextInChain = &squareSpirv.chain,
        .label = {
            .data   = "comp_pl",
            .length = sizeof("comp_pl"),
        }
    };
    return wgpuDeviceCreateShaderModule(device, &computeModuleDesc);
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
    

    WGPUShaderModule squareModule = compileComputeModule(device, computesquare_spv_data, computesquare_spv_data_len);
    WGPUShaderModule rootModule = compileComputeModule(device, computeroot_spv_data, computeroot_spv_data_len);
    
    WGPUReflectionInfoCallbackInfo reflectionCallbackInfo = {
        .nextInChain = NULL,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = reflectionCallback,
        .userdata1 = NULL,
        .userdata2 = NULL
    };

    WGPUComputePipelineDescriptor sqplDesc = {
        .nextInChain = NULL,
        .label = STRVIEW("Kopmute paipline"),
        .compute = {
            .constantCount = 0,
            .constants = NULL,
            .entryPoint = STRVIEW("main"),
            .module = squareModule,
            .nextInChain = NULL
        }
    };
    WGPUComputePipelineDescriptor rplDesc = {
        .nextInChain = NULL,
        .label = STRVIEW("Kopmute paipline"),
        .compute = {
            .constantCount = 0,
            .constants = NULL,
            .entryPoint = STRVIEW("main"),
            .module = rootModule,
            .nextInChain = NULL
        }
    };
    WGPUBindGroupLayoutEntry bglEntries[2] = {
        [0] = {
            .binding = 0,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type = WGPUBufferBindingType_ReadOnlyStorage,
                .hasDynamicOffset = 0,
                .minBindingSize = 4
            }
        },
        [1] = {
            .binding = 1,
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
        .entryCount = 2
    };
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    WGPUPipelineLayout pllayout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor){
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &layout,
    });

    sqplDesc.layout = pllayout;
    rplDesc.layout = pllayout;
    WGPUComputePipeline sqpl = wgpuDeviceCreateComputePipeline(device, &sqplDesc);
    WGPUComputePipeline rpl = wgpuDeviceCreateComputePipeline(device, &rplDesc);
    size_t floatCount = 32;
    WGPUBuffer buffer1 = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = floatCount * sizeof(float),
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst
    });

    WGPUBuffer buffer2 = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = floatCount * sizeof(float),
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst
    });
    
    WGPUBuffer readableBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = floatCount * sizeof(float),
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead
    });

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    
    float* floatData = calloc(floatCount, sizeof(float));
    for(uint32_t i = 0;i < floatCount;i++){
        floatData[i] = i;
    }

    WGPUBindGroupEntry entries[2] = {
        (WGPUBindGroupEntry){
            .binding = 0,
            .buffer = buffer1,
            .size = floatCount * sizeof(float),
        },
        (WGPUBindGroupEntry){
            .binding = 1,
            .buffer = buffer2,
            .size = floatCount * sizeof(float),
        },
    };
    
    WGPUBindGroupEntry entries2[2] = {
        (WGPUBindGroupEntry){
            .binding = 0,
            .buffer = buffer2,
            .size = floatCount * sizeof(float),
        },
        (WGPUBindGroupEntry){
            .binding = 1,
            .buffer = buffer1,
            .size = floatCount * sizeof(float),
        },
    };

    wgpuQueueWriteBuffer(queue, buffer1, 0, floatData, floatCount * sizeof(float));
    WGPUBindGroup group1 = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
        .entries = entries,
        .entryCount = 2,
        .layout = layout
    });
    WGPUBindGroup group2 = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
        .entries = entries2,
        .entryCount = 2,
        .layout = layout
    });

    WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(device, NULL);
    WGPUComputePassEncoder cpenc = wgpuCommandEncoderBeginComputePass(cenc, NULL);
    wgpuComputePassEncoderSetPipeline(cpenc, sqpl);
    wgpuComputePassEncoderSetBindGroup(cpenc, 0, group1, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(cpenc, 1, 1, 1);
    wgpuComputePassEncoderSetPipeline(cpenc, sqpl);
    wgpuComputePassEncoderSetBindGroup(cpenc, 0, group2, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(cpenc, 1, 1, 1);
    wgpuComputePassEncoderEnd(cpenc);
    wgpuComputePassEncoderRelease(cpenc);
    wgpuCommandEncoderCopyBufferToBuffer(cenc, buffer1, 0, readableBuffer, 0, 64);
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
    wgpuBufferRelease(buffer1);
    wgpuBufferRelease(buffer2);
    
    wgpuShaderModuleRelease(squareModule);
    wgpuShaderModuleRelease(rootModule);
    wgpuComputePipelineRelease(sqpl);
    wgpuComputePipelineRelease(rpl);
    wgpuBindGroupRelease(group1);
    wgpuBindGroupRelease(group2);
    wgpuBindGroupLayoutRelease(layout);
    wgpuPipelineLayoutRelease(pllayout);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(requestedAdapter);
    wgpuInstanceRelease(instance);

    return 0;
}