#include <cstdlib>
#include "SPIRV/GlslangToSpv.h"
#include "glslang/Include/glslang_c_interface.h"
#include <glslang/Public/ShaderLang.h>
#include <glslang_c_api.h>
#include <webgpu/webgpu.h>
const extern TBuiltInResource DefaultTBuiltInResource_RG;
static EShLanguage wgpuShaderStageToGlslang(WGPUShaderStage stage){
    if(stage == WGPUShaderStage_Vertex) return EShLangVertex;
    if(stage == WGPUShaderStage_TessControl) return EShLangTessControl;
    if(stage == WGPUShaderStage_TessEvaluation) return EShLangTessEvaluation;
    if(stage == WGPUShaderStage_Geometry) return EShLangGeometry;
    if(stage == WGPUShaderStage_Fragment) return EShLangFragment;
    if(stage == WGPUShaderStage_Compute) return EShLangCompute;
    if(stage == WGPUShaderStage_RayGen) return EShLangRayGen;
    if(stage == WGPUShaderStage_Intersect) return EShLangIntersect;
    if(stage == WGPUShaderStage_AnyHit) return EShLangAnyHit;
    if(stage == WGPUShaderStage_ClosestHit) return EShLangClosestHit;
    if(stage == WGPUShaderStage_Miss) return EShLangMiss;
    if(stage == WGPUShaderStage_Callable) return EShLangCallable;
    if(stage == WGPUShaderStage_Task) return EShLangTask;
    if(stage == WGPUShaderStage_Mesh) return EShLangMesh;
    rg_trap();
    return (EShLanguage)~0;
}
static WGPUShaderStageEnum wgpuShaderStageToEnum(WGPUShaderStage stage){
    if(stage == WGPUShaderStage_Vertex) return WGPUShaderStageEnum_Vertex;
    if(stage == WGPUShaderStage_TessControl) return WGPUShaderStageEnum_TessControl;
    if(stage == WGPUShaderStage_TessEvaluation) return WGPUShaderStageEnum_TessEvaluation;
    if(stage == WGPUShaderStage_Geometry) return WGPUShaderStageEnum_Geometry;
    if(stage == WGPUShaderStage_Fragment) return WGPUShaderStageEnum_Fragment;
    if(stage == WGPUShaderStage_Compute) return WGPUShaderStageEnum_Compute;
    if(stage == WGPUShaderStage_RayGen) return WGPUShaderStageEnum_RayGen;
    if(stage == WGPUShaderStage_Intersect) return WGPUShaderStageEnum_Intersect;
    if(stage == WGPUShaderStage_AnyHit) return WGPUShaderStageEnum_AnyHit;
    if(stage == WGPUShaderStage_ClosestHit) return WGPUShaderStageEnum_ClosestHit;
    if(stage == WGPUShaderStage_Miss) return WGPUShaderStageEnum_Miss;
    if(stage == WGPUShaderStage_Callable) return WGPUShaderStageEnum_Callable;
    if(stage == WGPUShaderStage_Task) return WGPUShaderStageEnum_Task;
    if(stage == WGPUShaderStage_Mesh) return WGPUShaderStageEnum_Mesh;
    return (WGPUShaderStageEnum)~0;
}
static std::vector<uint32_t> glsl_to_spirv_single(WGPUDevice device, WGPUStringView source, EShLanguage stage, glslang::EShTargetClientVersion targetVulkanVersion, glslang::EShTargetLanguageVersion targetSpirvVersion){
    
    glslang::TShader shader(stage);
    shader.setEnvInput (glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, targetVulkanVersion);
    shader.setEnvClient(glslang::EShClientVulkan, targetVulkanVersion);
    shader.setEnvTarget(glslang::EShTargetSpv, targetSpirvVersion);
    const char* nullterminatedSource = nullptr;
    char* nts = nullptr;
    if(source.length != WGPU_STRLEN){
        nts = (char*)malloc(source.length + 1);
        memcpy(nts, source.data, source.length);
        nts[source.length] = '\0';
        nullterminatedSource = nts;
    }else{
        nullterminatedSource = source.data;
    }
    shader.setStrings(&nullterminatedSource, 1);

    auto stageToString = [](EShLanguage stage){
        switch(stage){
            case EShLangVertex: return "EShLangVertex";
            case EShLangTessControl: return "EShLangTessControl";
            case EShLangTessEvaluation: return "EShLangTessEvaluation";
            case EShLangGeometry: return "EShLangGeometry";
            case EShLangFragment: return "EShLangFragment";
            case EShLangCompute: return "EShLangCompute";
            case EShLangRayGen: return "EShLangRayGen";
            case EShLangIntersect: return "EShLangIntersect";
            case EShLangAnyHit: return "EShLangAnyHit";
            case EShLangClosestHit: return "EShLangClosestHit";
            case EShLangMiss: return "EShLangMiss";
            case EShLangCallable: return "EShLangCallable";
            case EShLangTask: return "EShLangTask";
            case EShLangMesh: return "EShLangMesh";
            default: return "??unknown ShaderStage??";
        }
    };

    TBuiltInResource Resources = DefaultTBuiltInResource_RG;
    Resources.maxComputeWorkGroupSizeX = 1024;
    Resources.maxComputeWorkGroupSizeY = 1024;
    Resources.maxComputeWorkGroupSizeZ = 1024;
    Resources.maxCombinedTextureImageUnits = 8;

    Resources.limits.generalUniformIndexing = true;
    Resources.limits.generalVariableIndexing = true;
    Resources.maxDrawBuffers = MAX_COLOR_ATTACHMENTS;

    EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
    char errorBuffer[2048];
    
    if(!shader.parse(&Resources, targetVulkanVersion, ECoreProfile, false, false, messages)){
        int n = snprintf(errorBuffer, sizeof(errorBuffer) - 1, "%s GLSL Parsing Failed: %s", stageToString(stage), shader.getInfoLog());
        if(device->uncapturedErrorCallbackInfo.callback){

            device->uncapturedErrorCallbackInfo.callback(&device, WGPUErrorType_Validation, WGPUStringView{
                errorBuffer, WGPU_STRLEN
            }, device->uncapturedErrorCallbackInfo.userdata1, device->uncapturedErrorCallbackInfo.userdata2);
        }
        puts(errorBuffer);
        
    }
    else{
        glslang::TProgram program;
        program.addShader(&shader);
        if(!program.link(messages)){
            snprintf(errorBuffer, sizeof(errorBuffer) - 1, "Error linkin shader: %s", program.getInfoLog());
            puts(errorBuffer);
        }
        glslang::TIntermediate* intermediate = program.getIntermediate(stage);
        std::vector<uint32_t> output;
        glslang::GlslangToSpv(*intermediate, output);
        if(nts){
            free(nts);
        }
        return output;
    }
    return std::vector<uint32_t>{};
}
static int glslang_initialize_process_called = 0;
WGPUShaderModule wgpuDeviceCreateShaderModuleGLSL(WGPUDevice device, const WGPUShaderModuleDescriptor* shDesc){
    if(glslang_initialize_process_called == 0){
        glslang::InitializeProcess();
        glslang_initialize_process_called = 1;
    }
    wgvk_assert(shDesc->nextInChain->sType == WGPUSType_ShaderSourceGLSL, "nextInChain->sType must be WGPUSType_ShaderSourceGLSL");
    
    WGPUShaderSourceGLSL* source = (WGPUShaderSourceGLSL*)shDesc->nextInChain;
    auto vulkanTarget = device->capabilities.dynamicRendering
        ? glslang::EShTargetVulkan_1_3 : glslang::EShTargetVulkan_1_1;
    auto spirvTarget = device->capabilities.dynamicRendering
        ? glslang::EShTargetSpv_1_6 : glslang::EShTargetSpv_1_3;
    std::vector<uint32_t> spirvSource = glsl_to_spirv_single(device, source->code, wgpuShaderStageToGlslang(source->stage), vulkanTarget, spirvTarget);

    if(spirvSource.empty()){
        return nullptr;
    }

    WGPUShaderModule shadermodule = (WGPUShaderModuleImpl*)RL_CALLOC(1, sizeof(WGPUShaderModuleImpl));
    if (!shadermodule) return nullptr;

    WGPUShaderSourceGLSL* copy = (WGPUShaderSourceGLSL*)RL_CALLOC(1, sizeof(WGPUShaderSourceGLSL));
    if (!copy) {
        RL_FREE(shadermodule);
        return nullptr;
    }

    copy->chain.sType = source->chain.sType;
    copy->stage = source->stage;
    
    size_t length = wgpuStrlen(source->code);
    copy->code.data = (char*)std::malloc(length + 1);
    if (!copy->code.data) {
        RL_FREE(copy);
        RL_FREE(shadermodule);
        return nullptr;
    }

    std::memcpy(const_cast<char*>(copy->code.data), source->code.data, length);
    const_cast<char*>(copy->code.data)[length] = '\0';
    copy->code.length = WGPU_STRLEN;
    
    shadermodule->source = (WGPUChainedStruct*)copy;
    shadermodule->device = device;
    shadermodule->refCount = 1;
    
    VkShaderModule vkModule = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo sci = {
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        nullptr,
        0,
        spirvSource.size() * sizeof(uint32_t),
        spirvSource.data(),
    };
    
    VkResult createResult = device->functions.vkCreateShaderModule(device->device, &sci, nullptr, &vkModule);
    if(createResult != VK_SUCCESS){
        std::free(const_cast<char*>(copy->code.data));
        RL_FREE(copy);
        RL_FREE(shadermodule);
        return nullptr;
    }
    
    WGPUShaderStageEnum stageIndex = wgpuShaderStageToEnum(source->stage);
    if(stageIndex < WGPUShaderStageEnum_EnumCount) {
        shadermodule->modules[stageIndex].module = vkModule;
        // GLSL entry point is typically "main". Safely copy it into the fixed-size buffer.
        // epName is char[16] in WGPUShaderModuleSingleEntryPoint.
        const char* entryName = "main";
        #if defined(_MSC_VER)
            strncpy_s(shadermodule->modules[stageIndex].epName, sizeof(shadermodule->modules[stageIndex].epName), entryName, _TRUNCATE);
        #else
            strncpy(shadermodule->modules[stageIndex].epName, entryName, sizeof(shadermodule->modules[stageIndex].epName) - 1);
            shadermodule->modules[stageIndex].epName[sizeof(shadermodule->modules[stageIndex].epName) - 1] = '\0';
        #endif
    } else {
        // Fallback for safety to ensure the module handle isn't lost if the stage mapping fails,
        // though strictly speaking this path implies an invalid stage.
        shadermodule->vulkanModuleMultiEP = vkModule;
    }
    
    return shadermodule;
}

const TBuiltInResource DefaultTBuiltInResource_RG = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,
    /* .maxMeshOutputVerticesEXT = */ 256,
    /* .maxMeshOutputPrimitivesEXT = */ 256,
    /* .maxMeshWorkGroupSizeX_EXT = */ 128,
    /* .maxMeshWorkGroupSizeY_EXT = */ 128,
    /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
    /* .maxTaskWorkGroupSizeX_EXT = */ 128,
    /* .maxTaskWorkGroupSizeY_EXT = */ 128,
    /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
    /* .maxMeshViewCountEXT = */ 4,
    /* .maxDualSourceDrawBuffersEXT = */ 1,

    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }
};