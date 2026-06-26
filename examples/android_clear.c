/**
 * WGVK Android Example - Clear Screen
 *
 * Minimal NativeActivity app that initializes WebGPU via WGVK
 * and clears the screen to red. No shaders, no geometry.
 */
#include <stdlib.h>
#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <string.h>

#include "wgvk.h"

#define LOG_TAG "WGVK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define STRVIEW(str) \
    (WGPUStringView) { .data = str, .length = sizeof(str) - 1 }

typedef struct {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat format;
    uint32_t width;
    uint32_t height;
    int initialized;
} AppState;

static void on_adapter(WGPURequestAdapterStatus status,
                        WGPUAdapter adapter, WGPUStringView message,
                        void *userdata1, void *userdata2) {
    (void)status;
    (void)message;
    (void)userdata2;
    *(WGPUAdapter *)userdata1 = adapter;
}

static void on_device(WGPURequestDeviceStatus status, WGPUDevice device,
                       WGPUStringView message, void *userdata1,
                       void *userdata2) {
    (void)status;
    (void)message;
    (void)userdata2;
    *(WGPUDevice *)userdata1 = device;
}

static int init_wgpu(AppState *state) {
    // Instance
    WGPUInstanceFeatureName features[] = {
        WGPUInstanceFeatureName_TimedWaitAny,
    };
    WGPUInstanceDescriptor idesc = {
        .requiredFeatures = features,
        .requiredFeatureCount = 1,
    };
    state->instance = wgpuCreateInstance(&idesc);
    if (!state->instance) {
        LOGE("Failed to create instance");
        return 0;
    }
    LOGI("Instance created");

    // Adapter
    WGPURequestAdapterOptions aopts = {
        .featureLevel = WGPUFeatureLevel_Core,
    };
    WGPUFutureWaitInfo wait = {
        .future = wgpuInstanceRequestAdapter(
            state->instance, &aopts,
            (WGPURequestAdapterCallbackInfo){
                .callback = on_adapter,
                .userdata1 = &state->adapter,
            }),
    };
    wgpuInstanceWaitAny(state->instance, 1, &wait, ~(uint64_t)0);
    if (!state->adapter) {
        LOGE("Failed to get adapter");
        return 0;
    }
    LOGI("Adapter acquired");

    // Device
    WGPUDeviceDescriptor ddesc = {
        .label = STRVIEW("device"),
    };
    wait = (WGPUFutureWaitInfo){
        .future = wgpuAdapterRequestDevice(
            state->adapter, &ddesc,
            (WGPURequestDeviceCallbackInfo){
                .callback = on_device,
                .mode = WGPUCallbackMode_WaitAnyOnly,
                .userdata1 = &state->device,
            }),
    };
    wgpuInstanceWaitAny(state->instance, 1, &wait, ~(uint64_t)0);
    if (!state->device) {
        LOGE("Failed to get device");
        return 0;
    }
    state->queue = wgpuDeviceGetQueue(state->device);
    LOGI("Device acquired");

    return 1;
}

static void create_surface(AppState *state, ANativeWindow *window) {
    state->width = ANativeWindow_getWidth(window);
    state->height = ANativeWindow_getHeight(window);

    WGPUSurfaceSourceAndroidNativeWindow src = {
        .chain = {.sType = WGPUSType_SurfaceSourceAndroidNativeWindow},
        .window = window,
    };
    WGPUSurfaceDescriptor sdesc = {
        .nextInChain = &src.chain,
        .label = STRVIEW("surface"),
    };
    state->surface = wgpuInstanceCreateSurface(state->instance, &sdesc);
    if (!state->surface) {
        LOGE("Failed to create surface");
        return;
    }
    LOGI("Surface created: %ux%u", state->width, state->height);

    // Query preferred format from surface capabilities
    WGPUSurfaceCapabilities caps = {0};
    wgpuSurfaceGetCapabilities(state->surface, state->adapter, &caps);
    state->format = caps.formatCount > 0
                        ? caps.formats[0]
                        : WGPUTextureFormat_BGRA8Unorm;
    LOGI("Using format: %d", state->format);
    wgpuSurfaceCapabilitiesFreeMembers(caps);

    wgpuSurfaceConfigure(state->surface,
                         &(const WGPUSurfaceConfiguration){
                             .device = state->device,
                             .format = state->format,
                             .width = state->width,
                             .height = state->height,
                             .alphaMode = WGPUCompositeAlphaMode_Opaque,
                             .presentMode = WGPUPresentMode_Fifo,
                         });
    LOGI("Surface configured");
}

static void render_frame(AppState *state) {
    if (!state->surface)
        return;

    WGPUSurfaceTexture stex;
    wgpuSurfaceGetCurrentTexture(state->surface, &stex);
    if (stex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal)
        return;

    WGPUTextureView view = wgpuTextureCreateView(
        stex.texture,
        &(const WGPUTextureViewDescriptor){
            .format = state->format,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
            .usage = WGPUTextureUsage_RenderAttachment,
        });

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(
        state->device, &(const WGPUCommandEncoderDescriptor){0});

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(
        enc, &(const WGPURenderPassDescriptor){
                 .colorAttachmentCount = 1,
                 .colorAttachments =
                     (const WGPURenderPassColorAttachment[]){
                         {
                             .view = view,
                             .loadOp = WGPULoadOp_Clear,
                             .storeOp = WGPUStoreOp_Store,
                             .clearValue = {.r = 1, .g = 0, .b = 0, .a = 1},
                         },
                     },
             });
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, NULL);
    wgpuQueueSubmit(state->queue, 1, &cmd);

    wgpuCommandBufferRelease(cmd);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(enc);
    wgpuTextureViewRelease(view);
    wgpuTextureRelease(stex.texture);

    wgpuSurfacePresent(state->surface);
}

static void cleanup(AppState *state) {
    if (state->surface) {
        wgpuSurfaceRelease(state->surface);
        state->surface = NULL;
    }
    if (state->queue) {
        wgpuQueueRelease(state->queue);
        state->queue = NULL;
    }
    if (state->device) {
        wgpuDeviceRelease(state->device);
        state->device = NULL;
    }
    if (state->adapter) {
        wgpuAdapterRelease(state->adapter);
        state->adapter = NULL;
    }
    if (state->instance) {
        wgpuInstanceRelease(state->instance);
        state->instance = NULL;
    }
    state->initialized = 0;
}

// --- NativeActivity callbacks ---

static void on_window_created(ANativeActivity *activity,
                               ANativeWindow *window) {
    AppState *state = (AppState *)activity->instance;
    LOGI("Window created");

    if (!state->initialized) {
        if (!init_wgpu(state)) {
            LOGE("WebGPU init failed");
            return;
        }
        state->initialized = 1;
    }

    create_surface(state, window);
    render_frame(state);
}

static void on_window_resized(ANativeActivity *activity,
                               ANativeWindow *window) {
    AppState *state = (AppState *)activity->instance;
    uint32_t w = ANativeWindow_getWidth(window);
    uint32_t h = ANativeWindow_getHeight(window);
    if (w == state->width && h == state->height)
        return;

    state->width = w;
    state->height = h;
    LOGI("Resize: %ux%u", w, h);

    if (state->surface) {
        wgpuSurfaceConfigure(state->surface,
                             &(const WGPUSurfaceConfiguration){
                                 .device = state->device,
                                 .format = state->format,
                                 .width = w,
                                 .height = h,
                                 .alphaMode = WGPUCompositeAlphaMode_Opaque,
                                 .presentMode = WGPUPresentMode_Fifo,
                             });
        render_frame(state);
    }
}

static void on_window_redraw(ANativeActivity *activity,
                              ANativeWindow *window) {
    (void)window;
    render_frame((AppState *)activity->instance);
}

static void on_window_destroyed(ANativeActivity *activity,
                                 ANativeWindow *window) {
    (void)window;
    AppState *state = (AppState *)activity->instance;
    LOGI("Window destroyed");
    if (state->surface) {
        wgpuSurfaceRelease(state->surface);
        state->surface = NULL;
    }
}

static void on_destroy(ANativeActivity *activity) {
    AppState *state = (AppState *)activity->instance;
    LOGI("Destroying");
    cleanup(state);
    free(state);
}

JNIEXPORT void ANativeActivity_onCreate(ANativeActivity *activity,
                                         void *savedState,
                                         size_t savedStateSize) {
    (void)savedState;
    (void)savedStateSize;

    LOGI("onCreate");

    AppState *state = calloc(1, sizeof(AppState));
    activity->instance = state;

    activity->callbacks->onNativeWindowCreated = on_window_created;
    activity->callbacks->onNativeWindowResized = on_window_resized;
    activity->callbacks->onNativeWindowRedrawNeeded = on_window_redraw;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    activity->callbacks->onDestroy = on_destroy;
}
