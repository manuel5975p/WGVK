#ifndef COMMON_H_
#define COMMON_H_
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <webgpu/webgpu.h>

typedef struct wgpu_base{
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUSurface surface;
    WGPUQueue queue;
    GLFWwindow* window;
}wgpu_base;

#ifndef STRVIEW
#define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif
#ifdef __EMSCRIPTEN__
#  define GLFW_EXPOSE_NATIVE_EMSCRIPTEN
#  ifndef GLFW_PLATFORM_EMSCRIPTEN // not defined in older versions of emscripten
#    define GLFW_PLATFORM_EMSCRIPTEN 0
#  endif
#else // __EMSCRIPTEN__
#  if SUPPORT_XLIB_SURFACE == 1
#    define GLFW_EXPOSE_NATIVE_X11
#  endif
#  if SUPPORT_WAYLAND_SURFACE == 1
#    define GLFW_EXPOSE_NATIVE_WAYLAND
#  endif
#  ifdef _GLFW_COCOA
#    define GLFW_EXPOSE_NATIVE_COCOA
#  endif
#  ifdef _WIN32
#    define GLFW_EXPOSE_NATIVE_WIN32
#  endif
#endif // __EMSCRIPTEN__

#ifdef GLFW_EXPOSE_NATIVE_COCOA
#  include <Foundation/Foundation.h>
#  include <QuartzCore/CAMetalLayer.h>
#endif

#ifndef __EMSCRIPTEN__
#  include <GLFW/glfw3native.h>
#endif

#include <stdint.h>

/* ---------- POSIX / Unix-like ---------- */
#if defined(__unix__) || defined(__APPLE__)
  #include <time.h>

  static inline uint64_t nanoTime(void)
  {
      struct timespec ts;
  #if defined(CLOCK_MONOTONIC_RAW)        /* Linux, FreeBSD */
      clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  #else                                   /* macOS 10.12+, other POSIX */
      clock_gettime(CLOCK_MONOTONIC, &ts);
  #endif
      return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  }

/* ---------- Windows ---------- */
#elif defined(_WIN32)
  #include <windows.h>

  static inline uint64_t nanoTime(void)
  {
      static LARGE_INTEGER freq = { 0 };
      if (freq.QuadPart == 0)               /* one-time init */
          QueryPerformanceFrequency(&freq);

      LARGE_INTEGER counter;
      QueryPerformanceCounter(&counter);
      /* scale ticks → ns: (ticks * 1e9) / freq */
      return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
  }

#else
  #error "Platform not supported"
#endif

static void adapterCallbackFunction(
        WGPURequestAdapterStatus status,
        WGPUAdapter adapter,
        WGPUStringView label,
        void* userdata1,
        void* userdata2
    ){
    *((WGPUAdapter*)userdata1) = adapter;
}
static void deviceCallbackFunction(
        WGPURequestDeviceStatus status,
        WGPUDevice device,
        WGPUStringView message,
        void* userdata1,
        void* userdata2
    ){
    *((WGPUDevice*)userdata1) = device;
}
static void CloseWindowCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}
#define timeoutNS 1000000000
wgpu_base wgpu_init(){
    //#ifdef __EMSCRIPTEN__
    //WGPUInstance instance = wgpuCreateInstance(NULL);
    //#else
    #ifndef __EMSCRIPTEN__
    WGPUInstanceLayerSelection lsel = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_InstanceLayerSelection
        }
    };
    const char* layernames[] = {"VK_LAYER_KHRONOS_validation"};
    lsel.instanceLayers = layernames;
    lsel.instanceLayerCount = 1;
    WGPUInstanceFeatureName instanceFeatures[2] = {
        WGPUInstanceFeatureName_TimedWaitAny,
        WGPUInstanceFeatureName_ShaderSourceSPIRV,
    };
    WGPUInstanceDescriptor instanceDescriptor = {
        .nextInChain = 
        #ifdef NDEBUG
        NULL
        #else
        &lsel.chain
        #endif
        ,
        .requiredFeatures = instanceFeatures,
        .requiredFeatureCount = 2,
    };
    #else
    WGPUInstanceDescriptor instanceDescriptor = {
        .capabilities = {
            .timedWaitAnyEnable = 1
        }
    };
    #endif
    WGPUInstance instance = wgpuCreateInstance(&instanceDescriptor);
    
    //#endif

    

    WGPURequestAdapterOptions adapterOptions = {0};
    adapterOptions.featureLevel = WGPUFeatureLevel_Compatibility;
    adapterOptions.backendType = WGPUBackendType_WebGPU;
    WGPURequestAdapterCallbackInfo adapterCallback = {0};
    adapterCallback.callback = adapterCallbackFunction;
    adapterCallback.mode = WGPUCallbackMode_WaitAnyOnly;

    WGPUAdapter requestedAdapter;
    adapterCallback.userdata1 = (void*)&requestedAdapter;
    

    WGPUFuture aFuture = wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallback);
    WGPUFutureWaitInfo winfo = {
        .future = aFuture,
        .completed = 0
    };

    wgpuInstanceWaitAny(instance, 1, &winfo, timeoutNS);
    WGPUStringView deviceLabel = {"WGPU Device", sizeof("WGPU Device") - 1};
    if(requestedAdapter == NULL){
        fprintf(stderr, "Adapter is NULL\n");
    }

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
    wgpuInstanceWaitAny(instance, 1, &requestDeviceFutureWaitInfo, timeoutNS);

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    glfwInit();
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(900, 900, "GLFW Window", NULL, NULL);

    #ifdef _WIN32
    WGPUSurfaceSourceWindowsHWND surfaceChainObj = {
        .chain = {
            .sType = WGPUSType_SurfaceSourceWindowsHWND,
            .next = NULL
        },
        .hwnd = glfwGetWin32Window(window),
        .hinstance = GetModuleHandle(NULL)
    };
    WGPUChainedStruct* surfaceChain = &surfaceChainObj.chain;
    #elif !defined(__EMSCRIPTEN__)
    WGPUSurfaceSourceXlibWindow surfaceChainX11;
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    surfaceChainX11.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    surfaceChainX11.chain.next = NULL;
    surfaceChainX11.display = x11_display;
    surfaceChainX11.window = x11_window;

    struct wl_display* native_display = glfwGetWaylandDisplay();
    struct wl_surface* native_surface = glfwGetWaylandWindow(window);
    WGPUSurfaceSourceWaylandSurface surfaceChainWayland;
    surfaceChainWayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
    surfaceChainWayland.chain.next = NULL;
    surfaceChainWayland.display = native_display;
    surfaceChainWayland.surface = native_surface;
    WGPUChainedStruct* surfaceChain = NULL;
    if(x11_window == 0){
        printf("Using wayland\n");
        surfaceChain = (WGPUChainedStruct*)&surfaceChainWayland;
    }
    else{
        printf("Using X11\n");
        surfaceChain = (WGPUChainedStruct*)&surfaceChainX11;
    }
    #else
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector fromCanvasHTMLSelector;
    fromCanvasHTMLSelector.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    fromCanvasHTMLSelector.selector = (WGPUStringView){ "canvas", WGPU_STRLEN };
    WGPUChainedStruct* surfaceChain = (WGPUChainedStruct*)&fromCanvasHTMLSelector;
    #endif
    WGPUSurfaceDescriptor surfaceDescriptor = {
        .nextInChain = surfaceChain
    };
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    WGPUSurfaceCapabilities caps = {0};
    WGPUPresentMode desiredPresentMode = WGPUPresentMode_Fifo;
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDescriptor);

    wgpuSurfaceGetCapabilities(surface, requestedAdapter, &caps);
    
    wgpuSurfaceConfigure(surface, &(const WGPUSurfaceConfiguration){
        .alphaMode = WGPUCompositeAlphaMode_Opaque,
        .presentMode = desiredPresentMode,
        .device = device,
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = width,
        .height = height
    });

    glfwSetKeyCallback(window, CloseWindowCallback);
    return (wgpu_base){
        .instance = instance,
        .adapter = requestedAdapter,
        .device = device,
        .queue = queue,
        .surface = surface,
        .window = window
    };
}
#endif
