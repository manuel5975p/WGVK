// clang-format off

#ifndef WGPU_H_INCLUDED
#define WGPU_H_INCLUDED

#ifdef __cplusplus
#include <cstdint>
#include <cstddef>
extern "C"{
#else
#include <stdint.h>
#include <stddef.h>
#endif

#define WGPU_NULLABLE
#define WGPU_FUNCTION_ATTRIBUTE
#define VMA_MIN_ALIGNMENT 32

#if WGVK_DISABLE_ASSERT == 1234

    #if defined(__clang__)
        #define wgvk_assert(Condition, Message, ...) __builtin_assume(Condition)
    #elif defined(__GNUC__)
        #define wgvk_assert(Condition, Message, ...) do{if(!(Condition)){__builtin_unreachable();}}while(0)
    #elif defined(_MSC_VER)
        #include <intrin.h> // For __assume on MSVC
        #define wgvk_assert(Condition, Message, ...) __assume(Condition)
    #else
        #define wgvk_assert(Condition, Message, ...) ((void)0)
    #endif

#else
    #define wgvk_assert(Condition, Message, ...)                                                      \
    do {                                                                                              \
        char buffer_for_snprintf_sdfsd[4096] = {0};                                                   \
        snprintf(buffer_for_snprintf_sdfsd, 4096, "Internal bug: assertion failed: %s", Message);     \
        if (!(Condition)) {                                                                           \
            fprintf(stderr, "%s\n", buffer_for_snprintf_sdfsd);                                       \
            rg_trap();                                                                                \
            abort();                                                                                  \
        }                                                                                             \
    } while (0)
#endif

#if !defined(WGVK_SHARED_LIBRARY) || WGVK_SHARED_LIBRARY == 0 //Static case
    #ifdef __cplusplus
    #define WGVK_EXPORT extern "C"
    #else
    #define WGVK_EXPORT
    #endif
#else
    #if defined(_WIN32)
        #if defined(WGVK_EXPORTS) && WGVK_EXPORTS != 0
            #ifdef __cplusplus
                #define WGVK_EXPORT extern "C" __declspec(dllexport)
            #else
                #define WGVK_EXPORT __declspec(dllexport)
            #endif
        #else
            #ifdef __cplusplus
                #define WGVK_EXPORT extern "C" __declspec(dllimport)
            #else
                #define WGVK_EXPORT __declspec(dllimport)
            #endif
        #endif
    #else
        #ifdef __cplusplus
            #define WGVK_EXPORT extern "C" __attribute__((visibility("default")))
        #else
            #define WGVK_EXPORT __attribute__((visibility("default")))
        #endif
    #endif
#endif

#define WGPU_ENUM_ATTRIBUTE
#define WGPU_STRUCT_ATTRIBUTE
#define WGPU_STRUCTURE_ATTRIBUTE



typedef uint64_t WGPUFlags;
typedef uint32_t WGPUBool;
typedef uint32_t WGPUBool32;

struct WGPUTextureImpl;
struct WGPUTextureViewImpl;
struct WGPUBufferImpl;
struct WGPUBindGroupImpl;
struct WGPUBindGroupLayoutImpl;
struct WGPUPipelineLayoutImpl;
struct WGPUBufferImpl;
struct WGPUFutureImpl;
struct WGPURenderPassEncoderImpl;
struct WGPUComputePassEncoderImpl;
struct WGPURenderBundleImpl;
struct WGPURenderBundleEncoderImpl;
struct WGPUCommandEncoderImpl;
struct WGPUCommandBufferImpl;
struct WGPUTextureImpl;
struct WGPUTextureViewImpl;
struct WGPUQueueImpl;
struct WGPUQuerySetImpl;
struct WGPUInstanceImpl;
struct WGPUAdapterImpl;
struct WGPUDeviceImpl;
struct WGPUSurfaceImpl;
struct WGPUShaderModuleImpl;
struct WGPURenderPipelineImpl;
struct WGPUComputePipelineImpl;
struct WGPUTopLevelAccelerationStructureImpl;
struct WGPUBottomLevelAccelerationStructureImpl;
struct WGPURaytracingPipelineImpl;
struct WGPURaytracingPassEncoderImpl;

typedef struct WGPUSurfaceImpl* WGPUSurface;
typedef struct WGPUBindGroupLayoutImpl* WGPUBindGroupLayout;
typedef struct WGPUPipelineLayoutImpl* WGPUPipelineLayout;
typedef struct WGPUBindGroupImpl* WGPUBindGroup;
typedef struct WGPUBufferImpl* WGPUBuffer;
typedef struct WGPUQueueImpl* WGPUQueue;
typedef struct WGPUQuerySetImpl* WGPUQuerySet;
typedef struct WGPUInstanceImpl* WGPUInstance;
typedef struct WGPUAdapterImpl* WGPUAdapter;
typedef struct WGPUDeviceImpl* WGPUDevice;
typedef struct WGPURenderPassEncoderImpl* WGPURenderPassEncoder;
typedef struct WGPUComputePassEncoderImpl* WGPUComputePassEncoder;
typedef struct WGPURenderBundleImpl* WGPURenderBundle;
typedef struct WGPURenderBundleEncoderImpl* WGPURenderBundleEncoder;
typedef struct WGPUCommandBufferImpl* WGPUCommandBuffer;
typedef struct WGPUCommandEncoderImpl* WGPUCommandEncoder;
typedef struct WGPUTextureImpl* WGPUTexture;
typedef struct WGPUTextureViewImpl* WGPUTextureView;
typedef struct WGPUSamplerImpl* WGPUSampler;
typedef struct WGPUFenceImpl* WGPUFence;
typedef struct WGPURenderPipelineImpl* WGPURenderPipeline;
typedef struct WGPUShaderModuleImpl* WGPUShaderModule;
typedef struct WGPUComputePipelineImpl* WGPUComputePipeline;
typedef struct WGPURayTracingAccelerationContainerImpl* WGPURayTracingAccelerationContainer;
typedef struct WGPURayTracingShaderBindingTableImpl* WGPURayTracingShaderBindingTable;
typedef struct WGPURaytracingPipelineImpl* WGPURaytracingPipeline;
typedef struct WGPURaytracingPassEncoderImpl* WGPURaytracingPassEncoder;

typedef enum WGPUShaderStageEnum{
    WGPUShaderStageEnum_Vertex,
    WGPUShaderStageEnum_Fragment,
    WGPUShaderStageEnum_Compute,
    WGPUShaderStageEnum_TessControl,
    WGPUShaderStageEnum_TessEvaluation,
    WGPUShaderStageEnum_Geometry,
    WGPUShaderStageEnum_RayGen,
    WGPUShaderStageEnum_RayGenNV = WGPUShaderStageEnum_RayGen,
    WGPUShaderStageEnum_Intersect,
    WGPUShaderStageEnum_IntersectNV = WGPUShaderStageEnum_Intersect,
    WGPUShaderStageEnum_AnyHit,
    WGPUShaderStageEnum_AnyHitNV = WGPUShaderStageEnum_AnyHit,
    WGPUShaderStageEnum_ClosestHit,
    WGPUShaderStageEnum_ClosestHitNV = WGPUShaderStageEnum_ClosestHit,
    WGPUShaderStageEnum_Miss,
    WGPUShaderStageEnum_MissNV = WGPUShaderStageEnum_Miss,
    WGPUShaderStageEnum_Callable,
    WGPUShaderStageEnum_CallableNV = WGPUShaderStageEnum_Callable,
    WGPUShaderStageEnum_Task,
    WGPUShaderStageEnum_TaskNV = WGPUShaderStageEnum_Task,
    WGPUShaderStageEnum_Mesh,
    WGPUShaderStageEnum_MeshNV = WGPUShaderStageEnum_Mesh,
    WGPUShaderStageEnum_EnumCount,
    WGPUShaderStageEnum_Force32 = 0x7FFFFFFF
}WGPUShaderStageEnum;

typedef WGPUFlags WGPUShaderStage;
static const WGPUShaderStage WGPUShaderStage_None = 0;
static const WGPUShaderStage WGPUShaderStage_Vertex = (((WGPUFlags)1) << WGPUShaderStageEnum_Vertex);
static const WGPUShaderStage WGPUShaderStage_TessControl = (((WGPUFlags)1) << WGPUShaderStageEnum_TessControl);
static const WGPUShaderStage WGPUShaderStage_TessEvaluation = (((WGPUFlags)1) << WGPUShaderStageEnum_TessEvaluation);
static const WGPUShaderStage WGPUShaderStage_Geometry = (((WGPUFlags)1) << WGPUShaderStageEnum_Geometry);
static const WGPUShaderStage WGPUShaderStage_Fragment = (((WGPUFlags)1) << WGPUShaderStageEnum_Fragment);
static const WGPUShaderStage WGPUShaderStage_Compute = (((WGPUFlags)1) << WGPUShaderStageEnum_Compute);
static const WGPUShaderStage WGPUShaderStage_RayGen = (((WGPUFlags)1) << WGPUShaderStageEnum_RayGen);
static const WGPUShaderStage WGPUShaderStage_RayGenNV = (((WGPUFlags)1) << WGPUShaderStageEnum_RayGenNV);
static const WGPUShaderStage WGPUShaderStage_Intersect = (((WGPUFlags)1) << WGPUShaderStageEnum_Intersect);
static const WGPUShaderStage WGPUShaderStage_IntersectNV = (((WGPUFlags)1) << WGPUShaderStageEnum_IntersectNV);
static const WGPUShaderStage WGPUShaderStage_AnyHit = (((WGPUFlags)1) << WGPUShaderStageEnum_AnyHit);
static const WGPUShaderStage WGPUShaderStage_AnyHitNV = (((WGPUFlags)1) << WGPUShaderStageEnum_AnyHitNV);
static const WGPUShaderStage WGPUShaderStage_ClosestHit = (((WGPUFlags)1) << WGPUShaderStageEnum_ClosestHit);
static const WGPUShaderStage WGPUShaderStage_ClosestHitNV = (((WGPUFlags)1) << WGPUShaderStageEnum_ClosestHitNV);
static const WGPUShaderStage WGPUShaderStage_Miss = (((WGPUFlags)1) << WGPUShaderStageEnum_Miss);
static const WGPUShaderStage WGPUShaderStage_MissNV = (((WGPUFlags)1) << WGPUShaderStageEnum_MissNV);
static const WGPUShaderStage WGPUShaderStage_Callable = (((WGPUFlags)1) << WGPUShaderStageEnum_Callable);
static const WGPUShaderStage WGPUShaderStage_CallableNV = (((WGPUFlags)1) << WGPUShaderStageEnum_CallableNV);
static const WGPUShaderStage WGPUShaderStage_Task = (((WGPUFlags)1) << WGPUShaderStageEnum_Task);
static const WGPUShaderStage WGPUShaderStage_TaskNV = (((WGPUFlags)1) << WGPUShaderStageEnum_TaskNV);
static const WGPUShaderStage WGPUShaderStage_Mesh = (((WGPUFlags)1) << WGPUShaderStageEnum_Mesh);
static const WGPUShaderStage WGPUShaderStage_MeshNV = (((WGPUFlags)1) << WGPUShaderStageEnum_MeshNV);
static const WGPUShaderStage WGPUShaderStage_EnumCount = (((WGPUFlags)1) << WGPUShaderStageEnum_EnumCount);

typedef WGPUFlags WGPUTextureUsage;
static const WGPUTextureUsage WGPUTextureUsage_None = 0x0000000000000000;
static const WGPUTextureUsage WGPUTextureUsage_CopySrc = 0x0000000000000001;
static const WGPUTextureUsage WGPUTextureUsage_CopyDst = 0x0000000000000002;
static const WGPUTextureUsage WGPUTextureUsage_TextureBinding = 0x0000000000000004;
static const WGPUTextureUsage WGPUTextureUsage_StorageBinding = 0x0000000000000008;
static const WGPUTextureUsage WGPUTextureUsage_RenderAttachment = 0x0000000000000010;
static const WGPUTextureUsage WGPUTextureUsage_TransientAttachment = 0x0000000000001000;
static const WGPUTextureUsage WGPUTextureUsage_StorageAttachment = 0x0000000000002000;

typedef WGPUFlags WGPUBufferUsage;
static const WGPUBufferUsage WGPUBufferUsage_None = 0x0000000000000000;
static const WGPUBufferUsage WGPUBufferUsage_MapRead = 0x0000000000000001;
static const WGPUBufferUsage WGPUBufferUsage_MapWrite = 0x0000000000000002;
static const WGPUBufferUsage WGPUBufferUsage_CopySrc = 0x0000000000000004;
static const WGPUBufferUsage WGPUBufferUsage_CopyDst = 0x0000000000000008;
static const WGPUBufferUsage WGPUBufferUsage_Index = 0x0000000000000010;
static const WGPUBufferUsage WGPUBufferUsage_Vertex = 0x0000000000000020;
static const WGPUBufferUsage WGPUBufferUsage_Uniform = 0x0000000000000040;
static const WGPUBufferUsage WGPUBufferUsage_Storage = 0x0000000000000080;
static const WGPUBufferUsage WGPUBufferUsage_Indirect = 0x0000000000000100;
static const WGPUBufferUsage WGPUBufferUsage_QueryResolve = 0x0000000000000200;
static const WGPUBufferUsage WGPUBufferUsage_ShaderDeviceAddress          = 0x0000000010000000;
static const WGPUBufferUsage WGPUBufferUsage_AccelerationStructureInput   = 0x0000000020000000;
static const WGPUBufferUsage WGPUBufferUsage_AccelerationStructureStorage = 0x0000000040000000;
static const WGPUBufferUsage WGPUBufferUsage_ShaderBindingTable           = 0x0000000080000000;
static const WGPUBufferUsage WGPUBufferUsage_Raytracing                   = 0x00000000F0000080;

typedef WGPUFlags WGPUColorWriteMask;
static const WGPUColorWriteMask WGPUColorWriteMask_None = 0x0000000000000000;
static const WGPUColorWriteMask WGPUColorWriteMask_Red = 0x0000000000000001;
static const WGPUColorWriteMask WGPUColorWriteMask_Green = 0x0000000000000002;
static const WGPUColorWriteMask WGPUColorWriteMask_Blue = 0x0000000000000004;
static const WGPUColorWriteMask WGPUColorWriteMask_Alpha = 0x0000000000000008;
static const WGPUColorWriteMask WGPUColorWriteMask_All = 0x000000000000000F;

typedef enum WGPUStatus {
    WGPUStatus_Success = 0x00000001,
    WGPUStatus_Error = 0x00000002,
    WGPUStatus_Force32 = 0x7FFFFFFF
} WGPUStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUWaitStatus {
    WGPUWaitStatus_Success  = 0x00000001,
    WGPUWaitStatus_TimedOut = 0x00000002,
    WGPUWaitStatus_Error    = 0x00000003,
    WGPUWaitStatus_Force32  = 0x7FFFFFFF
} WGPUWaitStatus;

typedef enum WGPUPresentMode{
    WGPUPresentMode_Undefined = 0x00000000,
    WGPUPresentMode_Fifo = 0x00000001,
    WGPUPresentMode_FifoRelaxed = 0x00000002,
    WGPUPresentMode_Immediate = 0x00000003,
    WGPUPresentMode_Mailbox = 0x00000004,
} WGPUPresentMode;

typedef enum WGPUTextureAspect {
    WGPUTextureAspect_Undefined   = 0x00000000,
    WGPUTextureAspect_All         = 0x00000001,
    WGPUTextureAspect_StencilOnly = 0x00000002,
    WGPUTextureAspect_DepthOnly   = 0x00000003,
    WGPUTextureAspect_Plane0Only  = 0x00050000,
    WGPUTextureAspect_Plane1Only  = 0x00050001,
    WGPUTextureAspect_Plane2Only  = 0x00050002,
    WGPUTextureAspect_Force32     = 0x7FFFFFFF
} WGPUTextureAspect;

typedef enum WGPUPrimitiveTopology {
    WGPUPrimitiveTopology_Undefined = 0x00000000,
    WGPUPrimitiveTopology_PointList = 0x00000001,
    WGPUPrimitiveTopology_LineList = 0x00000002,
    WGPUPrimitiveTopology_LineStrip = 0x00000003,
    WGPUPrimitiveTopology_TriangleList = 0x00000004,
    WGPUPrimitiveTopology_TriangleStrip = 0x00000005,
    WGPUPrimitiveTopology_Force32 = 0x7FFFFFFF
} WGPUPrimitiveTopology WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUSType {
    WGPUSType_ShaderSourceSPIRV = 0x00000001,
    WGPUSType_ShaderSourceWGSL = 0x00000002,
    WGPUSType_SurfaceSourceMetalLayer = 0x00000004,
    WGPUSType_SurfaceSourceWindowsHWND = 0x00000005,
    WGPUSType_SurfaceSourceXlibWindow = 0x00000006,
    WGPUSType_SurfaceSourceWaylandSurface = 0x00000007,
    WGPUSType_SurfaceSourceAndroidNativeWindow = 0x00000008,
    WGPUSType_SurfaceSourceXCBWindow = 0x00000009,
    WGPUSType_SurfaceColorManagement = 0x0000000A,
    WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector = 0x00040000,
    // non-standard sTypes
    WGPUSType_TextureComponentSwizzleDescriptor = 0x00050047,
    WGPUSType_InstanceLayerSelection = 0x10000001,
    WGPUSType_BufferAllocatorSelector = 0x10000002,
    WGPUSType_ShaderSourceGLSL = 0x10000003,
    WGPUSType_PrimitiveLineWidthInfo = 0x10000004,
    WGPUSType_SurfaceSourceDrmPlane = 0x10000005,
}WGPUSType WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUCallbackMode {
    WGPUCallbackMode_WaitAnyOnly = 0x00000001,
    WGPUCallbackMode_AllowProcessEvents = 0x00000002,
    WGPUCallbackMode_AllowSpontaneous = 0x00000003,
    WGPUCallbackMode_Force32 = 0x7FFFFFFF
} WGPUCallbackMode;

typedef struct WGPUStringView{
    const char* data;
    size_t length;
}WGPUStringView;

#define WGPU_ARRAY_LAYER_COUNT_UNDEFINED (UINT32_MAX)
#define WGPU_COPY_STRIDE_UNDEFINED (UINT32_MAX)
#define WGPU_DEPTH_CLEAR_VALUE_UNDEFINED (NAN)
#define WGPU_DEPTH_SLICE_UNDEFINED (UINT32_MAX)
#define WGPU_LIMIT_U32_UNDEFINED (UINT32_MAX)
#define WGPU_LIMIT_U64_UNDEFINED (UINT64_MAX)
#define WGPU_MIP_LEVEL_COUNT_UNDEFINED (UINT32_MAX)
#define WGPU_QUERY_SET_INDEX_UNDEFINED (UINT32_MAX)
#define WGPU_STRLEN (SIZE_MAX)
#define WGPU_WHOLE_MAP_SIZE (SIZE_MAX)
#define WGPU_WHOLE_SIZE (UINT64_MAX)

typedef struct WGPUTexelCopyBufferLayout {
    uint64_t offset;
    uint32_t bytesPerRow;
    uint32_t rowsPerImage;
} WGPUTexelCopyBufferLayout;

typedef enum WGPUCompareFunction {
    WGPUCompareFunction_Undefined = 0x00000000,
    WGPUCompareFunction_Never = 0x00000001,
    WGPUCompareFunction_Less = 0x00000002,
    WGPUCompareFunction_Equal = 0x00000003,
    WGPUCompareFunction_LessEqual = 0x00000004,
    WGPUCompareFunction_Greater = 0x00000005,
    WGPUCompareFunction_NotEqual = 0x00000006,
    WGPUCompareFunction_GreaterEqual = 0x00000007,
    WGPUCompareFunction_Always = 0x00000008,
    WGPUCompareFunction_Force32 = 0x7fffffff,
} WGPUCompareFunction;

typedef WGPUFlags WGPUMapMode;
static const WGPUMapMode WGPUMapMode_None = 0x0000000000000000;
static const WGPUMapMode WGPUMapMode_Read = 0x0000000000000001;
static const WGPUMapMode WGPUMapMode_Write = 0x0000000000000002;

typedef enum WGPUTextureDimension{
    WGPUTextureDimension_Undefined = 0x00000000,
    WGPUTextureDimension_1D        = 0x00000001,
    WGPUTextureDimension_2D        = 0x00000002,
    WGPUTextureDimension_3D        = 0x00000003,
    WGPUTextureDimension_Force32   = 0x7fffffff
}WGPUTextureDimension;



typedef enum WGPUTextureViewDimension{
    WGPUTextureViewDimension_Undefined = 0x00000000,
    WGPUTextureViewDimension_1D = 0x00000001,
    WGPUTextureViewDimension_2D = 0x00000002,
    WGPUTextureViewDimension_2DArray = 0x00000003,
    WGPUTextureViewDimension_Cube = 0x00000004,
    WGPUTextureViewDimension_CubeArray = 0x00000005,
    WGPUTextureViewDimension_3D = 0x00000006,
    WGPUTextureViewDimension_Force32 = 0x7FFFFFFF
}WGPUTextureViewDimension;

typedef enum WGPUOptionalBool {
    WGPUOptionalBool_False = 0x00000000,
    WGPUOptionalBool_True = 0x00000001,
    WGPUOptionalBool_Undefined = 0x00000002,
    WGPUOptionalBool_Force32 = 0x7FFFFFFF
} WGPUOptionalBool WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUCullMode{
    WGPUCullMode_Undefined = 0x00000000,
    WGPUCullMode_None = 0x00000001,
    WGPUCullMode_Front = 0x00000002,
    WGPUCullMode_Back = 0x00000003,
    WGPUCullMode_Force32 = 0x7FFFFFFF
} WGPUCullMode;

typedef enum WGPULoadOp {
    WGPULoadOp_Undefined = 0x00000000,
    WGPULoadOp_Load = 0x00000001,
    WGPULoadOp_Clear = 0x00000002,
    WGPULoadOp_ExpandResolveTexture = 0x00050003,
    WGPULoadOp_Force32 = 0x7FFFFFFF
} WGPULoadOp;

typedef enum WGPUStoreOp {
    WGPUStoreOp_Undefined = 0x00000000,
    WGPUStoreOp_Store = 0x00000001,
    WGPUStoreOp_Discard = 0x00000002,
    WGPUStoreOp_Force32 = 0x7FFFFFFF
} WGPUStoreOp;

typedef enum WGPUFrontFace {
    WGPUFrontFace_Undefined = 0x00000000,
    WGPUFrontFace_CCW = 0x00000001,
    WGPUFrontFace_CW = 0x00000002,
    WGPUFrontFace_Force32 = 0x7FFFFFFF
} WGPUFrontFace WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUVertexStepMode { 
    WGPUVertexStepMode_Undefined = 0x0, 
    WGPUVertexStepMode_Vertex = 0x1,
    WGPUVertexStepMode_Instance = 0x2,
    WGPUVertexStepMode_Force32 = 0x7FFFFFFF 
} WGPUVertexStepMode;

typedef enum WGPUIndexFormat {
    WGPUIndexFormat_Undefined = 0x00000000,
    WGPUIndexFormat_Uint16 = 0x00000001,
    WGPUIndexFormat_Uint32 = 0x00000002,
    WGPUIndexFormat_Force32 = 0x7FFFFFFF
} WGPUIndexFormat WGPU_ENUM_ATTRIBUTE;

typedef enum WGPURequestAdapterStatus {
    WGPURequestAdapterStatus_Success = 0x00000001,
    WGPURequestAdapterStatus_CallbackCancelled = 0x00000002,
    WGPURequestAdapterStatus_Unavailable = 0x00000003,
    WGPURequestAdapterStatus_Error = 0x00000004,
    WGPURequestAdapterStatus_Force32 = 0x7FFFFFFF
} WGPURequestAdapterStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPURequestDeviceStatus {
    WGPURequestDeviceStatus_Success = 0x00000001,
    WGPURequestDeviceStatus_CallbackCancelled = 0x00000002,
    WGPURequestDeviceStatus_Error = 0x00000003,
    WGPURequestDeviceStatus_Force32 = 0x7FFFFFFF
} WGPURequestDeviceStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUBufferBindingType {
    WGPUBufferBindingType_BindingNotUsed = 0x00000000,
    WGPUBufferBindingType_Undefined = 0x00000001,
    WGPUBufferBindingType_Uniform = 0x00000002,
    WGPUBufferBindingType_Storage = 0x00000003,
    WGPUBufferBindingType_ReadOnlyStorage = 0x00000004,
    WGPUBufferBindingType_Force32 = 0x7FFFFFFF
} WGPUBufferBindingType;

typedef enum WGPUSamplerBindingType {
    WGPUSamplerBindingType_BindingNotUsed = 0x00000000,
    WGPUSamplerBindingType_Undefined = 0x00000001,
    WGPUSamplerBindingType_Filtering = 0x00000002,
    WGPUSamplerBindingType_NonFiltering = 0x00000003,
    WGPUSamplerBindingType_Comparison = 0x00000004,
    WGPUSamplerBindingType_Force32 = 0x7FFFFFFF
} WGPUSamplerBindingType;

typedef enum WGPUStorageTextureAccess {
    WGPUStorageTextureAccess_BindingNotUsed = 0x00000000,
    WGPUStorageTextureAccess_Undefined = 0x00000001,
    WGPUStorageTextureAccess_WriteOnly = 0x00000002,
    WGPUStorageTextureAccess_ReadOnly = 0x00000003,
    WGPUStorageTextureAccess_ReadWrite = 0x00000004,
    WGPUStorageTextureAccess_Force32 = 0x7FFFFFFF
} WGPUStorageTextureAccess;

typedef enum WGPUTextureFormat {
    WGPUTextureFormat_Undefined = 0x00000000,
    WGPUTextureFormat_R8Unorm = 0x00000001,
    WGPUTextureFormat_R8Snorm = 0x00000002,
    WGPUTextureFormat_R8Uint = 0x00000003,
    WGPUTextureFormat_R8Sint = 0x00000004,
    WGPUTextureFormat_R16Unorm = 0x00000005,
    WGPUTextureFormat_R16Snorm = 0x00000006,
    WGPUTextureFormat_R16Uint = 0x00000007,
    WGPUTextureFormat_R16Sint = 0x00000008,
    WGPUTextureFormat_R16Float = 0x00000009,
    WGPUTextureFormat_RG8Unorm = 0x0000000A,
    WGPUTextureFormat_RG8Snorm = 0x0000000B,
    WGPUTextureFormat_RG8Uint = 0x0000000C,
    WGPUTextureFormat_RG8Sint = 0x0000000D,
    WGPUTextureFormat_R32Float = 0x0000000E,
    WGPUTextureFormat_R32Uint = 0x0000000F,
    WGPUTextureFormat_R32Sint = 0x00000010,
    WGPUTextureFormat_RG16Unorm = 0x00000011,
    WGPUTextureFormat_RG16Snorm = 0x00000012,
    WGPUTextureFormat_RG16Uint = 0x00000013,
    WGPUTextureFormat_RG16Sint = 0x00000014,
    WGPUTextureFormat_RG16Float = 0x00000015,
    WGPUTextureFormat_RGBA8Unorm = 0x00000016,
    WGPUTextureFormat_RGBA8UnormSrgb = 0x00000017,
    WGPUTextureFormat_RGBA8Snorm = 0x00000018,
    WGPUTextureFormat_RGBA8Uint = 0x00000019,
    WGPUTextureFormat_RGBA8Sint = 0x0000001A,
    WGPUTextureFormat_BGRA8Unorm = 0x0000001B,
    WGPUTextureFormat_BGRA8UnormSrgb = 0x0000001C,
    WGPUTextureFormat_RGB10A2Uint = 0x0000001D,
    WGPUTextureFormat_RGB10A2Unorm = 0x0000001E,
    WGPUTextureFormat_RG11B10Ufloat = 0x0000001F,
    WGPUTextureFormat_RGB9E5Ufloat = 0x00000020,
    WGPUTextureFormat_RG32Float = 0x00000021,
    WGPUTextureFormat_RG32Uint = 0x00000022,
    WGPUTextureFormat_RG32Sint = 0x00000023,
    WGPUTextureFormat_RGBA16Unorm = 0x00000024,
    WGPUTextureFormat_RGBA16Snorm = 0x00000025,
    WGPUTextureFormat_RGBA16Uint = 0x00000026,
    WGPUTextureFormat_RGBA16Sint = 0x00000027,
    WGPUTextureFormat_RGBA16Float = 0x00000028,
    WGPUTextureFormat_RGBA32Float = 0x00000029,
    WGPUTextureFormat_RGBA32Uint = 0x0000002A,
    WGPUTextureFormat_RGBA32Sint = 0x0000002B,
    WGPUTextureFormat_Stencil8 = 0x0000002C,
    WGPUTextureFormat_Depth16Unorm = 0x0000002D,
    WGPUTextureFormat_Depth24Plus = 0x0000002E,
    WGPUTextureFormat_Depth24PlusStencil8 = 0x0000002F,
    WGPUTextureFormat_Depth32Float = 0x00000030,
    WGPUTextureFormat_Depth32FloatStencil8 = 0x00000031,
    WGPUTextureFormat_BC1RGBAUnorm = 0x00000032,
    WGPUTextureFormat_BC1RGBAUnormSrgb = 0x00000033,
    WGPUTextureFormat_BC2RGBAUnorm = 0x00000034,
    WGPUTextureFormat_BC2RGBAUnormSrgb = 0x00000035,
    WGPUTextureFormat_BC3RGBAUnorm = 0x00000036,
    WGPUTextureFormat_BC3RGBAUnormSrgb = 0x00000037,
    WGPUTextureFormat_BC4RUnorm = 0x00000038,
    WGPUTextureFormat_BC4RSnorm = 0x00000039,
    WGPUTextureFormat_BC5RGUnorm = 0x0000003A,
    WGPUTextureFormat_BC5RGSnorm = 0x0000003B,
    WGPUTextureFormat_BC6HRGBUfloat = 0x0000003C,
    WGPUTextureFormat_BC6HRGBFloat = 0x0000003D,
    WGPUTextureFormat_BC7RGBAUnorm = 0x0000003E,
    WGPUTextureFormat_BC7RGBAUnormSrgb = 0x0000003F,
    WGPUTextureFormat_ETC2RGB8Unorm = 0x00000040,
    WGPUTextureFormat_ETC2RGB8UnormSrgb = 0x00000041,
    WGPUTextureFormat_ETC2RGB8A1Unorm = 0x00000042,
    WGPUTextureFormat_ETC2RGB8A1UnormSrgb = 0x00000043,
    WGPUTextureFormat_ETC2RGBA8Unorm = 0x00000044,
    WGPUTextureFormat_ETC2RGBA8UnormSrgb = 0x00000045,
    WGPUTextureFormat_EACR11Unorm = 0x00000046,
    WGPUTextureFormat_EACR11Snorm = 0x00000047,
    WGPUTextureFormat_EACRG11Unorm = 0x00000048,
    WGPUTextureFormat_EACRG11Snorm = 0x00000049,
    WGPUTextureFormat_ASTC4x4Unorm = 0x0000004A,
    WGPUTextureFormat_ASTC4x4UnormSrgb = 0x0000004B,
    WGPUTextureFormat_ASTC5x4Unorm = 0x0000004C,
    WGPUTextureFormat_ASTC5x4UnormSrgb = 0x0000004D,
    WGPUTextureFormat_ASTC5x5Unorm = 0x0000004E,
    WGPUTextureFormat_ASTC5x5UnormSrgb = 0x0000004F,
    WGPUTextureFormat_ASTC6x5Unorm = 0x00000050,
    WGPUTextureFormat_ASTC6x5UnormSrgb = 0x00000051,
    WGPUTextureFormat_ASTC6x6Unorm = 0x00000052,
    WGPUTextureFormat_ASTC6x6UnormSrgb = 0x00000053,
    WGPUTextureFormat_ASTC8x5Unorm = 0x00000054,
    WGPUTextureFormat_ASTC8x5UnormSrgb = 0x00000055,
    WGPUTextureFormat_ASTC8x6Unorm = 0x00000056,
    WGPUTextureFormat_ASTC8x6UnormSrgb = 0x00000057,
    WGPUTextureFormat_ASTC8x8Unorm = 0x00000058,
    WGPUTextureFormat_ASTC8x8UnormSrgb = 0x00000059,
    WGPUTextureFormat_ASTC10x5Unorm = 0x0000005A,
    WGPUTextureFormat_ASTC10x5UnormSrgb = 0x0000005B,
    WGPUTextureFormat_ASTC10x6Unorm = 0x0000005C,
    WGPUTextureFormat_ASTC10x6UnormSrgb = 0x0000005D,
    WGPUTextureFormat_ASTC10x8Unorm = 0x0000005E,
    WGPUTextureFormat_ASTC10x8UnormSrgb = 0x0000005F,
    WGPUTextureFormat_ASTC10x10Unorm = 0x00000060,
    WGPUTextureFormat_ASTC10x10UnormSrgb = 0x00000061,
    WGPUTextureFormat_ASTC12x10Unorm = 0x00000062,
    WGPUTextureFormat_ASTC12x10UnormSrgb = 0x00000063,
    WGPUTextureFormat_ASTC12x12Unorm = 0x00000064,
    WGPUTextureFormat_ASTC12x12UnormSrgb = 0x00000065,
    WGPUTextureFormat_R8BG8Biplanar420Unorm = 0x00050006,
    WGPUTextureFormat_R10X6BG10X6Biplanar420Unorm = 0x00050007,
    WGPUTextureFormat_R8BG8A8Triplanar420Unorm = 0x00050008,
    WGPUTextureFormat_R8BG8Biplanar422Unorm = 0x00050009,
    WGPUTextureFormat_R8BG8Biplanar444Unorm = 0x0005000A,
    WGPUTextureFormat_R10X6BG10X6Biplanar422Unorm = 0x0005000B,
    WGPUTextureFormat_R10X6BG10X6Biplanar444Unorm = 0x0005000C,
    WGPUTextureFormat_External = 0x0005000D,
    WGPUTextureFormat_Force32 = 0x7FFFFFFF
} WGPUTextureFormat WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUTextureSampleType {
    WGPUTextureSampleType_BindingNotUsed = 0x00000000,
    WGPUTextureSampleType_Undefined = 0x00000001,
    WGPUTextureSampleType_Float = 0x00000002,
    WGPUTextureSampleType_UnfilterableFloat = 0x00000003,
    WGPUTextureSampleType_Depth = 0x00000004,
    WGPUTextureSampleType_Sint = 0x00000005,
    WGPUTextureSampleType_Uint = 0x00000006,
    WGPUTextureSampleType_Force32 = 0x7FFFFFFF
} WGPUTextureSampleType;


typedef enum WGPUFilterMode {
    WGPUFilterMode_Undefined = 0x00000000,
    WGPUFilterMode_Nearest = 0x00000001,
    WGPUFilterMode_Linear = 0x00000002,
    WGPUFilterMode_Force32 = 0x7FFFFFFF
} WGPUFilterMode WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUMipmapFilterMode {
    WGPUMipmapFilterMode_Undefined = 0x00000000,
    WGPUMipmapFilterMode_Nearest = 0x00000001,
    WGPUMipmapFilterMode_Linear = 0x00000002,
    WGPUMipmapFilterMode_Force32 = 0x7FFFFFFF
} WGPUMipmapFilterMode WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUAddressMode {
    WGPUAddressMode_Undefined = 0x00000000,
    WGPUAddressMode_ClampToEdge = 0x00000001,
    WGPUAddressMode_Repeat = 0x00000002,
    WGPUAddressMode_MirrorRepeat = 0x00000003,
    WGPUAddressMode_Force32 = 0x7FFFFFFF
} WGPUAddressMode WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUBackendType {
    WGPUBackendType_Undefined = 0x00000000,
    WGPUBackendType_Null = 0x00000001,
    WGPUBackendType_WebGPU = 0x00000002,
    WGPUBackendType_D3D11 = 0x00000003,
    WGPUBackendType_D3D12 = 0x00000004,
    WGPUBackendType_Metal = 0x00000005,
    WGPUBackendType_Vulkan = 0x00000006,
    WGPUBackendType_OpenGL = 0x00000007,
    WGPUBackendType_OpenGLES = 0x00000008,
    WGPUBackendType_Force32 = 0x7FFFFFFF
} WGPUBackendType WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUAdapterType {
    WGPUAdapterType_DiscreteGPU = 0x00000001,
    WGPUAdapterType_IntegratedGPU = 0x00000002,
    WGPUAdapterType_CPU = 0x00000003,
    WGPUAdapterType_Unknown = 0x00000004,
    WGPUAdapterType_Force32 = 0x7FFFFFFF
} WGPUAdapterType WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUPowerPreference {
    WGPUPowerPreference_Undefined = 0x00000000,
    WGPUPowerPreference_LowPower = 0x00000001,
    WGPUPowerPreference_HighPerformance = 0x00000002,
    WGPUPowerPreference_Force32 = 0x7FFFFFFF
} WGPUPowerPreference WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUFeatureLevel {
    WGPUFeatureLevel_Undefined = 0x00000000,
    WGPUFeatureLevel_Compatibility = 0x00000001,
    WGPUFeatureLevel_Core = 0x00000002,
    WGPUFeatureLevel_Force32 = 0x7FFFFFFF
} WGPUFeatureLevel WGPU_ENUM_ATTRIBUTE;

// Missing Enums
typedef enum WGPUErrorFilter {
    WGPUErrorFilter_Validation = 0x00000001,
    WGPUErrorFilter_OutOfMemory = 0x00000002,
    WGPUErrorFilter_Internal = 0x00000003,
    WGPUErrorFilter_Force32 = 0x7FFFFFFF
} WGPUErrorFilter WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUBufferMapState {
    WGPUBufferMapState_Unmapped = 0x00000001,
    WGPUBufferMapState_Pending = 0x00000002,
    WGPUBufferMapState_Mapped = 0x00000003,
    WGPUBufferMapState_Force32 = 0x7FFFFFFF
} WGPUBufferMapState WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUCompilationInfoRequestStatus {
    WGPUCompilationInfoRequestStatus_Success = 0x00000001,
    WGPUCompilationInfoRequestStatus_CallbackCancelled = 0x00000002,
    WGPUCompilationInfoRequestStatus_Force32 = 0x7FFFFFFF
} WGPUCompilationInfoRequestStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUCompilationMessageType {
    WGPUCompilationMessageType_Error = 0x00000001,
    WGPUCompilationMessageType_Warning = 0x00000002,
    WGPUCompilationMessageType_Info = 0x00000003,
    WGPUCompilationMessageType_Force32 = 0x7FFFFFFF
} WGPUCompilationMessageType WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUCreatePipelineAsyncStatus {
    WGPUCreatePipelineAsyncStatus_Success = 0x00000001,
    WGPUCreatePipelineAsyncStatus_CallbackCancelled = 0x00000002,
    WGPUCreatePipelineAsyncStatus_ValidationError = 0x00000003,
    WGPUCreatePipelineAsyncStatus_InternalError = 0x00000004,
    WGPUCreatePipelineAsyncStatus_Force32 = 0x7FFFFFFF
} WGPUCreatePipelineAsyncStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUPopErrorScopeStatus {
    WGPUPopErrorScopeStatus_Success = 0x00000001,
    WGPUPopErrorScopeStatus_CallbackCancelled = 0x00000002,
    WGPUPopErrorScopeStatus_Error = 0x00000003,
    WGPUPopErrorScopeStatus_Force32 = 0x7FFFFFFF
} WGPUPopErrorScopeStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUPredefinedColorSpace {
    WGPUPredefinedColorSpace_SRGB = 0x00000001,
    WGPUPredefinedColorSpace_DisplayP3 = 0x00000002,
    WGPUPredefinedColorSpace_Force32 = 0x7FFFFFFF
} WGPUPredefinedColorSpace WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUQueryType {
    WGPUQueryType_Occlusion = 0x00000001,
    WGPUQueryType_Timestamp = 0x00000002,
    WGPUQueryType_Force32 = 0x7FFFFFFF
} WGPUQueryType WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUQueueWorkDoneStatus {
    WGPUQueueWorkDoneStatus_Success = 0x00000001,
    WGPUQueueWorkDoneStatus_CallbackCancelled = 0x00000002,
    WGPUQueueWorkDoneStatus_Error = 0x00000003,
    WGPUQueueWorkDoneStatus_Force32 = 0x7FFFFFFF
} WGPUQueueWorkDoneStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUSubgroupMatrixComponentType {
    WGPUSubgroupMatrixComponentType_F32 = 0x00000001,
    WGPUSubgroupMatrixComponentType_F16 = 0x00000002,
    WGPUSubgroupMatrixComponentType_U32 = 0x00000003,
    WGPUSubgroupMatrixComponentType_I32 = 0x00000004,
    WGPUSubgroupMatrixComponentType_Force32 = 0x7FFFFFFF
} WGPUSubgroupMatrixComponentType WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUToneMappingMode {
    WGPUToneMappingMode_Standard = 0x00000001,
    WGPUToneMappingMode_Extended = 0x00000002,
    WGPUToneMappingMode_Force32 = 0x7FFFFFFF
} WGPUToneMappingMode WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUWGSLLanguageFeatureName {
    WGPUWGSLLanguageFeatureName_ReadonlyAndReadwriteStorageTextures = 0x00000001,
    WGPUWGSLLanguageFeatureName_Packed4x8IntegerDotProduct = 0x00000002,
    WGPUWGSLLanguageFeatureName_UnrestrictedPointerParameters = 0x00000003,
    WGPUWGSLLanguageFeatureName_PointerCompositeAccess = 0x00000004,
    WGPUWGSLLanguageFeatureName_SizedBindingArray = 0x00000005,
    WGPUWGSLLanguageFeatureName_Force32 = 0x7FFFFFFF
} WGPUWGSLLanguageFeatureName WGPU_ENUM_ATTRIBUTE;
typedef enum WGPUErrorType {
    WGPUErrorType_NoError = 0x00000001,
    WGPUErrorType_Validation = 0x00000002,
    WGPUErrorType_OutOfMemory = 0x00000003,
    WGPUErrorType_Internal = 0x00000004,
    WGPUErrorType_Unknown = 0x00000005,
    WGPUErrorType_Force32 = 0x7FFFFFFF
} WGPUErrorType WGPU_ENUM_ATTRIBUTE;


typedef enum WGPUDeviceLostReason {
    WGPUDeviceLostReason_Unknown = 0x00000001,
    WGPUDeviceLostReason_Destroyed = 0x00000002,
    WGPUDeviceLostReason_CallbackCancelled = 0x00000003,
    WGPUDeviceLostReason_FailedCreation = 0x00000004,
    WGPUDeviceLostReason_Force32 = 0x7FFFFFFF
} WGPUDeviceLostReason WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUVertexFormat {
    WGPUVertexFormat_Uint8 = 0x00000001,
    WGPUVertexFormat_Uint8x2 = 0x00000002,
    WGPUVertexFormat_Uint8x4 = 0x00000003,
    WGPUVertexFormat_Sint8 = 0x00000004,
    WGPUVertexFormat_Sint8x2 = 0x00000005,
    WGPUVertexFormat_Sint8x4 = 0x00000006,
    WGPUVertexFormat_Unorm8 = 0x00000007,
    WGPUVertexFormat_Unorm8x2 = 0x00000008,
    WGPUVertexFormat_Unorm8x4 = 0x00000009,
    WGPUVertexFormat_Snorm8 = 0x0000000A,
    WGPUVertexFormat_Snorm8x2 = 0x0000000B,
    WGPUVertexFormat_Snorm8x4 = 0x0000000C,
    WGPUVertexFormat_Uint16 = 0x0000000D,
    WGPUVertexFormat_Uint16x2 = 0x0000000E,
    WGPUVertexFormat_Uint16x4 = 0x0000000F,
    WGPUVertexFormat_Sint16 = 0x00000010,
    WGPUVertexFormat_Sint16x2 = 0x00000011,
    WGPUVertexFormat_Sint16x4 = 0x00000012,
    WGPUVertexFormat_Unorm16 = 0x00000013,
    WGPUVertexFormat_Unorm16x2 = 0x00000014,
    WGPUVertexFormat_Unorm16x4 = 0x00000015,
    WGPUVertexFormat_Snorm16 = 0x00000016,
    WGPUVertexFormat_Snorm16x2 = 0x00000017,
    WGPUVertexFormat_Snorm16x4 = 0x00000018,
    WGPUVertexFormat_Float16 = 0x00000019,
    WGPUVertexFormat_Float16x2 = 0x0000001A,
    WGPUVertexFormat_Float16x4 = 0x0000001B,
    WGPUVertexFormat_Float32 = 0x0000001C,
    WGPUVertexFormat_Float32x2 = 0x0000001D,
    WGPUVertexFormat_Float32x3 = 0x0000001E,
    WGPUVertexFormat_Float32x4 = 0x0000001F,
    WGPUVertexFormat_Uint32 = 0x00000020,
    WGPUVertexFormat_Uint32x2 = 0x00000021,
    WGPUVertexFormat_Uint32x3 = 0x00000022,
    WGPUVertexFormat_Uint32x4 = 0x00000023,
    WGPUVertexFormat_Sint32 = 0x00000024,
    WGPUVertexFormat_Sint32x2 = 0x00000025,
    WGPUVertexFormat_Sint32x3 = 0x00000026,
    WGPUVertexFormat_Sint32x4 = 0x00000027,
    WGPUVertexFormat_Unorm10_10_10_2 = 0x00000028,
    WGPUVertexFormat_Unorm8x4BGRA = 0x00000029,
    WGPUVertexFormat_Force32 = 0x7FFFFFFF
} WGPUVertexFormat WGPU_ENUM_ATTRIBUTE;
typedef enum WGPUSurfaceGetCurrentTextureStatus {
    WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal = 0x00000001,
    WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal = 0x00000002,
    WGPUSurfaceGetCurrentTextureStatus_Timeout = 0x00000003,
    WGPUSurfaceGetCurrentTextureStatus_Outdated = 0x00000004,
    WGPUSurfaceGetCurrentTextureStatus_Lost = 0x00000005,
    WGPUSurfaceGetCurrentTextureStatus_Error = 0x00000006,
    WGPUSurfaceGetCurrentTextureStatus_Force32 = 0x7FFFFFFF
} WGPUSurfaceGetCurrentTextureStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUInstanceFeatureName {
    WGPUInstanceFeatureName_TimedWaitAny = 0x00000001,
    WGPUInstanceFeatureName_ShaderSourceSPIRV = 0x00000002,
    WGPUInstanceFeatureName_MultipleDevicesPerAdapter = 0x00000003,
    WGPUInstanceFeatureName_Force32 = 0x7FFFFFFF
} WGPUInstanceFeatureName WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUFeatureName {
    WGPUFeatureName_DepthClipControl = 0x00000001,
    WGPUFeatureName_Depth32FloatStencil8 = 0x00000002,
    WGPUFeatureName_TimestampQuery = 0x00000003,
    WGPUFeatureName_TextureCompressionBC = 0x00000004,
    WGPUFeatureName_TextureCompressionBCSliced3D = 0x00000005,
    WGPUFeatureName_TextureCompressionETC2 = 0x00000006,
    WGPUFeatureName_TextureCompressionASTC = 0x00000007,
    WGPUFeatureName_TextureCompressionASTCSliced3D = 0x00000008,
    WGPUFeatureName_IndirectFirstInstance = 0x00000009,
    WGPUFeatureName_ShaderF16 = 0x0000000A,
    WGPUFeatureName_RG11B10UfloatRenderable = 0x0000000B,
    WGPUFeatureName_BGRA8UnormStorage = 0x0000000C,
    WGPUFeatureName_Float32Filterable = 0x0000000D,
    WGPUFeatureName_Float32Blendable = 0x0000000E,
    WGPUFeatureName_ClipDistances = 0x0000000F,
    WGPUFeatureName_DualSourceBlending = 0x00000010,
    WGPUFeatureName_Subgroups = 0x00000011,
    WGPUFeatureName_CoreFeaturesAndLimits = 0x00000012,
    WGPUFeatureName_TextureFormatNV12 = 0x00000013,
    WGPUFeatureName_TextureFormatP010 = 0x00000014,
    WGPUFeatureName_Force32 = 0x7FFFFFFF
} WGPUFeatureName;

typedef enum WGPUMapAsyncStatus {
    WGPUMapAsyncStatus_Success = 0x00000001,
    WGPUMapAsyncStatus_CallbackCancelled = 0x00000002,
    WGPUMapAsyncStatus_Error = 0x00000003,
    WGPUMapAsyncStatus_Aborted = 0x00000004,
    WGPUMapAsyncStatus_Force32 = 0x7FFFFFFF
} WGPUMapAsyncStatus WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUCompositeAlphaMode {
    WGPUCompositeAlphaMode_Auto = 0x00000000,
    WGPUCompositeAlphaMode_Opaque = 0x00000001,
    WGPUCompositeAlphaMode_Premultiplied = 0x00000002,
    WGPUCompositeAlphaMode_Unpremultiplied = 0x00000003,
    WGPUCompositeAlphaMode_Inherit = 0x00000004,
    WGPUCompositeAlphaMode_Force32 = 0x7FFFFFFF
} WGPUCompositeAlphaMode WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUComponentSwizzle {
    WGPUComponentSwizzle_Undefined = 0x00000000,
    WGPUComponentSwizzle_Zero = 0x00000001,
    WGPUComponentSwizzle_One = 0x00000002,
    WGPUComponentSwizzle_R = 0x00000003,
    WGPUComponentSwizzle_G = 0x00000004,
    WGPUComponentSwizzle_B = 0x00000005,
    WGPUComponentSwizzle_A = 0x00000006,
    WGPUComponentSwizzle_Force32 = 0x7FFFFFFF
} WGPUComponentSwizzle WGPU_ENUM_ATTRIBUTE;

typedef enum WGPURayTracingAccelerationGeometryType{
    WGPURayTracingAccelerationGeometryType_Triangles = 0x00000001,
    WGPURayTracingAccelerationGeometryType_AABBs     = 0x00000002,
    WGPURayTracingAccelerationGeometryType_Force32   = 0x7FFFFFFF,
}WGPURayTracingAccelerationGeometryType;

typedef enum WGPURayTracingAccelerationContainerLevel{
    WGPURayTracingAccelerationContainerLevel_Bottom = 0x00000001,
    WGPURayTracingAccelerationContainerLevel_Top = 0x00000002,
    WGPURayTracingAccelerationContainerLevel_Force32 = 0x7FFFFFFF,
}WGPURayTracingAccelerationContainerLevel;

typedef enum WGPURayTracingShaderBindingTableGroupType{
    WGPURayTracingShaderBindingTableGroupType_General = 0x00000001,
    WGPURayTracingShaderBindingTableGroupType_TrianglesHitGroup = 0x00000002,
    WGPURayTracingShaderBindingTableGroupType_ProceduralHitGroup = 0x00000003,
    WGPURayTracingShaderBindingTableGroupType_Force32 = 0x7FFFFFFF,
}WGPURayTracingShaderBindingTableGroupType;

typedef WGPUFlags WGPURayTracingAccelerationGeometryUsage;
static const WGPURayTracingAccelerationGeometryUsage WGPURayTracingAccelerationGeometryUsage_Opaque = 0x00000001;
static const WGPURayTracingAccelerationGeometryUsage WGPURayTracingAccelerationGeometryUsage_AllowAnyHit = 0x00000002;

typedef WGPUFlags WGPURayTracingAccelerationInstanceUsage;
static const WGPURayTracingAccelerationInstanceUsage WGPURayTracingAccelerationInstanceUsage_TriangleCullDisable = 0x00000001;
static const WGPURayTracingAccelerationInstanceUsage WGPURayTracingAccelerationInstanceUsage_TriangleFrontCounterclockwise = 0x00000002;
static const WGPURayTracingAccelerationInstanceUsage WGPURayTracingAccelerationInstanceUsage_ForceOpaque = 0x00000004;
static const WGPURayTracingAccelerationInstanceUsage WGPURayTracingAccelerationInstanceUsage_ForceNoOpaque = 0x00000008;

typedef WGPUFlags WGPURayTracingAccelerationContainerUsage;
static const WGPURayTracingAccelerationContainerUsage WGPURayTracingAccelerationContainerUsage_AllowUpdate = 0x00000001;
static const WGPURayTracingAccelerationContainerUsage WGPURayTracingAccelerationContainerUsage_PreferFastTrace = 0x00000002;
static const WGPURayTracingAccelerationContainerUsage WGPURayTracingAccelerationContainerUsage_PreferFastBuild = 0x00000004;
static const WGPURayTracingAccelerationContainerUsage WGPURayTracingAccelerationContainerUsage_LowMemory = 0x00000008;


typedef struct WGPUChainedStruct {
    struct WGPUChainedStruct* next;
    WGPUSType sType;
} WGPUChainedStruct;

struct WGPUCompilationInfo; // Forward declaration
typedef void (*WGPUCompilationInfoCallback)(WGPUCompilationInfoRequestStatus status, struct WGPUCompilationInfo const * compilationInfo, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUCreateComputePipelineAsyncCallback)(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUCreateRenderPipelineAsyncCallback)(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUPopErrorScopeCallback)(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUQueueWorkDoneCallback)(WGPUQueueWorkDoneStatus status, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE;

// Missing Structs
typedef struct WGPUCompilationInfoCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPUCompilationInfoCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPUCompilationInfoCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUCreateComputePipelineAsyncCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPUCreateComputePipelineAsyncCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPUCreateComputePipelineAsyncCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUCreateRenderPipelineAsyncCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPUCreateRenderPipelineAsyncCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPUCreateRenderPipelineAsyncCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUPopErrorScopeCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPUPopErrorScopeCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPUPopErrorScopeCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUQueueWorkDoneCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPUQueueWorkDoneCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPUQueueWorkDoneCallbackInfo WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUAdapterPropertiesSubgroups {
    WGPUChainedStruct chain;
    uint32_t subgroupMinSize;
    uint32_t subgroupMaxSize;
} WGPUAdapterPropertiesSubgroups WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUBindGroupLayoutEntryArraySize {
    WGPUChainedStruct chain;
    uint32_t arraySize;
} WGPUBindGroupLayoutEntryArraySize WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUCompilationMessage {
    WGPUChainedStruct * nextInChain;
    WGPUStringView message;
    WGPUCompilationMessageType type;
    uint64_t lineNum;
    uint64_t linePos;
    uint64_t offset;
    uint64_t length;
} WGPUCompilationMessage WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUPassTimestampWrites {
    WGPUChainedStruct * nextInChain;
    WGPUQuerySet querySet;
    uint32_t beginningOfPassWriteIndex;
    uint32_t endOfPassWriteIndex;
} WGPUPassTimestampWrites WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUQuerySetDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUQueryType type;
    uint32_t count;
} WGPUQuerySetDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPURenderPassMaxDrawCount {
    WGPUChainedStruct chain;
    uint64_t maxDrawCount;
} WGPURenderPassMaxDrawCount WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPURequestAdapterWebXROptions {
    WGPUChainedStruct chain;
    WGPUBool xrCompatible;
} WGPURequestAdapterWebXROptions WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUSupportedFeatures {
    size_t featureCount;
    WGPUFeatureName const * features;
} WGPUSupportedFeatures WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUSupportedWGSLLanguageFeatures {
    size_t featureCount;
    WGPUWGSLLanguageFeatureName const * features;
} WGPUSupportedWGSLLanguageFeatures WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUSurfaceColorManagement {
    WGPUChainedStruct chain;
    WGPUPredefinedColorSpace colorSpace;
    WGPUToneMappingMode toneMappingMode;
} WGPUSurfaceColorManagement WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUTextureBindingViewDimensionDescriptor {
    WGPUChainedStruct chain;
    WGPUTextureViewDimension textureBindingViewDimension;
} WGPUTextureBindingViewDimensionDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUCompilationInfo {
    WGPUChainedStruct * nextInChain;
    size_t messageCount;
    WGPUCompilationMessage const * messages;
} WGPUCompilationInfo WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUComputePassDescriptor {
    WGPUChainedStruct * nextInChain;
    WGPUStringView label;
    WGPU_NULLABLE WGPUPassTimestampWrites const * timestampWrites;
} WGPUComputePassDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPURayTracingPassDescriptor{
    WGPURayTracingShaderBindingTable shaderBindingTable;
    uint32_t maxRecursionDepth;
    uint32_t maxPayloadSize;
}WGPURayTracingPassDescriptor;

typedef struct WGPUTexelCopyBufferInfo {
    WGPUTexelCopyBufferLayout layout;
    WGPUBuffer buffer;
} WGPUTexelCopyBufferInfo;

typedef struct WGPUOrigin3D {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} WGPUOrigin3D;

typedef struct WGPUFuture {
    uint64_t id;
} WGPUFuture;

typedef struct WGPUExtent3D {
    uint32_t width;
    uint32_t height;
    uint32_t depthOrArrayLayers;
} WGPUExtent3D;

typedef struct WGPUTexelCopyTextureInfo {
    WGPUTexture texture;
    uint32_t mipLevel;
    WGPUOrigin3D origin;
    WGPUTextureAspect aspect;
} WGPUTexelCopyTextureInfo;



typedef struct WGPUSurfaceSourceMetalLayer {
    WGPUChainedStruct chain;
    void* layer;
} WGPUSurfaceSourceMetalLayer;

typedef struct WGPUSurfaceSourceWindowsHWND {
    WGPUChainedStruct chain;
    void * hinstance;
    void * hwnd;
} WGPUSurfaceSourceWindowsHWND;

typedef struct WGPUSurfaceSourceXlibWindow {
    WGPUChainedStruct chain;
    void* display;
    uint64_t window;
} WGPUSurfaceSourceXlibWindow;

typedef struct WGPUSurfaceSourceXCBWindow {
    WGPUChainedStruct chain;
    void* connection;
    uint32_t window;
} WGPUSurfaceSourceXCBWindow;

typedef struct WGPUSurfaceSourceWaylandSurface {
    WGPUChainedStruct chain;
    void* display;
    void* surface;
} WGPUSurfaceSourceWaylandSurface;

typedef struct WGPUSurfaceSourceAndroidNativeWindow {
    WGPUChainedStruct chain;
    void* window;
} WGPUSurfaceSourceAndroidNativeWindow;

typedef struct WGPUEmscriptenSurfaceSourceCanvasHTMLSelector {
    WGPUChainedStruct chain;
    WGPUStringView selector;
} WGPUEmscriptenSurfaceSourceCanvasHTMLSelector;

typedef enum WGPUDrmModeSelectType {
    WGPUDrmModeSelect_Default = 0,     // backend picks preferred mode
    WGPUDrmModeSelect_ByIndex,         // index into the connector’s advertised modes
    WGPUDrmModeSelect_ByGeometry       // pick by WxH + refresh (milli-Hz)
} WGPUDrmModeSelectType;

typedef struct WGPUDrmModeByGeometry {
    uint32_t width;
    uint32_t height;
    uint32_t refreshMilliHz;           // e.g. 60000 for 60 Hz
} WGPUDrmModeByGeometry;

typedef struct WGPUDrmModeSelect{
    WGPUDrmModeSelectType type;
    union{
        WGPUDrmModeByGeometry geometry;
        uint32_t index;
    };
}WGPUDrmModeSelect;

// The only job of this struct: let the backend create VkDisplayPlaneSurfaceKHR.
typedef struct WGPUSurfaceSourceDrmPlane {
    WGPUChainedStruct chain;
    WGPUAdapter adapter;
    int32_t   drmFd;
    uint32_t  connectorId;

    uint32_t  crtcId;
    uint32_t  planeId;

    
    WGPUDrmModeSelect modeSelect;
    
    WGPUBool acquireExclusive;
} WGPUSurfaceSourceDrmPlane;



typedef struct WGPUSurfaceDescriptor{
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
} WGPUSurfaceDescriptor;

typedef struct WGPUAdapterInfo {
    WGPUChainedStruct * nextInChain;
    WGPUStringView vendor;
    WGPUStringView architecture;
    WGPUStringView device;
    WGPUStringView description;
    WGPUBackendType backendType;
    WGPUAdapterType adapterType;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t subgroupMinSize;
    uint32_t subgroupMaxSize;
} WGPUAdapterInfo WGPU_STRUCT_ATTRIBUTE;

typedef struct WGPURequestAdapterOptions {
    WGPUChainedStruct * nextInChain;
    WGPUFeatureLevel featureLevel;
    WGPUPowerPreference powerPreference;
    WGPUBool forceFallbackAdapter;
    WGPUBackendType backendType;
    WGPU_NULLABLE WGPUSurface compatibleSurface;
} WGPURequestAdapterOptions;

typedef struct WGPUInstanceCapabilities {
    WGPUChainedStruct* nextInChain;
    WGPUBool timedWaitAnyEnable;
    size_t timedWaitAnyMaxCount;
} WGPUInstanceCapabilities;

typedef struct WGPUInstanceLimits {
    WGPUChainedStruct * nextInChain;
    size_t timedWaitAnyMaxCount;
} WGPUInstanceLimits WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUInstanceLayerSelection{
    WGPUChainedStruct chain;
    const char* const* instanceLayers;
    uint32_t instanceLayerCount;
}WGPUInstanceLayerSelection;

typedef struct WGPUInstanceDescriptor {
    WGPUChainedStruct * nextInChain;
    size_t requiredFeatureCount;
    WGPUInstanceFeatureName const * requiredFeatures;
    WGPU_NULLABLE WGPUInstanceLimits const * requiredLimits;
} WGPUInstanceDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUBindGroupEntry{
    WGPUChainedStruct* nextInChain;
    uint32_t binding;
    WGPUBuffer buffer;
    uint64_t offset;
    uint64_t size;
    WGPUSampler sampler;
    WGPUTextureView textureView;
    WGPURayTracingAccelerationContainer accelerationStructure;
}WGPUBindGroupEntry;

typedef struct WGPUTextureBindingLayout {
    WGPUChainedStruct * nextInChain;
    WGPUTextureSampleType sampleType;
    WGPUTextureViewDimension viewDimension;
    WGPUBool multisampled;
} WGPUTextureBindingLayout;

typedef struct WGPUSamplerBindingLayout {
    WGPUChainedStruct * nextInChain;
    WGPUSamplerBindingType type;
} WGPUSamplerBindingLayout;

typedef struct WGPUStorageTextureBindingLayout {
    WGPUChainedStruct * nextInChain;
    WGPUStorageTextureAccess access;
    WGPUTextureFormat format;
    WGPUTextureViewDimension viewDimension;
} WGPUStorageTextureBindingLayout;

typedef struct WGPUBufferBindingLayout {
    WGPUChainedStruct * nextInChain;
    WGPUBufferBindingType type;
    WGPUBool hasDynamicOffset;
    uint64_t minBindingSize;
} WGPUBufferBindingLayout;

typedef struct WGPUBindGroupLayoutEntry {
    WGPUChainedStruct * nextInChain;
    uint32_t binding;
    WGPUShaderStage visibility;
    WGPUBufferBindingLayout buffer;
    WGPUSamplerBindingLayout sampler;
    WGPUTextureBindingLayout texture;
    WGPUStorageTextureBindingLayout storageTexture;
    WGPUBool accelerationStructure;
} WGPUBindGroupLayoutEntry;

typedef struct WGPUSamplerDescriptor {
    WGPUChainedStruct * nextInChain;
    WGPUStringView label;
    WGPUAddressMode addressModeU;
    WGPUAddressMode addressModeV;
    WGPUAddressMode addressModeW;
    WGPUFilterMode magFilter;
    WGPUFilterMode minFilter;
    WGPUMipmapFilterMode mipmapFilter;
    float lodMinClamp;
    float lodMaxClamp;
    WGPUCompareFunction compare;
    uint16_t maxAnisotropy;
} WGPUSamplerDescriptor;

typedef struct WGPUFutureWaitInfo {
    WGPUFuture future;
    WGPUBool32 completed;
} WGPUFutureWaitInfo;

typedef struct WGPULimits {
    WGPUChainedStruct* nextInChain;
    uint32_t maxTextureDimension1D;
    uint32_t maxTextureDimension2D;
    uint32_t maxTextureDimension3D;
    uint32_t maxTextureArrayLayers;
    uint32_t maxBindGroups;
    uint32_t maxBindGroupsPlusVertexBuffers;
    uint32_t maxBindingsPerBindGroup;
    uint32_t maxDynamicUniformBuffersPerPipelineLayout;
    uint32_t maxDynamicStorageBuffersPerPipelineLayout;
    uint32_t maxSampledTexturesPerShaderStage;
    uint32_t maxSamplersPerShaderStage;
    uint32_t maxStorageBuffersPerShaderStage;
    uint32_t maxStorageTexturesPerShaderStage;
    uint32_t maxUniformBuffersPerShaderStage;
    uint64_t maxUniformBufferBindingSize;
    uint64_t maxStorageBufferBindingSize;
    uint32_t minUniformBufferOffsetAlignment;
    uint32_t minStorageBufferOffsetAlignment;
    uint32_t maxVertexBuffers;
    uint64_t maxBufferSize;
    uint32_t maxVertexAttributes;
    uint32_t maxVertexBufferArrayStride;
    uint32_t maxInterStageShaderVariables;
    uint32_t maxColorAttachments;
    uint32_t maxColorAttachmentBytesPerSample;
    uint32_t maxComputeWorkgroupStorageSize;
    uint32_t maxComputeInvocationsPerWorkgroup;
    uint32_t maxComputeWorkgroupSizeX;
    uint32_t maxComputeWorkgroupSizeY;
    uint32_t maxComputeWorkgroupSizeZ;
    uint32_t maxComputeWorkgroupsPerDimension;
    uint32_t maxStorageBuffersInVertexStage;
    uint32_t maxStorageTexturesInVertexStage;
    uint32_t maxStorageBuffersInFragmentStage;
    uint32_t maxStorageTexturesInFragmentStage;
}WGPULimits;

typedef struct WGPUQueueDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
}WGPUQueueDescriptor;

typedef void (*WGPUProc)(void) WGPU_FUNCTION_ATTRIBUTE;
typedef void (*WGPUDeviceLostCallback)(const WGPUDevice*, WGPUDeviceLostReason, struct WGPUStringView, void*, void*);
typedef void (*WGPUUncapturedErrorCallback)(const WGPUDevice*, WGPUErrorType, struct WGPUStringView, void*, void*);

typedef struct WGPUDeviceLostCallbackInfo {
    WGPUChainedStruct * nextInChain;
    int mode;
    WGPUDeviceLostCallback callback;
    void* userdata1;
    void* userdata2;
} WGPUDeviceLostCallbackInfo;
typedef struct WGPUUncapturedErrorCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUUncapturedErrorCallback callback;
    void* userdata1;
    void* userdata2;
} WGPUUncapturedErrorCallbackInfo;

typedef struct WGPUDeviceDescriptor {
    WGPUChainedStruct * nextInChain;
    WGPUStringView label;
    size_t requiredFeatureCount;
    WGPUFeatureName const * requiredFeatures;
    WGPU_NULLABLE WGPULimits const * requiredLimits;
    WGPUQueueDescriptor defaultQueue;
    WGPUDeviceLostCallbackInfo deviceLostCallbackInfo;
    WGPUUncapturedErrorCallbackInfo uncapturedErrorCallbackInfo;
} WGPUDeviceDescriptor WGPU_STRUCT_ATTRIBUTE;

typedef struct WGPUColor {
    double r;
    double g;
    double b;
    double a;
} WGPUColor;

typedef struct WGPURenderPassColorAttachment{
    WGPUChainedStruct* nextInChain;
    WGPUTextureView view;
    WGPUTextureView resolveTarget;
    uint32_t depthSlice;
    WGPULoadOp loadOp;
    WGPUStoreOp storeOp;
    WGPUColor clearValue;
}WGPURenderPassColorAttachment;

typedef struct WGPURenderPassDepthStencilAttachment{
    WGPUChainedStruct* nextInChain;
    WGPUTextureView view;
    WGPULoadOp depthLoadOp;
    WGPUStoreOp depthStoreOp;
    float depthClearValue;
    uint32_t depthReadOnly;
    WGPULoadOp stencilLoadOp;
    WGPUStoreOp stencilStoreOp;
    uint32_t stencilClearValue;
    uint32_t stencilReadOnly;
}WGPURenderPassDepthStencilAttachment;

typedef struct WGPURenderPassDescriptor {
    WGPUChainedStruct * nextInChain;
    WGPUStringView label;
    size_t colorAttachmentCount;
    const WGPURenderPassColorAttachment* colorAttachments;
    WGPU_NULLABLE const WGPURenderPassDepthStencilAttachment* depthStencilAttachment;
    WGPU_NULLABLE WGPUQuerySet occlusionQuerySet;
    WGPU_NULLABLE const WGPUPassTimestampWrites* timestampWrites;
} WGPURenderPassDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPURenderBundleDescriptor {
    WGPUChainedStruct * nextInChain;
    WGPUStringView label;
}WGPURenderBundleDescriptor;

typedef struct WGPURenderBundleEncoderDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    size_t colorFormatCount;
    const WGPUTextureFormat* colorFormats;
    WGPUTextureFormat depthStencilFormat;
    uint32_t sampleCount;
    WGPUBool depthReadOnly;
    WGPUBool stencilReadOnly;
}WGPURenderBundleEncoderDescriptor;

typedef struct WGPUCommandEncoderDescriptor{
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
}WGPUCommandEncoderDescriptor;

typedef struct WGPUTextureDescriptor{
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUTextureUsage usage;
    WGPUTextureDimension dimension;
    WGPUExtent3D size;
    WGPUTextureFormat format;
    uint32_t mipLevelCount;
    uint32_t sampleCount;
    size_t viewFormatCount;
    const WGPUTextureFormat* viewFormats;
}WGPUTextureDescriptor;

typedef struct WGPUTextureViewDescriptor{
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUTextureFormat format;
    WGPUTextureViewDimension dimension;
    uint32_t baseMipLevel;
    uint32_t mipLevelCount;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;
    WGPUTextureAspect aspect;
    WGPUTextureUsage usage;
}WGPUTextureViewDescriptor;

typedef struct WGPUTextureComponentSwizzle {
    WGPUComponentSwizzle r;
    WGPUComponentSwizzle g;
    WGPUComponentSwizzle b;
    WGPUComponentSwizzle a;
} WGPUTextureComponentSwizzle WGPU_STRUCTURE_ATTRIBUTE;

// Can be chained in WGPUTextureViewDescriptor
typedef struct WGPUTextureComponentSwizzleDescriptor {
    WGPUChainedStruct chain;
    WGPUTextureComponentSwizzle swizzle;
} WGPUTextureComponentSwizzleDescriptor WGPU_STRUCTURE_ATTRIBUTE;

typedef struct WGPUBufferAllocatorSelector{
    WGPUChainedStruct chain;
    WGPUBool forceBuiltin;
}WGPUBufferAllocatorSelector;

typedef struct WGPUBufferDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUBufferUsage usage;
    uint64_t size;
    WGPUBool mappedAtCreation;
} WGPUBufferDescriptor;

typedef void (*WGPUBufferMapCallback)(WGPUMapAsyncStatus status, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2);

typedef struct WGPUBufferMapCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPUBufferMapCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPUBufferMapCallbackInfo;

typedef struct WGPUBindGroupDescriptor{
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUBindGroupLayout layout;
    size_t entryCount;
    const WGPUBindGroupEntry* entries;
}WGPUBindGroupDescriptor;

typedef struct WGPUBindGroupLayoutDescriptor {
    WGPUChainedStruct * nextInChain;
    WGPUStringView label;
    size_t entryCount;
    WGPUBindGroupLayoutEntry const * entries;
} WGPUBindGroupLayoutDescriptor;

typedef struct WGPUPipelineLayoutDescriptor {
    const WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    size_t bindGroupLayoutCount;
    const WGPUBindGroupLayout * bindGroupLayouts;
    uint32_t immediateDataRangeByteSize;
}WGPUPipelineLayoutDescriptor;

typedef struct WGPUSurfaceTexture {
    WGPUChainedStruct * nextInChain;
    WGPUTexture texture;
    WGPUSurfaceGetCurrentTextureStatus status;
} WGPUSurfaceTexture;

typedef struct WGPUSurfaceCapabilities {
    WGPUChainedStruct * nextInChain;
    WGPUTextureUsage usages;
    size_t formatCount;
    WGPUTextureFormat const * formats;
    size_t presentModeCount;
    WGPUPresentMode const * presentModes;
    size_t alphaModeCount;
    WGPUCompositeAlphaMode const * alphaModes;
} WGPUSurfaceCapabilities WGPU_STRUCT_ATTRIBUTE;

typedef struct WGPUConstantEntry {
    WGPUChainedStruct* nextInChain;
    WGPUStringView key;
    double value;
} WGPUConstantEntry;

typedef struct WGPUVertexAttribute {
    WGPUChainedStruct* nextInChain;
    WGPUVertexFormat format;
    uint64_t offset;
    uint32_t shaderLocation;
}WGPUVertexAttribute;

typedef struct WGPUVertexBufferLayout {
    WGPUChainedStruct* nextInChain;
    WGPUVertexStepMode stepMode;
    uint64_t arrayStride;
    size_t attributeCount;
    const WGPUVertexAttribute* attributes;
} WGPUVertexBufferLayout;

typedef struct WGPUVertexState {
    WGPUChainedStruct* nextInChain;
    WGPUShaderModule module;
    WGPUStringView entryPoint;
    size_t constantCount;
    const WGPUConstantEntry* constants;
    size_t bufferCount;
    const WGPUVertexBufferLayout* buffers;
} WGPUVertexState;

typedef enum WGPUBlendOperation {
    WGPUBlendOperation_Undefined = 0x00000000,
    WGPUBlendOperation_Add = 0x00000001,
    WGPUBlendOperation_Subtract = 0x00000002,
    WGPUBlendOperation_ReverseSubtract = 0x00000003,
    WGPUBlendOperation_Min = 0x00000004,
    WGPUBlendOperation_Max = 0x00000005,
    WGPUBlendOperation_Force32 = 0x7FFFFFFF
} WGPUBlendOperation WGPU_ENUM_ATTRIBUTE;

typedef enum WGPUBlendFactor {
    WGPUBlendFactor_Undefined = 0x00000000,
    WGPUBlendFactor_Zero = 0x00000001,
    WGPUBlendFactor_One = 0x00000002,
    WGPUBlendFactor_Src = 0x00000003,
    WGPUBlendFactor_OneMinusSrc = 0x00000004,
    WGPUBlendFactor_SrcAlpha = 0x00000005,
    WGPUBlendFactor_OneMinusSrcAlpha = 0x00000006,
    WGPUBlendFactor_Dst = 0x00000007,
    WGPUBlendFactor_OneMinusDst = 0x00000008,
    WGPUBlendFactor_DstAlpha = 0x00000009,
    WGPUBlendFactor_OneMinusDstAlpha = 0x0000000A,
    WGPUBlendFactor_SrcAlphaSaturated = 0x0000000B,
    WGPUBlendFactor_Constant = 0x0000000C,
    WGPUBlendFactor_OneMinusConstant = 0x0000000D,
    WGPUBlendFactor_Src1 = 0x0000000E,
    WGPUBlendFactor_OneMinusSrc1 = 0x0000000F,
    WGPUBlendFactor_Src1Alpha = 0x00000010,
    WGPUBlendFactor_OneMinusSrc1Alpha = 0x00000011,
    WGPUBlendFactor_Force32 = 0x7FFFFFFF
} WGPUBlendFactor WGPU_ENUM_ATTRIBUTE;

typedef struct WGPUBlendComponent {
    WGPUBlendOperation operation;
    WGPUBlendFactor srcFactor;
    WGPUBlendFactor dstFactor;
    #ifdef __cplusplus
    constexpr bool operator==(const WGPUBlendComponent& other)const noexcept{
        return operation == other.operation && srcFactor == other.srcFactor && dstFactor == other.dstFactor;
    }
    #endif
} WGPUBlendComponent;

typedef struct WGPUBlendState {
    WGPUBlendComponent color;
    WGPUBlendComponent alpha;
    #ifdef __cplusplus
    constexpr bool operator==(const WGPUBlendState& other)const noexcept{
        return color == other.color && alpha == other.alpha;
    }
    #endif
} WGPUBlendState;



typedef struct WGPUShaderSourceSPIRV {
    WGPUChainedStruct chain;
    // In uin32_t's, NOT in bytes
    uint32_t codeSize;
    const uint32_t* code;
} WGPUShaderSourceSPIRV;

typedef struct WGPUShaderSourceWGSL {
    WGPUChainedStruct chain;
    WGPUStringView code;
} WGPUShaderSourceWGSL;

typedef struct WGPUShaderSourceGLSL {
    WGPUChainedStruct chain;
    WGPUShaderStage stage;
    WGPUStringView code;
} WGPUShaderSourceGLSL;

typedef struct WGPUShaderModuleDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
} WGPUShaderModuleDescriptor;

typedef struct WGPUColorTargetState {
    WGPUChainedStruct* nextInChain;
    WGPUTextureFormat format;
    const WGPUBlendState* blend;
    WGPUColorWriteMask writeMask;
} WGPUColorTargetState;

typedef struct WGPUFragmentState {
    WGPUChainedStruct* nextInChain;
    WGPUShaderModule module;
    WGPUStringView entryPoint;
    size_t constantCount;
    const WGPUConstantEntry* constants;
    size_t targetCount;
    const WGPUColorTargetState* targets;
} WGPUFragmentState;

typedef struct WGPUCommandBufferDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
} WGPUCommandBufferDescriptor;

typedef struct WGPUPrimitiveLineWidthInfo{
    WGPUChainedStruct chain;
    uint32_t lineWidth;
}WGPUPrimitiveLineWidthInfo;

typedef struct WGPUPrimitiveState {
    WGPUChainedStruct* nextInChain;
    WGPUPrimitiveTopology topology;
    WGPUIndexFormat stripIndexFormat;
    WGPUFrontFace frontFace;
    WGPUCullMode cullMode;
    WGPUBool32 unclippedDepth;
} WGPUPrimitiveState;

typedef enum WGPUStencilOperation {
    WGPUStencilOperation_Undefined = 0x00000000,
    WGPUStencilOperation_Keep = 0x00000001,
    WGPUStencilOperation_Zero = 0x00000002,
    WGPUStencilOperation_Replace = 0x00000003,
    WGPUStencilOperation_Invert = 0x00000004,
    WGPUStencilOperation_IncrementClamp = 0x00000005,
    WGPUStencilOperation_DecrementClamp = 0x00000006,
    WGPUStencilOperation_IncrementWrap = 0x00000007,
    WGPUStencilOperation_DecrementWrap = 0x00000008,
    WGPUStencilOperation_Force32 = 0x7FFFFFFF
} WGPUStencilOperation WGPU_ENUM_ATTRIBUTE;

typedef struct WGPUStencilFaceState {
    WGPUCompareFunction compare;
    WGPUStencilOperation failOp;
    WGPUStencilOperation depthFailOp;
    WGPUStencilOperation passOp;
} WGPUStencilFaceState;

typedef struct WGPUDepthStencilState {
    WGPUChainedStruct* nextInChain;
    WGPUTextureFormat format;
    WGPUBool32 depthWriteEnabled;
    WGPUCompareFunction depthCompare;
    
    WGPUStencilFaceState stencilFront;
    WGPUStencilFaceState stencilBack;
    uint32_t stencilReadMask;
    uint32_t stencilWriteMask;
    int32_t depthBias;
    float depthBiasSlopeScale;
    float depthBiasClamp;
} WGPUDepthStencilState;
typedef struct WGPUBufferBindingInfo {
    WGPUChainedStruct * nextInChain;
    WGPUBufferBindingType type;
    uint64_t minBindingSize;
}WGPUBufferBindingInfo;
typedef struct WGPUSamplerBindingInfo {
    // same as WGPUSamplerBindingLayout
    WGPUChainedStruct * nextInChain;
    WGPUSamplerBindingType type;
}WGPUSamplerBindingInfo;
typedef struct WGPUTextureBindingInfo {
    WGPUChainedStruct * nextInChain;
    WGPUTextureSampleType sampleType;
    WGPUTextureViewDimension viewDimension;
    // no ‘multisampled’
}WGPUTextureBindingInfo;
typedef struct WGPUStorageTextureBindingInfo {
    // same as WGPUStorageTextureBindingLayout
    WGPUChainedStruct* nextInChain;
    WGPUStorageTextureAccess access;
    WGPUTextureFormat format;
    WGPUTextureViewDimension viewDimension;
}WGPUStorageTextureBindingInfo;
typedef struct WGPUGlobalReflectionInfo {
    WGPUStringView name;
    uint32_t bindGroup;
    uint32_t binding;
    WGPUShaderStage visibility;
    WGPUBufferBindingInfo buffer;
    WGPUSamplerBindingInfo sampler;
    WGPUTextureBindingInfo texture;
    WGPUStorageTextureBindingInfo storageTexture;
}WGPUGlobalReflectionInfo;


typedef enum WGPUReflectionComponentType{
    WGPUReflectionComponentType_Invalid,
    WGPUReflectionComponentType_Sint32,
    WGPUReflectionComponentType_Uint32,
    WGPUReflectionComponentType_Float32,
    WGPUReflectionComponentType_Float16
}WGPUReflectionComponentType;

typedef enum WGPUReflectionCompositionType{
    WGPUReflectionCompositionType_Invalid,
    WGPUReflectionCompositionType_Scalar,
    WGPUReflectionCompositionType_Vec2,
    WGPUReflectionCompositionType_Vec3,
    WGPUReflectionCompositionType_Vec4
}WGPUReflectionCompositionType;


typedef struct WGPUReflectionAttribute{
    uint32_t location;
    WGPUReflectionComponentType componentType;
    WGPUReflectionCompositionType compositionType;
}WGPUReflectionAttribute;

typedef struct WGPUAttributeReflectionInfo{
    uint32_t attributeCount;
    WGPUReflectionAttribute* attributes;
}WGPUAttributeReflectionInfo;

typedef enum WGPUReflectionInfoRequestStatus {
    WGPUReflectionInfoRequestStatus_Unused            = 0x00000000,
    WGPUReflectionInfoRequestStatus_Success           = 0x00000001,
    WGPUReflectionInfoRequestStatus_CallbackCancelled = 0x00000002,
    WGPUReflectionInfoRequestStatus_Force32           = 0x7FFFFFFF
}WGPUReflectionInfoRequestStatus;



typedef struct WGPUReflectionInfo {
    WGPUChainedStruct* nextInChain;
    uint32_t globalCount;
    const WGPUGlobalReflectionInfo* globals;
    const WGPUAttributeReflectionInfo* inputAttributes;
    const WGPUAttributeReflectionInfo* outputAttributes;
}WGPUReflectionInfo;
typedef void (*WGPUReflectionInfoCallback)(WGPUReflectionInfoRequestStatus status, WGPUReflectionInfo const* reflectionInfo, void* userdata1, void* userdata2);

typedef struct WGPUReflectionInfoCallbackInfo {
    WGPUChainedStruct* nextInChain;
    WGPUCallbackMode mode;
    WGPUReflectionInfoCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
}WGPUReflectionInfoCallbackInfo;

typedef struct WGPUMultisampleState {
    WGPUChainedStruct* nextInChain;
    uint32_t count;
    uint32_t mask;
    WGPUBool32 alphaToCoverageEnabled;
} WGPUMultisampleState;

typedef struct WGPUComputeState {
    WGPUChainedStruct * nextInChain;
    WGPUShaderModule module;
    WGPUStringView entryPoint;
    size_t constantCount;
    WGPUConstantEntry const * constants;
} WGPUComputeState;

typedef struct WGPURenderPipelineDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUPipelineLayout layout;
    WGPUVertexState vertex;
    WGPUPrimitiveState primitive;
    const WGPUDepthStencilState* depthStencil;
    WGPUMultisampleState multisample;
    const WGPUFragmentState* fragment;
} WGPURenderPipelineDescriptor;

typedef struct WGPUComputePipelineDescriptor {
    WGPUChainedStruct* nextInChain;
    WGPUStringView label;
    WGPUPipelineLayout layout;
    WGPUComputeState compute;
} WGPUComputePipelineDescriptor;

typedef struct WGPUSurfaceConfiguration {
    WGPUChainedStruct* nextInChain;
    WGPUDevice device;
    WGPUTextureFormat format;
    WGPUTextureUsage usage;
    uint32_t width;
    uint32_t height;
    size_t viewFormatCount;
    const WGPUTextureFormat* viewFormats;
    WGPUCompositeAlphaMode alphaMode;
    WGPUPresentMode presentMode;
} WGPUSurfaceConfiguration WGPU_STRUCT_ATTRIBUTE;

typedef void (*WGPURequestAdapterCallback)(WGPURequestAdapterStatus status, WGPUAdapter adapter, struct WGPUStringView message, void* userdata1, void* userdata2);
typedef void (*WGPURequestDeviceCallback) (WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) WGPU_FUNCTION_ATTRIBUTE;

typedef struct WGPURequestAdapterCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPURequestAdapterCallback callback;
    void* userdata1;
    void* userdata2;
} WGPURequestAdapterCallbackInfo;

typedef struct WGPURequestDeviceCallbackInfo {
    WGPUChainedStruct * nextInChain;
    WGPUCallbackMode mode;
    WGPURequestDeviceCallback callback;
    WGPU_NULLABLE void* userdata1;
    WGPU_NULLABLE void* userdata2;
} WGPURequestDeviceCallbackInfo WGPU_STRUCT_ATTRIBUTE;

typedef struct WGPUTransform3DDescriptor{
    float x;
    float y;
    float z;
}WGPUTransform3DDescriptor;

typedef struct WGPURayTracingAccelerationInstanceTransformDescriptor{
    WGPUTransform3DDescriptor translation;
    WGPUTransform3DDescriptor rotation;
    WGPUTransform3DDescriptor scale;
}WGPURayTracingAccelerationInstanceTransformDescriptor;

typedef struct WGPURayTracingAccelerationGeometryVertexDescriptor{
    WGPUBuffer buffer;
    WGPUVertexFormat format;
    uint32_t stride;
    uint32_t offset;
    uint32_t count;
}WGPURayTracingAccelerationGeometryVertexDescriptor;

typedef struct WGPURayTracingAccelerationGeometryIndexDescriptor{
    WGPUBuffer buffer;
    WGPUIndexFormat format;
    uint32_t offset;
    uint32_t count;
}WGPURayTracingAccelerationGeometryIndexDescriptor;

typedef struct WGPURayTracingAccelerationGeometryAABBDescriptor{
    WGPUBuffer buffer;
    uint32_t stride;
    uint32_t offset;
    uint32_t count;
}WGPURayTracingAccelerationGeometryAABBDescriptor;

typedef struct WGPURayTracingAccelerationGeometryDescriptor{
    WGPURayTracingAccelerationGeometryUsage usage;
    WGPURayTracingAccelerationGeometryType type;
    WGPURayTracingAccelerationGeometryVertexDescriptor vertex;
    WGPURayTracingAccelerationGeometryIndexDescriptor index;
    WGPURayTracingAccelerationGeometryAABBDescriptor aabb;
}WGPURayTracingAccelerationGeometryDescriptor;

typedef struct WGPUTransformMatrix {
    float matrix[3][4];
} WGPUTransformMatrix;

typedef struct WGPURayTracingAccelerationInstanceDescriptor{
    WGPURayTracingAccelerationInstanceUsage usage;
    uint8_t mask;
    uint32_t instanceId;
    uint32_t instanceOffset;
    //WGPURayTracingAccelerationInstanceTransformDescriptor transform;
    WGPUTransformMatrix transformMatrix;
    WGPURayTracingAccelerationContainer geometryContainer;
}WGPURayTracingAccelerationInstanceDescriptor;

typedef struct WGPURayTracingAccelerationContainerDescriptor{
    WGPURayTracingAccelerationContainerUsage usage;
    WGPURayTracingAccelerationContainerLevel level;
    uint32_t geometryCount;
    uint32_t instanceCount;
    const WGPURayTracingAccelerationGeometryDescriptor* geometries;
    const WGPURayTracingAccelerationInstanceDescriptor* instances;
}WGPURayTracingAccelerationContainerDescriptor;

typedef struct WGPURayTracingStateDescriptor{
    WGPURayTracingShaderBindingTable shaderBindingTable;
    uint32_t maxRecursionDepth;
    uint32_t maxPayloadSize;
}WGPURayTracingStateDescriptor;

typedef struct WGPURayTracingPipelineDescriptor{
    WGPUPipelineLayout layout;
    WGPURayTracingStateDescriptor rayTracingState;
}WGPURayTracingPipelineDescriptor;

typedef struct WGPURayTracingShaderBindingTableStageDescriptor{
    WGPUShaderStage	stage;
    WGPUShaderModule module;
}WGPURayTracingShaderBindingTableStageDescriptor;

typedef struct WGPURayTracingShaderBindingTableGroupDescriptor{
    WGPURayTracingShaderBindingTableGroupType type;
    uint32_t generalIndex;
    uint32_t closestHitIndex;
    uint32_t anyHitIndex;
    uint32_t intersectionIndex;
}WGPURayTracingShaderBindingTableGroupDescriptor;

typedef struct WGPURayTracingShaderBindingTableDescriptor{
    uint32_t stageCount;
    const WGPURayTracingShaderBindingTableStageDescriptor* stages;
    uint32_t groupCount;
    const WGPURayTracingShaderBindingTableGroupDescriptor* groups;
}WGPURayTracingShaderBindingTableDescriptor;

#ifdef __cplusplus
extern "C"{
#endif
WGVK_EXPORT WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor *descriptor);
WGVK_EXPORT WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount, WGPUFutureWaitInfo* futures, uint64_t timeoutNS);
WGVK_EXPORT WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallbackInfo callbackInfo);
WGVK_EXPORT WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptor);
WGVK_EXPORT WGPUStatus wgpuDeviceGetAdapterInfo(WGPUDevice device, WGPUAdapterInfo * adapterInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuAdapterGetLimits(WGPUAdapter adapter, WGPULimits * limits) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter, WGPU_NULLABLE WGPUDeviceDescriptor const * options, WGPURequestDeviceCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUQueue wgpuDeviceGetQueue(WGPUDevice device);
WGVK_EXPORT void wgpuSurfaceGetCapabilities(WGPUSurface wgpuSurface, WGPUAdapter adapter, WGPUSurfaceCapabilities* capabilities);
WGVK_EXPORT void wgpuSurfaceConfigure(WGPUSurface surface, const WGPUSurfaceConfiguration* config);
WGVK_EXPORT void wgpuSurfaceRelease(WGPUSurface surface);
WGVK_EXPORT WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor);
WGVK_EXPORT WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor *descriptor);

WGVK_EXPORT uint32_t wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUTextureDimension wgpuTextureGetDimension(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUTextureFormat wgpuTextureGetFormat(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT uint32_t wgpuTextureGetHeight(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT uint32_t wgpuTextureGetMipLevelCount(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT uint32_t wgpuTextureGetSampleCount(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUTextureUsage wgpuTextureGetUsage(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT uint32_t wgpuTextureGetWidth(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;

WGVK_EXPORT WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor);
WGVK_EXPORT WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* desc);
WGVK_EXPORT void wgpuQueueWriteBuffer(WGPUQueue cSelf, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size);
WGVK_EXPORT void wgpuBufferMap(WGPUBuffer buffer, WGPUMapMode mapmode, size_t offset, size_t size, void** data);
WGVK_EXPORT void wgpuBufferUnmap(WGPUBuffer buffer);

WGVK_EXPORT WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo);
WGVK_EXPORT size_t wgpuBufferGetSize(WGPUBuffer buffer);
WGVK_EXPORT void wgpuQueueWriteTexture(WGPUQueue queue, WGPUTexelCopyTextureInfo const * destination, const void* data, size_t dataSize, WGPUTexelCopyBufferLayout const * dataLayout, WGPUExtent3D const * writeSize);

WGVK_EXPORT WGPUFence wgpuDeviceCreateFence                      (WGPUDevice device);
WGVK_EXPORT void wgpuFenceWait                                   (WGPUFence fence, uint64_t timeoutNS);
WGVK_EXPORT void wgpuFencesWait                                  (const WGPUFence* fences, uint32_t fenceCount, uint64_t timeoutNS);
WGVK_EXPORT void wgpuFenceAttachCallback                         (WGPUFence fence, void(*callback)(void*), void* userdata);
WGVK_EXPORT void wgpuFenceAddRef                                 (WGPUFence fence);
WGVK_EXPORT void wgpuFenceRelease                                (WGPUFence fence);
WGVK_EXPORT WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout  (WGPUDevice device, const WGPUBindGroupLayoutDescriptor* bindGroupLayoutDescriptor);
WGVK_EXPORT WGPUShaderModule    wgpuDeviceCreateShaderModule     (WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor);
WGVK_EXPORT WGPUPipelineLayout  wgpuDeviceCreatePipelineLayout   (WGPUDevice device, const WGPUPipelineLayoutDescriptor* pldesc);
WGVK_EXPORT WGPURenderPipeline  wgpuDeviceCreateRenderPipeline   (WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor);
WGVK_EXPORT WGPUComputePipeline wgpuDeviceCreateComputePipeline  (WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor);
WGVK_EXPORT WGPUFuture          wgpuShaderModuleGetReflectionInfo(WGPUShaderModule shaderModule, WGPUReflectionInfoCallbackInfo callbackInfo);
WGVK_EXPORT WGPUBindGroup wgpuDeviceCreateBindGroup              (WGPUDevice device, const WGPUBindGroupDescriptor* bgdesc);
WGVK_EXPORT void wgpuWriteBindGroup                              (WGPUDevice device, WGPUBindGroup, const WGPUBindGroupDescriptor* bgdesc);
WGVK_EXPORT WGPUCommandEncoder wgpuDeviceCreateCommandEncoder    (WGPUDevice device, const WGPUCommandEncoderDescriptor* cdesc);
WGVK_EXPORT WGPUCommandBuffer wgpuCommandEncoderFinish           (WGPUCommandEncoder commandEncoder, WGPU_NULLABLE WGPUCommandBufferDescriptor const * descriptor);
WGVK_EXPORT void wgpuDeviceTick                                  (WGPUDevice device);
WGVK_EXPORT void wgpuQueueSubmit                                 (WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* buffers);
WGVK_EXPORT void wgpuQueueWaitIdle                               (WGPUQueue queue);
WGVK_EXPORT void wgpuCommandEncoderCopyBufferToBuffer            (WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size);
WGVK_EXPORT void wgpuCommandEncoderCopyBufferToTexture           (WGPUCommandEncoder commandEncoder, const WGPUTexelCopyBufferInfo*  source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize);
WGVK_EXPORT void wgpuCommandEncoderCopyTextureToBuffer           (WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyBufferInfo* destination, const WGPUExtent3D* copySize);
WGVK_EXPORT void wgpuCommandEncoderCopyTextureToTexture          (WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize);
WGVK_EXPORT void wgpuRenderPassEncoderDraw                       (WGPURenderPassEncoder rpenc, uint32_t vertices, uint32_t instances, uint32_t firstvertex, uint32_t firstinstance);
WGVK_EXPORT void wgpuRenderPassEncoderDrawIndexed                (WGPURenderPassEncoder rpenc, uint32_t indices, uint32_t instances, uint32_t firstindex, int32_t basevertex, uint32_t firstinstance);
WGVK_EXPORT void wgpuRenderPassEncoderSetBindGroup               (WGPURenderPassEncoder rpenc, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets);
WGVK_EXPORT void wgpuRenderPassEncoderSetPipeline                (WGPURenderPassEncoder rpenc, WGPURenderPipeline renderPipeline);
WGVK_EXPORT void wgpuRenderPassEncoderEnd                        (WGPURenderPassEncoder rrpenc);
WGVK_EXPORT void wgpuRenderPassEncoderRelease                    (WGPURenderPassEncoder rpenc);
WGVK_EXPORT void wgpuRenderPassEncoderAddRef                     (WGPURenderPassEncoder rpenc);
WGVK_EXPORT void wgpuRenderPassEncoderSetIndexBuffer             (WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderSetVertexBuffer            (WGPURenderPassEncoder rpe, uint32_t binding, WGPUBuffer buffer, size_t offset, uint64_t size);
WGVK_EXPORT void wgpuRenderPassEncoderDrawIndexedIndirect        (WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderDrawIndirect               (WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderSetBlendConstant           (WGPURenderPassEncoder renderPassEncoder, WGPUColor const * color) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderSetViewport                (WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderSetScissorRect             (WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
WGVK_EXPORT void wgpuComputePassEncoderSetPipeline               (WGPUComputePassEncoder cpe, WGPUComputePipeline computePipeline);
WGVK_EXPORT void wgpuComputePassEncoderSetBindGroup              (WGPUComputePassEncoder cpe, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const* dynamicOffsets);
WGVK_EXPORT void wgpuRaytracingPassEncoderSetPipeline            (WGPURaytracingPassEncoder cpe, WGPURaytracingPipeline raytracingPipeline);
WGVK_EXPORT void wgpuRaytracingPassEncoderSetBindGroup           (WGPURaytracingPassEncoder cpe, uint32_t groupIndex, WGPUBindGroup bindGroup, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets);
WGVK_EXPORT void wgpuRaytracingPassEncoderTraceRays              (WGPURaytracingPassEncoder cpe,  uint32_t rayGenerationOffset, uint32_t rayHitOffset, uint32_t rayMissOffset, uint32_t width, uint32_t height, uint32_t depth);
WGVK_EXPORT void wgpuComputePassEncoderDispatchWorkgroups        (WGPUComputePassEncoder cpe, uint32_t x, uint32_t y, uint32_t z);
WGVK_EXPORT void wgpuComputePassEncoderRelease                   (WGPUComputePassEncoder cpenc);
WGVK_EXPORT void wgpuSurfaceGetCurrentTexture                    (WGPUSurface surface, WGPUSurfaceTexture * surfaceTexture);
WGVK_EXPORT void wgpuSurfacePresent                              (WGPUSurface surface);
WGVK_EXPORT WGPURaytracingPassEncoder wgpuCommandEncoderBeginRaytracingPass(WGPUCommandEncoder enc, const WGPURayTracingPassDescriptor* rtDesc);
WGVK_EXPORT void wgpuRaytracingPassEncoderEnd(WGPURaytracingPassEncoder commandEncoder);
WGVK_EXPORT WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder enc, const WGPUComputePassDescriptor* cpdesc);
WGVK_EXPORT void wgpuComputePassEncoderEnd(WGPUComputePassEncoder commandEncoder);
WGVK_EXPORT WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder enc, const WGPURenderPassDescriptor* rpdesc);
    
WGVK_EXPORT void wgpuCommandEncoderBuildRayTracingAccelerationContainer(WGPUCommandEncoder encoder, WGPURayTracingAccelerationContainer container);
WGVK_EXPORT void wgpuCommandEncoderCopyRayTracingAccelerationContainer(WGPUCommandEncoder encoder, WGPURayTracingAccelerationContainer source, WGPURayTracingAccelerationContainer dest);
WGVK_EXPORT void wgpuCommandEncoderUpdateRayTracingAccelerationContainer(WGPUCommandEncoder encoder, WGPURayTracingAccelerationContainer container);

WGVK_EXPORT WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, WGPURenderBundleEncoderDescriptor const * descriptor);
WGVK_EXPORT WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, WGPU_NULLABLE WGPURenderBundleDescriptor const * descriptor);
WGVK_EXPORT void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPU_NULLABLE WGPUBindGroup group, size_t dynamicOffsetCount, uint32_t const * dynamicOffsets) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPU_NULLABLE WGPUBuffer buffer, uint64_t offset, uint64_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderAddRef(WGPURenderBundleEncoder renderBundleEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, size_t bundleCount, WGPURenderBundle const * bundles) WGPU_FUNCTION_ATTRIBUTE;

WGVK_EXPORT void wgpuAdapterInfoFreeMembers(WGPUAdapterInfo value) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuGetInstanceCapabilities(WGPUInstanceCapabilities * capabilities) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUProc wgpuGetProcAddress(WGPUStringView procName) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSupportedFeaturesFreeMembers(WGPUSupportedFeatures value) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures value) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities value) WGPU_FUNCTION_ATTRIBUTE;

WGVK_EXPORT void wgpuAdapterGetFeatures(WGPUAdapter adapter, WGPUSupportedFeatures * features) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuAdapterGetInfo(WGPUAdapter adapter, WGPUAdapterInfo * info) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBool wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature) WGPU_FUNCTION_ATTRIBUTE;

WGVK_EXPORT void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;

WGVK_EXPORT void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;


WGVK_EXPORT void wgpuBufferDestroy(WGPUBuffer buffer) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void const * wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void * wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer buffer) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBufferUsage wgpuBufferGetUsage(WGPUBuffer buffer) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuBufferReadMappedRange(WGPUBuffer buffer, size_t offset, void * data, size_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuBufferSetLabel(WGPUBuffer buffer, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuBufferWriteMappedRange(WGPUBuffer buffer, size_t offset, void const * data, size_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandBufferAddRef(WGPUCommandBuffer commandBuffer) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, WGPUStringView markerLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, WGPUStringView groupLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuCommandEncoderAddRef(WGPUCommandEncoder commandEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, WGPUStringView markerLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, WGPUStringView groupLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePassEncoderAddRef(WGPUComputePassEncoder computePassEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBindGroupLayout wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuComputePipelineAddRef(WGPUComputePipeline computePipeline) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, WGPUQuerySetDescriptor const * descriptor) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, WGPURenderPipelineDescriptor const * descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuDeviceDestroy(WGPUDevice device) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuDeviceGetFeatures(WGPUDevice device, WGPUSupportedFeatures * features) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuDeviceGetLimits(WGPUDevice device, WGPULimits * limits) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuDeviceGetLostFuture(WGPUDevice device) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuDevicePopErrorScope(WGPUDevice device, WGPUPopErrorScopeCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuDeviceSetLabel(WGPUDevice device, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUStatus wgpuInstanceGetWGSLLanguageFeatures(WGPUInstance instance, WGPUSupportedWGSLLanguageFeatures * features) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBool wgpuInstanceHasWGSLLanguageFeature(WGPUInstance instance, WGPUWGSLLanguageFeatureName feature) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuInstanceProcessEvents(WGPUInstance instance) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuQuerySetDestroy(WGPUQuerySet querySet) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT uint32_t wgpuQuerySetGetCount(WGPUQuerySet querySet) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUQueryType wgpuQuerySetGetType(WGPUQuerySet querySet) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuQuerySetSetLabel(WGPUQuerySet querySet, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuQuerySetAddRef(WGPUQuerySet querySet) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuQuerySetRelease(WGPUQuerySet querySet) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuQueueSetLabel(WGPUQueue queue, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleSetLabel(WGPURenderBundle renderBundle, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleAddRef(WGPURenderBundle renderBundle) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleRelease(WGPURenderBundle renderBundle) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView markerLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView groupLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, WGPUStringView markerLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderMultiDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPU_NULLABLE WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderMultiDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPU_NULLABLE WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, WGPUStringView groupLabel) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUBindGroupLayout wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuRenderPipelineAddRef(WGPURenderPipeline renderPipeline) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSamplerSetLabel(WGPUSampler sampler, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPUFuture wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSurfaceSetLabel(WGPUSurface surface, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSurfaceUnconfigure(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuSurfaceAddRef(WGPUSurface surface) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuTextureDestroy(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuTextureSetLabel(WGPUTexture texture, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT void wgpuTextureViewSetLabel(WGPUTextureView textureView, WGPUStringView label) WGPU_FUNCTION_ATTRIBUTE;
WGVK_EXPORT WGPURayTracingShaderBindingTable wgpuDeviceCreateRayTracingShaderBindingTable(WGPUDevice device, const WGPURayTracingShaderBindingTableDescriptor* descriptor);
WGVK_EXPORT WGPURayTracingAccelerationContainer wgpuDeviceCreateRayTracingAccelerationContainer(WGPUDevice device, const WGPURayTracingAccelerationContainerDescriptor* descriptor);
WGVK_EXPORT WGPURaytracingPipeline wgpuDeviceCreateRayTracingPipeline(WGPUDevice device, const WGPURayTracingPipelineDescriptor* descriptor);

WGVK_EXPORT void wgpuInstanceAddRef                       (WGPUInstance instance);
WGVK_EXPORT void wgpuAdapterAddRef                        (WGPUAdapter adapter);
WGVK_EXPORT void wgpuDeviceAddRef                         (WGPUDevice device);
WGVK_EXPORT void wgpuQueueAddRef                          (WGPUQueue device);
WGVK_EXPORT void wgpuReleaseRaytracingPassEncoder         (WGPURaytracingPassEncoder rtenc);
WGVK_EXPORT void wgpuTextureAddRef                        (WGPUTexture texture);
WGVK_EXPORT void wgpuTextureViewAddRef                    (WGPUTextureView textureView);
WGVK_EXPORT void wgpuSamplerAddRef                        (WGPUSampler texture);
WGVK_EXPORT void wgpuBufferAddRef                         (WGPUBuffer buffer);
WGVK_EXPORT void wgpuBindGroupAddRef                      (WGPUBindGroup bindGroup);
WGVK_EXPORT void wgpuShaderModuleAddRef                   (WGPUShaderModule module);
WGVK_EXPORT void wgpuBindGroupLayoutAddRef                (WGPUBindGroupLayout bindGroupLayout);
WGVK_EXPORT void wgpuPipelineLayoutAddRef                 (WGPUPipelineLayout pipelineLayout);
WGVK_EXPORT void wgpuCommandEncoderRelease                (WGPUCommandEncoder commandBuffer);
WGVK_EXPORT void wgpuCommandBufferRelease                 (WGPUCommandBuffer commandBuffer);

WGVK_EXPORT void wgpuInstanceRelease                      (WGPUInstance instance);
WGVK_EXPORT void wgpuAdapterRelease                       (WGPUAdapter adapter);
WGVK_EXPORT void wgpuDeviceRelease                        (WGPUDevice device);
WGVK_EXPORT void wgpuQueueRelease                         (WGPUQueue device);
WGVK_EXPORT void wgpuComputePassEncoderRelease            (WGPUComputePassEncoder rpenc);
WGVK_EXPORT void wgpuComputePipelineRelease               (WGPUComputePipeline pipeline);
WGVK_EXPORT void wgpuRenderPipelineRelease                (WGPURenderPipeline pipeline);
WGVK_EXPORT void wgpuBufferRelease                        (WGPUBuffer buffer);
WGVK_EXPORT void wgpuBindGroupRelease                     (WGPUBindGroup commandBuffer);
WGVK_EXPORT void wgpuBindGroupLayoutRelease               (WGPUBindGroupLayout commandBuffer);
WGVK_EXPORT void wgpuBindGroupLayoutRelease               (WGPUBindGroupLayout bglayout);
WGVK_EXPORT void wgpuPipelineLayoutRelease                (WGPUPipelineLayout layout);
WGVK_EXPORT void wgpuTextureRelease                       (WGPUTexture texture);
WGVK_EXPORT void wgpuTextureViewRelease                   (WGPUTextureView view);
WGVK_EXPORT void wgpuSamplerRelease                       (WGPUSampler sampler);
WGVK_EXPORT void wgpuShaderModuleRelease                  (WGPUShaderModule module);

WGVK_EXPORT WGPUCommandEncoder wgpuResetCommandBuffer(WGPUCommandBuffer commandEncoder);

WGVK_EXPORT void wgpuCommandEncoderTraceRays(WGPURenderPassEncoder encoder);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
} //extern "C"
    #if SUPPORT_WGPU_BACKEND == 1
        constexpr bool operator==(const WGPUBlendComponent& a, const WGPUBlendComponent& b) noexcept{
            return a.operation == b.operation && a.srcFactor == b.srcFactor && a.dstFactor == b.dstFactor;
        }
        constexpr bool operator==(const WGPUBlendState& a, const WGPUBlendState& b) noexcept{
            return a.color == b.color && a.alpha == b.alpha;
        }
    #endif
#endif

#endif // WGPU_H_INCLUDED
