/*
 * MIT License
 * 
 * wgvk.c - A single file WebGPU implementation in C11
 * 
 * Copyright (c) 2025 @manuel5975p
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "wgvk_config.h"
#include <stdatomic.h>
#include <stdint.h>
#define VK_NO_PROTOTYPES
#include "vulkan/vulkan_core.h"
#define VOLK_IMPLEMENTATION
#ifndef zeroinit
    #define zeroinit = {0}
#endif

#ifdef _WIN32
    #define SUPPORT_WIN32_SURFACE 1
#endif
#if SUPPORT_DRM_SURFACE == 1
    #define VK_KHR_display 1
    #define VK_EXT_acquire_drm_display 1
#endif

#if SUPPORT_XLIB_SURFACE == 1
    #define VK_KHR_xlib_surface 1
#endif
#if SUPPORT_WAYLAND_SURFACE == 1
    #define VK_KHR_wayland_surface 1 // Define set to 1 for clarity
#endif
#if SUPPORT_WIN32_SURFACE == 1
    #define VK_KHR_win32_surface 1 // Define set to 1 for clarity
#endif
#if SUPPORT_ANDROID_SURFACE == 1
    #define VK_KHR_android_surface 1 // Define set to 1 for clarity
#endif
#if SUPPORT_METAL_SURFACE == 1 // For macOS/iOS using MoltenVK
    #define VK_EXT_metal_surface 1 // Define set to 1 for clarity (Note: EXT, not KHR)
#endif

#if SUPPORT_XLIB_SURFACE == 1
    #include <X11/Xlib.h>
    #define VK_NO_PROTOTYPES
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_xlib.h>
#endif

#if SUPPORT_WAYLAND_SURFACE == 1
    #include <wayland-client.h>
    #include <wayland-client-protocol.h>
    #define VK_NO_PROTOTYPES
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_wayland.h>
#endif

#if SUPPORT_WIN32_SURFACE == 1
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define VK_NO_PROTOTYPES
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_win32.h>
#endif

#if SUPPORT_ANDROID_SURFACE == 1
    #include <android/native_window.h>
    #define VK_NO_PROTOTYPES
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_android.h>
#endif

#if SUPPORT_METAL_SURFACE == 1 // For macOS/iOS using MoltenVK
    #define VK_EXT_metal_surface 1 // Define set to 1 for clarity (Note: EXT, not KHR)
    // No specific native C header needed here usually, CAMetalLayer is often handled via void*
    // If Objective-C interop is used elsewhere, #import <Metal/Metal.h> would be needed there.
    #define VK_NO_PROTOTYPES
    #include <vulkan/vulkan.h>
    #include <vulkan/vulkan_metal.h>
#endif
#include <external/volk.h>
#include <inttypes.h>
#define Font rlFont
#define Matrix rlMatrix
#include <wgvk_structs_impl.h>

#if SUPPORT_WGSL == 1
    #include <tint_c_api.h>
#endif
#if SUPPORT_GLSL == 1
    #include <glslang_c_api.h>
#endif

#ifndef STRVIEW
    #define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif

#include "spirv_reflect.c"
#include <wgvk.h>
#include <external/VmaUsage.h>
#include <stdarg.h>
#include <wgvk_structs_impl.h>

/* ---------- POSIX / Unix-like ---------- */
#if defined(__unix__) || defined(__APPLE__)
  #include <time.h>

  static inline uint64_t wgvkNanoTime(void)
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

  static inline uint64_t wgvkNanoTime(void)
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


static void DeviceCallback(WGPUDevice device, WGPUErrorType type, WGPUStringView msg){
    if(device->uncapturedErrorCallbackInfo.callback){
        device->uncapturedErrorCallbackInfo.callback(&device, type, msg, device->uncapturedErrorCallbackInfo.userdata1, device->uncapturedErrorCallbackInfo.userdata2);
    }
}

#ifdef TRACELOG
    #undef TRACELOG
#endif
#define TRACELOG(level, ...) wgpuTraceLog(level, __VA_ARGS__)
void wgpuTraceLog(int logType, const char *text, ...);
const char* vkErrorString(int code);

#define WGPU_LOG_TRACE 1
#define WGPU_LOG_DEBUG 2
#define WGPU_LOG_INFO 3
#define WGPU_LOG_WARNING 4
#define WGPU_LOG_ERROR 5
#define WGPU_LOG_FATAL 6


static inline uint32_t findMemoryType(WGPUAdapter adapter, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    
    if(adapter->memProperties.memoryTypeCount == 0){
        vkGetPhysicalDeviceMemoryProperties(adapter->physicalDevice, &adapter->memProperties);
    }

    for (uint32_t i = 0; i < adapter->memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (adapter->memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    assert(false && "failed to find suitable memory type!");
    return ~0u;
}

static void doSurfaceCreation(WGPUInstance instance, WGPUSurface ret, WGPUChainedStruct* descriptor){
    switch(descriptor->sType){
        default: return;
        #if SUPPORT_METAL_SURFACE == 1
        case WGPUSType_SurfaceSourceMetalLayer:{
            WGPUSurfaceSourceMetalLayer* metalSource = (WGPUSurfaceSourceMetalLayer*)descriptor;
            VkMetalSurfaceCreateInfoEXT sci = {
                .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
                .pLayer = metalSource->layer
            };
            vkCreateMetalSurfaceEXT(
                instance->instance,
                &sci,
                NULL,
                &ret->surface
            );
            ret->surfaceType = SurfaceImplType_MetalLayer;
        }break;
        #endif
        #if SUPPORT_WIN32_SURFACE == 1
        case WGPUSType_SurfaceSourceWindowsHWND:{
            WGPUSurfaceSourceWindowsHWND* hwndSource = (WGPUSurfaceSourceWindowsHWND*)descriptor;
            VkWin32SurfaceCreateInfoKHR sci = {
                .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
                .hinstance = hwndSource->hinstance,
                .hwnd = hwndSource->hwnd
            };
            vkCreateWin32SurfaceKHR
            (
                instance->instance,
                &sci,
                NULL,
                &ret->surface
            );
            ret->surfaceType = SurfaceImplType_WindowsHWND;
        }break;
        #endif
        #if SUPPORT_XLIB_SURFACE == 1
        case WGPUSType_SurfaceSourceXlibWindow:{
            WGPUSurfaceSourceXlibWindow* xlibSource = (WGPUSurfaceSourceXlibWindow*)descriptor;
            wgvk_assert(xlibSource->window != 0, "xlibSource->window may not be 0");
            wgvk_assert(xlibSource->display != NULL, "xlibSource->display may not be 0");
            VkXlibSurfaceCreateInfoKHR sci = {
                .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
                .window = xlibSource->window,
                .dpy = (Display*)xlibSource->display
            };
            vkCreateXlibSurfaceKHR
            (
                instance->instance,
                &sci,
                NULL,
                &ret->surface
            );
            ret->surfaceType = SurfaceImplType_XlibWindow;
        }break;
        #endif
        #if SUPPORT_WAYLAND_SURFACE == 1
        case WGPUSType_SurfaceSourceWaylandSurface:{
            WGPUSurfaceSourceWaylandSurface* waylandSource = (WGPUSurfaceSourceWaylandSurface*)descriptor;
            wgvk_assert(waylandSource->surface != NULL, "waylandSource->window may not be 0");
            wgvk_assert(waylandSource->display != NULL, "waylandSource->display may not be 0");
            VkWaylandSurfaceCreateInfoKHR sci = {
                .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
                .surface = (struct wl_surface*)waylandSource->surface,
                .display = (struct wl_display*)waylandSource->display
            };
            vkCreateWaylandSurfaceKHR(
                instance->instance,
                &sci,
                NULL,
                &ret->surface
            );
            ret->surfaceType = SurfaceImplType_WaylandSurface;
        }break;
        #endif
        #if SUPPORT_ANDROID_SURFACE == 1
        case WGPUSType_SurfaceSourceAndroidNativeWindow:{
            WGPUSurfaceSourceAndroidNativeWindow* androidSource = (WGPUSurfaceSourceAndroidNativeWindow*)descriptor;
            VkAndroidSurfaceCreateInfoKHR sci = {
                .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
                .window = (struct ANativeWindow*)androidSource->window
            };
            vkCreateAndroidSurfaceKHR(
                instance->instance,
                &sci,
                NULL,
                &ret->surface
            );
            ret->surfaceType = SurfaceImplType_AndroidNativeWindow;
        }break;
        #endif
        #if SUPPORT_XCB_SURFACE == 1
        case WGPUSType_SurfaceSourceXCBWindow:{
            WGPUSurfaceSourceXCBWindow* xcbSource = (WGPUSurfaceSourceXCBWindow*)descriptor;
            VkXcbSurfaceCreateInfoKHR sci = {
                .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
                .connection = (xcb_connection_t*)xcbSource->connection,
                .window = xcbSource->window
            };
            vkCreateXcbSurfaceKHR(
                instance->instance,
                &sci,
                NULL,
                &ret->surface
            );
            ret->surfaceType = SurfaceImplType_XCBWindow;
        }break;
        #endif
        #if SUPPORT_DRM_SURFACE == 1
        case WGPUSType_SurfaceSourceDrmPlane:{
            WGPUSurfaceSourceDrmPlane* drm = (WGPUSurfaceSourceDrmPlane*)descriptor;
                WGPUAdapter adapter = drm->adapter;
                VkPhysicalDevice phys = adapter->physicalDevice;
                {
                    VkPhysicalDeviceDrmPropertiesEXT drmProps = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT };
                    VkPhysicalDeviceProperties2 p2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &drmProps };
                    vkGetPhysicalDeviceProperties2(phys, &p2);
                }
                VkDisplayKHR display = VK_NULL_HANDLE;
                
                if (vkGetDrmDisplayEXT) {
                    VkResult getDrmDisplayResult = vkGetDrmDisplayEXT(phys, drm->drmFd, drm->connectorId, &display);
                    if (getDrmDisplayResult != VK_SUCCESS) {
                        display = VK_NULL_HANDLE;
                    } else if (drm->acquireExclusive && vkAcquireDrmDisplayEXT) {
                        // Try to acquire exclusive control if requested
                        VkResult acquireDrmDisplayResult = vkAcquireDrmDisplayEXT(phys, drm->drmFd, display);
                    }
                }
                if (display == VK_NULL_HANDLE) { // This branch is already sus
                    uint32_t displayCount = 0;
                    if (vkGetPhysicalDeviceDisplayPropertiesKHR(phys, &displayCount, NULL) != VK_SUCCESS || displayCount == 0) {
                        ret->surface = VK_NULL_HANDLE;
                        return;
                    }
                    wgvk_assert(displayCount <= 64, "More than 64 displays not supported");
                    VkDisplayPropertiesKHR displays[64] = {0};
                    if (vkGetPhysicalDeviceDisplayPropertiesKHR(phys, &displayCount, displays) != VK_SUCCESS) {
                        ret->surface = VK_NULL_HANDLE;
                        return;
                    }
                    // if caller provided a nonzero connectorId but EXT isn’t available
                    // we can’t reliably map it therefore pick the first display.
                    display = displays[0].display;
                }
                uint32_t modeCount = 64;
                VkResult getDisplayModeResult = vkGetDisplayModePropertiesKHR(phys, display, &modeCount, NULL);
                VkDisplayModePropertiesKHR modeProps[64] = {0};
                getDisplayModeResult |= vkGetDisplayModePropertiesKHR(phys, display, &modeCount, modeProps);
                if (getDisplayModeResult != VK_SUCCESS){
                    fprintf(stderr, "Surface creation failed: vkGetDisplayModePropertiesKHR returned %s", vkErrorString(getDisplayModeResult));
                    ret->surface = VK_NULL_HANDLE;
                    return;
                }
                else if(modeCount == 0) {
                    fprintf(stderr, "Surface creation failed: vkGetDisplayModePropertiesKHR returned 0 modes");
                    ret->surface = VK_NULL_HANDLE;
                    return;
                }
            
                uint32_t chosenModeIndex = 0;
                if (drm->modeSelect.type == WGPUDrmModeSelect_ByIndex) {

                    chosenModeIndex = (drm->modeSelect.index < modeCount) ? drm->modeSelect.index : 0;
                } else {
                    /* Adjust field names if your WGPUDrmModeByGeometry differs */
                    uint32_t targetW  = drm->modeSelect.geometry.width;
                    uint32_t targetH  = drm->modeSelect.geometry.height;
                    uint32_t targetHz = drm->modeSelect.geometry.refreshMilliHz;
                
                    uint32_t i;
                    uint32_t best = 0;
                    uint32_t bestRefreshDiff = 0xFFFFFFFFu;
                    int foundExactWH = 0;
                
                    for (i = 0; i < modeCount && i < 64; ++i) {
                        const VkDisplayModeParametersKHR* p = &modeProps[i].parameters;
                        if (p->visibleRegion.width == targetW && p->visibleRegion.height == targetH) {
                            if (targetHz == 0) { best = i; foundExactWH = 1; break; }
                            {
                                uint32_t hz = p->refreshRate;
                                uint32_t diff = (hz > targetHz) ? (hz - targetHz) : (targetHz - hz);
                                if (!foundExactWH || diff < bestRefreshDiff) {
                                    bestRefreshDiff = diff;
                                    best = i;
                                    foundExactWH = 1;
                                }
                            }
                        }
                    }
                    chosenModeIndex = foundExactWH ? best : 0;
                }
            
                VkDisplayModeKHR displayMode = modeProps[chosenModeIndex].displayMode;
                VkDisplayModeParametersKHR modeParams = modeProps[chosenModeIndex].parameters;
            
                /* --- Step 3: Pick a display plane that supports this display --- */
                uint32_t planeCount = 64;
                VkDisplayPlanePropertiesKHR planeProps[64] = {0};
                if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(phys, &planeCount, planeProps) != VK_SUCCESS ||
                    planeCount == 0) {
                    break;
                }
            
                uint32_t chosenPlane = 0xFFFFFFFFu;
            
                /* Treat drm->planeId as an index hint */
                if (drm->planeId < planeCount) {
                    uint32_t supportedCount = 64;
                    VkDisplayKHR supported[64] = {0};
                    if (vkGetDisplayPlaneSupportedDisplaysKHR(phys, drm->planeId, &supportedCount, supported) == VK_SUCCESS &&
                        supportedCount > 0) {
                        uint32_t si;
                        for (si = 0; si < supportedCount && si < 64; ++si) {
                            if (supported[si] == display) { chosenPlane = drm->planeId; break; }
                        }
                    }
                }
            
                if (chosenPlane == 0xFFFFFFFFu) {
                    uint32_t i;
                    for (i = 0; i < planeCount && i < 64; ++i) {
                        uint32_t supportedCount = 64;
                        VkDisplayKHR supported[64] = {0};
                        if (vkGetDisplayPlaneSupportedDisplaysKHR(phys, i, &supportedCount, supported) != VK_SUCCESS ||
                            supportedCount == 0) {
                            continue;
                        }
                        {
                            uint32_t si;
                            for (si = 0; si < supportedCount && si < 64; ++si) {
                                if (supported[si] == display) { chosenPlane = i; break; }
                            }
                        }
                        if (chosenPlane != 0xFFFFFFFFu) break;
                    }
                }
            
                if (chosenPlane == 0xFFFFFFFFu) {
                    wgvk_assert(false, "No plane supports the display");
                    break; /* No plane supports the display */
                }
            
                VkDisplayPlaneCapabilities2KHR caps = {
                    .sType = VK_STRUCTURE_TYPE_DISPLAY_PLANE_CAPABILITIES_2_KHR
                };

                VkDisplayPlaneInfo2KHR planeInfo2 = {
                    .sType = VK_STRUCTURE_TYPE_DISPLAY_PLANE_INFO_2_KHR,
                    .planeIndex = chosenPlane,
                    .mode = displayMode,
                };
                VkResult getDPC = vkGetDisplayPlaneCapabilities2KHR(phys, &planeInfo2, &caps);
                if(getDPC != VK_SUCCESS){
                    abort();
                }
                /* --- Step 4: Create the display surface --- */
                VkDisplaySurfaceCreateInfoKHR sci = {0};
                sci.sType           = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
                sci.displayMode     = displayMode;
                sci.planeIndex      = chosenPlane;
                sci.planeStackIndex = planeProps[chosenPlane].currentStackIndex;
                sci.transform       = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
                sci.globalAlpha     = 1.0f;
                sci.alphaMode       = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
                sci.imageExtent     = modeParams.visibleRegion;
                VkSurfaceKHR surface = NULL;
                if (vkCreateDisplayPlaneSurfaceKHR(instance->instance, &sci, NULL, &surface) != VK_SUCCESS) {
                    wgvk_assert(false, "Surface creation failed");
                }
                wgvk_assert(surface, "Surface creation failed");
                VkBool32 supported = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(phys, 0, surface, &supported);
                wgvk_assert(supported, "Created surface is not supported by physicalDevice");
                ret->surface = surface;
                ret->surfaceType = SurfaceImplType_DrmPlane;

        }break;
        #endif
    }
}

WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance, const WGPUSurfaceDescriptor* descriptorRoot){
    ENTRY();
    wgvk_assert(descriptorRoot->nextInChain, "SurfaceDescriptor must have a nextInChain");
    WGPUSurface ret = RL_CALLOC(1, sizeof(WGPUSurfaceImpl));
    ret->refCount = 1;
    int colorSpaceSet = 0;
    int surfaceCreated = 0;
    WGPUChainedStruct* head = descriptorRoot->nextInChain;
    do{
        switch(head->sType){
            case WGPUSType_SurfaceSourceWaylandSurface:               // [[fallthrough]];
            case WGPUSType_SurfaceSourceXlibWindow:                   // [[fallthrough]];
            case WGPUSType_SurfaceSourceXCBWindow:                    // [[fallthrough]];
            case WGPUSType_SurfaceSourceWindowsHWND:                  // [[fallthrough]];
            case WGPUSType_SurfaceSourceMetalLayer:                   // [[fallthrough]];
            case WGPUSType_SurfaceSourceAndroidNativeWindow:          // [[fallthrough]];
            case WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector:
                doSurfaceCreation(instance, ret, head);
                surfaceCreated = 1;
            break;
            case WGPUSType_SurfaceColorManagement: {
                WGPUSurfaceColorManagement* colorManagement = (WGPUSurfaceColorManagement*)head;
                ret->colorSpace = colorManagement->colorSpace;
                ret->tonemappingMode = colorManagement->toneMappingMode;
                colorSpaceSet = 1;
            }break;
            default:{
                wgvk_assert(false, "Unsupported SType for SurfaceDescriptor.nextInChain: %d\n", head->sType);
            }
        }
        head = head->next;
    }while(head != NULL);
    wgvk_assert(surfaceCreated != 0, "WGPUSurfaceDescriptor must have an SType chained that's some kind of WGPUSType_SurfaceSource[...]");
    if(colorSpaceSet == 0){
        ret->colorSpace = WGPUPredefinedColorSpace_SRGB;
        ret->tonemappingMode = WGPUToneMappingMode_Standard;
    }
    EXIT();
    return ret;
}

char* sw_sprintf(const char* format, ...) {
    if (!format) return NULL;
    
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);
    
    // Calculate required buffer size by processing format string
    size_t total_len = 0;
    const char* p = format;
    
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                total_len += 1; // Literal %
                p++;
                continue;
            }
            
            // Skip flags, width, precision
            while (*p && strchr("-+ #0", *p)) p++;
            while (*p && *p >= '0' && *p <= '9') p++;
            if (*p == '.') {
                p++;
                while (*p && *p >= '0' && *p <= '9') p++;
            }
            
            // Check for our custom specifier
            if (*p == 's' && p[1] == 'w') {
                WGPUStringView sv = va_arg(args1, WGPUStringView);
                if (sv.data) {
                    if (sv.length == WGPU_STRLEN) {
                        total_len += strlen(sv.data);
                    } else {
                        total_len += sv.length;
                    }
                }
                p += 2; // Skip "sw"
            } else if (*p) {
                // Standard specifier - estimate conservatively
                switch (*p) {
                    case 'd': case 'i': case 'o': case 'x': case 'X': case 'u':
                        va_arg(args1, int);
                        total_len += 32; // Conservative for 64-bit ints
                        break;
                    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                        va_arg(args1, double);
                        total_len += 64; // Conservative for doubles
                        break;
                    case 's':
                        {
                            const char* s = va_arg(args1, const char*);
                            total_len += s ? strlen(s) : 6; // "(null)"
                        }
                        break;
                    case 'c':
                        va_arg(args1, int);
                        total_len += 1;
                        break;
                    case 'p':
                        va_arg(args1, void*);
                        total_len += 18; // "0x" + 16 hex digits
                        break;
                    default:
                        total_len += 1; // Unknown, just add something
                        break;
                }
                p++;
            }
        } else {
            total_len += 1; // Literal character
            p++;
        }
    }
    
    va_end(args1);
    
    // Allocate buffer with some padding for safety
    char* buffer = RL_MALLOC(total_len + 16);
    if (!buffer) {
        va_end(args2);
        return NULL;
    }
    
    // Build the actual string
    char* out = buffer;
    p = format;
    
    while (*p) {
        if (*p == '%') {
            p++;
            if (*p == '%') {
                *out++ = '%';
                p++;
                continue;
            }
            
            // Parse format specifier
            const char* spec_start = p - 1;
            
            // Skip flags, width, precision
            while (*p && strchr("-+ #0", *p)) p++;
            while (*p && *p >= '0' && *p <= '9') p++;
            if (*p == '.') {
                p++;
                while (*p && *p >= '0' && *p <= '9') p++;
            }
            
            if (*p == 's' && p[1] == 'w') {
                // Handle %sw specifier
                WGPUStringView sv = va_arg(args2, WGPUStringView);
                if (sv.data) {
                    if (sv.length == WGPU_STRLEN) {
                        strcpy(out, sv.data);
                        out += strlen(sv.data);
                    } else {
                        memcpy(out, sv.data, sv.length);
                        out += sv.length;
                    }
                }
                p += 2;
            } else if (*p) {
                // Standard specifier - create temporary format and use sprintf
                size_t spec_len = p - spec_start + 1;
                char temp_format[32];
                if (spec_len < sizeof(temp_format)) {
                    memcpy(temp_format, spec_start, spec_len);
                    temp_format[spec_len] = '\0';
                    
                    char temp_buf[256];
                    int written = 0;
                    
                    switch (*p) {
                        case 'd': case 'i': case 'o': case 'x': case 'X': case 'u':
                            written = sprintf(temp_buf, temp_format, va_arg(args2, int));
                            break;
                        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                            written = sprintf(temp_buf, temp_format, va_arg(args2, double));
                            break;
                        case 's':
                            written = sprintf(temp_buf, temp_format, va_arg(args2, const char*));
                            break;
                        case 'c':
                            written = sprintf(temp_buf, temp_format, va_arg(args2, int));
                            break;
                        case 'p':
                            written = sprintf(temp_buf, temp_format, va_arg(args2, void*));
                            break;
                        default: 
                            break;
                    }
                    
                    if (written > 0) {
                        memcpy(out, temp_buf, written);
                        out += written;
                    }
                }
                p++;
            }
        } else {
            *out++ = *p++;
        }
    }
    
    *out = '\0';
    va_end(args2);
    
    // Resize buffer to exact size needed
    size_t actual_len = out - buffer;
    char* final_buffer = realloc(buffer, actual_len + 1);
    return final_buffer ? final_buffer : buffer;
}


WGPUStatus wgpuDeviceGetAdapterInfo(WGPUDevice device, WGPUAdapterInfo* adapterInfo) WGPU_FUNCTION_ATTRIBUTE {
    ENTRY();
    if (device == NULL || adapterInfo == NULL || device->adapter == NULL || device->adapter->physicalDevice == VK_NULL_HANDLE) {
        EXIT();
        return WGPUStatus_Error;
    }
    EXIT();
    return wgpuAdapterGetInfo(device->adapter, adapterInfo);
}
#ifndef MIN
    #define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
WGPUStatus wgpuAdapterGetLimits(WGPUAdapter adapter, WGPULimits* limits) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    const VkPhysicalDeviceProperties2* deviceProperties2 = &adapter->deviceInfoCache.properties;

    limits->maxTextureDimension1D = deviceProperties2->properties.limits.maxImageDimension1D;
    limits->maxTextureDimension2D = deviceProperties2->properties.limits.maxImageDimension2D;
    limits->maxTextureDimension3D = deviceProperties2->properties.limits.maxImageDimension3D;
    limits->maxTextureArrayLayers = deviceProperties2->properties.limits.maxImageArrayLayers;
    limits->maxBindGroups = deviceProperties2->properties.limits.maxBoundDescriptorSets;
    limits->maxBindGroupsPlusVertexBuffers = deviceProperties2->properties.limits.maxBoundDescriptorSets + deviceProperties2->properties.limits.maxVertexInputBindings;
    limits->maxBindingsPerBindGroup = deviceProperties2->properties.limits.maxDescriptorSetStorageBuffers;
    limits->maxDynamicUniformBuffersPerPipelineLayout = deviceProperties2->properties.limits.maxDescriptorSetUniformBuffersDynamic;
    limits->maxDynamicStorageBuffersPerPipelineLayout = deviceProperties2->properties.limits.maxDescriptorSetStorageBuffersDynamic;
    limits->maxSampledTexturesPerShaderStage = deviceProperties2->properties.limits.maxPerStageDescriptorSampledImages;
    limits->maxSamplersPerShaderStage = deviceProperties2->properties.limits.maxPerStageDescriptorSamplers;
    limits->maxStorageBuffersPerShaderStage = deviceProperties2->properties.limits.maxPerStageDescriptorStorageBuffers;
    limits->maxStorageTexturesPerShaderStage = deviceProperties2->properties.limits.maxPerStageDescriptorStorageImages;
    limits->maxUniformBuffersPerShaderStage = deviceProperties2->properties.limits.maxPerStageDescriptorUniformBuffers;
    limits->maxUniformBufferBindingSize = deviceProperties2->properties.limits.maxUniformBufferRange;
    limits->maxStorageBufferBindingSize = deviceProperties2->properties.limits.maxStorageBufferRange;
    limits->minUniformBufferOffsetAlignment = deviceProperties2->properties.limits.minUniformBufferOffsetAlignment;
    limits->minStorageBufferOffsetAlignment = deviceProperties2->properties.limits.minStorageBufferOffsetAlignment;
    limits->maxVertexBuffers = deviceProperties2->properties.limits.maxVertexInputBindings;
    limits->maxBufferSize = deviceProperties2->properties.limits.maxMemoryAllocationCount; // This is a weak mapping, ideally there's a buffer size limit.
    limits->maxVertexAttributes = deviceProperties2->properties.limits.maxVertexInputAttributes;
    limits->maxVertexBufferArrayStride = deviceProperties2->properties.limits.maxVertexInputBindingStride;
    limits->maxInterStageShaderVariables = deviceProperties2->properties.limits.maxFragmentInputComponents; // Assuming this maps to inter-stage variables.
    limits->maxColorAttachments = deviceProperties2->properties.limits.maxColorAttachments;

    limits->maxComputeWorkgroupStorageSize = deviceProperties2->properties.limits.maxComputeSharedMemorySize;
    limits->maxComputeInvocationsPerWorkgroup = deviceProperties2->properties.limits.maxComputeWorkGroupInvocations;
    limits->maxComputeWorkgroupSizeX = deviceProperties2->properties.limits.maxComputeWorkGroupSize[0];
    limits->maxComputeWorkgroupSizeY = deviceProperties2->properties.limits.maxComputeWorkGroupSize[1];
    limits->maxComputeWorkgroupSizeZ = deviceProperties2->properties.limits.maxComputeWorkGroupSize[2];
    limits->maxComputeWorkgroupsPerDimension = MIN(deviceProperties2->properties.limits.maxComputeWorkGroupCount[0], MIN(deviceProperties2->properties.limits.maxComputeWorkGroupCount[1], deviceProperties2->properties.limits.maxComputeWorkGroupCount[2]));
    
    limits->maxStorageBuffersInVertexStage = deviceProperties2->properties.limits.maxPerStageDescriptorStorageBuffers;
    limits->maxStorageTexturesInVertexStage = deviceProperties2->properties.limits.maxPerStageDescriptorStorageImages;
    limits->maxStorageBuffersInFragmentStage = deviceProperties2->properties.limits.maxPerStageDescriptorStorageBuffers;
    limits->maxStorageTexturesInFragmentStage = deviceProperties2->properties.limits.maxPerStageDescriptorStorageImages;

    EXIT();
    return WGPUStatus_Success;
}
typedef struct FenceInFrameCacheCallbackUserdata{
    WGPUFence fence;
    PerframeCache* frameCache;
}FenceInFrameCacheCallbackUserdata;

static void fenceFreeCallback(void* userdata_){
    FenceInFrameCacheCallbackUserdata* userdata = (FenceInFrameCacheCallbackUserdata*)userdata_;
    WGPUCommandBufferVector* insert = PendingCommandBufferMap_get(&userdata->frameCache->pendingCommandBuffers, userdata->fence);
    wgvk_assert(insert != NULL, "Fence freed but not present in FIF cache");

    for(size_t i = 0;i < insert->size;i++){
        WGPUCommandBuffer bufferi = insert->data[i];
        wgvk_assert(bufferi->refCount == 1, "Unreasonable refCount");
        wgpuCommandBufferRelease(bufferi);
    }
    WGPUCommandBufferVector_clear(insert);
    wgpuFenceRelease(userdata->fence);
}

void PerframeCache_pushFenceDependencies(PerframeCache* pfcache, WGPUFence fence, WGPUCommandBufferVector* commandBuffers){
    PendingCommandBufferMap* map = &pfcache->pendingCommandBuffers;

    WGPUCommandBufferVector* insert = PendingCommandBufferMap_get(map, fence);
    if(insert != NULL){
        TRACELOG(WGPU_LOG_WARNING, "fence already appears in this PerframeCache's pcm");
        for(size_t i = 0;i < commandBuffers->size;i++){
            wgpuCommandBufferAddRef(*WGPUCommandBufferVector_get(commandBuffers, i));
            WGPUCommandBufferVector_push_back(insert, *WGPUCommandBufferVector_get(commandBuffers, i));
        }
        WGPUCommandBufferVector_free(commandBuffers);
    }
    else{
        PendingCommandBufferMap_put(map, fence, *commandBuffers);
        for(size_t i = 0;i < commandBuffers->size;i++){
            wgpuCommandBufferAddRef(*WGPUCommandBufferVector_get(commandBuffers, i));
        }
        wgpuFenceAddRef(fence);
        FenceInFrameCacheCallbackUserdata* userdata = RL_CALLOC(1, sizeof(FenceInFrameCacheCallbackUserdata));
        userdata->fence = fence;
        userdata->frameCache = pfcache;
        CallbackWithUserdata cwu = {
            .callback = fenceFreeCallback,
            .freeUserData = RL_FREE,
            .userdata = userdata,
        };
        CallbackWithUserdataVector_push_back(&fence->callbacksOnWaitComplete, cwu);
    }
}

WGPUStatus FIFCache_init(FIFCache* fifCache, WGPUDevice device, uint32_t queueFamily){
    fifCache->device = device;
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkCommandPoolCreateInfo pci = { 
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamily
    };

    for(uint32_t i = 0;i < framesInFlight;i++){
        VkCommandPool* pool = &fifCache->frameCaches[i].commandPool;
        VkSemaphore* fts = &fifCache->frameCaches[i].finalTransitionSemaphore;
        VkCommandBuffer* ftb = &fifCache->frameCaches[i].finalTransitionBuffer;
        VkResult scres = device->functions.vkCreateSemaphore(device->device, &sci, NULL, fts);
        VkResult cpcres = device->functions.vkCreateCommandPool(device->device, &pci, NULL, pool);
        if(scres != VK_SUCCESS || cpcres != VK_SUCCESS){
            return  WGPUStatus_Error;
        }
        const VkCommandBufferAllocateInfo cbai = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = *pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };
        device->functions.vkAllocateCommandBuffers(device->device, &cbai, ftb);
        fifCache->frameCaches[i].finalTransitionFence = wgpuDeviceCreateFence(device);
        VkSemaphoreVector* semvec = &fifCache->frameCaches[i].syncState.semaphores;
        VkSemaphoreVector_reserve(semvec, 100);
        semvec->size = 100;
        for(uint32_t j = 0;j < semvec->size;j++){
            if(device->functions.vkCreateSemaphore(device->device, &sci, NULL, semvec->data + j) != VK_SUCCESS){
                return WGPUStatus_Error;
            }
        }
        device->functions.vkCreateSemaphore(device->device, &sci, NULL, &fifCache->frameCaches[i].syncState.acquireImageSemaphore);
    }
    return WGPUStatus_Success;
}

void SyncState_destroy(WGPUDevice device, SyncState* syncState){
    device->functions.vkDestroySemaphore(device->device, syncState->acquireImageSemaphore, NULL);
    for(uint32_t s = 0;s < syncState->semaphores.size;s++){
        device->functions.vkDestroySemaphore(device->device, syncState->semaphores.data[s], NULL);
    }
    VkSemaphoreVector_free(&syncState->semaphores);
}

void FIFCache_destroy(FIFCache* fcache){
    for(uint32_t i = 0;i < framesInFlight;i++){
        PerframeCache* cache = fcache->frameCaches + i;
        PendingCommandBufferMap* pcm = &cache->pendingCommandBuffers;
        for(size_t c = 0;c < pcm->current_capacity;c++){
            PendingCommandBufferMap_kv_pair* kvp = pcm->table + c;
            if(kvp->key != PHM_EMPTY_SLOT_KEY){
                WGPUFence keyfence = (WGPUFence)kvp->key;
                if(keyfence->state == WGPUFenceState_InUse){
                    wgpuFenceWait(keyfence, ((uint64_t)1) << 28);
                }
                keyfence->state = WGPUFenceState_Finished;
                wgpuFenceRelease(keyfence);
                WGPUCommandBufferVector* value = &kvp->value;
                for(size_t cbi = 0;cbi < value->size;cbi++){
                    wgpuCommandBufferRelease(value->data[cbi]);
                }
            }
        }
        PendingCommandBufferMap_free(pcm);
        //PendingCommandBufferMap_for_each(pcm, resetFenceAndReleaseBuffers, device);
        WGPUDevice device = fcache->device;

        device->functions.vkFreeCommandBuffers(device->device, cache->commandPool, 1, &cache->finalTransitionBuffer);
        device->functions.vkDestroySemaphore(device->device, cache->finalTransitionSemaphore, NULL);
        SyncState_destroy(fcache->device, &fcache->frameCaches[i].syncState);
        wgpuFenceRelease(cache->finalTransitionFence);
        
        if(cache->commandBuffers.size){
            device->functions.vkFreeCommandBuffers(device->device, cache->commandPool, cache->commandBuffers.size, cache->commandBuffers.data);
            VkCommandBufferVector_free(&cache->commandBuffers);
        }
        for(size_t bgc = 0;bgc < cache->bindGroupCache.current_capacity;bgc++){
            if(cache->bindGroupCache.table[bgc].key != PHM_EMPTY_SLOT_KEY && cache->bindGroupCache.table[bgc].key != PHM_DELETED_SLOT_KEY){
                DescriptorSetAndPoolVector* dspv = &cache->bindGroupCache.table[bgc].value;
                for(size_t vi = 0;vi < dspv->size;vi++){
                    //device->functions.vkFreeDescriptorSets(device->device, dspv->data[i].pool, 1, &dspv->data[i].set);
                    device->functions.vkDestroyDescriptorPool(device->device, dspv->data[i].pool, NULL);
                }
                DescriptorSetAndPoolVector_free(dspv);
            }
        }
        BindGroupCacheMap_free(&cache->bindGroupCache);
        device->functions.vkDestroyCommandPool(device->device, cache->commandPool, NULL);
    }
}

static RenderPassLayout GetRenderPassLayout2(const RenderPassCommandBegin* rpdesc){
    RenderPassLayout ret zeroinit;
    if(rpdesc->depthAttachmentPresent){
        ret.depthAttachmentPresent = 1U;
        ret.depthAttachment = CLITERAL(AttachmentDescriptor){
            .format = rpdesc->depthStencilAttachment.view->format, 
            .sampleCount = rpdesc->depthStencilAttachment.view->sampleCount,
            .loadop =  toVulkanLoadOperation(rpdesc->depthStencilAttachment.depthLoadOp),
            .storeop = toVulkanStoreOperation(rpdesc->depthStencilAttachment.depthStoreOp)
        };
    }

    ret.colorAttachmentCount = rpdesc->colorAttachmentCount;
    wgvk_assert(ret.colorAttachmentCount < MAX_COLOR_ATTACHMENTS, "Too many color attachments");
    for(uint32_t i = 0;i < rpdesc->colorAttachmentCount;i++){
        ret.colorAttachments[i] = CLITERAL(AttachmentDescriptor){
            .format = rpdesc->colorAttachments[i].view->format, 
            .sampleCount = rpdesc->colorAttachments[i].view->sampleCount,
            .loadop =  toVulkanLoadOperation (rpdesc->colorAttachments[i].loadOp),
            .storeop = toVulkanStoreOperation(rpdesc->colorAttachments[i].storeOp)
        };
        bool ihasresolve = rpdesc->colorAttachments[i].resolveTarget;
        if(i > 0){
            bool iminus1hasresolve = rpdesc->colorAttachments[i - 1].resolveTarget;
            wgvk_assert(ihasresolve == iminus1hasresolve, "Some of the attachments have resolve, others do not, impossible");
        }
        if(rpdesc->colorAttachments[i].resolveTarget != 0){
            ret.colorResolveAttachments[i] = CLITERAL(AttachmentDescriptor){
                .format = rpdesc->colorAttachments[i].resolveTarget->format, 
                .sampleCount = rpdesc->colorAttachments[i].resolveTarget->sampleCount,
                .loadop =  toVulkanLoadOperation (rpdesc->colorAttachments[i].loadOp),
                .storeop = toVulkanStoreOperation(rpdesc->colorAttachments[i].storeOp)
            };
        }
    }
    return ret;
}

RenderPassLayout GetRenderPassLayout(const WGPURenderPassDescriptor* rpdesc){
    RenderPassLayout ret zeroinit;
    //ret.colorResolveIndex = VK_ATTACHMENT_UNUSED;
    
    if(rpdesc->depthStencilAttachment){
        wgvk_assert(rpdesc->depthStencilAttachment->view, "Depth stencil attachment passed but null view");
        ret.depthAttachmentPresent = 1U;
        ret.depthAttachment = CLITERAL(AttachmentDescriptor){
            .format = rpdesc->depthStencilAttachment->view->format, 
            .sampleCount = rpdesc->depthStencilAttachment->view->sampleCount,
            .loadop =  toVulkanLoadOperation (rpdesc->depthStencilAttachment->depthLoadOp ),
            .storeop = toVulkanStoreOperation(rpdesc->depthStencilAttachment->depthStoreOp)
        };
    }
    
    ret.colorAttachmentCount = rpdesc->colorAttachmentCount;
    wgvk_assert(ret.colorAttachmentCount < MAX_COLOR_ATTACHMENTS, "Too many color attachments");
    for(uint32_t i = 0;i < rpdesc->colorAttachmentCount;i++){
        ret.colorAttachments[i] = CLITERAL(AttachmentDescriptor){
            .format = rpdesc->colorAttachments[i].view->format, 
            .sampleCount = rpdesc->colorAttachments[i].view->sampleCount,
            .loadop =  toVulkanLoadOperation(rpdesc->colorAttachments[i].loadOp  ),
            .storeop = toVulkanStoreOperation(rpdesc->colorAttachments[i].storeOp)
        };
        bool ihasresolve = rpdesc->colorAttachments[i].resolveTarget;
        if(i > 0){
            bool iminus1hasresolve = rpdesc->colorAttachments[i - 1].resolveTarget;
            wgvk_assert(ihasresolve == iminus1hasresolve, "Some of the attachments have resolve, others do not, impossible");
        }
        if(rpdesc->colorAttachments[i].resolveTarget != 0){
            ret.colorResolveAttachments[i] = CLITERAL(AttachmentDescriptor){
                .format = rpdesc->colorAttachments[i].resolveTarget->format, 
                .sampleCount = rpdesc->colorAttachments[i].resolveTarget->sampleCount,
                .loadop =  toVulkanLoadOperation(rpdesc->colorAttachments[i].loadOp),
                .storeop = toVulkanStoreOperation(rpdesc->colorAttachments[i].storeOp)
            };
        }
    }
    return ret;
}

static inline bool is__depthVk(VkFormat fmt){
    return fmt == VK_FORMAT_D32_SFLOAT || fmt == VK_FORMAT_D32_SFLOAT_S8_UINT || fmt == VK_FORMAT_D24_UNORM_S8_UINT;
}

static inline VkSampleCountFlagBits toVulkanSampleCount(uint32_t samples){
    if(samples & (samples - 1)){
        return VK_SAMPLE_COUNT_1_BIT;
    }
    else{
        switch(samples){
            case 2: return VK_SAMPLE_COUNT_2_BIT;
            case 4: return VK_SAMPLE_COUNT_4_BIT;
            case 8: return VK_SAMPLE_COUNT_8_BIT;
            case 16: return VK_SAMPLE_COUNT_16_BIT;
            case 32: return VK_SAMPLE_COUNT_32_BIT;
            case 64: return VK_SAMPLE_COUNT_64_BIT;
            default: return VK_SAMPLE_COUNT_1_BIT;
        }
    }
}

static VkAttachmentDescription atttransformFunction(AttachmentDescriptor att){
    VkAttachmentDescription ret zeroinit;
    ret.samples    = toVulkanSampleCount(att.sampleCount);
    ret.format     = att.format;
    ret.loadOp     = att.loadop;
    ret.storeOp    = att.storeop;
    ret.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ret.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ret.initialLayout  = (att.loadop == VK_ATTACHMENT_LOAD_OP_LOAD ? (is__depthVk(att.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) : (VK_IMAGE_LAYOUT_UNDEFINED));
    if(is__depthVk(att.format)){
        ret.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }else{
        ret.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    return ret;
};

LayoutedRenderPass LoadRenderPassFromLayout(WGPUDevice device, RenderPassLayout layout){
    LayoutedRenderPass* lrp = RenderPassCache_get(&device->renderPassCache, layout);
    if(lrp)
        return *lrp;

    //TRACELOG(WGPU_LOG_INFO, "Loading new renderpass");
    
    VkAttachmentDescriptionVector allAttachments;
    VkAttachmentDescriptionVector_init(&allAttachments);
    uint32_t depthAttachmentIndex = VK_ATTACHMENT_UNUSED; // index for depth attachment if any
    uint32_t colorResolveIndex = VK_ATTACHMENT_UNUSED; // index for depth attachment if any
    for(uint32_t i = 0; i < layout.colorAttachmentCount;i++){
        VkAttachmentDescriptionVector_push_back(&allAttachments, atttransformFunction(layout.colorAttachments[i]));
    }
    
    if(layout.depthAttachmentPresent){
        depthAttachmentIndex = allAttachments.size;
        VkAttachmentDescriptionVector_push_back(&allAttachments, atttransformFunction(layout.depthAttachment));
    }
    //TODO check if there
    if(layout.colorAttachmentCount && layout.colorResolveAttachments[0].format){
        colorResolveIndex = allAttachments.size;
        for(uint32_t i = 0;i < layout.colorAttachmentCount;i++){
            VkAttachmentDescriptionVector_push_back(&allAttachments, atttransformFunction(layout.colorResolveAttachments[i]));
        }
    }

    
    const uint32_t colorAttachmentCount = layout.colorAttachmentCount;
    

    // Set up color attachment references for the subpass.
    VkAttachmentReference colorRefs[MAX_COLOR_ATTACHMENTS] = {0}; // list of color attachments
    uint32_t colorIndex = 0;
    for (uint32_t i = 0; i < colorAttachmentCount; i++) {
        if (!is__depthVk(layout.colorAttachments[i].format)) {
            colorRefs[colorIndex].attachment = i;
            colorRefs[colorIndex].layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ++colorIndex;
        }
    }

    // Set up subpass description.
    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = colorIndex,
        .pColorAttachments       = colorIndex ? colorRefs : NULL,
    };


    // Assign depth attachment if present.
    VkAttachmentReference depthRef = {};
    if (depthAttachmentIndex != VK_ATTACHMENT_UNUSED) {
        depthRef.attachment = depthAttachmentIndex;
        depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        subpass.pDepthStencilAttachment = &depthRef;
    } else {
        subpass.pDepthStencilAttachment = NULL;
    }

    VkAttachmentReference resolveRef = {};
    if (colorResolveIndex != VK_ATTACHMENT_UNUSED) {
        resolveRef.attachment = colorResolveIndex;
        resolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        subpass.pResolveAttachments = &resolveRef;
    } else {
        subpass.pResolveAttachments = NULL;
    }
    

    VkRenderPassCreateInfo rpci = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        NULL,
        0,
        allAttachments.size, allAttachments.data,
        1, &subpass,
        0, NULL
    };
    
    // (Optional: add subpass dependencies if needed.)
    LayoutedRenderPass ret zeroinit;
    VkAttachmentDescriptionVector_move(&ret.allAttachments, &allAttachments);
    //ret.allAttachments = std::move(allAttachments);
    VkResult result = device->functions.vkCreateRenderPass(device->device, &rpci, NULL, &ret.renderPass);
    // (Handle errors appropriately in production code)
    if(result == VK_SUCCESS){
        RenderPassCache_put(&device->renderPassCache, layout, ret);
        //device->renderPassCache.emplace(layout, ret);
        ret.layout = layout;
        return ret;
    }
    TRACELOG(WGPU_LOG_FATAL, "Error creating renderpass: %s", vkErrorString(result));
    rg_trap();
    return ret;
}

void wgpuTraceLog(int logType, const char *text, ...){
    ENTRY();
    // Message has level below current threshold, don't emit
    //if(logType < tracelogLevel)return;

    va_list args;
    va_start(args, text);

    //if (traceLog){
    //    traceLog(logType, text, args);
    //    va_end(args);
    //    return;
    //}
    #define MAX_TRACELOG_MSG_LENGTH 16384
    char buffer[MAX_TRACELOG_MSG_LENGTH] = {0};
    int needs_reset = 0;
    switch (logType){
        case WGPU_LOG_TRACE:   strcpy(buffer, "TRACE: "); break;
        case WGPU_LOG_DEBUG:   strcpy(buffer, "DEBUG: "); break;
        case WGPU_LOG_INFO:    strcpy(buffer, TERMCTL_GREEN "INFO: "); needs_reset = 1;break;
        case WGPU_LOG_WARNING: strcpy(buffer, TERMCTL_YELLOW "WARNING: ");needs_reset = 1; break;
        case WGPU_LOG_ERROR:   strcpy(buffer, TERMCTL_RED "ERROR: ");needs_reset = 1; break;
        case WGPU_LOG_FATAL:   strcpy(buffer, TERMCTL_RED "FATAL: "); break;
        default: break;
    }
    size_t offset_now = strlen(buffer);
    
    unsigned int textSize = (unsigned int)strlen(text);
    memcpy(buffer + strlen(buffer), text, (textSize < (MAX_TRACELOG_MSG_LENGTH - 12))? textSize : (MAX_TRACELOG_MSG_LENGTH - 12));
    if(needs_reset){
        strcat(buffer, TERMCTL_RESET "\n");
    }
    else{
        strcat(buffer, "\n");
    }
    vfprintf(stderr, buffer, args);
    fflush  (stderr);

    va_end(args);
    // If fatal logging, exit program
    if (logType == WGPU_LOG_FATAL){
        fputs(TERMCTL_RED "Exiting due to fatal error!\n" TERMCTL_RESET, stderr);
        rg_trap();
        exit(EXIT_FAILURE); 
    }

    EXIT();
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != NULL) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){
        wgpuTraceLog(WGPU_LOG_ERROR, pCallbackData->pMessage);
        rg_trap();
    }
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
        wgpuTraceLog(WGPU_LOG_WARNING, pCallbackData->pMessage);
    }
    else if(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT){
        wgpuTraceLog(WGPU_LOG_INFO, pCallbackData->pMessage);
    }

    return VK_FALSE;
}

static inline bool isWritingAccess(VkAccessFlags flags){
    return flags & (
          VK_ACCESS_SHADER_WRITE_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_TRANSFER_WRITE_BIT
        | VK_ACCESS_HOST_WRITE_BIT
        | VK_ACCESS_MEMORY_WRITE_BIT
        | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT
        | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT
        | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR
        | VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_EXT
    );
}
static inline int endswith_(const char* str, const char* suffix) {
    if (!str || !suffix)
        return 0;
        
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len)
        return 0;
        
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor* descriptor) {
    ENTRY();
    WGPUInstance ret = (WGPUInstance)RL_CALLOC(1, sizeof(WGPUInstanceImpl));
    if (!ret) {
        fprintf(stderr, "calloc failed to allocate memory for WGPUInstance\n");
        return NULL;
    }
    ret->refCount = 1;
    VkResult vresult = volkInitialize();
    if(vresult != VK_SUCCESS){
        fprintf(stderr, "Failed to initialize Vulkan. Most likely it's not installed\n");
        return NULL;
    }
    ret->instance = VK_NULL_HANDLE;
    ret->debugMessenger = VK_NULL_HANDLE;

    const VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "WGPU Application",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "WGPU Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        #if VULKAN_USE_DYNAMIC_RENDERING == 1 || VULKAN_ENABLE_RAYTRACING == 1
        .apiVersion = VK_API_VERSION_1_3,
        #else
        .apiVersion = VK_API_VERSION_1_1,
        #endif
    };

    uint32_t availableExtensionCount = 0;
    VkResult enumResult = vkEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, NULL);
    
    VkExtensionProperties* availableExtensions = NULL;
    const char** enabledExtensions = NULL; // Array of pointers to enabled names
    const uint32_t maxEnabledExtensions = 32;


    if (enumResult != VK_SUCCESS || availableExtensionCount == 0) {
        fprintf(stderr, "Warning: Failed to query instance extensions or none found (Error: %d). Proceeding without optional extensions.\n", (int)enumResult);
        availableExtensionCount = 0;
    } else {
        availableExtensions = (VkExtensionProperties*)RL_CALLOC(availableExtensionCount, sizeof(VkExtensionProperties));
        if (!availableExtensions) {
            fprintf(stderr, "Error: Failed to allocate memory for available instance extensions properties.\n");
            RL_FREE(ret);
            return NULL;
        }
        enumResult = vkEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, availableExtensions);
        if (enumResult != VK_SUCCESS) {
            fprintf(stderr, "Warning: Failed to retrieve instance extension properties (Error: %d). Proceeding without optional extensions.\n", (int)enumResult);
            RL_FREE(availableExtensions);
            availableExtensions = NULL;
            availableExtensionCount = 0;
        }
    }

    // Allocate buffer for the names of extensions we will actually enable
    enabledExtensions = (const char**)RL_CALLOC(maxEnabledExtensions, sizeof(const char*));
    if (!enabledExtensions) {
        RL_FREE(availableExtensions);
        RL_FREE(ret);
        return NULL;
    }

    int needsPortabilityEnumeration = 1;
    #ifdef __APPLE__    
    needsPortabilityEnumeration = 1;
    #endif
    int portabilityEnumerationAvailable = 1;

    // Iterate through available extensions and enable the desired ones
    uint32_t enabledExtensionCount = 0;
    for (uint32_t i = 0; i < availableExtensionCount; ++i) {
        const char* currentExtName = availableExtensions[i].extensionName;
        if(endswith_(currentExtName, "surface") || strstr(currentExtName, "debug") != NULL || strstr(currentExtName, "swapchain_colorspace") != NULL){
            enabledExtensions[enabledExtensionCount++] = currentExtName;
        }
    }
    #if SUPPORT_DRM_SURFACE == 1
        enabledExtensions[enabledExtensionCount++] = "VK_KHR_display";
        enabledExtensions[enabledExtensionCount++] = "VK_EXT_acquire_drm_display";
        enabledExtensions[enabledExtensionCount++] = "VK_EXT_direct_mode_display";
        enabledExtensions[enabledExtensionCount++] = "VK_KHR_get_display_properties2";
    #endif
    VkInstanceCreateFlags instanceCreateFlags = 0;
    // Handle portability enumeration: Enable it *if* needed and available
    if (needsPortabilityEnumeration) {
        if (portabilityEnumerationAvailable && enabledExtensionCount < maxEnabledExtensions) {
            enabledExtensions[enabledExtensionCount++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            instanceCreateFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        } else if (!portabilityEnumerationAvailable) {
            fprintf(stderr, "Error: An enabled surface extension requires '%s', but it is not available! Instance creation may fail.\n", VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        } else {
            // fprintf(stderr, "Warning: Portability enumeration needed but exceeded max enabled count (%u).\n", maxEnabledExtensions);
        }
    }
    // 2. Define Instance Create Info

    


    // --- End Extension Handling ---

    // 4. Specify Layers (if requested)
    VkLayerProperties availableLayers[64] = {0};
    uint32_t availableLayerCount = 0;
    VkResult layerEnumResult = vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL);
    if(layerEnumResult == VK_SUCCESS){
        layerEnumResult = vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers);
    }
    if(layerEnumResult != VK_SUCCESS){
        fprintf(stderr, "vkEnumerateInstanceLayerProperties failed: %s\n", vkErrorString(layerEnumResult));
    }

    char nullTerminatedRequestedLayers[64][64] = {0};
    const char* nullTerminatedRequestedLayerPointers[64] = {0};
    uint32_t requestedAvailableLayerCount = 0;
    
    WGPUInstanceLayerSelection* ils = NULL;
    int debugUtilsAvailable = 0; // Check if debug utils was actually enabled

    for (uint32_t i = 0; i < enabledExtensionCount; ++i) {
        if (strcmp(enabledExtensions[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            debugUtilsAvailable = 1;
            break;
        }
    }

    if (descriptor && descriptor->nextInChain) {
        if (((WGPUChainedStruct*)descriptor->nextInChain)->sType == WGPUSType_InstanceLayerSelection) {
            ils = (WGPUInstanceLayerSelection*)descriptor->nextInChain;
            for(uint32_t l = 0;l < ils->instanceLayerCount;l++){
                const char* layerName = ils->instanceLayers[l];
                int found = 0;
                uint32_t al = 0;
                for(al = 0;al < availableLayerCount;al++){
                    if(strcmp(availableLayers[al].layerName, layerName) == 0){
                        found = 1;
                        break;
                    }
                }
                if(found){
                    char* dest = nullTerminatedRequestedLayers[requestedAvailableLayerCount];
                    memcpy(dest, layerName, strlen(layerName) + 1);
                    nullTerminatedRequestedLayerPointers[requestedAvailableLayerCount] = dest;
                    ++requestedAvailableLayerCount;
                }
            }
        }
        // TODO: Handle other potential structs in nextInChain if necessary
    }
    else {
        //nullTerminatedRequestedLayerPointers[requestedAvailableLayerCount] = "VK_LAYER_PROFILING_framerate";
        //++requestedAvailableLayerCount;
    }
    VkValidationFeaturesEXT validationFeatures zeroinit;
    VkValidationFeatureEnableEXT enables[] = {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
    };
    // If layers are enabled, configure specific validation features, BUT only if debug utils is available
    if (requestedAvailableLayerCount > 0) {
        if(debugUtilsAvailable) {
            validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
            validationFeatures.enabledValidationFeatureCount = sizeof(enables) / sizeof(enables[0]);
            validationFeatures.pEnabledValidationFeatures = enables;
            validationFeatures.pNext = NULL; // For now ok. what would required non-null here is if ici already had a non-null pNext
            //fprintf(stdout, "Enabling synchronization validation feature.\n");
        } else {
            //fprintf(stderr, "Warning: Requested validation layers but VK_EXT_debug_utils extension was not found/enabled. Debug messenger and specific validation features cannot be enabled.\n");
            //ici.enabledLayerCount = 0; // Disable layers if debug utils isn't there
            //ici.ppEnabledLayerNames = NULL;
            //requestedLayerCount = 0; // Update count for subsequent checks
            //fprintf(stdout, "Disabling requested layers due to missing VK_EXT_debug_utils.\n");
        }
    } else {
        //fprintf(stdout, "Validation layers not requested or not found in descriptor chain.\n");
    }

    // 5. Create the Vulkan Instance
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = (validationFeatures.sType == VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT) ? &validationFeatures : NULL,
        .flags = instanceCreateFlags,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = enabledExtensionCount,
        .ppEnabledExtensionNames = enabledExtensions,
        .enabledLayerCount   = (validationFeatures.sType == VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT) ? requestedAvailableLayerCount : 0,
        .ppEnabledLayerNames = (validationFeatures.sType == VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT) ? nullTerminatedRequestedLayerPointers : NULL,
    };
    //printf("Enabled layer count: %d\n", ici.enabledLayerCount);
    VkResult result = vkCreateInstance(&ici, NULL, &ret->instance);
    if(result != VK_SUCCESS){
        fprintf(stderr, "vkCreateInstance failed: %s\n", vkErrorString(result));
    }
    // --- Free temporary extension memory ---
    RL_FREE(availableExtensions);        // Properties buffer
    RL_FREE((void*)enabledExtensions);   // Names buffer (pointers into availableExtensions)
    availableExtensions = NULL;
    enabledExtensions = NULL;
    // --- End Freeing Extension Memory ---


    if (result != VK_SUCCESS) {
        RL_FREE(ret);
        return NULL;
    }
    ret->currentFutureId = 1;
    FutureIDMap_init(&ret->g_futureIDMap);
    // 6. Load instance-level functions using volk
    volkLoadInstance(ret->instance);

    // 7. Create Debug Messenger (if layers requested AND debug utils was available/enabled)
    if (requestedAvailableLayerCount > 0 && debugUtilsAvailable && vkCreateDebugUtilsMessengerEXT) {
        VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo zeroinit;
        dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgCreateInfo.pfnUserCallback = debugCallback;

        result = CreateDebugUtilsMessengerEXT(ret->instance, &dbgCreateInfo, NULL, &ret->debugMessenger);
        if (result != VK_SUCCESS) {
            //fprintf(stderr, "Warning: Failed to create debug messenger (Error: %d).\n", (int)result);
            ret->debugMessenger = VK_NULL_HANDLE;
        } else {
            //fprintf(stdout, "Vulkan Debug Messenger created successfully.\n");
        }
    } else if (requestedAvailableLayerCount > 0) {
        //fprintf(stdout, "Debug messenger creation skipped because VK_EXT_debug_utils extension was not available/enabled or layers were disabled.\n");
    }


    // 8. Return the created instance handle
    return ret;
    EXIT();
}
WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount, WGPUFutureWaitInfo* futureWaitInfos, uint64_t timeoutNS){
    ENTRY();
    for(uint32_t i = 0;i < futureCount;i++){
        if(!futureWaitInfos[i].completed){
            WGPUFutureImpl* futureObject = FutureIDMap_get(&instance->g_futureIDMap, futureWaitInfos[i].future.id);
            futureObject->functionCalledOnWaitAny(futureObject->userdataForFunction);
            if(futureObject->freeUserData){
                futureObject->freeUserData(futureObject->userdataForFunction);
            }
            futureWaitInfos[i].completed = 1;
        }
    }
    return WGPUWaitStatus_Success;
    EXIT();
}
typedef struct userdataforcreateadapter{
    WGPUInstance instance;
    WGPURequestAdapterCallbackInfo info;
    WGPURequestAdapterOptions options;
} userdataforcreateadapter;
static inline VkPhysicalDeviceType tvkpdt(WGPUAdapterType atype){
    switch(atype){
        case WGPUAdapterType_DiscreteGPU:{
            return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        }break;
        case WGPUAdapterType_IntegratedGPU:{
            return VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        }break;
        case WGPUAdapterType_CPU:{
            return VK_PHYSICAL_DEVICE_TYPE_CPU;
        }break;
        case WGPUAdapterType_Unknown:{
            return VK_PHYSICAL_DEVICE_TYPE_OTHER;
        }break;
        default:
        case WGPUAdapterType_Force32:
        rg_unreachable();
        {
            return VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM;
        }break;
    }
}
typedef enum PhysicalDeviceArchitecture {
    PhysicalDeviceArchitecture_Unknown,
    PhysicalDeviceArchitecture_AMD_GCN1,
    PhysicalDeviceArchitecture_AMD_GCN2,
    PhysicalDeviceArchitecture_AMD_GCN3,
    PhysicalDeviceArchitecture_AMD_GCN4,
    PhysicalDeviceArchitecture_AMD_GCN5,
    PhysicalDeviceArchitecture_AMD_RDNA1,
    PhysicalDeviceArchitecture_AMD_RDNA2,
    PhysicalDeviceArchitecture_AMD_RDNA3,
    PhysicalDeviceArchitecture_AMD_RDNA4,
    PhysicalDeviceArchitecture_AMD_CDNA1,
    PhysicalDeviceArchitecture_ARM_Midgard,
    PhysicalDeviceArchitecture_ARM_Bifrost,
    PhysicalDeviceArchitecture_ARM_Valhall,
    PhysicalDeviceArchitecture_ARM_Gen5,
    PhysicalDeviceArchitecture_Broadcom_VideoCore,
    PhysicalDeviceArchitecture_Google_Swiftshader,
    PhysicalDeviceArchitecture_ImgTec_Rogue,
    PhysicalDeviceArchitecture_ImgTec_Furian,
    PhysicalDeviceArchitecture_ImgTec_Albiorix,
    PhysicalDeviceArchitecture_Intel_Gen7,
    PhysicalDeviceArchitecture_Intel_Gen8,
    PhysicalDeviceArchitecture_Intel_Gen9,
    PhysicalDeviceArchitecture_Intel_Gen11,
    PhysicalDeviceArchitecture_Intel_Gen12LP,
    PhysicalDeviceArchitecture_Intel_Gen12HP,
    PhysicalDeviceArchitecture_Intel_XeLPG,
    PhysicalDeviceArchitecture_Intel_Xe2LPG,
    PhysicalDeviceArchitecture_Intel_Xe2HPG,
    PhysicalDeviceArchitecture_Intel_Xe3LPG,
    PhysicalDeviceArchitecture_Mesa_Software,
    PhysicalDeviceArchitecture_Microsoft_WARP,
    PhysicalDeviceArchitecture_Nvidia_Fermi,
    PhysicalDeviceArchitecture_Nvidia_Kepler,
    PhysicalDeviceArchitecture_Nvidia_Maxwell,
    PhysicalDeviceArchitecture_Nvidia_Pascal,
    PhysicalDeviceArchitecture_Nvidia_Turing,
    PhysicalDeviceArchitecture_Nvidia_Ampere,
    PhysicalDeviceArchitecture_Nvidia_Lovelace,
    PhysicalDeviceArchitecture_Nvidia_Blackwell,
    PhysicalDeviceArchitecture_Nvidia_Volta,
    PhysicalDeviceArchitecture_QualcommPCI_Adreno4xx,
    PhysicalDeviceArchitecture_QualcommPCI_Adreno5xx,
    PhysicalDeviceArchitecture_QualcommPCI_Adreno6xx,
    PhysicalDeviceArchitecture_QualcommPCI_Adreno7xx,
    PhysicalDeviceArchitecture_QualcommPCI_Adreno8xx,
    PhysicalDeviceArchitecture_QualcommACPI_Adreno6xx,
    PhysicalDeviceArchitecture_QualcommACPI_Adreno7xx,
    PhysicalDeviceArchitecture_Samsung_RDNA2,
    PhysicalDeviceArchitecture_Samsung_RDNA3,
}PhysicalDeviceArchitecture;


static const char* PhysicalDeviceArchitecture_ToString(const PhysicalDeviceArchitecture arch) {
    switch(arch) {
        case PhysicalDeviceArchitecture_Unknown: return "";
        case PhysicalDeviceArchitecture_AMD_GCN1: return "gcn-1";
        case PhysicalDeviceArchitecture_AMD_GCN2: return "gcn-2";
        case PhysicalDeviceArchitecture_AMD_GCN3: return "gcn-3";
        case PhysicalDeviceArchitecture_AMD_GCN4: return "gcn-4";
        case PhysicalDeviceArchitecture_AMD_GCN5: return "gcn-5";
        case PhysicalDeviceArchitecture_AMD_RDNA1: return "rdna-1";
        case PhysicalDeviceArchitecture_AMD_RDNA2: return "rdna-2";
        case PhysicalDeviceArchitecture_AMD_RDNA3: return "rdna-3";
        case PhysicalDeviceArchitecture_AMD_RDNA4: return "rdna-4";
        case PhysicalDeviceArchitecture_AMD_CDNA1: return "cdna-1";
        case PhysicalDeviceArchitecture_ARM_Midgard: return "midgard";
        case PhysicalDeviceArchitecture_ARM_Bifrost: return "bifrost";
        case PhysicalDeviceArchitecture_ARM_Valhall: return "valhall";
        case PhysicalDeviceArchitecture_ARM_Gen5: return "gen-5";
        case PhysicalDeviceArchitecture_Broadcom_VideoCore: return "videocore";
        case PhysicalDeviceArchitecture_Google_Swiftshader: return "swiftshader";
        case PhysicalDeviceArchitecture_ImgTec_Rogue: return "rogue";
        case PhysicalDeviceArchitecture_ImgTec_Furian: return "furian";
        case PhysicalDeviceArchitecture_ImgTec_Albiorix: return "albiorix";
        case PhysicalDeviceArchitecture_Intel_Gen7: return "gen-7";
        case PhysicalDeviceArchitecture_Intel_Gen8: return "gen-8";
        case PhysicalDeviceArchitecture_Intel_Gen9: return "gen-9";
        case PhysicalDeviceArchitecture_Intel_Gen11: return "gen-11";
        case PhysicalDeviceArchitecture_Intel_Gen12LP: return "gen-12lp";
        case PhysicalDeviceArchitecture_Intel_Gen12HP: return "gen-12hp";
        case PhysicalDeviceArchitecture_Intel_XeLPG: return "xe-lpg";
        case PhysicalDeviceArchitecture_Intel_Xe2LPG: return "xe-2lpg";
        case PhysicalDeviceArchitecture_Intel_Xe2HPG: return "xe-2hpg";
        case PhysicalDeviceArchitecture_Intel_Xe3LPG: return "xe-3lpg";
        case PhysicalDeviceArchitecture_Mesa_Software: return "software";
        case PhysicalDeviceArchitecture_Microsoft_WARP: return "warp";
        case PhysicalDeviceArchitecture_Nvidia_Fermi: return "fermi";
        case PhysicalDeviceArchitecture_Nvidia_Kepler: return "kepler";
        case PhysicalDeviceArchitecture_Nvidia_Maxwell: return "maxwell";
        case PhysicalDeviceArchitecture_Nvidia_Pascal: return "pascal";
        case PhysicalDeviceArchitecture_Nvidia_Turing: return "turing";
        case PhysicalDeviceArchitecture_Nvidia_Ampere: return "ampere";
        case PhysicalDeviceArchitecture_Nvidia_Lovelace: return "lovelace";
        case PhysicalDeviceArchitecture_Nvidia_Blackwell: return "blackwell";
        case PhysicalDeviceArchitecture_Nvidia_Volta: return "volta";
        case PhysicalDeviceArchitecture_QualcommPCI_Adreno4xx: return "adreno-4xx";
        case PhysicalDeviceArchitecture_QualcommPCI_Adreno5xx: return "adreno-5xx";
        case PhysicalDeviceArchitecture_QualcommPCI_Adreno6xx: return "adreno-6xx";
        case PhysicalDeviceArchitecture_QualcommPCI_Adreno7xx: return "adreno-7xx";
        case PhysicalDeviceArchitecture_QualcommPCI_Adreno8xx: return "adreno-8xx";
        case PhysicalDeviceArchitecture_QualcommACPI_Adreno6xx: return "adreno-6xx";
        case PhysicalDeviceArchitecture_QualcommACPI_Adreno7xx: return "adreno-7xx";
        case PhysicalDeviceArchitecture_Samsung_RDNA2: return "rdna-2";
        case PhysicalDeviceArchitecture_Samsung_RDNA3: return "rdna-3";
    }
    return "";
}
static const char* DriverID_ToString(const VkDriverId id){
    switch(id){
        case VK_DRIVER_ID_AMD_PROPRIETARY: return "AMD_PROPRIETARY";
        case VK_DRIVER_ID_AMD_OPEN_SOURCE: return "AMD_OPEN_SOURCE";
        case VK_DRIVER_ID_MESA_RADV: return "MESA_RADV";
        case VK_DRIVER_ID_NVIDIA_PROPRIETARY: return "NVIDIA_PROPRIETARY";
        case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS: return "INTEL_PROPRIETARY_WINDOWS";
        case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA: return "INTEL_OPEN_SOURCE_MESA";
        case VK_DRIVER_ID_IMAGINATION_PROPRIETARY: return "IMAGINATION_PROPRIETARY";
        case VK_DRIVER_ID_QUALCOMM_PROPRIETARY: return "QUALCOMM_PROPRIETARY";
        case VK_DRIVER_ID_ARM_PROPRIETARY: return "ARM_PROPRIETARY";
        case VK_DRIVER_ID_GOOGLE_SWIFTSHADER: return "GOOGLE_SWIFTSHADER";
        case VK_DRIVER_ID_GGP_PROPRIETARY: return "GGP_PROPRIETARY";
        case VK_DRIVER_ID_BROADCOM_PROPRIETARY: return "BROADCOM_PROPRIETARY";
        case VK_DRIVER_ID_MESA_LLVMPIPE: return "MESA_LLVMPIPE";
        case VK_DRIVER_ID_MOLTENVK: return "MOLTENVK";
        case VK_DRIVER_ID_COREAVI_PROPRIETARY: return "COREAVI_PROPRIETARY";
        case VK_DRIVER_ID_JUICE_PROPRIETARY: return "JUICE_PROPRIETARY";
        case VK_DRIVER_ID_VERISILICON_PROPRIETARY: return "VERISILICON_PROPRIETARY";
        case VK_DRIVER_ID_MESA_TURNIP: return "MESA_TURNIP";
        case VK_DRIVER_ID_MESA_V3DV: return "MESA_V3DV";
        case VK_DRIVER_ID_MESA_PANVK: return "MESA_PANVK";
        case VK_DRIVER_ID_SAMSUNG_PROPRIETARY: return "SAMSUNG_PROPRIETARY";
        case VK_DRIVER_ID_MESA_VENUS: return "MESA_VENUS";
        case VK_DRIVER_ID_MESA_DOZEN: return "MESA_DOZEN";
        case VK_DRIVER_ID_MESA_NVK: return "MESA_NVK";
        case VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA: return "IMAGINATION_OPEN_SOURCE_MESA";
        case VK_DRIVER_ID_MESA_HONEYKRISP: return "MESA_HONEYKRISP";
        case VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN: return "VULKAN_SC_EMULATION_ON_VULKAN";
        case VK_DRIVER_ID_MESA_KOSMICKRISP: return "MESA_KOSMICKRISP";
        default: return "<unknown VkDriverId>";
    }
}

#define kVendorID_AMD 0x1002
#define kVendorID_Apple 0x106b
#define kVendorID_ARM 0x13B5
#define kVendorID_Broadcom 0x14e4
#define kVendorID_Google 0x1AE0
#define kVendorID_ImgTec 0x1010
#define kVendorID_Intel 0x8086
#define kVendorID_Mesa 0x10005
#define kVendorID_Microsoft 0x1414
#define kVendorID_Nvidia 0x10DE
#define kVendorID_QualcommPCI 0x5143
#define kVendorID_QualcommACPI 0x4D4F4351
#define kVendorID_Samsung 0x144d
#define kVendorID_Huawei 0x19e5

PhysicalDeviceArchitecture GetArchitecture(uint32_t vendorId, uint32_t deviceId) {
    ENTRY();
    switch(vendorId) {
        default: return PhysicalDeviceArchitecture_Unknown;
        case kVendorID_AMD: {
            switch (deviceId & 0xFFF0) {
                case 0x1300:
                case 0x1310:
                case 0x6600:
                case 0x6610:
                case 0x6660:
                case 0x6790:
                case 0x6800:
                case 0x6810:
                case 0x6820:
                case 0x6830:
                    return PhysicalDeviceArchitecture_AMD_GCN1;
                case 0x6640:
                case 0x6650:
                case 0x67A0:
                case 0x67B0:
                case 0x9830:
                case 0x9850:
                    return PhysicalDeviceArchitecture_AMD_GCN2;
                case 0x6900:
                case 0x6920:
                case 0x6930:
                case 0x7300:
                case 0x9870:
                case 0x98E0:
                    return PhysicalDeviceArchitecture_AMD_GCN3;
                case 0x67C0:
                case 0x67D0:
                case 0x67E0:
                case 0x67F0:
                case 0x6980:
                case 0x6990:
                case 0x6FD0:
                case 0x9920:
                    return PhysicalDeviceArchitecture_AMD_GCN4;
                case 0x66A0:
                case 0x6860:
                case 0x6870:
                case 0x6940:
                case 0x69A0:
                case 0x15D0:
                case 0x1630:
                    return PhysicalDeviceArchitecture_AMD_GCN5;
                case 0x7310:
                case 0x7340:
                case 0x7360:
                    return PhysicalDeviceArchitecture_AMD_RDNA1;
                case 0x73A0:
                case 0x73B0:
                case 0x73D0:
                case 0x73E0:
                case 0x73F0:
                case 0x7400:
                case 0x7420:
                case 0x7430:
                case 0x1430:
                case 0x1500:
                case 0x15E0:
                case 0x1640:
                case 0x1680:
                case 0x13c0:
                case 0x13f0:
                    return PhysicalDeviceArchitecture_AMD_RDNA2;
                case 0x7440:
                case 0x7470:
                case 0x7480:
                case 0x15B0:
                case 0x7450:
                case 0x1900:
                case 0x1580:
                case 0x1110:
                    return PhysicalDeviceArchitecture_AMD_RDNA3;
                case 0x7550:
                case 0x7590:
                    return PhysicalDeviceArchitecture_AMD_RDNA4;
                case 0x7380:
                    return PhysicalDeviceArchitecture_AMD_CDNA1;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_ARM: {
            switch (deviceId & 0xF0000000) {
                case 0x00000000:
                    return PhysicalDeviceArchitecture_ARM_Midgard;
                case 0x60000000:
                case 0x70000000:
                    return PhysicalDeviceArchitecture_ARM_Bifrost;
                case 0x90000000:
                case 0xA0000000:
                case 0xB0000000:
                    return PhysicalDeviceArchitecture_ARM_Valhall;
                case 0xC0000000:
                case 0xD0000000:
                    return PhysicalDeviceArchitecture_ARM_Gen5;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Broadcom: {
            switch (deviceId & 0x00000000) {
                case 0x00000000:
                    return PhysicalDeviceArchitecture_Broadcom_VideoCore;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Google: {
            switch (deviceId) {
                case 0xC0DE:
                    return PhysicalDeviceArchitecture_Google_Swiftshader;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_ImgTec: {
            switch (deviceId & 0xFF000000) {
                case 0x00000000:
                case 0x22000000:
                case 0x24000000:
                    return PhysicalDeviceArchitecture_ImgTec_Rogue;
                case 0x1b000000:
                    return PhysicalDeviceArchitecture_ImgTec_Furian;
                case 0x35000000:
                case 0x36000000:
                    return PhysicalDeviceArchitecture_ImgTec_Albiorix;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Intel: {
            switch (deviceId & 0xFF00) {
                case 0x0100:
                case 0x0400:
                case 0x0A00:
                case 0x0D00:
                case 0x0F00:
                    return PhysicalDeviceArchitecture_Intel_Gen7;
                case 0x1600:
                case 0x2200:
                    return PhysicalDeviceArchitecture_Intel_Gen8;
                case 0x1900:
                case 0x3100:
                case 0x3E00:
                case 0x5A00:
                case 0x5900:
                case 0x8700:
                case 0x9B00:
                    return PhysicalDeviceArchitecture_Intel_Gen9;
                case 0x8A00:
                case 0x4E00:
                case 0x9800:
                    return PhysicalDeviceArchitecture_Intel_Gen11;
                case 0x4600:
                case 0x4C00:
                case 0x4900:
                case 0x9A00:
                case 0xA700:
                    return PhysicalDeviceArchitecture_Intel_Gen12LP;
                case 0x4F00:
                case 0x5600:
                    return PhysicalDeviceArchitecture_Intel_Gen12HP;
                case 0x7D00:
                case 0xB600:
                    return PhysicalDeviceArchitecture_Intel_XeLPG;
                case 0x6400:
                    return PhysicalDeviceArchitecture_Intel_Xe2LPG;
                case 0xE200:
                    return PhysicalDeviceArchitecture_Intel_Xe2HPG;
                case 0xB000:
                    return PhysicalDeviceArchitecture_Intel_Xe3LPG;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Mesa: {
            switch (deviceId) {
                case 0x0000:
                    return PhysicalDeviceArchitecture_Mesa_Software;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Microsoft: {
            switch (deviceId) {
                case 0x8c:
                    return PhysicalDeviceArchitecture_Microsoft_WARP;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Nvidia: {
            switch (deviceId & 0xFFFFFF00) {
                case 0x0D00:
                    return PhysicalDeviceArchitecture_Nvidia_Fermi;
                case 0x0F00:
                case 0x1000:
                case 0x1100:
                case 0x1200:
                    return PhysicalDeviceArchitecture_Nvidia_Kepler;
                case 0x1300:
                case 0x1400:
                case 0x1600:
                case 0x1700:
                    return PhysicalDeviceArchitecture_Nvidia_Maxwell;
                case 0x1500:
                case 0x1B00:
                case 0x1C00:
                case 0x1D00:
                    return PhysicalDeviceArchitecture_Nvidia_Pascal;
                case 0x1E00:
                case 0x1F00:
                case 0x2100:
                    return PhysicalDeviceArchitecture_Nvidia_Turing;
                case 0x2200:
                case 0x2400:
                case 0x2500:
                case 0x2000:
                    return PhysicalDeviceArchitecture_Nvidia_Ampere;
                case 0x2600:
                case 0x2700:
                case 0x2800:
                    return PhysicalDeviceArchitecture_Nvidia_Lovelace;
                case 0x2b00:
                case 0x2c00:
                case 0x2d00:
                case 0x2f00:
                    return PhysicalDeviceArchitecture_Nvidia_Blackwell;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
            switch (deviceId & 0xFF000000) {
                case 0x1e000000:
                    return PhysicalDeviceArchitecture_Nvidia_Kepler;
                case 0x92000000:
                    return PhysicalDeviceArchitecture_Nvidia_Maxwell;
                case 0x93000000:
                    return PhysicalDeviceArchitecture_Nvidia_Pascal;
                case 0x97000000:
                    return PhysicalDeviceArchitecture_Nvidia_Ampere;
                case 0xa5000000:
                    return PhysicalDeviceArchitecture_Nvidia_Volta;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_QualcommPCI: {
            switch (deviceId & 0xFF000000) {
                case 0x04000000:
                    return PhysicalDeviceArchitecture_QualcommPCI_Adreno4xx;
                case 0x05000000:
                    return PhysicalDeviceArchitecture_QualcommPCI_Adreno5xx;
                case 0x06000000:
                    return PhysicalDeviceArchitecture_QualcommPCI_Adreno6xx;
                case 0x07000000:
                case 0x43000000:
                case 0x36000000:
                case 0x37000000:
                    return PhysicalDeviceArchitecture_QualcommPCI_Adreno7xx;
                case 0x44000000:
                    return PhysicalDeviceArchitecture_QualcommPCI_Adreno8xx;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_QualcommACPI: {
            switch (deviceId & 0xFFFFFF00) {
                case 0x41333800:
                case 0x36334100:
                case 0x41333400:
                case 0x36333600:
                    return PhysicalDeviceArchitecture_QualcommACPI_Adreno6xx;
                case 0x37314400:
                case 0x36334300:
                    return PhysicalDeviceArchitecture_QualcommACPI_Adreno7xx;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Samsung: {
            switch (deviceId & 0xFFFFFFFF) {
                case 0x000073A0:
                case 0x01300100:
                    return PhysicalDeviceArchitecture_Samsung_RDNA2;
                case 0x02600200:
                    return PhysicalDeviceArchitecture_Samsung_RDNA3;
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
        case kVendorID_Huawei: {
            switch (deviceId & 0xFFFFFFFF) {
                default: return PhysicalDeviceArchitecture_Unknown;
            }
        } break;
    }
    EXIT();
    return PhysicalDeviceArchitecture_Unknown;
}

void wgpuCreateAdapter_sync(void* userdata_v){
    ENTRY();
    userdataforcreateadapter* userdata = (userdataforcreateadapter*)userdata_v;
    uint32_t physicalDeviceCount;
    vkEnumeratePhysicalDevices(userdata->instance->instance, &physicalDeviceCount, NULL);
    VkPhysicalDevice* pds = (VkPhysicalDevice*)RL_CALLOC(physicalDeviceCount, sizeof(VkPhysicalDevice));
    VkResult result = vkEnumeratePhysicalDevices(userdata->instance->instance, &physicalDeviceCount, pds);
    if(result != VK_SUCCESS){
        const char res[] = "vkEnumeratePhysicalDevices failed";
        userdata->info.callback(
            WGPURequestAdapterStatus_Unavailable,
            NULL, CLITERAL(WGPUStringView){
                .data = res,
                .length = sizeof(res) - 1
            },
            userdata->info.userdata1,
            userdata->info.userdata2
        );
        return;
    }
    // Handle forceFallbackAdapter - must find CPU device or fail
    uint32_t i = physicalDeviceCount;
    if (userdata->options.forceFallbackAdapter) {
        for(uint32_t j = 0; j < physicalDeviceCount; j++){
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(pds[j], &properties);
            if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU){
                i = j;
                break;
            }
        }
        
        if(i >= physicalDeviceCount) {
            // No CPU device found - forceFallbackAdapter requires one
            RL_FREE((void*)pds);
            const char res[] = "forceFallbackAdapter requested but no CPU device available";
            userdata->info.callback(
                WGPURequestAdapterStatus_Unavailable,
                NULL, CLITERAL(WGPUStringView){
                    .data = res,
                    .length = sizeof(res) - 1
                },
                userdata->info.userdata1,
                userdata->info.userdata2
            );
            return;
        }
    } else {
        VkPhysicalDeviceType preferenceOrder[3];
        if (userdata->options.powerPreference == WGPUPowerPreference_LowPower) {
            // Low power: prefer integrated → discrete → CPU
            preferenceOrder[0] = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            preferenceOrder[1] = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            preferenceOrder[2] = VK_PHYSICAL_DEVICE_TYPE_CPU;
        } else {
            // High performance or unspecified: prefer discrete → integrated → CPU
            preferenceOrder[0] = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            preferenceOrder[1] = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
            preferenceOrder[2] = VK_PHYSICAL_DEVICE_TYPE_CPU;
        }
        
        // Try to find a GPU matching the preference
        for(uint32_t p = 0; p < 3 && i >= physicalDeviceCount; p++){
            for(uint32_t j = 0; j < physicalDeviceCount; j++){
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(pds[j], &properties);
                if(properties.deviceType == preferenceOrder[p]){
                    i = j;
                    break;
                }
            }
        }
        
        // If nothing matched, fail
        if(i >= physicalDeviceCount) {
            RL_FREE((void*)pds);
            const char res[] = "No suitable adapter found matching power preference";
            userdata->info.callback(
                WGPURequestAdapterStatus_Unavailable,
                NULL, CLITERAL(WGPUStringView){
                    .data = res,
                    .length = sizeof(res) - 1
                },
                userdata->info.userdata1,
                userdata->info.userdata2
            );
            return;
        }
    }
    WGPUAdapter adapter = (WGPUAdapter)RL_CALLOC(1, sizeof(WGPUAdapterImpl));
    adapter->instance = userdata->instance;
    wgpuInstanceAddRef(userdata->instance);
    adapter->refCount = 1;
    adapter->physicalDevice = pds[i];
    VkPhysicalDeviceDriverProperties driverProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES
    };
    VkPhysicalDeviceProperties2 pdProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &driverProperties,
    };
    vkGetPhysicalDeviceProperties2(adapter->physicalDevice, &pdProperties);
    vkGetPhysicalDeviceMemoryProperties(adapter->physicalDevice, &adapter->memProperties);

    char* writePos = adapter->cachedDeviceDescription;
    int length1 = snprintf(writePos, 511, "%s %s", driverProperties.driverName, driverProperties.driverInfo);
    writePos += length1;

    uint32_t queueFamilyPropertyCount;

    vkGetPhysicalDeviceQueueFamilyProperties(adapter->physicalDevice, &queueFamilyPropertyCount, NULL);
    VkQueueFamilyProperties* props = (VkQueueFamilyProperties*)RL_CALLOC(queueFamilyPropertyCount, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(adapter->physicalDevice, &queueFamilyPropertyCount, props);
    adapter->queueIndices = CLITERAL(QueueIndices){
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED
    };

    for(uint32_t i = 0;i < queueFamilyPropertyCount;i++){
        if(adapter->queueIndices.graphicsIndex == VK_QUEUE_FAMILY_IGNORED && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)){
            adapter->queueIndices.graphicsIndex = i;
            adapter->queueIndices.computeIndex = i;
            adapter->queueIndices.transferIndex = i;
            adapter->queueIndices.presentIndex = i;
            break;
        }
    }
    RL_FREE((void*)pds);
    RL_FREE((void*)props);
    adapter->deviceInfoCache = (VulkanDeviceInfo){0};
    
    VulkanDeviceInfo* infoCache = &adapter->deviceInfoCache;

    infoCache->knobs.shaderFloat16Int8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
    infoCache->knobs.shaderFloat16Int8Features.pNext = &infoCache->knobs._16BitStorageFeatures;
    infoCache->knobs.features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    infoCache->knobs.features.pNext = &infoCache->knobs.shaderFloat16Int8Features;
    infoCache->knobs._16BitStorageFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES;
    infoCache->knobs._16BitStorageFeatures.pNext = &infoCache->knobs.subgroupSizeControlFeatures;
    infoCache->knobs.subgroupSizeControlFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES;
    infoCache->knobs.subgroupSizeControlFeatures.pNext = &infoCache->knobs.zeroInitializeWorkgroupMemoryFeatures;
    infoCache->knobs.zeroInitializeWorkgroupMemoryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ZERO_INITIALIZE_WORKGROUP_MEMORY_FEATURES;
    infoCache->knobs.zeroInitializeWorkgroupMemoryFeatures.pNext = &infoCache->knobs.demoteToHelperInvocationFeatures;
    infoCache->knobs.demoteToHelperInvocationFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DEMOTE_TO_HELPER_INVOCATION_FEATURES;
    infoCache->knobs.demoteToHelperInvocationFeatures.pNext = &infoCache->knobs.shaderIntegerDotProductFeatures;
    infoCache->knobs.shaderIntegerDotProductFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES;
    infoCache->knobs.shaderIntegerDotProductFeatures.pNext = &infoCache->knobs.depthClipEnableFeatures;
    infoCache->knobs.depthClipEnableFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_CLIP_ENABLE_FEATURES_EXT;
    infoCache->knobs.depthClipEnableFeatures.pNext = &infoCache->knobs.robustness2Features;
    infoCache->knobs.robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
    infoCache->knobs.robustness2Features.pNext = &infoCache->knobs.samplerYCbCrConversionFeatures;
    infoCache->knobs.samplerYCbCrConversionFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
    infoCache->knobs.samplerYCbCrConversionFeatures.pNext = &infoCache->knobs.shaderSubgroupExtendedTypes;
    infoCache->knobs.shaderSubgroupExtendedTypes.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SUBGROUP_EXTENDED_TYPES_FEATURES;
    infoCache->knobs.shaderSubgroupExtendedTypes.pNext = &infoCache->knobs.vulkanMemoryModelFeatures;
    infoCache->knobs.vulkanMemoryModelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
    infoCache->knobs.vulkanMemoryModelFeatures.pNext = &infoCache->knobs.cooperativeMatrixFeatures;
    infoCache->knobs.cooperativeMatrixFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR;
    infoCache->knobs.cooperativeMatrixFeatures.pNext = &infoCache->knobs.descriptorIndexingFeatures;
    infoCache->knobs.descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    infoCache->knobs.descriptorIndexingFeatures.pNext = &infoCache->knobs.pipelineRobustnessFeatures;
    infoCache->knobs.pipelineRobustnessFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_FEATURES_EXT;
    infoCache->knobs.pipelineRobustnessFeatures.pNext = NULL;

    infoCache->properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    infoCache->properties.pNext = &infoCache->propertiesMaintenance3;
    infoCache->propertiesMaintenance3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
    infoCache->propertiesMaintenance3.pNext = &infoCache->driverProperties;
    infoCache->driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    infoCache->driverProperties.pNext = &infoCache->subgroupSizeControlProperties;
    infoCache->subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;
    infoCache->subgroupSizeControlProperties.pNext = &infoCache->shaderIntegerDotProductProperties;
    infoCache->shaderIntegerDotProductProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES;
    infoCache->shaderIntegerDotProductProperties.pNext = &infoCache->propertiesMaintenance4;
    infoCache->propertiesMaintenance4.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_PROPERTIES;
    infoCache->propertiesMaintenance4.pNext = &infoCache->subgroupProperties;
    infoCache->subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    infoCache->subgroupProperties.pNext = &infoCache->externalMemoryHostProperties;
    infoCache->externalMemoryHostProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
    infoCache->externalMemoryHostProperties.pNext = &infoCache->cooperativeMatrixProperties;
    infoCache->cooperativeMatrixProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_KHR;
    infoCache->cooperativeMatrixProperties.pNext = &infoCache->descriptorIndexingProperties;
    infoCache->descriptorIndexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
    infoCache->descriptorIndexingProperties.pNext = &infoCache->pipelineRobustnessProperties;
    infoCache->pipelineRobustnessProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PIPELINE_ROBUSTNESS_PROPERTIES_EXT;
    infoCache->pipelineRobustnessProperties.pNext = &infoCache->propertiesMaintenance5;
    infoCache->propertiesMaintenance5.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_PROPERTIES_KHR;
    infoCache->propertiesMaintenance5.pNext = NULL;

    vkGetPhysicalDeviceProperties2(adapter->physicalDevice, &infoCache->properties);
    vkGetPhysicalDeviceFeatures2(adapter->physicalDevice, &infoCache->knobs.features);

    userdata->info.callback(WGPURequestAdapterStatus_Success, adapter, CLITERAL(WGPUStringView){NULL, 0}, userdata->info.userdata1, userdata->info.userdata2);
    EXIT();
}
WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance, const WGPURequestAdapterOptions* options, WGPURequestAdapterCallbackInfo callbackInfo){
    ENTRY();
    userdataforcreateadapter* info = (userdataforcreateadapter*)RL_CALLOC(1, sizeof(userdataforcreateadapter));
    info->instance = instance;
    if(options)
        info->options = *options;
    info->info = callbackInfo;
    WGPUFutureImpl ret = {
        .userdataForFunction = info,
        .functionCalledOnWaitAny = wgpuCreateAdapter_sync,
        .freeUserData = RL_FREE
    };

    uint64_t id = instance->currentFutureId++; //atomic?
    FutureIDMap_put(&instance->g_futureIDMap, id, ret);
    return (WGPUFuture){ id };
    EXIT();
}

static int cmp_uint32_(const void *a, const void *b) {
    uint32_t ua = *(const uint32_t *)a;
    uint32_t ub = *(const uint32_t *)b;
    if (ua < ub) return -1;
    if (ua > ub) return 1;
    return 0;
}

static inline VkSemaphore CreateSemaphoreD(WGPUDevice device){
    VkSemaphore ret = NULL;
    VkSemaphoreCreateInfo sci = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    
    VkResult res = device->functions.vkCreateSemaphore(device->device, &sci, NULL, &ret);
    if(res != VK_SUCCESS){
        TRACELOG(WGPU_LOG_ERROR, "Error creating semaphore");
    }
    return ret;
}

static size_t sort_uniqueuints(uint32_t *arr, size_t count) {
    if (count <= 1) return count;
    qsort(arr, count, sizeof(uint32_t), cmp_uint32_);
    size_t unique_count = 1;
    for (size_t i = 1; i < count; ++i) {
        if (arr[i] != arr[unique_count - 1]) {
            arr[unique_count++] = arr[i];
        }
    }
    return unique_count;
}

#define RG_SWAP(a, b) do { \
    typeof(a) temp = (a);  \
    (a) = (b);             \
    (b) = temp;            \
} while (0)

typedef struct userdataforcreatedevice{
    WGPUAdapter adapter;
    WGPUDeviceDescriptor deviceDescriptor;
    WGPURequestDeviceCallbackInfo callbackInfo;
}userdataforcreatedevice;


WGPUDevice wgpuAdapterCreateDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor){
    ENTRY();
    //std::pair<WGPUDevice, WGPUQueue> ret = {0,0};
    
    int requiresYCbCr = 0;
    for(uint32_t i = 0;i < descriptor->requiredFeatureCount;i++){
        WGPUFeatureName feature = descriptor->requiredFeatures[i];
        switch(feature){
            case WGPUFeatureName_TextureFormatNV12:
            case WGPUFeatureName_TextureFormatP010:
                requiresYCbCr = 1;
                break;
            default:
            (void)0; 
        }
    }
    // Collect unique queue families
    uint32_t queueFamilies[3] = {
        adapter->queueIndices.graphicsIndex,
        adapter->queueIndices.computeIndex,
        adapter->queueIndices.presentIndex
    };
    uint32_t queueFamilyCount = sort_uniqueuints(queueFamilies, 3);
    
    // Create queue create infos
    VkDeviceQueueCreateInfo queueCreateInfos[8] = {0};
    uint32_t queueCreateInfoCount = 0;
    float queuePriority = 1.0f;

    for (uint32_t queueFamilyIndex = 0;queueFamilyIndex < queueFamilyCount; queueFamilyIndex++) {
        uint32_t queueFamily = queueFamilies[queueFamilyIndex]; 
        if(queueFamily == VK_QUEUE_FAMILY_IGNORED)continue; // TODO handle this better
        VkDeviceQueueCreateInfo queueCreateInfo zeroinit;
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos[queueCreateInfoCount++] = queueCreateInfo;
    }
    
    uint32_t deviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(adapter->physicalDevice, NULL, &deviceExtensionCount, NULL);
    VkExtensionProperties* deprops = (VkExtensionProperties*)RL_CALLOC(deviceExtensionCount, sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(adapter->physicalDevice, NULL, &deviceExtensionCount, deprops);
    
    const char* deviceExtensionsToLookFor[] = {
        //#ifndef FORCE_HEADLESS
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME,
        VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
        #if RENDERBUNDLES_AS_SECONDARY_COMMANDBUFFERS == 1
        VK_KHR_MAINTENANCE_7_EXTENSION_NAME,
        #endif
        //#endif
        #if VULKAN_ENABLE_RAYTRACING == 1
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,      // "VK_KHR_acceleration_structure"
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,        // "VK_KHR_ray_tracing_pipeline"
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,    // "VK_KHR_deferred_host_operations" - required by acceleration structure
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,         // "VK_EXT_descriptor_indexing" - needed for bindless descriptors
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,       // "VK_KHR_buffer_device_address" - needed by AS
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,                   // "VK_KHR_spirv_1_4" - required for ray tracing shaders
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,       // "VK_KHR_shader_float_controls" - required by spirv_1_4
        #endif
        VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,    // For NV12/P010
    };
    #define deviceExtensionsToLookForCount (sizeof(deviceExtensionsToLookFor) / sizeof(const char*))
    
    int depthClipControl_Found = 0;
    int depthClipEnable_Found = 0;

    const char* deviceExtensionsFound[deviceExtensionsToLookForCount + 4];
    uint32_t extInsertIndex = 0;
    for(uint32_t i = 0;i < deviceExtensionsToLookForCount;i++){
        int deviceExtensionFound = 0;
        for(uint32_t j = 0;j < deviceExtensionCount;j++){
            if(strcmp(deprops[j].extensionName, VK_EXT_DEPTH_CLIP_CONTROL_EXTENSION_NAME) == 0){
                depthClipControl_Found = 1;
            }
            if(strcmp(deprops[j].extensionName, VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME) == 0){
                depthClipEnable_Found = 1;
            }

            if(strcmp(deviceExtensionsToLookFor[i], deprops[j].extensionName) == 0){
                deviceExtensionsFound[extInsertIndex++] = deviceExtensionsToLookFor[i];
                deviceExtensionFound = 1;
                goto innerbreak;
            }
        }
        innerbreak:
        if(deviceExtensionFound == 0){
            printf("Device extension not found: %s\n", deviceExtensionsToLookFor[i]);
        }
        
    }

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR deviceFeaturesAddressKhr = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipelineFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &deviceFeaturesAddressKhr,
    };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &pipelineFeatures,
    };

    VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES,
        .samplerYcbcrConversion = requiresYCbCr ? VK_TRUE : VK_FALSE,
    };

    VkPhysicalDeviceVulkan13Features v13features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &accelerationStructureFeatures,
    };
    
    VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };
    if (requiresYCbCr) {
        ycbcrFeatures.pNext = &v13features;
        deviceFeatures.pNext = &ycbcrFeatures;
    } else {
        deviceFeatures.pNext = &v13features;
    }
    vkGetPhysicalDeviceFeatures2(adapter->physicalDevice, &deviceFeatures);
    if(pipelineFeatures.rayTracingPipeline == VK_TRUE){
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
        VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
            &rayTracingPipelineProperties,
        };
        VkPhysicalDeviceProperties2 deviceProperties2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            &accelerationStructureProperties,
        };
        vkGetPhysicalDeviceProperties2(adapter->physicalDevice, &deviceProperties2);
        adapter->rayTracingPipelineProperties = rayTracingPipelineProperties;
        adapter->accelerationStructureProperties = accelerationStructureProperties;
    }
    else{
        adapter->rayTracingPipelineProperties    = (VkPhysicalDeviceRayTracingPipelinePropertiesKHR){0};
        adapter->accelerationStructureProperties = (VkPhysicalDeviceAccelerationStructurePropertiesKHR){0};
    }
    #ifdef __APPLE__
    deviceExtensionsFound[extInsertIndex++] = "VK_KHR_portability_subset";
    #endif
    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures,
        .queueCreateInfoCount = queueCreateInfoCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledExtensionCount = extInsertIndex,
        .ppEnabledExtensionNames = deviceExtensionsFound,
    };
    
    WGPUDevice retDevice = RL_CALLOC(1, sizeof(WGPUDeviceImpl));

    retDevice->refCount = 1;
    WGPUQueue retQueue = RL_CALLOC(1, sizeof(WGPUQueueImpl));
    retQueue->refCount = 0;
    VkResult dcresult = vkCreateDevice(adapter->physicalDevice, &createInfo, NULL, &(retDevice->device));

    if (dcresult != VK_SUCCESS) {
        TRACELOG(WGPU_LOG_FATAL, "vkCreateDevice failed: %s", vkErrorString(dcresult));
    } else {
        //TRACELOG(WGPU_LOG_INFO, "Successfully created logical device");
        volkLoadDeviceTable(&retDevice->functions, retDevice->device);
        retDevice->capabilities.depthClipEnable = depthClipEnable_Found;    
        retDevice->capabilities.depthClipControl = depthClipControl_Found;    
    }
    retDevice->capabilities.dynamicRendering = v13features.dynamicRendering;
    retDevice->capabilities.raytracing = pipelineFeatures.rayTracingPipeline && accelerationStructureFeatures.accelerationStructure;
    retDevice->capabilities.shaderDeviceAddress = deviceFeaturesAddressKhr.bufferDeviceAddress;
    retDevice->uncapturedErrorCallbackInfo = descriptor->uncapturedErrorCallbackInfo;

    // Retrieve and assign queues
    
    QueueIndices indices = adapter->queueIndices;
    retDevice->uncapturedErrorCallbackInfo = descriptor->uncapturedErrorCallbackInfo;
    retDevice->functions.vkGetDeviceQueue(retDevice->device, indices.graphicsIndex, 0, &retQueue->graphicsQueue);
    //#ifndef FORCE_HEADLESS
    if(indices.presentIndex != VK_QUEUE_FAMILY_IGNORED){
        retDevice->functions.vkGetDeviceQueue(retDevice->device, indices.presentIndex, 0, &retQueue->presentQueue);
    }
    //#endif
    if (indices.computeIndex != indices.graphicsIndex && indices.computeIndex != indices.presentIndex) {
        retDevice->functions.vkGetDeviceQueue(retDevice->device, indices.computeIndex, 0, &retQueue->computeQueue);
    } else {
        // If compute Index is same as graphics or present, assign accordingly
        if (indices.computeIndex == indices.graphicsIndex) {
            retQueue->computeQueue = retQueue->graphicsQueue;
        } else if (indices.computeIndex == indices.presentIndex) {
            retQueue->computeQueue = retQueue->presentQueue;
        }
    }
    const VkCommandPoolCreateInfo pci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    retDevice->functions.vkCreateCommandPool(retDevice->device, &pci, NULL, &retDevice->secondaryCommandPool);
    
    WGPUCommandEncoderDescriptor cedesc = {0};

    FenceCache_Init(retDevice, &retDevice->fenceCache);
    FIFCache_init(&retDevice->fifCache, retDevice, adapter->queueIndices.graphicsIndex);
    
    retQueue->presubmitCache = wgpuDeviceCreateCommandEncoder(retDevice, &cedesc);
    VkDeviceSize limit = (((uint64_t)1) << 30);

    VkPhysicalDeviceMemoryProperties2 memoryProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    vkGetPhysicalDeviceMemoryProperties2(adapter->physicalDevice, &memoryProperties);

    VkDeviceSize heapsizes[128] = {0};
    
    for(uint32_t i = 0;i < memoryProperties.memoryProperties.memoryHeapCount;i++){
        heapsizes[i] = limit;
    }
    uint32_t hostVisibleCoherentIndex = 0;
    for(hostVisibleCoherentIndex = 0;hostVisibleCoherentIndex < memoryProperties.memoryProperties.memoryTypeCount;hostVisibleCoherentIndex++){
        if(memoryProperties.memoryProperties.memoryTypes[hostVisibleCoherentIndex].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){
            break;
        }
    }
    
    #if USE_VMA_ALLOCATOR == 1
    
    const VmaPoolCreateInfo vpci = {
        .minAllocationAlignment = 64,
        .memoryTypeIndex = hostVisibleCoherentIndex,
        .blockSize = (1 << 16)
    };
    VmaDeviceMemoryCallbacks callbacks = {0};
    VmaVulkanFunctions vmaVulkanFunctions = {0};
    VmaAllocatorCreateInfo aci = {
        .instance = adapter->instance->instance,
        .physicalDevice = adapter->physicalDevice,
        .device = retDevice->device,
        .preferredLargeHeapBlockSize = 1 << 15,
        .pHeapSizeLimit = heapsizes,
        .pDeviceMemoryCallbacks = &callbacks
        #if VULKAN_ENABLE_RAYTRACING == 1
       ,.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        #endif
    };
    vmaImportVulkanFunctionsFromVolk(&aci, &vmaVulkanFunctions);
    aci.pVulkanFunctions = &vmaVulkanFunctions;
    VkResult allocatorCreateResult = vmaCreateAllocator(&aci, &retDevice->allocator);
    if(allocatorCreateResult != VK_SUCCESS){
        DeviceCallback(retDevice, WGPUErrorType_Internal, STRVIEW("Failed to create allocator"));
    }
    
    vmaCreatePool(retDevice->allocator, &vpci, &retDevice->aligned_hostVisiblePool);
    #endif
    retDevice->thread_pool = wgvk_thread_pool_create(4);
    wgvkAllocator_init(&retDevice->builtinAllocator, adapter->physicalDevice, retDevice, &retDevice->functions);
    {;
        retDevice->queue = retQueue;
        retDevice->adapter = adapter;
        wgpuAdapterAddRef(adapter);
        retDevice->adapter = adapter;
        retQueue->device = retDevice;
    }
    RL_FREE(deprops);
    return retDevice;
    EXIT();
}

static void wgpuAdapterCreateDevice_sync(void* userdata){
    ENTRY();
    userdataforcreatedevice* data = (userdataforcreatedevice*)userdata;
    WGPUDevice device = wgpuAdapterCreateDevice(data->adapter, &data->deviceDescriptor);
    if(device){
        data->callbackInfo.callback(
            WGPURequestDeviceStatus_Success,
            device,
            (WGPUStringView){0},
            data->callbackInfo.userdata1,
            data->callbackInfo.userdata2
        );
    }
    else{
        data->callbackInfo.callback(
            WGPURequestDeviceStatus_Error,
            NULL,
            STRVIEW("Error"),
            data->callbackInfo.userdata1,
            data->callbackInfo.userdata2
        );
    }
    EXIT();
}

WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter, WGPU_NULLABLE WGPUDeviceDescriptor const * options, WGPURequestDeviceCallbackInfo callbackInfo) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    userdataforcreatedevice* userdata = RL_CALLOC(1, sizeof(userdataforcreatedevice));
    userdata->adapter = adapter;
    userdata->callbackInfo = callbackInfo;
    userdata->deviceDescriptor = *options;
    WGPUFutureImpl impl = {
        .userdataForFunction = userdata,
        .functionCalledOnWaitAny = wgpuAdapterCreateDevice_sync,
        .freeUserData = RL_FREE
    };
    uint64_t id = adapter->instance->currentFutureId++;
    FutureIDMap_put(&adapter->instance->g_futureIDMap, id, impl);
    return (WGPUFuture){id};
    EXIT();
}

WGPUQueue wgpuDeviceGetQueue(WGPUDevice device){
    ENTRY();
    wgpuQueueAddRef(device->queue);
    return device->queue;
    EXIT();
}
typedef struct userdataformapbufferasync{
    WGPUBuffer buffer;
    WGPUMapMode mode;
    size_t offset;
    size_t size;
    WGPUBufferMapCallbackInfo info;
}userdataformapbufferasync;

void wgpuBufferMapSync(void* data){
    ENTRY();
    userdataformapbufferasync* info = (userdataformapbufferasync*)data;
    void* mapdata = NULL;
    wgpuBufferMap(info->buffer, info->mode, info->offset, info->size, &mapdata);
    
    info->info.callback(WGPUMapAsyncStatus_Success, (WGPUStringView){"", 0}, info->info.userdata1, info->info.userdata2);
    EXIT();
}
void wgpuBufferMap(WGPUBuffer buffer, WGPUMapMode mapmode, size_t offset, size_t size, void** data);

WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* desc){
    ENTRY();
    //vmaCreateAllocator(const VmaAllocatorCreateInfo * _Nonnull pCreateInfo, VmaAllocator  _Nullable * _Nonnull pAllocator)
    
    if(desc->usage & WGPUBufferUsage_MapRead){
        if(desc->usage & ~(WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead)){
            DeviceCallback(device, WGPUErrorType_Validation, STRVIEW("WGPUBufferUsage_MapRead used with something other than WGPUBufferUsage_CopyDst"));
        }
    }
    if(desc->usage & WGPUBufferUsage_MapWrite){
        if(desc->usage & ~(WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite)){
            DeviceCallback(device, WGPUErrorType_Validation, STRVIEW("WGPUBufferUsage_MapWrite used with something other than WGPUBufferUsage_CopySrc"));
        }
    }
    WGPUBuffer wgpuBuffer = RL_CALLOC(1, sizeof(WGPUBufferImpl));

    uint32_t cacheIndex = device->submittedFrames % framesInFlight;

    wgpuBuffer->device = device;
    wgpuBuffer->cacheIndex = cacheIndex;
    wgpuBuffer->refCount = 1;
    wgpuBuffer->usage = desc->usage;
    
    
    const VkBufferCreateInfo bufferDesc = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = desc->size,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = toVulkanBufferUsage(desc->usage),
    };
    
    VkMemoryPropertyFlags propertyToFind = 0;
    if(desc->usage & (WGPUBufferUsage_MapRead | WGPUBufferUsage_MapWrite)){
        propertyToFind = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    else{
        // propertyToFind = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        propertyToFind = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    #if USE_VMA_ALLOCATOR == 1
    VmaAllocationCreateInfo vallocInfo = {
        .preferredFlags = propertyToFind,
    };
    VmaAllocation allocation zeroinit;
    VmaAllocationInfo allocationInfo zeroinit;
    VkResult vmabufferCreateResult = vmaCreateBuffer(device->allocator, &bufferDesc, &vallocInfo, &wgpuBuffer->buffer, &allocation, &allocationInfo);

    if(vmabufferCreateResult != VK_SUCCESS){
        DeviceCallback(device, WGPUErrorType_OutOfMemory, STRVIEW("Failed to create allocator"));
        TRACELOG(WGPU_LOG_ERROR, "Could not allocate buffer: %s", vkErrorString(vmabufferCreateResult));
        RL_FREE(wgpuBuffer);
        return NULL;
    }
    wgpuBuffer->vmaAllocation = allocation;
    wgpuBuffer->allocationType = AllocationTypeVMA;
    #else
    device->functions.vkCreateBuffer(device->device, &bufferDesc, NULL, &wgpuBuffer->buffer);
    wgvkAllocation allocation = {0};
    VkMemoryRequirements requirements = {0};
    device->functions.vkGetBufferMemoryRequirements(device->device, wgpuBuffer->buffer, &requirements);
    if(desc->usage & WGPUBufferUsage_Raytracing){
        requirements.alignment = 256;
    }
    bool ret = wgvkAllocator_alloc(&device->builtinAllocator, &requirements, propertyToFind, &allocation);
    wgpuBuffer->allocationType = AllocationTypeBuiltin;
    wgpuBuffer->builtinAllocation = allocation;
    device->functions.vkBindBufferMemory(device->device, wgpuBuffer->buffer, allocation.pool->chunks[allocation.chunk_index].memory, allocation.offset);
    #endif
    wgpuBuffer->memoryProperties = propertyToFind;

    if(desc->usage & WGPUBufferUsage_ShaderDeviceAddress){
        const VkBufferDeviceAddressInfo bdai = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR,
            .buffer = wgpuBuffer->buffer
        };
        wgpuBuffer->address = device->functions.vkGetBufferDeviceAddress(device->device, &bdai);
    }
    if(desc->mappedAtCreation){
        // TODO
        void* mapData = NULL;
        wgpuBufferMap(wgpuBuffer, (desc->usage & WGPUBufferUsage_MapWrite) ? WGPUMapMode_Write : WGPUMapMode_Read, 0, desc->size, &mapData);

    }
    return wgpuBuffer;
    EXIT();
}

void wgpuBufferMap(WGPUBuffer buffer, WGPUMapMode mapmode, size_t offset, size_t size, void** data){
    ENTRY();
    WGPUDevice device = buffer->device;
    if(size == WGPU_WHOLE_SIZE){
        size = wgpuBufferGetSize(buffer);
    }
    if(buffer->latestFence){
        if(buffer->latestFence->state == WGPUFenceState_InUse){
            wgpuFenceWait(buffer->latestFence, ((uint64_t)1) << 40);
        }
        wgpuFenceRelease(buffer->latestFence);
        buffer->latestFence = NULL;
    }
    buffer->mapState = WGPUBufferMapState_Mapped;
    switch(buffer->allocationType){
        case AllocationTypeBuiltin:{
            wgvkAllocation* allocation = &buffer->builtinAllocation;
            WgvkMemoryChunk* chunk = &allocation->pool->chunks[allocation->chunk_index];
            if(chunk->mapCount++ == 0){
                VkResult mapResult = device->functions.vkMapMemory(device->device, chunk->memory, 0, chunk->allocator.size_in_bytes, 0, &chunk->mapped);
                wgvk_assert(mapResult == VK_SUCCESS, "Mapping memory failed: %s", vkErrorString(mapResult));
            }
            *data = (void*)(((uint8_t*)chunk->mapped) + allocation->offset + offset);
            buffer->mappedRange = *data;
        }break;
        #if USE_VMA_ALLOCATOR == 1
        case AllocationTypeVMA: {
            vmaMapMemory(buffer->device->allocator, buffer->vmaAllocation, data);
            buffer->mappedRange = *data;
        }break;
        #endif
        case AllocationTypeJustMemory: {
            device->functions.vkMapMemory(device->device, buffer->justMemory, offset, size, 0, data);
            buffer->mappedRange = *data;
        }break;
        default:
        rg_unreachable();
        *data = NULL;
    }
    
    EXIT();
}

void wgpuBufferUnmap(WGPUBuffer buffer){
    ENTRY();
    WGPUDevice device = buffer->device;
    buffer->mappedRange = NULL;
    buffer->mapState = WGPUBufferMapState_Unmapped;
    switch(buffer->allocationType){
        case AllocationTypeBuiltin:{
            wgvkAllocation* allocation = &buffer->builtinAllocation;
            WgvkMemoryChunk* chunk = &allocation->pool->chunks[allocation->chunk_index];
            if(--chunk->mapCount == 0){
                device->functions.vkUnmapMemory(device->device, chunk->memory);
            }
        }break;
        #if USE_VMA_ALLOCATOR
        case AllocationTypeVMA: {
            vmaUnmapMemory(buffer->device->allocator, buffer->vmaAllocation);
        }break;
        #endif
        case AllocationTypeJustMemory: {
            device->functions.vkUnmapMemory(device->device, buffer->justMemory);
        }break;
        default:
        rg_unreachable();
    }
    EXIT();
}


WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size, WGPUBufferMapCallbackInfo callbackInfo){
    ENTRY();
    userdataformapbufferasync* info = (userdataformapbufferasync*)RL_CALLOC(1, sizeof(userdataformapbufferasync));
    info->buffer = buffer;
    info->mode = mode;
    info->offset = offset;
    info->size = size;
    info->info = callbackInfo;
    WGPUFutureImpl ret = {
        .userdataForFunction = info,
        .functionCalledOnWaitAny = wgpuBufferMapSync,
        .freeUserData = RL_FREE
    };
    uint64_t id = atomic_fetch_add_explicit(&buffer->device->adapter->instance->currentFutureId, 1, memory_order_relaxed);
    FutureIDMap_put(&buffer->device->adapter->instance->g_futureIDMap, id, ret);
    return (WGPUFuture){ id };
    EXIT();
}

size_t wgpuBufferGetSize(WGPUBuffer buffer){
    ENTRY();
    WGPUDevice device = buffer->device;
    switch(buffer->allocationType){
        case AllocationTypeBuiltin:{
            return buffer->builtinAllocation.size;
        }break;

        #if USE_VMA_ALLOCATOR == 1
        case AllocationTypeVMA: {
            VmaAllocationInfo info zeroinit;
            vmaGetAllocationInfo(buffer->device->allocator, buffer->vmaAllocation, &info);
            return info.size;
        }break;
        #endif
        case AllocationTypeJustMemory: {
            rg_unreachable();
        }break;
        default:
        rg_unreachable();
        
    }
    EXIT();
}

void wgpuQueueWriteBuffer(WGPUQueue cSelf, WGPUBuffer buffer, uint64_t bufferOffset, const void* data, size_t size){
    ENTRY();
    
    void* mappedMemory = NULL;
    VkBufferMemoryBarrier bb = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = NULL,
        .srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer->buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    //cSelf->device->functions.vkCmdPipelineBarrier(
    //    cSelf->presubmitCache->buffer,
    //    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    //    VK_PIPELINE_STAGE_TRANSFER_BIT,
    //    0,
    //    0, NULL, 
    //    1, &bb,
    //    0, NULL
    //);
    if(buffer->memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT){
        void* mappedMemory = NULL;
        wgpuBufferMap(buffer, WGPUMapMode_Write, bufferOffset, size, &mappedMemory);
        
        if (mappedMemory != NULL) {
            // Memory is host mappable: copy data and unmap.
            memcpy(mappedMemory, data, size);
            wgpuBufferUnmap(buffer);
            
        }
    }
    else{
        WGPUBufferDescriptor stDesc zeroinit;
        stDesc.size = size;
        stDesc.usage = WGPUBufferUsage_MapWrite;
        WGPUBuffer stagingBuffer = wgpuDeviceCreateBuffer(cSelf->device, &stDesc);
        wgpuQueueWriteBuffer(cSelf, stagingBuffer, bufferOffset, data, size);
        wgpuCommandEncoderCopyBufferToBuffer(cSelf->presubmitCache, stagingBuffer, 0, buffer, bufferOffset, size);
        wgpuBufferRelease(stagingBuffer);
    }
    EXIT();
}

void wgpuQueueWriteTexture(WGPUQueue queue, const WGPUTexelCopyTextureInfo* destination, const void* data, size_t dataSize, const WGPUTexelCopyBufferLayout* dataLayout, const WGPUExtent3D* writeSize){
    ENTRY();

    WGPUBufferDescriptor bdesc zeroinit;
    bdesc.size = dataSize;
    bdesc.usage = WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite;
    WGPUBuffer stagingBuffer = wgpuDeviceCreateBuffer(queue->device, &bdesc);
    void* mappedMemory = NULL;
    wgpuBufferMap(stagingBuffer, WGPUMapMode_Write, 0, dataSize, &mappedMemory);
    if(mappedMemory != NULL){
        memcpy(mappedMemory, data, dataSize);
        wgpuBufferUnmap(stagingBuffer);
    }
    //WGPUCommandEncoder enkoder = wgpuDeviceCreateCommandEncoder(queue->device, NULL);
    WGPUTexelCopyBufferInfo source = {
        .buffer = stagingBuffer,
        .layout = *dataLayout
    };

    wgpuCommandEncoderCopyBufferToTexture(queue->presubmitCache, &source, destination, writeSize);
    //WGPUCommandBuffer puffer = wgpuCommandEncoderFinish(enkoder, NULL);

    //wgpuQueueSubmit(queue, 1, &puffer);
    //wgpuCommandEncoderRelease(enkoder);
    //wgpuCommandBufferRelease(puffer);
    wgpuBufferRelease(stagingBuffer);
    EXIT();
}

WGPUFence wgpuDeviceCreateFence(WGPUDevice device){
    ENTRY();
    WGPUFence fence = RL_CALLOC(1, sizeof(WGPUFenceImpl));
    fence->refCount = 1;
    fence->device = device;
    CallbackWithUserdataVector_init(&fence->callbacksOnWaitComplete);
    
    // Initialize the new synchronization primitives
    fence->wait_mutex = wgvk_mutex_create(wgvk_locktype_kernel);
    fence->wait_cond = wgvk_cond_create(wgvk_locktype_kernel);
    if (!fence->wait_mutex || !fence->wait_cond) {
        // Handle initialization failure
        if (fence->wait_mutex) wgvk_mutex_destroy(fence->wait_mutex);
        if (fence->wait_cond) wgvk_cond_destroy(fence->wait_cond);
        RL_FREE(fence);
        return NULL;
    }

    VkFenceCreateInfo createInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence->fence = FenceCache_GetFence(&device->fenceCache);
    // The fence starts in a "reset" state, ready to be used.
    atomic_init(&fence->state, WGPUFenceState_Reset);
    return fence;
    EXIT();
}

static inline void wgpuFenceRunWaitCompleteCallbacks(WGPUFence fence) {
    const size_t n = fence->callbacksOnWaitComplete.size;
    for (size_t i = 0; i < n; i++) {
        CallbackWithUserdata cb = fence->callbacksOnWaitComplete.data[i];
        if (cb.callback) {
            cb.callback(cb.userdata);
        }
    }
}

void wgpuFenceWait(WGPUFence fence, uint64_t timeoutNS) {
    ENTRY();

    if (!fence) {
        EXIT();
        return;
    }

    if (atomic_load_explicit(&fence->state, memory_order_acquire) == WGPUFenceState_Finished) {
        EXIT();
        return;
    }

    bool is_designated_waiter = false;
    
    // Attempt to become the designated waiter. This is an atomic operation.
    WGPUFenceState expected_state = WGPUFenceState_InUse;
    if (atomic_compare_exchange_strong_explicit(&fence->state, &expected_state, WGPUFenceState_Waiting, memory_order_acq_rel, memory_order_acquire)) {
        is_designated_waiter = true;
    }

    if (is_designated_waiter) {
        VkResult waitResult = fence->device->functions.vkWaitForFences(
            fence->device->device, 1, &fence->fence, VK_TRUE, timeoutNS);
        
        wgvk_assert(waitResult == VK_SUCCESS || waitResult == VK_TIMEOUT, "vkWaitForFences returned an unexpected error.");

        // Lock the mutex to safely update state and signal followers.
        wgvk_mutex_lock(fence->wait_mutex);
        if (waitResult == VK_SUCCESS) {
            wgpuFenceRunWaitCompleteCallbacks(fence);
            atomic_store_explicit(&fence->state, WGPUFenceState_Finished, memory_order_release);
        } else { // Timeout occurred
            // Revert state to InUse so another thread can attempt to wait again.
            atomic_store_explicit(&fence->state, WGPUFenceState_InUse, memory_order_release);
        }
        // Wake up ALL follower threads that might be waiting on the condition variable.
        wgvk_cond_broadcast(fence->wait_cond);
        wgvk_mutex_unlock(fence->wait_mutex);

    } else {
        wgvk_mutex_lock(fence->wait_mutex);
        while (atomic_load_explicit(&fence->state, memory_order_acquire) != WGPUFenceState_Finished) {
            wgvk_cond_wait(fence->wait_cond, fence->wait_mutex);
        }
        wgvk_mutex_unlock(fence->wait_mutex);
    }

    EXIT();
}

void wgpuFencesWait(const WGPUFence* fences, uint32_t fenceCount, uint64_t timeoutNS) {
    ENTRY();
    if (!fences || fenceCount == 0) { EXIT(); return; }
    for (uint32_t i = 0; i < fenceCount; i++) {
        if (fences[i]) {
            wgpuFenceWait(fences[i], timeoutNS);
        }
    }
    EXIT();
}

void wgpuFenceAttachCallback(WGPUFence fence, void(*callback)(void*), void* userdata){
    ENTRY();
    CallbackWithUserdataVector_push_back(&fence->callbacksOnWaitComplete, (CallbackWithUserdata){
        .callback = callback,
        .userdata = userdata
    });
    EXIT();
}

void wgpuFenceAddRef(WGPUFence fence){
    ENTRY();
    ++fence->refCount;
    EXIT();
}

void wgpuFenceRelease(WGPUFence fence){
    ENTRY();
    wgvk_assert(fence->refCount > 0, "refCount already zero");
    if(--fence->refCount == 0){
        wgvk_mutex_destroy(fence->wait_mutex);
        wgvk_cond_destroy(fence->wait_cond);

        FenceCache_PutFence(&fence->device->fenceCache, fence->fence);
        for(uint32_t i = 0; i < CallbackWithUserdataVector_size(&fence->callbacksOnWaitComplete); i++){
            CallbackWithUserdata* cbu = CallbackWithUserdataVector_get(&fence->callbacksOnWaitComplete, i);
            if(cbu->freeUserData){
                cbu->freeUserData(cbu->userdata);
            }
        }
        CallbackWithUserdataVector_free(&fence->callbacksOnWaitComplete);
        RL_FREE(fence);
    }
    EXIT();
}

WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor){
    ENTRY();
    VkDeviceMemory imageMemory zeroinit;
    // Adjust usage flags based on format (e.g., depth formats might need different usages)
    if(descriptor->viewFormatCount == 0){
        char* message = sw_sprintf("Texture descriptor [%s] contains zero view formats", descriptor->label);
        WGPUStringView msg = {
            .data = message,
            .length = strlen(message)
        };
        DeviceCallback(device, WGPUErrorType_Validation, msg);
        RL_FREE(message);

    }
    WGPUTexture ret = RL_CALLOC(1, sizeof(WGPUTextureImpl));
    ret->usage = toVulkanTextureUsage(descriptor->usage, descriptor->format);
    ret->dimension = toVulkanTextureDimension(descriptor->dimension);
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width = descriptor->size.width,
            .height = descriptor->size.height,
            .depth = 1
        },
        .mipLevels = descriptor->mipLevelCount,
        .arrayLayers = descriptor->size.depthOrArrayLayers,
        .format = toVulkanPixelFormat(descriptor->format),
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = toVulkanTextureUsage(descriptor->usage, descriptor->format),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = toVulkanSampleCount(descriptor->sampleCount),
    };

    if(descriptor->viewFormats == NULL || descriptor->viewFormatCount > 1 || descriptor->viewFormats[0] != descriptor->format){
        imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    }
    
    VkImage image zeroinit;
    if (device->functions.vkCreateImage(device->device, &imageInfo, NULL, &image) != VK_SUCCESS)
        TRACELOG(WGPU_LOG_FATAL, "Failed to create image!");
    
    VkMemoryRequirements memReq;
    device->functions.vkGetImageMemoryRequirements(device->device, image, &memReq);
    
    VkMemoryAllocateInfo allocInfo zeroinit;
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        device->adapter,
        memReq.memoryTypeBits, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    //wgvkAllocation allocation = {0};
    //wgvkAllocator_alloc(&device->builtinAllocator, &memReq, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &allocation);
    
    if (device->functions.vkAllocateMemory(device->device, &allocInfo, NULL, &imageMemory) != VK_SUCCESS){
        TRACELOG(WGPU_LOG_FATAL, "Failed to allocate image memory!");
    }
    device->functions.vkBindImageMemory(device->device, image, imageMemory, 0);

    ret->image = image;
    ret->memory = imageMemory;
    ret->device = device;
    ret->width =  descriptor->size.width;
    ret->height = descriptor->size.height;
    ret->format = toVulkanPixelFormat(descriptor->format);
    ret->sampleCount = descriptor->sampleCount;
    ret->depthOrArrayLayers = descriptor->size.depthOrArrayLayers;
    ret->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    ret->refCount = 1;
    ret->mipLevels = descriptor->mipLevelCount;
    ret->memory = imageMemory;
    Texture_ViewCache_init(&ret->viewCache);
    EXIT();
    return ret;
}

static inline uint32_t descriptorTypeContiguous(VkDescriptorType type){
    switch(type){
        case VK_DESCRIPTOR_TYPE_SAMPLER: return 0;
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return 1;
        case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return 2;
        case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: return 3;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return 4;
        case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return 5;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return 6;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return 7;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return 8;
        case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return 9;
        case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return 10;
        case VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK: return 11;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return 12;
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV: return 13;
        case VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM: return 14;
        case VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM: return 15;
        case VK_DESCRIPTOR_TYPE_MUTABLE_EXT: return 16;
        case VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV: return 17;
        case VK_DESCRIPTOR_TYPE_MAX_ENUM: return 20;
        default: rg_unreachable();
    }
}

static inline VkDescriptorType contiguousDescriptorType(uint32_t cont){
    switch(cont){
        case 0 : return VK_DESCRIPTOR_TYPE_SAMPLER;
        case 1 : return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case 2 : return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case 3 : return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case 4 : return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case 5 : return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case 6 : return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case 7 : return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case 8 : return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case 9 : return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case 10: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case 11: return VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
        case 12: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        case 13: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
        case 14: return VK_DESCRIPTOR_TYPE_SAMPLE_WEIGHT_IMAGE_QCOM;
        case 15: return VK_DESCRIPTOR_TYPE_BLOCK_MATCH_IMAGE_QCOM;
        case 16: return VK_DESCRIPTOR_TYPE_MUTABLE_EXT;
        case 17: return VK_DESCRIPTOR_TYPE_PARTITIONED_ACCELERATION_STRUCTURE_NV;
        case 20: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        default: rg_unreachable();
    }
}
#define DESCRIPTOR_TYPE_UPPER_LIMIT 32

void wgpuWriteBindGroup(WGPUDevice device, WGPUBindGroup wvBindGroup, const WGPUBindGroupDescriptor* bgdesc){
    ENTRY();
    
    wgvk_assert(bgdesc->layout != NULL, "WGPUBindGroupDescriptor::layout is null");
    
    if(wvBindGroup->pool == NULL){
        wvBindGroup->layout = bgdesc->layout;

        uint32_t counts[DESCRIPTOR_TYPE_UPPER_LIMIT] = {0};

        //std::unordered_map<VkDescriptorType, uint32_t> counts;
        for(uint32_t i = 0;i < bgdesc->layout->entryCount;i++){
            ++counts[descriptorTypeContiguous(extractVkDescriptorType(bgdesc->layout->entries + i))];
        }

        VkDescriptorPoolSize sizes[DESCRIPTOR_TYPE_UPPER_LIMIT];
        uint32_t VkDescriptorPoolSizeCount = 0;
        //sizes.reserve(counts.size());
        for(uint32_t i = 0;i < DESCRIPTOR_TYPE_UPPER_LIMIT;i++){
            if(counts[i] != 0){
                sizes[VkDescriptorPoolSizeCount++] = (VkDescriptorPoolSize){
                    .type = contiguousDescriptorType(i), 
                    .descriptorCount = counts[i]
                };
            }
        }
        VkDescriptorPoolCreateInfo dpci = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .maxSets = 1,
            .poolSizeCount = VkDescriptorPoolSizeCount,
            .pPoolSizes = sizes
        };
        device->functions.vkCreateDescriptorPool(device->device, &dpci, NULL, &wvBindGroup->pool);

        //VkCopyDescriptorSet copy{};
        //copy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;

        VkDescriptorSetAllocateInfo dsai = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = NULL,
            .descriptorPool = wvBindGroup->pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &bgdesc->layout->layout
        };

        device->functions.vkAllocateDescriptorSets(device->device, &dsai, &wvBindGroup->set);
    }
    ResourceUsage newResourceUsage;
    ResourceUsage_init(&newResourceUsage);
    for(uint32_t i = 0;i < bgdesc->entryCount;i++){
        WGPUBindGroupEntry entry = bgdesc->entries[i];
        if(entry.buffer){
            ru_trackBuffer(&newResourceUsage, (WGPUBuffer)entry.buffer, (BufferUsageRecord){
                0,
                0,
                VK_FALSE
            });
        }
        else if(entry.textureView){
            ru_trackTextureView(&newResourceUsage, (WGPUTextureView)entry.textureView);
        }
        else if(entry.sampler){
            ru_trackSampler(&newResourceUsage, entry.sampler);
        }
    }
    releaseAllAndClear(&wvBindGroup->resourceUsage);
    ResourceUsage_move(&wvBindGroup->resourceUsage, &newResourceUsage);

    
    uint32_t count = bgdesc->entryCount;
     
    VkWriteDescriptorSetVector writes zeroinit;
    VkDescriptorBufferInfoVector bufferInfos zeroinit;
    VkDescriptorImageInfoVector imageInfos zeroinit;
    VkWriteDescriptorSetAccelerationStructureKHRVector accelStructInfos zeroinit;


    VkWriteDescriptorSetVector_initWithSize(&writes, count);
    VkDescriptorBufferInfoVector_initWithSize(&bufferInfos, count);
    VkDescriptorImageInfoVector_initWithSize(&imageInfos, count);
    VkWriteDescriptorSetAccelerationStructureKHRVector_initWithSize(&accelStructInfos, count);

    for(uint32_t i = 0;i < count;i++){
        writes.data[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uint32_t binding = bgdesc->entries[i].binding;
        writes.data[i].dstBinding = binding;
        writes.data[i].dstSet = wvBindGroup->set;
        const WGPUBindGroupLayoutEntry* entryi = &bgdesc->layout->entries[i];
        const VkDescriptorType entryType = extractVkDescriptorType(bgdesc->layout->entries + i);
        writes.data[i].descriptorType = entryType;
        writes.data[i].descriptorCount = 1;
        switch(entryType){
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: //[[fallthrough]];
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:{
                WGPUBuffer bufferOfThatEntry = (WGPUBuffer)bgdesc->entries[i].buffer;
                ru_trackBuffer(&wvBindGroup->resourceUsage, bufferOfThatEntry, (BufferUsageRecord){0, 0, VK_FALSE});
                bufferInfos.data[i].buffer = bufferOfThatEntry->buffer;
                bufferInfos.data[i].offset = bgdesc->entries[i].offset;
                bufferInfos.data[i].range  = bgdesc->entries[i].size;
                writes.data[i].pBufferInfo = bufferInfos.data + i;
            }break;

            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:{
                ru_trackTextureView(&wvBindGroup->resourceUsage, (WGPUTextureView)bgdesc->entries[i].textureView);
                imageInfos.data[i].imageView   = ((WGPUTextureView)bgdesc->entries[i].textureView)->view;
                imageInfos.data[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                writes    .data[i].pImageInfo  = imageInfos.data + i;
            }break;
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:{
                ru_trackTextureView(&wvBindGroup->resourceUsage, (WGPUTextureView)bgdesc->entries[i].textureView);
                imageInfos.data[i].imageView   = ((WGPUTextureView)bgdesc->entries[i].textureView)->view;
                imageInfos.data[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                writes    .data[i].pImageInfo  = imageInfos.data + i;
            }break;
            case VK_DESCRIPTOR_TYPE_SAMPLER:{
                ru_trackSampler(&wvBindGroup->resourceUsage, bgdesc->entries[i].sampler);
                imageInfos.data[i].sampler    = bgdesc->entries[i].sampler->sampler;
                writes.    data[i].pImageInfo = imageInfos.data + i;
            }break;
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:{
                ru_trackAccelerationStructure(&wvBindGroup->resourceUsage, bgdesc->entries[i].accelerationStructure);
                accelStructInfos.data[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                accelStructInfos.data[i].accelerationStructureCount = 1;
                accelStructInfos.data[i].pAccelerationStructures = &(bgdesc->entries[i].accelerationStructure->accelerationStructure);
                writes.          data[i].pNext = accelStructInfos.data + i;
            }break;
            default:
            rg_unreachable();
        }
    }

    device->functions.vkUpdateDescriptorSets(device->device, writes.size, writes.data, 0, NULL);

    VkWriteDescriptorSetVector_free(&writes);
    VkDescriptorBufferInfoVector_free(&bufferInfos);
    VkDescriptorImageInfoVector_free(&imageInfos);
    VkWriteDescriptorSetAccelerationStructureKHRVector_free(&accelStructInfos);
    EXIT();
}



WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device, const WGPUBindGroupDescriptor* bgdesc){
    ENTRY();
    wgvk_assert(bgdesc->layout != NULL, "WGPUBindGroupDescriptor::layout is null");
    
    WGPUBindGroup ret = RL_CALLOC(1, sizeof(WGPUBindGroupImpl));
    ret->refCount = 1;
    ResourceUsage_init(&ret->resourceUsage);

    ret->device = device;
    ret->cacheIndex = device->submittedFrames % framesInFlight;

    PerframeCache* fcache = DeviceGetFIFCache(device, ret->cacheIndex);

    DescriptorSetAndPoolVector* dsap = BindGroupCacheMap_get(&fcache->bindGroupCache, bgdesc->layout);

    if(dsap == NULL || dsap->size == 0){ //Cache miss
        //TRACELOG(WGPU_LOG_INFO, "Allocating new VkDescriptorPool and -Set");
        VkDescriptorPoolCreateInfo dpci zeroinit;
        dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpci.maxSets = 1;

        uint32_t counts[DESCRIPTOR_TYPE_UPPER_LIMIT] = {0};

        for(uint32_t i = 0;i < bgdesc->layout->entryCount;i++){
            const VkDescriptorType vkdt = extractVkDescriptorType(bgdesc->layout->entries + i);
            const uint32_t contiguousIndex = descriptorTypeContiguous(vkdt);
            ++counts[contiguousIndex];
        }
        VkDescriptorPoolSize sizes[DESCRIPTOR_TYPE_UPPER_LIMIT] = {0};
        uint32_t VkDescriptorPoolSizeCount = 0;
        for(uint32_t i = 0;i < DESCRIPTOR_TYPE_UPPER_LIMIT;i++){
            if(counts[i] != 0){
                sizes[VkDescriptorPoolSizeCount++] = (VkDescriptorPoolSize){
                    .type = contiguousDescriptorType(i), 
                    .descriptorCount = counts[i]
                };
            }
        }

        dpci.poolSizeCount = VkDescriptorPoolSizeCount;
        dpci.pPoolSizes = sizes;
        dpci.maxSets = 1;
        device->functions.vkCreateDescriptorPool(device->device, &dpci, NULL, &ret->pool);

        //VkCopyDescriptorSet copy{};
        //copy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;

        VkDescriptorSetAllocateInfo dsai zeroinit;
        dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsai.descriptorPool = ret->pool;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = (VkDescriptorSetLayout*)&bgdesc->layout->layout;
        device->functions.vkAllocateDescriptorSets(device->device, &dsai, &ret->set);
    }
    else{
        ret->pool = dsap->data[dsap->size - 1].pool;
        ret->set  = dsap->data[dsap->size - 1].set;
        --dsap->size;
    }
    ret->entryCount = bgdesc->entryCount;

    ret->entries = RL_CALLOC(bgdesc->entryCount, sizeof(WGPUBindGroupEntry));
    if(bgdesc->entryCount > 0){
        memcpy(ret->entries, bgdesc->entries, bgdesc->entryCount * sizeof(WGPUBindGroupEntry));
    }
    wgpuWriteBindGroup(device, ret, bgdesc);
    ret->layout = bgdesc->layout;
    ++ret->layout->refCount;
    wgvk_assert(ret->layout != NULL, "ret->layout is NULL");
    return ret;
    EXIT();
}



WGPUBindGroupLayout wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* bgldesc){
    ENTRY();
    WGPUBindGroupLayout ret = RL_CALLOC(1, sizeof(WGPUBindGroupLayoutImpl));
    ret->refCount = 1;
    ret->device = device;
    ret->entryCount = bgldesc->entryCount;
    
    const WGPUBindGroupLayoutEntry* entries = bgldesc->entries;
    const uint32_t entryCount = bgldesc->entryCount;

    VkDescriptorSetLayoutBindingVector vkBindings;
    VkDescriptorSetLayoutBindingVector_initWithSize(&vkBindings, bgldesc->entryCount);

    for(uint32_t i = 0;i < bgldesc->entryCount;i++){
        vkBindings.data[i].descriptorCount = 1;
        vkBindings.data[i].binding = entries[i].binding;
        VkDescriptorType vkdtype = extractVkDescriptorType(entries + i);
        vkBindings.data[i].descriptorType = vkdtype;

        if(entries[i].visibility == 0){
            //TRACELOG(WGPU_LOG_WARNING, "Empty visibility detected, falling back to Vertex | Fragment | Compute mask");
            vkBindings.data[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
        }

        else{
            vkBindings.data[i].stageFlags = toVulkanShaderStageBits(entries[i].visibility);
        }
    }
    
    VkDescriptorSetLayoutCreateInfo slci = {
        .bindingCount = bgldesc->entryCount,
        .pBindings = vkBindings.data,
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    };

    VkResult createResult = device->functions.vkCreateDescriptorSetLayout(device->device, &slci, NULL, &ret->layout);
    if(createResult != VK_SUCCESS){
        RL_FREE(ret);
        return NULL;
    }
    WGPUBindGroupLayoutEntry* entriesCopy = (WGPUBindGroupLayoutEntry*)RL_CALLOC(entryCount, sizeof(WGPUBindGroupLayoutEntry));

    if(entryCount > 0){
        memcpy(entriesCopy, entries, entryCount * sizeof(WGPUBindGroupLayoutEntry));
    }
    ret->entries = entriesCopy;

    VkDescriptorSetLayoutBindingVector_free(&vkBindings);
    
    return ret;
    EXIT();
}
void wgpuPipelineLayoutRelease(WGPUPipelineLayout pllayout){
    ENTRY();
    if(!--pllayout->refCount){
        for(uint32_t i = 0;i < pllayout->bindGroupLayoutCount;i++){
            wgpuBindGroupLayoutRelease(pllayout->bindGroupLayouts[i]);
        }
        pllayout->device->functions.vkDestroyPipelineLayout(pllayout->device->device, pllayout->layout, NULL);
        RL_FREE((void*)pllayout->bindGroupLayouts);
        RL_FREE(pllayout);
    }
    EXIT();
}
WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device, const WGPUShaderModuleDescriptor* descriptor){
    ENTRY();
    WGPUShaderModule ret = RL_CALLOC(1, sizeof(WGPUShaderModuleImpl));
    ret->refCount = 1;
    ret->device = device;
    switch(descriptor->nextInChain->sType){
        case WGPUSType_ShaderSourceSPIRV:{
            const WGPUShaderSourceSPIRV* source = (WGPUShaderSourceSPIRV*)descriptor->nextInChain;
            VkShaderModuleCreateInfo sCreateInfo = {
                VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                NULL,
                0,
                source->codeSize * 4,
                source->code
            };
            device->functions.vkCreateShaderModule(device->device, &sCreateInfo, NULL, &ret->vulkanModuleMultiEP);
            ret->source = RL_CALLOC(1, sizeof(WGPUShaderSourceSPIRV));
            WGPUShaderSourceSPIRV* copySource = (WGPUShaderSourceSPIRV*)ret->source;
            copySource->chain.sType = WGPUSType_ShaderSourceSPIRV;
            copySource->code = RL_CALLOC(source->codeSize, 1);
            copySource->codeSize = source->codeSize;

            memcpy((void*)copySource->code, source->code, source->codeSize);
            ret->source = (WGPUChainedStruct*)copySource;
            return ret;
        }
        #if SUPPORT_WGSL == 1
        case WGPUSType_ShaderSourceWGSL: {
            const WGPUShaderSourceWGSL* source = (WGPUShaderSourceWGSL*)descriptor->nextInChain;
            size_t length = (source->code.length == WGPU_STRLEN) ? strlen(source->code.data) : source->code.length;
            
            tc_SpirvBlob blob = wgslToSpirv(source, 0, NULL);

            for(uint32_t i = 0;i < 16;i++){
                if(blob.entryPoints[i].codeSize){
                    VkShaderModuleCreateInfo sCreateInfo = {
                        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        NULL,
                        0,
                        blob.entryPoints[i].codeSize,
                        blob.entryPoints[i].code
                    };
                    device->functions.vkCreateShaderModule(device->device, &sCreateInfo, NULL, &ret->modules[i].module);
                    memcpy(ret->modules[i].epName, blob.entryPoints[i].entryPointName, 16);
                    RL_FREE(blob.entryPoints[i].code);
                }
            }

            WGPUShaderSourceWGSL* depot = RL_CALLOC(1, sizeof(WGPUShaderSourceWGSL));
            depot->chain.sType = WGPUSType_ShaderSourceWGSL;
            depot->code = CLITERAL(WGPUStringView){
                RL_CALLOC(length, 1),
                length
            };

            memcpy((void*)depot->code.data, source->code.data, length);
            
            ret->source = (WGPUChainedStruct*)depot;
            return ret;
        }
        #endif
        #if SUPPORT_GLSL == 1
        case WGPUSType_ShaderSourceGLSL: {
            return wgpuDeviceCreateShaderModuleGLSL(device, descriptor);
        }break;
        #endif
        default: {
            RL_FREE(ret);
            wgvk_assert(false, "Invalid shader source type");
            return NULL;
        }
    }
    EXIT();
}


WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device, const WGPUPipelineLayoutDescriptor* pldesc){
    ENTRY();
    WGPUPipelineLayout ret = RL_CALLOC(1, sizeof(WGPUPipelineLayoutImpl));
    ret->refCount = 1;
    wgvk_assert(ret->bindGroupLayoutCount <= 8, "Only supports up to 8 BindGroupLayouts");
    ret->device = device;
    ret->bindGroupLayoutCount = pldesc->bindGroupLayoutCount;
    ret->bindGroupLayouts = (WGPUBindGroupLayout*)RL_CALLOC(pldesc->bindGroupLayoutCount, sizeof(void*));
    if(pldesc->bindGroupLayoutCount > 0)
        memcpy((void*)ret->bindGroupLayouts, (void*)pldesc->bindGroupLayouts, pldesc->bindGroupLayoutCount * sizeof(void*));
    VkDescriptorSetLayout dslayouts[8] = {0};
    for(uint32_t i = 0;i < ret->bindGroupLayoutCount;i++){
        wgpuBindGroupLayoutAddRef(ret->bindGroupLayouts[i]);
        dslayouts[i] = ret->bindGroupLayouts[i]->layout;
    }
    VkPipelineLayoutCreateInfo lci = {0};
    lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lci.pSetLayouts = dslayouts;
    lci.setLayoutCount = ret->bindGroupLayoutCount;
    VkResult res = device->functions.vkCreatePipelineLayout(device->device, &lci, NULL, &ret->layout);
    if(res != VK_SUCCESS){
        wgpuPipelineLayoutRelease(ret);
        ret = NULL;
    }
    return ret;
    EXIT();
}


WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device, const WGPUCommandEncoderDescriptor* desc){
    ENTRY();
    WGPUCommandEncoder ret = RL_CALLOC(1, sizeof(WGPUCommandEncoderImpl));
    ret->cacheIndex = device->submittedFrames % framesInFlight;
    PerframeCache* pfcache = DeviceGetFIFCache(device, ret->cacheIndex);
    ret->device = device;
    ret->movedFrom = 0;
    //vkCreateCommandPool(device->device, &pci, NULL, &cache.commandPool);
    if(VkCommandBufferVector_empty(&pfcache->commandBuffers)){
        VkCommandBufferAllocateInfo bai = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pfcache->commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        device->functions.vkAllocateCommandBuffers(device->device, &bai, &ret->buffer);
        //printf("Allocating cb %p from pool %d\n", ret->buffer, (int)ret->cacheIndex);
    }
    else{
        ret->buffer = pfcache->commandBuffers.data[pfcache->commandBuffers.size - 1];
        //printf("Recycling cb %p from pool %d\n",  ret->buffer, (int)ret->cacheIndex);
        VkCommandBufferVector_pop_back(&pfcache->commandBuffers);
        //vkResetCommandBuffer(ret->buffer, 0);
    }

    const VkCommandBufferBeginInfo bbi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    
    device->functions.vkBeginCommandBuffer(ret->buffer, &bbi);
    
    return ret;
    EXIT();
}

static inline VkComponentSwizzle toVkSwizzleComponent(WGPUComponentSwizzle wgpuSwizzle){
    switch(wgpuSwizzle){
        case WGPUComponentSwizzle_Zero: return VK_COMPONENT_SWIZZLE_ZERO;
        case WGPUComponentSwizzle_One: return VK_COMPONENT_SWIZZLE_ONE;
        case WGPUComponentSwizzle_R: return VK_COMPONENT_SWIZZLE_R;
        case WGPUComponentSwizzle_G: return VK_COMPONENT_SWIZZLE_G;
        case WGPUComponentSwizzle_B: return VK_COMPONENT_SWIZZLE_B;
        case WGPUComponentSwizzle_A: return VK_COMPONENT_SWIZZLE_A;
        case WGPUComponentSwizzle_Undefined:
        case WGPUComponentSwizzle_Force32:
        rg_unreachable();
        wgvk_assert(false, "Invalid enum passed");
        abort();
    }
}
static inline WGPUTextureViewDimension toViewDim_(WGPUTextureDimension dim){
    if(dim == WGPUTextureDimension_1D)return WGPUTextureViewDimension_1D;
    if(dim == WGPUTextureDimension_2D)return WGPUTextureViewDimension_2D;
    if(dim == WGPUTextureDimension_3D)return WGPUTextureViewDimension_3D;
    return WGPUTextureViewDimension_Undefined;
}
WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor *descriptor){
    ENTRY();
    if(descriptor == NULL){
        WGPUTextureViewDescriptor tvDesc = {
            .format = fromVulkanPixelFormat(texture->format),
            .dimension = toViewDim_(fromVulkanTextureDimension(texture->dimension)),
            .baseMipLevel = 0,
            .mipLevelCount = texture->mipLevels,
            .baseArrayLayer = 0,
            .arrayLayerCount = texture->depthOrArrayLayers,
            .aspect = WGPUTextureAspect_All,
            .usage = texture->usage
        };
        return wgpuTextureCreateView(texture, &tvDesc);
    }
    VkComponentMapping swizzle = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY
    };
    if(descriptor->nextInChain && descriptor->nextInChain->sType == WGPUSType_TextureComponentSwizzleDescriptor){
        const WGPUTextureComponentSwizzleDescriptor* swDesc = (WGPUTextureComponentSwizzleDescriptor*)descriptor->nextInChain;
        swizzle.r = toVkSwizzleComponent(swDesc->swizzle.r);
        swizzle.g = toVkSwizzleComponent(swDesc->swizzle.g);
        swizzle.b = toVkSwizzleComponent(swDesc->swizzle.b);
        swizzle.a = toVkSwizzleComponent(swDesc->swizzle.a);
    }
    const VkImageViewCreateInfo ivci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .image = texture->image,
        .components = swizzle,
        .viewType = toVulkanTextureViewDimension(descriptor->dimension),
        .format = toVulkanPixelFormat(descriptor->format),
        .subresourceRange = {
            .aspectMask = toVulkanAspectMask(descriptor->aspect, descriptor->format),
            .baseMipLevel = descriptor->baseMipLevel,
            .levelCount = descriptor->mipLevelCount,
            .baseArrayLayer = descriptor->baseArrayLayer,
            .layerCount = descriptor->arrayLayerCount
        }
    };
    const SlimViewCreateInfo key = {
        .format = ivci.format,
        .subresourceRange = ivci.subresourceRange,
        .cmap = {
            .r = (SlimComponentSwizzle)ivci.components.r,
            .g = (SlimComponentSwizzle)ivci.components.g,
            .b = (SlimComponentSwizzle)ivci.components.b,
            .a = (SlimComponentSwizzle)ivci.components.a,
        },
        .viewType = ivci.viewType
    };
    WGPUTextureView* hit_pointer = Texture_ViewCache_get(&texture->viewCache, key);
    
    if(hit_pointer){
        WGPUTextureView hit = *hit_pointer;
        if(hit->refCount == 0){
            wgpuTextureAddRef(texture);
        }
        wgpuTextureViewAddRef(hit);
        return hit;
    }
    
    //if(!is__depthVk(ivci.format)){
    //    sr.aspectMask &= VK_IMAGE_ASPECT_COLOR_BIT;
    //}
    WGPUTextureView ret = RL_CALLOC(1, sizeof(WGPUTextureViewImpl));
    ret->refCount = 1;
    texture->device->functions.vkCreateImageView(texture->device->device, &ivci, NULL, &ret->view);
    ret->format = ivci.format;
    ret->texture = texture;
    ++texture->refCount;
    ret->width = texture->width;
    ret->height = texture->height;
    ret->sampleCount = texture->sampleCount;
    ret->depthOrArrayLayers = texture->depthOrArrayLayers;
    ret->subresourceRange = ivci.subresourceRange;
    
    Texture_ViewCache_put(&texture->viewCache, key, ret);
    return ret;
    EXIT();
}

uint32_t wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return texture->depthOrArrayLayers;
    EXIT();
}
WGPUTextureDimension wgpuTextureGetDimension(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return fromVulkanTextureDimension(texture->dimension);
    EXIT();
}
WGPUTextureFormat wgpuTextureGetFormat(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return fromVulkanPixelFormat(texture->format);
    EXIT();
}
uint32_t wgpuTextureGetHeight(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return texture->height;
    EXIT();
}
uint32_t wgpuTextureGetMipLevelCount(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return texture->mipLevels;
    EXIT();
}
uint32_t wgpuTextureGetSampleCount(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return texture->sampleCount;
    EXIT();
}
WGPUTextureUsage wgpuTextureGetUsage(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return fromVulkanWGPUTextureUsage(texture->usage);
    EXIT();
}
uint32_t wgpuTextureGetWidth(WGPUTexture texture) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    return texture->width;
    EXIT();
}

static inline VkClearValue toVkCV(const WGPUColor c){
    return (VkClearValue){
        .color.float32 = {
            (float)c.r,
            (float)c.g,
            (float)c.b,
            (float)c.a,
        }
    };
};

WGPURenderBundleEncoder wgpuDeviceCreateRenderBundleEncoder(WGPUDevice device, const WGPURenderBundleEncoderDescriptor* descriptor){
    ENTRY();
    WGPURenderBundleEncoder ret = RL_CALLOC(1, sizeof(WGPURenderBundleEncoderImpl));
    ret->refCount = 1;
    ret->device = device;

    ret->device = device;
    ret->movedFrom = 0;
    VkFormat* colorAttachmentFormats = RL_CALLOC(descriptor->colorFormatCount, sizeof(VkFormat));
    for(uint32_t i = 0;i < descriptor->colorFormatCount;i++){
        colorAttachmentFormats[i] = toVulkanPixelFormat(descriptor->colorFormats[i]);
    }
    ret->colorAttachmentCount = descriptor->colorFormatCount;
    ret->colorAttachmentFormats = colorAttachmentFormats;
    ret->depthStencilFormat = toVulkanPixelFormat(descriptor->depthStencilFormat);
    //if(VkCommandBufferVector_empty(&device->secondaryCommandBuffers)){
    //VkCommandBufferAllocateInfo bai = {
    //    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    //    .commandPool = device->secondaryCommandPool,
    //    .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
    //    .commandBufferCount = 1
    //};
    //device->functions.vkAllocateCommandBuffers(device->device, &bai, &ret->buffer);
    //}
    //else{
    //    ret->buffer = device->frameCaches[ret->cacheIndex].secondaryCommandBuffers.data[device->frameCaches[ret->cacheIndex].secondaryCommandBuffers.size - 1];
    //    VkCommandBufferVector_pop_back(&device->frameCaches[ret->cacheIndex].secondaryCommandBuffers);
    //}


    
    //device->functions.vkCmdSetViewport(ret->buffer, 0, 1, &viewPort);
    //device->functions.vkCmdSetScissor (ret->buffer, 0, 1, &scissor);
    
    //VkViewport dummy_viewport = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
    //VkRect2D dummy_scissor = { {0, 0}, {1, 1} };
    //vkCmdSetViewport(ret->buffer, 0, 1, &dummy_viewport);
    //vkCmdSetScissor (ret->buffer, 0, 1, &dummy_scissor);
    return ret;

    EXIT();
}
WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, WGPU_NULLABLE WGPURenderBundleDescriptor const * descriptor){
    ENTRY();
    WGPURenderBundle ret = RL_CALLOC(1, sizeof(WGPURenderBundleImpl));
    RenderPassCommandGenericVector_move(&ret->bufferedCommands, &renderBundleEncoder->bufferedCommands);
    renderBundleEncoder->movedFrom = 1;
    ret->device = renderBundleEncoder->device;
    ret->colorAttachmentFormats = renderBundleEncoder->colorAttachmentFormats;
    renderBundleEncoder->colorAttachmentFormats = NULL;
    ret->colorAttachmentCount = renderBundleEncoder->colorAttachmentCount;
    ret->depthStencilFormat = renderBundleEncoder->depthStencilFormat;
    //ret->device->functions.vkEndCommandBuffer(ret->commandBuffer);
    ret->refCount = 1;
    return ret;
    EXIT();
}


void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_draw,
        .draw = {
            vertexCount,
            instanceCount,
            firstVertex,
            firstInstance
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_draw_indexed,
        .drawIndexed = {
            indexCount,
            instanceCount,
            firstIndex,
            baseVertex,
            firstInstance
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_draw_indexed_indirect,
        .drawIndexedIndirect = {
            indirectBuffer,
            indirectOffset
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_draw_indirect,
        .drawIndirect = {
            indirectBuffer,
            indirectOffset
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPU_NULLABLE WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_set_bind_group,
        .setBindGroup = {
            groupIndex,
            group,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            dynamicOffsetCount,
            dynamicOffsets
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_set_index_buffer,
        .setIndexBuffer = {
            buffer,
            format,
            offset,
            size
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_set_render_pipeline,
        .setRenderPipeline = {
            pipeline
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPU_NULLABLE WGPUBuffer buffer, uint64_t offset, uint64_t size) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_set_vertex_buffer,
        .setVertexBuffer = {
            slot,
            buffer,
            offset
        }
    };
    RenderPassCommandGenericVector_push_back(&renderBundleEncoder->bufferedCommands, cmd);
    EXIT();
}

void wgpuRenderBundleEncoderAddRef(WGPURenderBundleEncoder renderBundleEncoder) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    ++renderBundleEncoder->refCount;

    EXIT();
}
void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    if(--renderBundleEncoder->refCount == 0){
        RL_FREE(renderBundleEncoder);
    }
    EXIT();
}















WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder enc, const WGPURenderPassDescriptor* rpdesc){
    ENTRY();
    WGPURenderPassEncoder ret = RL_CALLOC(1, sizeof(WGPURenderPassEncoderImpl));
    PerframeCache* frameCache = DeviceGetFIFCache(enc->device, enc->cacheIndex);
    VkCommandPool pool = frameCache->commandPool;

    ++enc->encodedCommandCount;
    ret->refCount = 2; //One for WGPURenderPassEncoder the other for the command buffer
    
    WGPURenderPassEncoderSet_add(&enc->referencedRPs, ret);
    //enc->referencedRPs.insert(ret);
    ret->device = enc->device;
    
    ret->cmdEncoder = enc;
    #if VULKAN_USE_DYNAMIC_RENDERING == 1
    //vkCmdBeginRendering(ret->cmdBuffer, &info);
    #else
    RenderPassLayout rplayout = GetRenderPassLayout(rpdesc);
    VkRenderPassBeginInfo rpbi zeroinit;
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    LayoutedRenderPass frp = LoadRenderPassFromLayout(enc->device, rplayout);
    ret->renderPass = frp.renderPass;

    VkImageView attachmentViews[2 * max_color_attachments + 2] = {0};// = (VkImageView* )RL_CALLOC(frp.allAttachments.size, sizeof(VkImageView) );
    VkClearValue clearValues   [2 * max_color_attachments + 2] = {0};// = (VkClearValue*)RL_CALLOC(frp.allAttachments.size, sizeof(VkClearValue));
    
    for(uint32_t i = 0;i < rplayout.colorAttachmentCount;i++){
        attachmentViews[i] = rpdesc->colorAttachments[i].view->view;
        clearValues[i] = toVkCV(rpdesc->colorAttachments[i].clearValue);
    }
    uint32_t insertIndex = rplayout.colorAttachmentCount;
    if(rpdesc->depthStencilAttachment){
        wgvk_assert(rplayout.depthAttachmentPresent, "renderpasslayout.depthAttachmentPresent != rpdesc->depthAttachment");
        clearValues[insertIndex].depthStencil.depth = rpdesc->depthStencilAttachment->depthClearValue;
        clearValues[insertIndex].depthStencil.stencil = rpdesc->depthStencilAttachment->stencilClearValue;
        attachmentViews[insertIndex++] = rpdesc->depthStencilAttachment->view->view;
    }
    
    if(rpdesc->colorAttachments[0].resolveTarget){
        for(uint32_t i = 0;i < rplayout.colorAttachmentCount;i++){
            wgvk_assert(rpdesc->colorAttachments[i].resolveTarget, "All must have resolve or none");
            clearValues[insertIndex] = toVkCV(rpdesc->colorAttachments[i].clearValue);
            attachmentViews[insertIndex++] = rpdesc->colorAttachments[i].resolveTarget->view;
        }
    }

    VkFramebufferCreateInfo fbci = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        ret->renderPass,
        frp.allAttachments.size,
        attachmentViews,
        rpdesc->colorAttachments[0].view->width,
        rpdesc->colorAttachments[0].view->height,
        1
    };
    fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbci.pAttachments = attachmentViews;
    fbci.attachmentCount = frp.allAttachments.size;

    fbci.width = rpdesc->colorAttachments[0].view->width;
    fbci.height = rpdesc->colorAttachments[0].view->height;
    fbci.layers = 1;
    
    fbci.renderPass = ret->renderPass;
    //VkResult fbresult = vkCreateFramebuffer(enc->device->device, &fbci, NULL, &ret->frameBuffer);
    //if(fbresult != VK_SUCCESS){
    //    TRACELOG(WGPU_LOG_FATAL, "Error creating framebuffer: %d", (int)fbresult);
    //}
    
    rpbi.renderPass = ret->renderPass;
    rpbi.renderArea = CLITERAL(VkRect2D){
        .offset = CLITERAL(VkOffset2D){0, 0},
        .extent = CLITERAL(VkExtent2D){rpdesc->colorAttachments[0].view->width, rpdesc->colorAttachments[0].view->height}
    };

    rpbi.framebuffer = ret->frameBuffer;
    
    
    rpbi.clearValueCount = frp.allAttachments.size;
    rpbi.pClearValues = clearValues;
    
    ret->cmdEncoder = enc;
    
    //vkCmdBeginRenderPass(ret->secondaryCmdBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    #endif
    ret->beginInfo = CLITERAL(RenderPassCommandBegin){
        .colorAttachmentCount = rpdesc->colorAttachmentCount,
        .depthAttachmentPresent = rpdesc->depthStencilAttachment != NULL
    };
    wgvk_assert(rpdesc->colorAttachmentCount <= max_color_attachments, "Too many colorattachments. supported=%d, provided=%d", ((int)MAX_COLOR_ATTACHMENTS), (int)rpdesc->colorAttachmentCount);
    memcpy(ret->beginInfo.colorAttachments, rpdesc->colorAttachments,rpdesc->colorAttachmentCount * sizeof(WGPURenderPassColorAttachment));
    if(rpdesc->depthStencilAttachment){
        ret->beginInfo.depthStencilAttachment = *rpdesc->depthStencilAttachment;
    }
    if(rpdesc->occlusionQuerySet){
        wgpuQuerySetAddRef(rpdesc->occlusionQuerySet);
        ret->beginInfo.occlusionQuerySet = rpdesc->occlusionQuerySet;
    }
    if(rpdesc->timestampWrites){
        ret->beginInfo.timestampWrites = *rpdesc->timestampWrites;
        ret->beginInfo.timestampWritesPresent = 1;
        wgpuQuerySetAddRef(ret->beginInfo.timestampWrites.querySet);
    }
    RenderPassCommandGenericVector_init(&ret->bufferedCommands);

    const ImageUsageSnap iur_color = {
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .subresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    const ImageUsageSnap iur_resolve = iur_color;
    
    const ImageUsageSnap iur_depth = {
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .subresource = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    
    for(uint32_t i = 0;i < rpdesc->colorAttachmentCount;i++){
        wgvk_assert(rpdesc->colorAttachments[i].view, "colorAttachments[%d].view is null", (int)i);
        ce_trackTextureView(enc, rpdesc->colorAttachments[i].view, iur_color);
    }

    for(uint32_t i = 0;i < rpdesc->colorAttachmentCount;i++){
        if(rpdesc->colorAttachments[i].resolveTarget){
            ce_trackTextureView(enc, rpdesc->colorAttachments[i].resolveTarget, iur_resolve);
        }
    }

    if(rpdesc->depthStencilAttachment){
        wgvk_assert(rpdesc->depthStencilAttachment->view, "depthStencilAttachment.view is null");
        ce_trackTextureView(enc, rpdesc->depthStencilAttachment->view, iur_depth);
    }
    //wgpuRenderPassEncoderSetViewport(ret, 0, 0, rpdesc->colorAttachments[0].view->width, rpdesc->colorAttachments[0].view->height, 0, 1);
    return ret;
    EXIT();
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder){
    ENTRY();
    
    WGPUDevice device = renderPassEncoder->device;
    VkCommandBuffer destination = renderPassEncoder->cmdEncoder->buffer;
    const size_t bufferSize = RenderPassCommandGenericVector_size(&renderPassEncoder->bufferedCommands);

    const RenderPassCommandBegin* beginInfo = &renderPassEncoder->beginInfo;

    

    VkImageView attachmentViews[2 * max_color_attachments + 2] = {0};// = (VkImageView* )RL_CALLOC(frp.allAttachments.size, sizeof(VkImageView) );
    VkClearValue clearValues   [2 * max_color_attachments + 2] = {0};// = (VkClearValue*)RL_CALLOC(frp.allAttachments.size, sizeof(VkClearValue));

    
    

    
    VkRect2D renderPassRect = {
        .offset = {0, 0},
        .extent = {
            beginInfo->colorAttachments[0].view->width, 
            beginInfo->colorAttachments[0].view->height
        }
    };

    

    


    for(size_t i = 0;i < renderPassEncoder->bufferedCommands.size;i++){
        const RenderPassCommandGeneric* cmd = &renderPassEncoder->bufferedCommands.data[i];
        if(cmd->type == rp_command_type_set_bind_group){
            const RenderPassCommandSetBindGroup* cmdSetBindGroup = &cmd->setBindGroup;
            const WGPUBindGroup       group  = cmdSetBindGroup->group;
            const WGPUBindGroupLayout layout = group->layout;
            for(uint32_t bindingIndex = 0;bindingIndex < layout->entryCount;bindingIndex++){

                wgvk_assert(group->entries[bindingIndex].binding == layout->entries[bindingIndex].binding, "Mismatch between layout and group, this will cause bugs.");
                
                const WGPUBindGroupEntry*       groupEntry  = &group ->entries[bindingIndex];
                const WGPUBindGroupLayoutEntry* layoutEntry = &layout->entries[bindingIndex];

                //uniform_type eType = layout->entries[bindingIndex].type;
                if(layout->entries[bindingIndex].buffer.type != WGPUBufferBindingType_BindingNotUsed){
                    wgvk_assert(group->entries[bindingIndex].buffer, "Layout indicates buffer but no buffer passed");
                    WGPUShaderStage visibility = layout->entries[bindingIndex].visibility;
                    wgvk_assert(visibility, "Empty visibility goddamnit");
                    ce_trackBuffer(
                        renderPassEncoder->cmdEncoder,
                        group->entries[bindingIndex].buffer,
                        (BufferUsageSnap){
                            .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            //.access = access_to_vk[layout->entries[bindingIndex].access], //TODO
                            .stage = toVulkanPipelineStageBits(visibility)
                        }
                    );
                }

                else if(layout->entries[bindingIndex].texture.sampleType != WGPUTextureSampleType_BindingNotUsed){
                    WGPUShaderStage visibility = layout->entries[bindingIndex].visibility;
                    wgvk_assert(visibility, "Empty visibility goddamnit");
                    if(visibility == 0){ //TODO: Get rid of this hack
                        visibility = (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute);
                    }
                    ce_trackTextureView(
                        renderPassEncoder->cmdEncoder,
                        group->entries[bindingIndex].textureView,
                        (ImageUsageSnap){
                            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            .access = VK_ACCESS_SHADER_READ_BIT,
                            .stage = toVulkanPipelineStageBits(visibility)
                        }
                    );
                }
                else if(layout->entries[bindingIndex].storageTexture.access != WGPUStorageTextureAccess_BindingNotUsed){
                    WGPUShaderStage visibility = layout->entries[bindingIndex].visibility;
                    wgvk_assert(visibility, "Empty visibility goddamnit");
                    if(visibility == 0){ //TODO: Get rid of this hack
                        visibility = (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute);
                    }
                    ce_trackTextureView(
                        renderPassEncoder->cmdEncoder,
                        group->entries[bindingIndex].textureView,
                        (ImageUsageSnap){
                            .layout = VK_IMAGE_LAYOUT_GENERAL,
                            .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            .stage = toVulkanPipelineStageBits(visibility)
                        }
                    );
                }
            }
        }
    }
    #if VULKAN_USE_DYNAMIC_RENDERING == 0
    RenderPassLayout rplayout = GetRenderPassLayout2(beginInfo);
    LayoutedRenderPass frp = LoadRenderPassFromLayout(renderPassEncoder->device, rplayout);
    VkRenderPass vkrenderPass = frp.renderPass;
    for(uint32_t i = 0;i < rplayout.colorAttachmentCount;i++){
        attachmentViews[i] =        renderPassEncoder->beginInfo.colorAttachments[i].view->view;
        clearValues[i]     = toVkCV(renderPassEncoder->beginInfo.colorAttachments[i].clearValue);
    }
    uint32_t insertIndex = rplayout.colorAttachmentCount;
    
    if(beginInfo->depthAttachmentPresent){
        clearValues[insertIndex].depthStencil.depth   = beginInfo->depthStencilAttachment.depthClearValue;
        clearValues[insertIndex].depthStencil.stencil = beginInfo->depthStencilAttachment.stencilClearValue;
        attachmentViews[insertIndex++]                = beginInfo->depthStencilAttachment.view->view;
    }
    
    if(beginInfo->colorAttachments[0].resolveTarget){
        for(uint32_t i = 0;i < rplayout.colorAttachmentCount;i++){
            wgvk_assert(beginInfo->colorAttachments[i].resolveTarget, "All must have resolve or none");
            clearValues[insertIndex] = toVkCV(beginInfo->colorAttachments[i].clearValue);
            attachmentViews[insertIndex++] =  beginInfo->colorAttachments[i].resolveTarget->view;
        }
    }
    const VkFramebufferCreateInfo fbci = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        NULL,
        0,
        vkrenderPass,
        frp.allAttachments.size,
        attachmentViews,
        beginInfo->colorAttachments[0].view->width,
        beginInfo->colorAttachments[0].view->height,
        1
    };

    device->functions.vkCreateFramebuffer(renderPassEncoder->device->device, &fbci, NULL, &renderPassEncoder->frameBuffer);
    const VkRenderPassBeginInfo rpbi = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = NULL,
        .renderPass = frp.renderPass,
        .framebuffer = renderPassEncoder->frameBuffer,
        .renderArea = renderPassRect,
        .clearValueCount = frp.allAttachments.size,
        .pClearValues = clearValues,
    };
    device->functions.vkCmdBeginRenderPass(destination, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    #else
    
    VkRenderingAttachmentInfo colorAttachments[max_color_attachments] zeroinit;
    for(uint32_t i = 0;i < beginInfo->colorAttachmentCount;i++){
        colorAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachments[i].clearValue.color.float32[0] = (float)beginInfo->colorAttachments[i].clearValue.r;
        colorAttachments[i].clearValue.color.float32[1] = (float)beginInfo->colorAttachments[i].clearValue.g;
        colorAttachments[i].clearValue.color.float32[2] = (float)beginInfo->colorAttachments[i].clearValue.b;
        colorAttachments[i].clearValue.color.float32[3] = (float)beginInfo->colorAttachments[i].clearValue.a;
        colorAttachments[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachments[i].imageView = beginInfo->colorAttachments[i].view->view;
        if(beginInfo->colorAttachments[i].resolveTarget){
            colorAttachments[i].resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachments[i].resolveImageView = beginInfo->colorAttachments[i].resolveTarget->view;
            colorAttachments[i].resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        }
        colorAttachments[i].loadOp = toVulkanLoadOperation(beginInfo->colorAttachments[i].loadOp);
        colorAttachments[i].storeOp = toVulkanStoreOperation(beginInfo->colorAttachments[i].storeOp);
    }

    const VkRenderingInfo info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        //.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT | VK_RENDERING_CONTENTS_INLINE_BIT_KHR,
        .colorAttachmentCount = beginInfo->colorAttachmentCount,
        .pColorAttachments = colorAttachments,
        .pDepthAttachment = beginInfo->depthAttachmentPresent ? &(const VkRenderingAttachmentInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .clearValue.depthStencil.depth = beginInfo->depthStencilAttachment.depthClearValue,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .imageView = beginInfo->depthStencilAttachment.view->view,
            .loadOp = toVulkanLoadOperation(beginInfo->depthStencilAttachment.depthLoadOp),
            .storeOp = toVulkanStoreOperation(beginInfo->depthStencilAttachment.depthStoreOp),
        } : NULL,
        .layerCount = 1,
        .renderArea = CLITERAL(VkRect2D){
            .offset = CLITERAL(VkOffset2D){0, 0},
            .extent = CLITERAL(VkExtent2D){beginInfo->colorAttachments[0].view->width, beginInfo->colorAttachments[0].view->height}
        }
    };
    device->functions.vkCmdBeginRendering(destination, &info);
    #endif
    float ones[4] = {1,1,1,1};
    device->functions.vkCmdSetBlendConstants(destination, ones);
    const float vpWidth = (float)beginInfo->colorAttachments[0].view->width;
    const float vpHeight = (float)beginInfo->colorAttachments[0].view->height;
    
    const VkViewport viewport = {
        .x        = 0,
        .y        = vpHeight,
        .width    = vpWidth,
        .height   = -vpHeight,
        .minDepth = 0,
        .maxDepth = 1,
    };

    const VkRect2D scissor = {
        .offset = {
            .x = 0,
            .y = 0,
        },
        .extent = {
            .width = vpWidth,
            .height = vpHeight,
        }
    };
    for(uint32_t i = 0;i < beginInfo->colorAttachmentCount;i++){
        device->functions.vkCmdSetViewport(destination, i, 1, &viewport);
        device->functions.vkCmdSetScissor (destination, i, 1, &scissor);
    }
    recordVkCommands(renderPassEncoder->cmdEncoder, renderPassEncoder->device, &renderPassEncoder->bufferedCommands, beginInfo);
    if(beginInfo->occlusionQuerySet){
        wgpuQuerySetRelease(beginInfo->occlusionQuerySet);
    }
    if(beginInfo->timestampWritesPresent){
        wgpuQuerySetRelease(beginInfo->timestampWrites.querySet);
    }

    //for(uint32_t i = 0;i < beginInfo->colorAttachmentCount;i++){
    //    wgvk_assert(beginInfo->colorAttachments[i].view, "colorAttachments[%d].view is null", (int)i);
    //    ce_trackTextureView(destination, rpdesc->colorAttachments[i].view, iur_color);
    //}
//
    //for(uint32_t i = 0;i < rpdesc->colorAttachmentCount;i++){
    //    if(rpdesc->colorAttachments[i].resolveTarget){
    //        ce_trackTextureView(enc, rpdesc->colorAttachments[i].resolveTarget, iur_resolve);
    //    }
    //}

    #if VULKAN_USE_DYNAMIC_RENDERING == 1
    device->functions.vkCmdEndRendering(destination);
    #else
    device->functions.vkCmdEndRenderPass(destination);
    #endif
    EXIT();
}
/**
 * @brief Ends a CommandEncoder into a CommandBuffer
 * @details This is a one-way transition for WebGPU, therefore we can move resource tracking
 * In Vulkan, this transition is merely a call to vkEndCommandBuffer.
 * 
 * The rest of this function just moves data from the Encoder struct into the buffer. 
 * 
 */
WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder commandEncoder, const WGPUCommandBufferDescriptor* bufferdesc){
    ENTRY();
    
    WGPUCommandBuffer ret = RL_CALLOC(1, sizeof(WGPUCommandBufferImpl));
    ret->refCount = 1;
    wgvk_assert(commandEncoder->movedFrom == 0, "Command encoder is already invalidated");
    commandEncoder->movedFrom = 1;
    commandEncoder->device->functions.vkEndCommandBuffer(commandEncoder->buffer);

    WGPURenderPassEncoderSet_move(&ret->referencedRPs, &commandEncoder->referencedRPs);
    WGPUComputePassEncoderSet_move(&ret->referencedCPs, &commandEncoder->referencedCPs);
    WGPURaytracingPassEncoderSet_move(&ret->referencedRTs, &commandEncoder->referencedRTs);
    ResourceUsage_move(&ret->resourceUsage, &commandEncoder->resourceUsage);
    ret->cacheIndex = commandEncoder->cacheIndex;
    ret->buffer = commandEncoder->buffer;
    ret->device = commandEncoder->device;
    commandEncoder->buffer = NULL;
    
    if(bufferdesc){
        ret->label = WGPUStringFromView(bufferdesc->label);
    }

    return ret;
    EXIT();
}

void recordVkCommand(CommandBufferAndSomeState* destination_, const RenderPassCommandGeneric* command, const RenderPassCommandBegin *beginInfo){
    VkCommandBuffer destinationVk = destination_->buffer;
    WGPUDevice device = destination_->device;
    switch(command->type){
        case rp_command_type_draw_indexed_indirect:{
            const RenderPassCommandDrawIndexedIndirect* drawIndexedIndirect = &command->drawIndexedIndirect;
            device->functions.vkCmdDrawIndexedIndirect(
                destinationVk,
                drawIndexedIndirect->indirectBuffer->buffer,
                drawIndexedIndirect->indirectOffset,
                1,
                20 // sizeof(VkDrawIndexedIndirectCommand) but irrelefant
            );
        }
        break;

        case rp_command_type_draw_indirect:{
            
            const RenderPassCommandDrawIndirect* drawIndirect = &command->drawIndirect;
            device->functions.vkCmdDrawIndirect(
                destinationVk,
                drawIndirect->indirectBuffer->buffer,
                drawIndirect->indirectOffset,
                1,
                16 // sizeof(VkDrawIndirectCommand) but irrelefant
            );
        }
        break;
        case rp_command_type_set_stencil_reference: {
            const RenderPassCommandSetStencilReference* setStencilReference = &command->setStencilReference;
            //device->functions.vkCmdSetStencilReference(
            //    destinationVk,
            //    VK_STENCIL_FACE_FRONT_BIT | VK_STENCIL_FACE_BACK_BIT,
            //    setStencilReference->reference
            //);
        }break;
        case rp_command_type_set_blend_constant:{
            const RenderPassCommandSetBlendConstant* setBlendConstant = &command->setBlendConstant;
            const float buffer[4] = {
                (float)setBlendConstant->color.r,
                (float)setBlendConstant->color.g,
                (float)setBlendConstant->color.b,
                (float)setBlendConstant->color.a,
            };
            device->functions.vkCmdSetBlendConstants(
                destinationVk,
                buffer
            );
        }
        break;
        case rp_command_type_set_viewport:{
            const RenderPassCommandSetViewport* vp = &command->setViewport;
            destination_->dynamicState.viewport = (VkViewport){vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth};
            const VkViewport viewport[8] = {
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
                {vp->x, vp->y, vp->width, vp->height, vp->minDepth, vp->maxDepth},
            };
            device->functions.vkCmdSetViewport(destinationVk, 0, beginInfo->colorAttachmentCount, viewport);
        }break;
        case rp_command_type_set_scissor_rect:{
            const RenderPassCommandSetScissorRect* sr = &command->setScissorRect;
            destination_->dynamicState.scissorRect = (VkRect2D){{sr->x, sr->y}, {sr->width, sr->height}};
            const VkRect2D scissors[8] = {
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
                {{sr->x, sr->y}, {sr->width, sr->height}},
            };
            device->functions.vkCmdSetScissor(destinationVk, 0, beginInfo->colorAttachmentCount, scissors);
        }break;

        case rp_command_type_draw: {
            const RenderPassCommandDraw* draw = &command->draw;
            device->functions.vkCmdDraw(
                destinationVk, 
                draw->vertexCount,
                draw->instanceCount,
                draw->firstVertex,
                draw->firstInstance
            );
        }
        break;
        case rp_command_type_draw_indexed: {
            const RenderPassCommandDrawIndexed* drawIndexed = &command->drawIndexed;
            device->functions.vkCmdDrawIndexed(
                destinationVk,
                drawIndexed->indexCount,
                drawIndexed->instanceCount,
                drawIndexed->firstIndex,
                drawIndexed->baseVertex,
                drawIndexed->firstInstance
            );
        }
        break;
        case rp_command_type_set_vertex_buffer: {
            const RenderPassCommandSetVertexBuffer* setVertexBuffer = &command->setVertexBuffer;
            device->functions.vkCmdBindVertexBuffers(
                destinationVk,
                setVertexBuffer->slot,
                1,
                &setVertexBuffer->buffer->buffer,
                &setVertexBuffer->offset
            );
        }
        break;
        case rp_command_type_set_index_buffer: {
            const RenderPassCommandSetIndexBuffer* setIndexBuffer = &command->setIndexBuffer;
            device->functions.vkCmdBindIndexBuffer(
                destinationVk,
                setIndexBuffer->buffer->buffer,
                setIndexBuffer->offset,
                toVulkanIndexFormat(setIndexBuffer->format)
            );
        }
        break;
        case rp_command_type_set_bind_group: {
            const RenderPassCommandSetBindGroup* setBindGroup = &command->setBindGroup;
            if(setBindGroup->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
                destination_->graphicsBindGroups[setBindGroup->groupIndex] = setBindGroup->group;
            else
                destination_->computeBindGroups[setBindGroup->groupIndex] = setBindGroup->group;
            if(destination_->lastLayout){
                device->functions.vkCmdBindDescriptorSets(
                    destinationVk,
                    setBindGroup->bindPoint,
                    destination_->lastLayout,
                    setBindGroup->groupIndex,
                    1,
                    &setBindGroup->group->set,
                    setBindGroup->dynamicOffsetCount,
                    setBindGroup->dynamicOffsets
                );
            }
        }
        break;
        case rp_command_type_set_render_pipeline: {
            const RenderPassCommandSetPipeline* setRenderPipeline = &command->setRenderPipeline;
            device->functions.vkCmdBindPipeline(
                destinationVk,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                setRenderPipeline->pipeline->renderPipeline
            );
            destination_->lastLayout = setRenderPipeline->pipeline->layout->layout;
        }
        break;
        case rp_command_type_set_raytracing_pipeline: {
            const RenderPassCommandSetRaytracingPipeline* setRaytracingPipeline = &command->setRaytracingPipeline;
            device->functions.vkCmdBindPipeline(
                destinationVk,
                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                setRaytracingPipeline->pipeline->raytracingPipeline
            );
            destination_->lastLayout = setRaytracingPipeline->pipeline->layout->layout;
            destination_->lastRaytracingPipeline = setRaytracingPipeline->pipeline;
        }
        break;
        case rt_command_type_trace_rays: {
            const RaytracingPassCommandTraceRays* traceRays = &command->traceRays;
                
            WGPURaytracingPipeline pipeline = destination_->lastRaytracingPipeline;
            wgvk_assert(pipeline != NULL, "vkCmdTraceRaysKHR called without a bound ray tracing pipeline.");
                
            WGPUBuffer sbtBuffer = pipeline->sbtBuffer;
            VkDeviceSize totalSbtSize = pipeline->totalSbtSize;
            wgvk_assert(sbtBuffer != NULL, "Ray tracing pipeline does not have a valid SBT buffer.");
                
            // Get properties to calculate the stride between handles.
            const VkPhysicalDeviceRayTracingPipelinePropertiesKHR* rtProperties = &device->adapter->rayTracingPipelineProperties;
            const uint32_t handleSize = rtProperties->shaderGroupHandleSize;
            const uint32_t handleAlignment = rtProperties->shaderGroupBaseAlignment;

            // handleStride is usually 32
            const VkDeviceSize handleStride = (handleSize + (handleAlignment - 1)) & ~(handleAlignment - 1);
                
            const VkDeviceAddress sbtBaseAddress = sbtBuffer->address;

            char groupDump_[32 * 3] = {0};
            void* groupDump = groupDump_;
            device->functions.vkGetRayTracingShaderGroupHandlesKHR(device->device, pipeline->raytracingPipeline, 0, 3, 32 * 3, groupDump);

            VkDeviceSize rayGenRegionSize = traceRays->rayMissOffset - traceRays->rayGenerationOffset;    
            VkDeviceSize missRegionSize = traceRays->rayHitOffset - traceRays->rayMissOffset;
            VkDeviceSize hitRegionSize = totalSbtSize - traceRays->rayHitOffset;
                
                
            const VkStridedDeviceAddressRegionKHR raygenSbtRegion = {
                .deviceAddress = sbtBaseAddress + traceRays->rayGenerationOffset * handleStride,
                .stride        = handleStride,
                .size          = handleStride
            };
        
            const VkStridedDeviceAddressRegionKHR missSbtRegion = {
                .deviceAddress = sbtBaseAddress + traceRays->rayMissOffset * handleStride,
                .stride        = handleStride,
                .size          = handleStride
            };
        
            const VkStridedDeviceAddressRegionKHR hitSbtRegion = {
                .deviceAddress = sbtBaseAddress + traceRays->rayHitOffset * handleStride,
                .stride        = handleStride,
                .size          = handleStride
            };
            
            const VkStridedDeviceAddressRegionKHR callableSbtRegion = { .deviceAddress = 0, .stride = 0, .size = 0 };
        
            device->functions.vkCmdTraceRaysKHR(
                destinationVk,
                &raygenSbtRegion,
                &missSbtRegion,
                &hitSbtRegion,
                &callableSbtRegion,
                traceRays->width,
                traceRays->height,
                traceRays->depth
            );
        }
        break;
        case rp_command_type_execute_renderbundle:{
            const RenderPassCommandExecuteRenderbundles* executeRenderBundles = &command->executeRenderBundles;
            WGPURenderBundle bundle = executeRenderBundles->renderBundle;
            DefaultDynamicState ds = destination_->dynamicState;
            VkCommandBuffer* maybeBuffer = DynamicStateCommandBufferMap_get(&bundle->encodedCommandBuffers, ds);
            VkCommandBuffer executedBuffer;
            #if RENDERBUNDLES_AS_SECONDARY_COMMANDBUFFERS == 1
            if(maybeBuffer){
                executedBuffer = *maybeBuffer;
            }
            else{
                VkCommandBufferAllocateInfo bai = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = device->secondaryCommandPool,
                    .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                    .commandBufferCount = 1
                };
                device->functions.vkAllocateCommandBuffers(device->device, &bai, &executedBuffer);
                
                VkCommandBufferInheritanceRenderingInfo renderingInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
                    .colorAttachmentCount = bundle->colorAttachmentCount,
                    .pColorAttachmentFormats = bundle->colorAttachmentFormats,
                    .depthAttachmentFormat = bundle->depthStencilFormat,
                    .stencilAttachmentFormat = bundle->depthStencilFormat,
                    .rasterizationSamples = 1 // todo
                };
            
                VkCommandBufferInheritanceInfo inheritanceInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
                    .pNext = &renderingInfo,
                };
            
                VkCommandBufferBeginInfo beginInfo = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                    .pInheritanceInfo = &inheritanceInfo
                };
                device->functions.vkBeginCommandBuffer(executedBuffer, &beginInfo);
                RenderPassCommandBegin dummyBeginInfo = {
                    .colorAttachmentCount = bundle->colorAttachmentCount
                };
                
                for(uint32_t ai = 0;ai < bundle->colorAttachmentCount;ai++){
                    device->functions.vkCmdSetViewport(executedBuffer, 0, 1, &ds.viewport   );
                    if(ds.scissorRect.extent.width != UINT32_MAX){
                        device->functions.vkCmdSetScissor (executedBuffer, 0, 1, &ds.scissorRect);
                    }
                    else{
                        VkRect2D defaultRect = {
                            .offset = {0, 0},
                            .extent = {ds.viewport.width, ds.viewport.height}
                        };
                        device->functions.vkCmdSetScissor (executedBuffer, 0, 1, &defaultRect);
                    }
                }
                recordVkCommands(executedBuffer, device, &bundle->bufferedCommands, &dummyBeginInfo);
                device->functions.vkEndCommandBuffer(executedBuffer);
                DynamicStateCommandBufferMap_put(&bundle->encodedCommandBuffers, ds, executedBuffer);
            }
            device->functions.vkCmdExecuteCommands(destination, 1, &executedBuffer);
            #else
            RenderPassCommandBegin dummyBeginInfo = {
                .colorAttachmentCount = bundle->colorAttachmentCount
            };
            recordVkCommands(destination_->cmdEncoder, device, &bundle->bufferedCommands, &dummyBeginInfo);
            #endif
        }break;
        case cp_command_type_set_compute_pipeline: {
            const ComputePassCommandSetPipeline* setComputePipeline = &command->setComputePipeline;
            memset((void*)destination_->computeBindGroups, 0, sizeof(destination_->computeBindGroups));
            device->functions.vkCmdBindPipeline(
                destinationVk,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                setComputePipeline->pipeline->computePipeline
            );

            destination_->lastLayout = setComputePipeline->pipeline->layout->layout;
        }
        break;
        case cp_command_type_dispatch_workgroups: {
            const ComputePassCommandDispatchWorkgroups* dispatch = &command->dispatchWorkgroups;
            //ce_trackBuffer(WGPUCommandEncoder encoder, WGPUBuffer buffer, BufferUsageSnap usage)
            for(uint32_t groupIndex = 0;groupIndex < 8;groupIndex++){
                if(destination_->computeBindGroups[groupIndex]){
                    const WGPUBindGroup group = destination_->computeBindGroups[groupIndex];
                    for(uint32_t entryIndex = 0;entryIndex < group->entryCount;entryIndex++){
                        const WGPUBindGroupEntry* entry = group->entries + entryIndex;
                        uint32_t bglEntryIndex = 0;
                        for(;bglEntryIndex < group->layout->entryCount;bglEntryIndex++){
                            if(group->layout->entries[bglEntryIndex].binding == entry->binding)break;
                        }
                        if(entry->buffer){
                            ce_trackBuffer(destination_->cmdEncoder, entry->buffer, (BufferUsageSnap){
                                .access = extractVkAccessFlags(group->layout->entries + bglEntryIndex),
                                .stage  = toVulkanPipelineStageBits(group->layout->entries[bglEntryIndex].visibility)
                            });
                        }
                    }
                }
            }
            device->functions.vkCmdDispatch(
                destinationVk, 
                dispatch->x, 
                dispatch->y, 
                dispatch->z
            );
            
        }
        break;
        case cp_command_type_dispatch_workgroups_indirect:{
            for(uint32_t groupIndex = 0;groupIndex < 8;groupIndex++){
                if(destination_->computeBindGroups[groupIndex]){
                    const WGPUBindGroup group = destination_->computeBindGroups[groupIndex];
                    for(uint32_t entryIndex = 0;entryIndex < group->entryCount;entryIndex++){
                        const WGPUBindGroupEntry* entry = group->entries + entryIndex;
                        uint32_t bglEntryIndex = 0;
                        for(;bglEntryIndex < group->layout->entryCount;bglEntryIndex++){
                            if(group->layout->entries[bglEntryIndex].binding == entry->binding)break;
                        }
                        if(entry->buffer){
                            ce_trackBuffer(destination_->cmdEncoder, entry->buffer, (BufferUsageSnap){
                                .access = extractVkAccessFlags(group->layout->entries + bglEntryIndex),
                                .stage  = toVulkanPipelineStageBits(group->layout->entries[bglEntryIndex].visibility)
                            });
                        }
                    }
                }
            }
            
            const ComputePassCommandDispatchWorkgroupsIndirect* dispatch = &command->dispatchWorkgroupsIndirect;

            ce_trackBuffer(destination_->cmdEncoder, dispatch->buffer, (BufferUsageSnap){
                .access = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                .stage  = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
            });
            
            device->functions.vkCmdDispatchIndirect(
                destinationVk,
                dispatch->buffer->buffer,
                dispatch->offset
            );
        }break;
        case rp_command_type_begin_occlusion_query:{
            const RenderPassCommandBeginOcclusionQuery* beginOcclusionQuery = &command->beginOcclusionQuery;
            if(beginInfo->occlusionQuerySet){
                destination_->activeOcclusionQueryIndex = beginOcclusionQuery->queryIndex;
                device->functions.vkCmdBeginQuery(
                    destinationVk,
                    beginInfo->occlusionQuerySet->queryPool,
                    beginOcclusionQuery->queryIndex,
                    0 // The only flag Vulkan has is VK_QUERY_CONTROL_PRECISE_BIT
                    // But WebGPU doesn't offer any way to expose this
                );
            }
        }break;
        case rp_command_type_end_occlusion_query:{
            if(beginInfo->occlusionQuerySet){
                device->functions.vkCmdEndQuery(
                    destinationVk,
                    beginInfo->occlusionQuerySet->queryPool,
                    destination_->activeOcclusionQueryIndex
                );
            }
        }break;
        case rp_command_type_insert_debug_marker:{

        }break;
        case rp_command_type_multi_draw_indexed_indirect:{

        }break;
        case rp_command_type_multi_draw_indirect:{

        }break;
    
        case rp_command_type_set_force32: // fallthrough
        case rp_command_type_enum_count:  // fallthrough
        case rp_command_type_invalid: wgvk_assert(false, "Invalid command type"); rg_unreachable();
    }
}

void recordVkCommands(WGPUCommandEncoder destination, WGPUDevice device, const RenderPassCommandGenericVector* commands, const RenderPassCommandBegin WGPU_NULLABLE *beginInfo){
    CommandBufferAndSomeState cal = {
        .cmdEncoder = destination,
        .buffer = destination->buffer,
        .device = device,
        .lastLayout = VK_NULL_HANDLE,
        .dynamicState.scissorRect = {
            .offset = {UINT32_MAX, UINT32_MAX},
            .extent = {UINT32_MAX, UINT32_MAX},
        }
    };

    for(size_t i = 0;i < commands->size;i++){
        const RenderPassCommandGeneric* cmd = RenderPassCommandGenericVector_get((RenderPassCommandGenericVector*)commands, i);
        recordVkCommand(&cal, cmd, beginInfo);
    }
}

void registerTransitionCallback(void* texture_, ImageUsageRecord* record, void* pscache_){
    WGPUTexture texture = (WGPUTexture)texture_;
    WGPUDevice device = texture->device;
    WGPUCommandEncoder pscache = (WGPUCommandEncoder)pscache_;
    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        texture->layout,
        record->initialLayout,
        pscache->device->adapter->queueIndices.graphicsIndex,
        pscache->device->adapter->queueIndices.graphicsIndex,
        texture->image,
        (VkImageSubresourceRange){
            is__depthVk(texture->format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            VK_REMAINING_MIP_LEVELS,
            0,
            VK_REMAINING_ARRAY_LAYERS
        }
    };
    device->functions.vkCmdPipelineBarrier(
        pscache->buffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    //ce_trackTexture(pscache, texture, artificial);
}


DEFINE_VECTOR(static inline, VkBufferMemoryBarrier, VkBufferMemoryBarrierVector)
DEFINE_VECTOR(static inline, VkMemoryBarrier, VkMemoryBarrierVector)
DEFINE_VECTOR(static inline, VkImageMemoryBarrier, VkImageMemoryBarrierVector)
typedef struct CmdBarrierSet{
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    VkBufferMemoryBarrierVector bufferBarriers;
    VkMemoryBarrierVector memoryBarriers;
    VkImageMemoryBarrierVector imageBarriers;
}CmdBarrierSet;

static void CmdBarrierSet_init(CmdBarrierSet* set){
    VkBufferMemoryBarrierVector_init(&set->bufferBarriers);
    VkMemoryBarrierVector_init(&set->memoryBarriers);
    VkImageMemoryBarrierVector_init(&set->imageBarriers);
}

static void CmdBarrierSet_free(CmdBarrierSet* set){
    VkBufferMemoryBarrierVector_free(&set->bufferBarriers);
    VkMemoryBarrierVector_free(&set->memoryBarriers);
    VkImageMemoryBarrierVector_free(&set->imageBarriers);
}

static void CmdBarrierSet_encode(WGPUCommandEncoder encoder, CmdBarrierSet* set){
    struct VolkDeviceTable* functions = &encoder->device->functions;
    functions->vkCmdPipelineBarrier(
        encoder->buffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        set->memoryBarriers.size, set->memoryBarriers.data,
        set->bufferBarriers.size, set->bufferBarriers.data,
        set->imageBarriers.size, set->imageBarriers.data
    );
}

static CmdBarrierSet GetCompatibilityBarriers(WGPUCommandBuffer srcBuffer, WGPUCommandBuffer dstBuffer){
    CmdBarrierSet ret;
    CmdBarrierSet_init(&ret);
    for(size_t i = 0;i < srcBuffer->resourceUsage.referencedTextures.current_capacity;i++){
        const ImageUsageRecordMap_kv_pair* srcPair = srcBuffer->resourceUsage.referencedTextures.table + i;
        const ImageUsageRecord* dstValue = ImageUsageRecordMap_get(&dstBuffer->resourceUsage.referencedTextures, srcPair->key);
        if(dstValue){
            VkImageMemoryBarrier insert = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .image = ((WGPUTexture)srcPair->key)->image,
                .srcQueueFamilyIndex = srcBuffer->device->adapter->queueIndices.graphicsIndex,
                .dstQueueFamilyIndex = srcBuffer->device->adapter->queueIndices.graphicsIndex,
                .oldLayout = srcPair->value.lastLayout,
                .newLayout = dstValue->initialLayout,
                .srcAccessMask = srcPair->value.lastAccess,
                .dstAccessMask = dstValue->initialAccess,
                .subresourceRange = dstValue->initiallyAccessedSubresource
            };
            VkImageMemoryBarrierVector_push_back(&ret.imageBarriers, insert);
        }
    }
    for(size_t i = 0;i < srcBuffer->resourceUsage.referencedBuffers.current_capacity;i++){
        const BufferUsageRecordMap_kv_pair* srcPair = srcBuffer->resourceUsage.referencedBuffers.table + i;
        const BufferUsageRecord* dstValue = BufferUsageRecordMap_get(&dstBuffer->resourceUsage.referencedBuffers, srcPair->key);
        if(dstValue){
            VkBufferMemoryBarrier insert = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .buffer = ((WGPUBuffer)srcPair->key)->buffer,
                .offset = 0,
                .size = ((WGPUBuffer)srcPair->key)->capacity,
                .srcQueueFamilyIndex = srcBuffer->device->adapter->queueIndices.graphicsIndex,
                .dstQueueFamilyIndex = srcBuffer->device->adapter->queueIndices.graphicsIndex,
                .srcAccessMask = srcPair->value.lastAccess,
                .dstAccessMask = dstValue->initialAccess
            };
            VkBufferMemoryBarrierVector_push_back(&ret.bufferBarriers, insert);
        }
    }

    return ret;
}
void generateInterspersedCompatibilityBarriers(WGPUCommandBuffer* buffers, uint32_t bufferCount, CmdBarrierSet* barrierSets){
    ImageUsageRecordMap referencedImages;
    BufferUsageRecordMap referencedBuffers;
    if(bufferCount == 0)return;
    WGPUDevice device = buffers[0]->device;
    ImageUsageRecordMap_init (&referencedImages);
    BufferUsageRecordMap_init(&referencedBuffers);

    for(uint32_t bufferIndex = 0;bufferIndex < bufferCount;bufferIndex++){
        ImageUsageRecordMap* imageUsage = &buffers[bufferIndex]->resourceUsage.referencedTextures;
        for(size_t i = 0;i < imageUsage->current_capacity;i++){
            const ImageUsageRecordMap_kv_pair* kvp = imageUsage->table + i;
            if(kvp->key != PHM_EMPTY_SLOT_KEY){
                WGPUTexture tex = (WGPUTexture)kvp->key;
                VkImageLayout srcLayout;
                VkAccessFlags srcAccess = 0;
                VkPipelineStageFlags srcStage;
                ImageUsageRecord* knowledge = ImageUsageRecordMap_get(&referencedImages, tex);
                if(knowledge){
                    srcLayout = knowledge->lastLayout;
                    srcAccess = knowledge->lastAccess;
                    srcStage  = knowledge->lastStage;
                }
                else{
                    srcLayout = tex->layout;
                    srcStage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                    srcAccess = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                }
                VkImageMemoryBarrier imageBarrier = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .image = tex->image,
                    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstAccessMask = kvp->value.initialAccess,
                    .oldLayout = srcLayout,
                    .newLayout = kvp->value.initialLayout,
                    .srcQueueFamilyIndex = device->adapter->queueIndices.graphicsIndex,
                    .dstQueueFamilyIndex = device->adapter->queueIndices.graphicsIndex,
                    .subresourceRange = kvp->value.initiallyAccessedSubresource
                };
                barrierSets[bufferIndex].srcStage |= srcStage;
                barrierSets[bufferIndex].dstStage |= kvp->value.initialStage;
                VkImageMemoryBarrierVector_push_back(&barrierSets[bufferIndex].imageBarriers, imageBarrier);
                if(knowledge){ 
                    knowledge->lastAccess              = kvp->value.lastAccess;
                    knowledge->lastStage               = kvp->value.lastStage;
                    knowledge->lastLayout              = kvp->value.lastLayout;
                    knowledge->lastAccessedSubresource = kvp->value.lastAccessedSubresource;
                }
                else{
                    ImageUsageRecord newRecord = {
                        .initialAccess = kvp->value.lastAccess,
                        .lastAccess = kvp->value.lastAccess,
                        .initiallyAccessedSubresource = kvp->value.lastAccessedSubresource,
                        .lastAccessedSubresource = kvp->value.lastAccessedSubresource,
                        .initialLayout = kvp->value.lastLayout,
                        .lastLayout = kvp->value.lastLayout,
                        .initialStage = kvp->value.lastStage,
                        .lastStage = kvp->value.lastStage
                    };
                    ImageUsageRecordMap_put(&referencedImages, tex, newRecord);
                }
            }
        }
        BufferUsageRecordMap* bufferUsage = &buffers[bufferIndex]->resourceUsage.referencedBuffers;
        for(size_t i = 0;i < bufferUsage->current_capacity;i++){
            const BufferUsageRecordMap_kv_pair* kvp = bufferUsage->table + i;
            if(kvp->key != PHM_EMPTY_SLOT_KEY){
                WGPUBuffer buf = (WGPUBuffer)kvp->key;
                VkAccessFlags srcAccess = 0;
                VkPipelineStageFlags srcStage;
                BufferUsageRecord* knowledge = BufferUsageRecordMap_get(&referencedBuffers, buf);
                if(knowledge){
                    srcAccess = knowledge->lastAccess;
                    srcStage  = knowledge->lastStage;
                }
                else{
                    srcStage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                    srcAccess = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                VkBufferMemoryBarrier bufferBarrier = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .buffer = buf->buffer,
                    .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = kvp->value.initialAccess,
                    .srcQueueFamilyIndex = device->adapter->queueIndices.graphicsIndex,
                    .dstQueueFamilyIndex = device->adapter->queueIndices.graphicsIndex,
                    .size = VK_WHOLE_SIZE
                };
                VkBufferMemoryBarrierVector_push_back(&barrierSets[bufferIndex].bufferBarriers, bufferBarrier);
                if(knowledge){ 
                    knowledge->lastAccess              = kvp->value.lastAccess;
                    knowledge->lastStage               = kvp->value.lastStage;
                }
                else{
                    BufferUsageRecord newRecord = {
                        .initialAccess = kvp->value.lastAccess,
                        .lastAccess = kvp->value.lastAccess,
                        .initialStage = kvp->value.lastStage,
                        .lastStage = kvp->value.lastStage,
                        .everWrittenTo = isWritingAccess(kvp->value.lastAccess)
                    };
                    BufferUsageRecordMap_put(&referencedBuffers, buf, newRecord);
                }
            }
        }
    }
    ImageUsageRecordMap_free(&referencedImages);
    BufferUsageRecordMap_free(&referencedBuffers);

}

void updateLayoutCallback(void* texture_, ImageUsageRecord* record, void* unused){
    WGPUTexture texture = (WGPUTexture)texture_;
    texture->layout = record->lastLayout;
}

void releaseCommandBuffersDependingOnFence(void* userdata){
    PendingCommandBufferListRef* list = (PendingCommandBufferListRef*)userdata;
    WGPUCommandBufferVector* bufferVector = PendingCommandBufferMap_get(list->map, list->fence);
    for(uint32_t i = 0;i < bufferVector->size;i++){
        wgpuCommandBufferRelease(bufferVector->data[i]);
    }
    WGPUCommandBufferVector_free(bufferVector);
}
void wgpuQueueWaitIdle(WGPUQueue queue){
    ENTRY();
    queue->device->functions.vkQueueWaitIdle(queue->graphicsQueue);
    EXIT();
}
DEFINE_VECTOR_WITH_INLINE_STORAGE(static inline, CmdBarrierSet, CmdBarrierSetILVector, 4);
const int use_single_submit = 1;
static const char* il_string(VkImageLayout layout){
    switch(layout){
        case VK_IMAGE_LAYOUT_UNDEFINED: return "VK_IMAGE_LAYOUT_UNDEFINED";
        case VK_IMAGE_LAYOUT_GENERAL: return "VK_IMAGE_LAYOUT_GENERAL";
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL";
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL";
        case VK_IMAGE_LAYOUT_PREINITIALIZED: return "VK_IMAGE_LAYOUT_PREINITIALIZED";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL: return "VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL";
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL: return "VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL";
        case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ: return "VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ";
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: return "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR: return "VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR: return "VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR: return "VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR";
        case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR: return "VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR";
        case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT: return "VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT";
        case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR: return "VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR: return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR: return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR: return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR";
        case VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT: return "VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR: return "VK_IMAGE_LAYOUT_VIDEO_ENCODE_QUANTIZATION_MAP_KHR";
        default: return " <Unknown Layout?> ";
    }
} 
void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* buffers){
    ENTRY();

    //VkCommandBufferVector submittable;
    WGPUCommandBufferVector submittableWGPU;

    //VkCommandBufferVector_initWithSize(&submittable, commandCount + 1);
    
    WGPUCommandEncoder pscache = queue->presubmitCache;
    const uint32_t cacheBufferNonEmpty = ((pscache->encodedCommandCount > 0) ? 1 : 0);
    WGPUCommandBufferVector_initWithSize(&submittableWGPU, commandCount + cacheBufferNonEmpty);
    
    // LayoutAssumptions_for_each(&pscache->resourceUsage.entryAndFinalLayouts, welldamn_sdfd, NULL);
    
    //WGPUCommandBuffer sbuffer = buffers[0];
    //ImageUsageRecordMap_for_each(&sbuffer->resourceUsage.referencedTextures, registerTransitionCallback, pscache); 
    
    WGPUCommandBufferDescriptor cbd = {
        .label = STRVIEW("PresubmitCache"),
    };
    WGPUCommandBuffer cachebuffer = wgpuCommandEncoderFinish(queue->presubmitCache, &cbd);
    
    //submittable.data[0] = cachebuffer->buffer;
    //for(size_t i = 0;i < commandCount;i++){
    //    submittable.data[i + 1] = buffers[i]->buffer;
    //}
    if(cacheBufferNonEmpty == 1){
        submittableWGPU.data[0] = cachebuffer;
    }
    for(size_t i = 0;i < commandCount;i++){
        submittableWGPU.data[i + cacheBufferNonEmpty] = buffers[i];
    }

    
    
    WGPUFence fence = wgpuDeviceCreateFence(queue->device);


    const uint64_t frameCount = queue->device->submittedFrames;
    const uint32_t cacheIndex = frameCount % framesInFlight;

    PerframeCache* perFrameCache = DeviceGetFIFCache(queue->device, cacheIndex);
    
    VkResult submitResult = 0;
    WGPUCommandBufferVector interspersedBuffers;
    WGPUCommandBufferVector_init(&interspersedBuffers);
    if(use_single_submit && submittableWGPU.size > 0){
        CmdBarrierSetILVector compatibilityBarrierSets;
        CmdBarrierSetILVector_initWithSize(&compatibilityBarrierSets, submittableWGPU.size);
        
        generateInterspersedCompatibilityBarriers(submittableWGPU.data, submittableWGPU.size, compatibilityBarrierSets.data);
        for(uint32_t i = 0;i < submittableWGPU.size;i++){
            const CmdBarrierSet* cbs = CmdBarrierSetILVector_get(&compatibilityBarrierSets, i);
            for(uint32_t ibi = 0;ibi < cbs->imageBarriers.size;ibi++){
                if(cbs->imageBarriers.data[i].oldLayout == VK_IMAGE_LAYOUT_UNDEFINED){
                    // Uncomment for debugging I guess
                    // fprintf(stderr, "%s:%d: Transitioning from undefined to %s\n", __FILE__, __LINE__, il_string(cbs->imageBarriers.data[i].newLayout));
                }
            }
            //printf("cbs size: %lu\n", cbs->bufferBarriers.size);
            WGPUCommandEncoder iencoder = wgpuDeviceCreateCommandEncoder(queue->device, NULL);
            queue->device->functions.vkCmdPipelineBarrier(
                iencoder->buffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                cbs->memoryBarriers.size, cbs->memoryBarriers.data,
                cbs->bufferBarriers.size, cbs->bufferBarriers.data,
                cbs->imageBarriers.size,  cbs->imageBarriers.data
            );
            WGPUCommandBuffer buffer = wgpuCommandEncoderFinish(iencoder, NULL);
            WGPUCommandBufferVector_push_back(&interspersedBuffers, buffer);
            wgpuCommandEncoderRelease(iencoder);
        }
        for(size_t i = 0;i < submittableWGPU.size;i++){
            CmdBarrierSet_free(CmdBarrierSetILVector_get(&compatibilityBarrierSets, i));
        }
        CmdBarrierSetILVector_free(&compatibilityBarrierSets);
        VkSemaphoreVector waitSemaphores;
        VkSemaphoreVector_init(&waitSemaphores);
        SyncState* syncState = DeviceGetSyncState(queue->device, cacheIndex);

        if(syncState->acquireImageSemaphoreSignalled){
            VkSemaphoreVector_push_back(&waitSemaphores, syncState->acquireImageSemaphore);
            syncState->acquireImageSemaphoreSignalled = false;
        }
        if(syncState->submits > 0){
            VkSemaphoreVector_push_back(&waitSemaphores, syncState->semaphores.data[syncState->submits]);
        }
        uint32_t submits = syncState->submits;
        VkPipelineStageFlags* waitFlags = (VkPipelineStageFlags*)RL_CALLOC(waitSemaphores.size, sizeof(VkPipelineStageFlags));
        for(uint32_t i = 0;i < waitSemaphores.size;i++){
            waitFlags[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        VkCommandBufferVector finalSubmittable = {0};
        VkCommandBufferVector_init(&finalSubmittable);
        VkCommandBufferVector_reserve(&finalSubmittable, submittableWGPU.size * 2);
        for(size_t i = 0;i < submittableWGPU.size;i++){
            VkCommandBufferVector_push_back(&finalSubmittable, interspersedBuffers.data[i]->buffer);
            VkCommandBufferVector_push_back(&finalSubmittable, submittableWGPU.data[i]->buffer);
        }
        const VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = finalSubmittable.size,
            .waitSemaphoreCount = waitSemaphores.size,
            .pWaitSemaphores = waitSemaphores.data,
            .pWaitDstStageMask = waitFlags,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = syncState->semaphores.data + syncState->submits + 1,
            .pCommandBuffers = finalSubmittable.data,
        };
        if(finalSubmittable.size){
            //printf("Submitting:\n");
            //for(uint32_t i = 0;i < finalSubmittable.size;i++){
            //    printf("  %p\n", finalSubmittable.data[i]);
            //}
            //printf("with fence %p\n", fence);
        }
        ++syncState->submits;
        WGPUFence submitFence = fence;
        submitResult = queue->device->functions.vkQueueSubmit(queue->graphicsQueue, 1, &submitInfo, submitFence ? submitFence->fence : VK_NULL_HANDLE);
        if(submitResult == VK_SUCCESS){
            submitFence->state = WGPUFenceState_InUse;
        }
        for(uint32_t i = 0;i < submittableWGPU.size;i++){
            ImageUsageRecordMap_for_each(&interspersedBuffers.data[i]->resourceUsage.referencedTextures, updateLayoutCallback, NULL);
            ImageUsageRecordMap_for_each(&submittableWGPU.data[i]->resourceUsage.referencedTextures, updateLayoutCallback, NULL);
        }
        VkSemaphoreVector_free(&waitSemaphores);
        VkCommandBufferVector_free(&finalSubmittable);
        for(size_t i = 0;i < interspersedBuffers.size;i++){
            // compensated by not calling addRef below
            // wgpuCommandBufferRelease(interspersedBuffers.data[i]);
        }
        
        RL_FREE(waitFlags);
    }
    else{
        //for(uint32_t i = 0;i < submittable.size;i++){
        //    VkSemaphoreVector waitSemaphores;
        //    VkSemaphoreVector_init(&waitSemaphores);
//
        //    if(queue->syncState[cacheIndex].acquireImageSemaphoreSignalled){
        //        VkSemaphoreVector_push_back(&waitSemaphores, queue->syncState[cacheIndex].acquireImageSemaphore);
        //        queue->syncState[cacheIndex].acquireImageSemaphoreSignalled = false;
        //    }
        //    if(queue->syncState[cacheIndex].submits > 0){
        //        VkSemaphoreVector_push_back(&waitSemaphores, queue->syncState[cacheIndex].semaphores.data[queue->syncState[cacheIndex].submits]);
        //    }
//
        //    uint32_t submits = queue->syncState[cacheIndex].submits;
        //    
        //    VkPipelineStageFlags* waitFlags = (VkPipelineStageFlags*)RL_CALLOC(waitSemaphores.size, sizeof(VkPipelineStageFlags));
        //    for(uint32_t i = 0;i < waitSemaphores.size;i++){
        //        waitFlags[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        //    }
//
        //    const VkSubmitInfo si = {
        //        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        //        .commandBufferCount = 1,
        //        .waitSemaphoreCount = waitSemaphores.size,
        //        .pWaitSemaphores = waitSemaphores.data,
        //        .pWaitDstStageMask = waitFlags,
        //        .signalSemaphoreCount = 1,
        //        .pSignalSemaphores = queue->syncState[cacheIndex].semaphores.data + queue->syncState[cacheIndex].submits + 1,
        //        .pCommandBuffers = submittable.data + i
        //    };
        //    
//
        //    ++queue->syncState[cacheIndex].submits;
//
        //    WGPUFence submitFence = (i == (submittable.size - 1)) ? fence : NULL;
        //    submitResult |= queue->device->functions.vkQueueSubmit(queue->graphicsQueue, 1, &si, submitFence ? submitFence->fence : VK_NULL_HANDLE);
        //    if(submitFence){
        //        submitFence->state = WGPUFenceState_InUse;
        //    }
//
        //    VkSemaphoreVector_free(&waitSemaphores);
        //    RL_FREE(waitFlags);
        //}
    }

    if(submitResult == VK_SUCCESS){

        for(uint32_t i = 0;i < submittableWGPU.size;i++){
            WGPUCommandBuffer submittedBuffer = submittableWGPU.data[i];
            BufferUsageRecordMap map = submittedBuffer->resourceUsage.referencedBuffers; 
            for(size_t refbEntry = 0;refbEntry < map.current_capacity;refbEntry++){
                const BufferUsageRecordMap_kv_pair* kv_pair = map.table + refbEntry;
                WGPUBuffer keybuffer = kv_pair->key;
                // TODO: since this causes a lot of unnecessary fence action
                // we should only wait for fences if these buffers are going to be mapped
                // However it is possible all buffers are allocated with the HOST_VISIBLE bit
                // Also everWrittenTo is currently always set to true
                if((kv_pair->key != PHM_DELETED_SLOT_KEY && kv_pair->key != PHM_EMPTY_SLOT_KEY) /*&& kv_pair->value.everWrittenTo != VK_FALSE && (keybuffer->usage & (WGPUBufferUsage_MapWrite | WGPUBufferUsage_MapRead))*/){
                    if(keybuffer->latestFence)
                        wgpuFenceRelease(keybuffer->latestFence);
                    keybuffer->latestFence = fence;
                    wgpuFenceAddRef(fence);
                    //CallbackWithUserdataVector_push_back(
                    //    &fence->callbacksOnWaitComplete
                    //);
                }
            }
        }
        
        WGPUCommandBufferVector insert;
        WGPUCommandBufferVector_init(&insert);
        
        WGPUCommandBufferVector_push_back(&insert, cachebuffer);
        
        // TODO IMPORTANT: Is this really not required here? wgpuCommandBufferAddRef(cachebuffer);
        

        for(size_t i = 0;i < commandCount;i++){
            WGPUCommandBufferVector_push_back(&insert, buffers[i]);
            //wgpuCommandBufferAddRef(buffers[i]);
        }
        for(size_t i = 0;i < interspersedBuffers.size;i++){
            WGPUCommandBufferVector_push_back(&insert, interspersedBuffers.data[i]);
        }

        PerframeCache_pushFenceDependencies(perFrameCache, fence, &insert);

        for(size_t i = 0;i < interspersedBuffers.size;i++){
            wgpuCommandBufferRelease(interspersedBuffers.data[i]);
        }
        WGPUCommandBufferVector_free(&interspersedBuffers);
        uint32_t cacheIndex = frameCount % framesInFlight;
        //PendingCommandBufferMap* pcm = &DeviceGetFIFCache(queue->device, cacheIndex)->pendingCommandBuffers;
        //WGPUCommandBufferVector* fence_iterator = PendingCommandBufferMap_get(pcm, (void*)fence);
        ////auto it = queue->pendingCommandBuffers[frameCount % framesInFlight].find(fence);
        //if(fence_iterator == NULL){
        //    WGPUCommandBufferVector insert;
        //    WGPUCommandBufferVector_init(&insert);
        //    PendingCommandBufferMap_put(pcm, (void*)fence, insert);
        //    fence_iterator = PendingCommandBufferMap_get(pcm, (void*)fence);
        //    PendingCommandBufferListRef* ud = RL_CALLOC(1, sizeof(PendingCommandBufferListRef));
        //    ud->fence = fence;
        //    ud->map = pcm;
        //    
        //    CallbackWithUserdataVector_push_back(&fence->callbacksOnWaitComplete, (CallbackWithUserdata){
        //        .callback = releaseCommandBuffersDependingOnFence,
        //        .userdata = ud,
        //        .freeUserData = free
        //    });
        //    wgvk_assert(fence_iterator != NULL, "Something is wrong with the hash set");
        //    WGPUCommandBufferVector_init(fence_iterator);
        //}
        //for(size_t i = 0;i < insert.size;i++){
        //    WGPUCommandBufferVector_push_back(fence_iterator, insert.data[i]);
        //}
        //WGPUCommandBufferVector_free(&insert);
    }else{
        DeviceCallback(queue->device, WGPUErrorType_Internal, STRVIEW("vkQueueSubmit failed"));
    }
    wgpuCommandEncoderRelease(queue->presubmitCache);
    wgpuCommandBufferRelease(cachebuffer);
    WGPUCommandEncoderDescriptor cedesc zeroinit;
    queue->presubmitCache = wgpuDeviceCreateCommandEncoder(queue->device, &cedesc);
    //VkCommandBufferVector_free(&submittable);
    WGPUCommandBufferVector_free(&submittableWGPU);
    EXIT();
}

static const char* colorSpaceString(VkColorSpaceKHR cs){
    switch(cs){
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
        case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT";
        case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT";
        case VK_COLOR_SPACE_BT709_LINEAR_EXT: return "VK_COLOR_SPACE_BT709_LINEAR_EXT";
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: return "VK_COLOR_SPACE_BT709_NONLINEAR_EXT";
        case VK_COLOR_SPACE_BT2020_LINEAR_EXT: return "VK_COLOR_SPACE_BT2020_LINEAR_EXT";
        case VK_COLOR_SPACE_HDR10_ST2084_EXT: return "VK_COLOR_SPACE_HDR10_ST2084_EXT";
        case VK_COLOR_SPACE_DOLBYVISION_EXT: return "VK_COLOR_SPACE_DOLBYVISION_EXT";
        case VK_COLOR_SPACE_HDR10_HLG_EXT: return "VK_COLOR_SPACE_HDR10_HLG_EXT";
        case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: return "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT";
        case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: return "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT";
        case VK_COLOR_SPACE_PASS_THROUGH_EXT: return "VK_COLOR_SPACE_PASS_THROUGH_EXT";
        case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT";
        case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD: return "VK_COLOR_SPACE_DISPLAY_NATIVE_AMD";
        default: return " <unknown colorspace> ";
    }
}
static const char* vkFormatString(VkFormat format){
    switch(format){
        default: return " <unknown VkFormat> ";
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R4G4_UNORM_PACK8: return "VK_FORMAT_R4G4_UNORM_PACK8";
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return "VK_FORMAT_R5G6B5_UNORM_PACK16";
        case VK_FORMAT_B5G6R5_UNORM_PACK16: return "VK_FORMAT_B5G6R5_UNORM_PACK16";
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
        case VK_FORMAT_R8_UNORM: return "VK_FORMAT_R8_UNORM";
        case VK_FORMAT_R8_SNORM: return "VK_FORMAT_R8_SNORM";
        case VK_FORMAT_R8_USCALED: return "VK_FORMAT_R8_USCALED";
        case VK_FORMAT_R8_SSCALED: return "VK_FORMAT_R8_SSCALED";
        case VK_FORMAT_R8_UINT: return "VK_FORMAT_R8_UINT";
        case VK_FORMAT_R8_SINT: return "VK_FORMAT_R8_SINT";
        case VK_FORMAT_R8_SRGB: return "VK_FORMAT_R8_SRGB";
        case VK_FORMAT_R8G8_UNORM: return "VK_FORMAT_R8G8_UNORM";
        case VK_FORMAT_R8G8_SNORM: return "VK_FORMAT_R8G8_SNORM";
        case VK_FORMAT_R8G8_USCALED: return "VK_FORMAT_R8G8_USCALED";
        case VK_FORMAT_R8G8_SSCALED: return "VK_FORMAT_R8G8_SSCALED";
        case VK_FORMAT_R8G8_UINT: return "VK_FORMAT_R8G8_UINT";
        case VK_FORMAT_R8G8_SINT: return "VK_FORMAT_R8G8_SINT";
        case VK_FORMAT_R8G8_SRGB: return "VK_FORMAT_R8G8_SRGB";
        case VK_FORMAT_R8G8B8_UNORM: return "VK_FORMAT_R8G8B8_UNORM";
        case VK_FORMAT_R8G8B8_SNORM: return "VK_FORMAT_R8G8B8_SNORM";
        case VK_FORMAT_R8G8B8_USCALED: return "VK_FORMAT_R8G8B8_USCALED";
        case VK_FORMAT_R8G8B8_SSCALED: return "VK_FORMAT_R8G8B8_SSCALED";
        case VK_FORMAT_R8G8B8_UINT: return "VK_FORMAT_R8G8B8_UINT";
        case VK_FORMAT_R8G8B8_SINT: return "VK_FORMAT_R8G8B8_SINT";
        case VK_FORMAT_R8G8B8_SRGB: return "VK_FORMAT_R8G8B8_SRGB";
        case VK_FORMAT_B8G8R8_UNORM: return "VK_FORMAT_B8G8R8_UNORM";
        case VK_FORMAT_B8G8R8_SNORM: return "VK_FORMAT_B8G8R8_SNORM";
        case VK_FORMAT_B8G8R8_USCALED: return "VK_FORMAT_B8G8R8_USCALED";
        case VK_FORMAT_B8G8R8_SSCALED: return "VK_FORMAT_B8G8R8_SSCALED";
        case VK_FORMAT_B8G8R8_UINT: return "VK_FORMAT_B8G8R8_UINT";
        case VK_FORMAT_B8G8R8_SINT: return "VK_FORMAT_B8G8R8_SINT";
        case VK_FORMAT_B8G8R8_SRGB: return "VK_FORMAT_B8G8R8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SNORM: return "VK_FORMAT_R8G8B8A8_SNORM";
        case VK_FORMAT_R8G8B8A8_USCALED: return "VK_FORMAT_R8G8B8A8_USCALED";
        case VK_FORMAT_R8G8B8A8_SSCALED: return "VK_FORMAT_R8G8B8A8_SSCALED";
        case VK_FORMAT_R8G8B8A8_UINT: return "VK_FORMAT_R8G8B8A8_UINT";
        case VK_FORMAT_R8G8B8A8_SINT: return "VK_FORMAT_R8G8B8A8_SINT";
        case VK_FORMAT_R8G8B8A8_SRGB: return "VK_FORMAT_R8G8B8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SNORM: return "VK_FORMAT_B8G8R8A8_SNORM";
        case VK_FORMAT_B8G8R8A8_USCALED: return "VK_FORMAT_B8G8R8A8_USCALED";
        case VK_FORMAT_B8G8R8A8_SSCALED: return "VK_FORMAT_B8G8R8A8_SSCALED";
        case VK_FORMAT_B8G8R8A8_UINT: return "VK_FORMAT_B8G8R8A8_UINT";
        case VK_FORMAT_B8G8R8A8_SINT: return "VK_FORMAT_B8G8R8A8_SINT";
        case VK_FORMAT_B8G8R8A8_SRGB: return "VK_FORMAT_B8G8R8A8_SRGB";
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32: return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32: return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32: return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
        case VK_FORMAT_A8B8G8R8_UINT_PACK32: return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SINT_PACK32: return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32: return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32: return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
        case VK_FORMAT_A2R10G10B10_SINT_PACK32: return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32: return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32: return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
        case VK_FORMAT_A2B10G10R10_SINT_PACK32: return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
        case VK_FORMAT_R16_UNORM: return "VK_FORMAT_R16_UNORM";
        case VK_FORMAT_R16_SNORM: return "VK_FORMAT_R16_SNORM";
        case VK_FORMAT_R16_USCALED: return "VK_FORMAT_R16_USCALED";
        case VK_FORMAT_R16_SSCALED: return "VK_FORMAT_R16_SSCALED";
        case VK_FORMAT_R16_UINT: return "VK_FORMAT_R16_UINT";
        case VK_FORMAT_R16_SINT: return "VK_FORMAT_R16_SINT";
        case VK_FORMAT_R16_SFLOAT: return "VK_FORMAT_R16_SFLOAT";
        case VK_FORMAT_R16G16_UNORM: return "VK_FORMAT_R16G16_UNORM";
        case VK_FORMAT_R16G16_SNORM: return "VK_FORMAT_R16G16_SNORM";
        case VK_FORMAT_R16G16_USCALED: return "VK_FORMAT_R16G16_USCALED";
        case VK_FORMAT_R16G16_SSCALED: return "VK_FORMAT_R16G16_SSCALED";
        case VK_FORMAT_R16G16_UINT: return "VK_FORMAT_R16G16_UINT";
        case VK_FORMAT_R16G16_SINT: return "VK_FORMAT_R16G16_SINT";
        case VK_FORMAT_R16G16_SFLOAT: return "VK_FORMAT_R16G16_SFLOAT";
        case VK_FORMAT_R16G16B16_UNORM: return "VK_FORMAT_R16G16B16_UNORM";
        case VK_FORMAT_R16G16B16_SNORM: return "VK_FORMAT_R16G16B16_SNORM";
        case VK_FORMAT_R16G16B16_USCALED: return "VK_FORMAT_R16G16B16_USCALED";
        case VK_FORMAT_R16G16B16_SSCALED: return "VK_FORMAT_R16G16B16_SSCALED";
        case VK_FORMAT_R16G16B16_UINT: return "VK_FORMAT_R16G16B16_UINT";
        case VK_FORMAT_R16G16B16_SINT: return "VK_FORMAT_R16G16B16_SINT";
        case VK_FORMAT_R16G16B16_SFLOAT: return "VK_FORMAT_R16G16B16_SFLOAT";
        case VK_FORMAT_R16G16B16A16_UNORM: return "VK_FORMAT_R16G16B16A16_UNORM";
        case VK_FORMAT_R16G16B16A16_SNORM: return "VK_FORMAT_R16G16B16A16_SNORM";
        case VK_FORMAT_R16G16B16A16_USCALED: return "VK_FORMAT_R16G16B16A16_USCALED";
        case VK_FORMAT_R16G16B16A16_SSCALED: return "VK_FORMAT_R16G16B16A16_SSCALED";
        case VK_FORMAT_R16G16B16A16_UINT: return "VK_FORMAT_R16G16B16A16_UINT";
        case VK_FORMAT_R16G16B16A16_SINT: return "VK_FORMAT_R16G16B16A16_SINT";
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case VK_FORMAT_R32_UINT: return "VK_FORMAT_R32_UINT";
        case VK_FORMAT_R32_SINT: return "VK_FORMAT_R32_SINT";
        case VK_FORMAT_R32_SFLOAT: return "VK_FORMAT_R32_SFLOAT";
        case VK_FORMAT_R32G32_UINT: return "VK_FORMAT_R32G32_UINT";
        case VK_FORMAT_R32G32_SINT: return "VK_FORMAT_R32G32_SINT";
        case VK_FORMAT_R32G32_SFLOAT: return "VK_FORMAT_R32G32_SFLOAT";
        case VK_FORMAT_R32G32B32_UINT: return "VK_FORMAT_R32G32B32_UINT";
        case VK_FORMAT_R32G32B32_SINT: return "VK_FORMAT_R32G32B32_SINT";
        case VK_FORMAT_R32G32B32_SFLOAT: return "VK_FORMAT_R32G32B32_SFLOAT";
        case VK_FORMAT_R32G32B32A32_UINT: return "VK_FORMAT_R32G32B32A32_UINT";
        case VK_FORMAT_R32G32B32A32_SINT: return "VK_FORMAT_R32G32B32A32_SINT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";
        case VK_FORMAT_R64_UINT: return "VK_FORMAT_R64_UINT";
        case VK_FORMAT_R64_SINT: return "VK_FORMAT_R64_SINT";
        case VK_FORMAT_R64_SFLOAT: return "VK_FORMAT_R64_SFLOAT";
        case VK_FORMAT_R64G64_UINT: return "VK_FORMAT_R64G64_UINT";
        case VK_FORMAT_R64G64_SINT: return "VK_FORMAT_R64G64_SINT";
        case VK_FORMAT_R64G64_SFLOAT: return "VK_FORMAT_R64G64_SFLOAT";
        case VK_FORMAT_R64G64B64_UINT: return "VK_FORMAT_R64G64B64_UINT";
        case VK_FORMAT_R64G64B64_SINT: return "VK_FORMAT_R64G64B64_SINT";
        case VK_FORMAT_R64G64B64_SFLOAT: return "VK_FORMAT_R64G64B64_SFLOAT";
        case VK_FORMAT_R64G64B64A64_UINT: return "VK_FORMAT_R64G64B64A64_UINT";
        case VK_FORMAT_R64G64B64A64_SINT: return "VK_FORMAT_R64G64B64A64_SINT";
        case VK_FORMAT_R64G64B64A64_SFLOAT: return "VK_FORMAT_R64G64B64A64_SFLOAT";
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
        case VK_FORMAT_D16_UNORM: return "VK_FORMAT_D16_UNORM";
        case VK_FORMAT_X8_D24_UNORM_PACK32: return "VK_FORMAT_X8_D24_UNORM_PACK32";
        case VK_FORMAT_D32_SFLOAT: return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_S8_UINT: return "VK_FORMAT_S8_UINT";
        case VK_FORMAT_D16_UNORM_S8_UINT: return "VK_FORMAT_D16_UNORM_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT: return "VK_FORMAT_D24_UNORM_S8_UINT";
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return "VK_FORMAT_D32_SFLOAT_S8_UINT";
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
        case VK_FORMAT_BC2_UNORM_BLOCK: return "VK_FORMAT_BC2_UNORM_BLOCK";
        case VK_FORMAT_BC2_SRGB_BLOCK: return "VK_FORMAT_BC2_SRGB_BLOCK";
        case VK_FORMAT_BC3_UNORM_BLOCK: return "VK_FORMAT_BC3_UNORM_BLOCK";
        case VK_FORMAT_BC3_SRGB_BLOCK: return "VK_FORMAT_BC3_SRGB_BLOCK";
        case VK_FORMAT_BC4_UNORM_BLOCK: return "VK_FORMAT_BC4_UNORM_BLOCK";
        case VK_FORMAT_BC4_SNORM_BLOCK: return "VK_FORMAT_BC4_SNORM_BLOCK";
        case VK_FORMAT_BC5_UNORM_BLOCK: return "VK_FORMAT_BC5_UNORM_BLOCK";
        case VK_FORMAT_BC5_SNORM_BLOCK: return "VK_FORMAT_BC5_SNORM_BLOCK";
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
        case VK_FORMAT_BC7_UNORM_BLOCK: return "VK_FORMAT_BC7_UNORM_BLOCK";
        case VK_FORMAT_BC7_SRGB_BLOCK: return "VK_FORMAT_BC7_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
        case VK_FORMAT_EAC_R11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
        case VK_FORMAT_EAC_R11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";
        case VK_FORMAT_G8B8G8R8_422_UNORM: return "VK_FORMAT_G8B8G8R8_422_UNORM";
        case VK_FORMAT_B8G8R8G8_422_UNORM: return "VK_FORMAT_B8G8R8G8_422_UNORM";
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM";
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM";
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM";
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM";
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: return "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM";
        case VK_FORMAT_R10X6_UNORM_PACK16: return "VK_FORMAT_R10X6_UNORM_PACK16";
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: return "VK_FORMAT_R10X6G10X6_UNORM_2PACK16";
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16";
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_R12X4_UNORM_PACK16: return "VK_FORMAT_R12X4_UNORM_PACK16";
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: return "VK_FORMAT_R12X4G12X4_UNORM_2PACK16";
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16";
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_G16B16G16R16_422_UNORM: return "VK_FORMAT_G16B16G16R16_422_UNORM";
        case VK_FORMAT_B16G16R16G16_422_UNORM: return "VK_FORMAT_B16G16R16G16_422_UNORM";
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM";
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM";
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM";
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM";
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: return "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM";
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM: return "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM";
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16";
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM: return "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM";
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16: return "VK_FORMAT_A4R4G4B4_UNORM_PACK16";
        case VK_FORMAT_A4B4G4R4_UNORM_PACK16: return "VK_FORMAT_A4B4G4R4_UNORM_PACK16";
        case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK";
        case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK: return "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK";
        case VK_FORMAT_A1B5G5R5_UNORM_PACK16: return "VK_FORMAT_A1B5G5R5_UNORM_PACK16";
        case VK_FORMAT_A8_UNORM: return "VK_FORMAT_A8_UNORM";
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG";
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG";
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG";
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: return "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG";
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG";
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG";
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG";
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG";
        case VK_FORMAT_R16G16_SFIXED5_NV: return "VK_FORMAT_R16G16_SFIXED5_NV";
    }
}

static inline VkColorSpaceKHR toVulkanColorSpace(WGPUPredefinedColorSpace wcsp, WGPUToneMappingMode tmmode){
    if(wcsp == WGPUPredefinedColorSpace_DisplayP3)return VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
    if(wcsp == WGPUPredefinedColorSpace_SRGB && tmmode == WGPUToneMappingMode_Extended)return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}

void wgpuSurfaceGetCapabilities(WGPUSurface wgpuSurface, WGPUAdapter adapter, WGPUSurfaceCapabilities* capabilities){
    ENTRY();
    if(wgpuSurface->capabilityCache.formatCount){
        *capabilities = wgpuSurface->capabilityCache;
        return;
    }

    VkSurfaceKHR surface = wgpuSurface->surface;
    VkSurfaceCapabilitiesKHR scap zeroinit;
    VkPhysicalDevice vk_physicalDevice = adapter->physicalDevice;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physicalDevice, surface, &scap);
    
    // Formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physicalDevice, surface, &formatCount, NULL);
    
    if (formatCount != 0 && wgpuSurface->formatCache == NULL) {
        wgpuSurface->formatCache = (VkSurfaceFormatKHR*)RL_CALLOC(formatCount, sizeof(VkSurfaceFormatKHR));
        wgpuSurface->wgpuFormatCache = (WGPUTextureFormat*)RL_CALLOC(formatCount, sizeof(WGPUTextureFormat));
        VkSurfaceFormatKHR* surfaceFormats = (VkSurfaceFormatKHR*)RL_CALLOC(formatCount, sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physicalDevice, surface, &formatCount, surfaceFormats);
        const VkColorSpaceKHR spaceToScan = toVulkanColorSpace(wgpuSurface->colorSpace, wgpuSurface->tonemappingMode);
        for(size_t i = 0;i < formatCount;i++){
            wgpuSurface->formatCache[i] = surfaceFormats[i];
            //printf("[%s + %s]\n", colorSpaceString(surfaceFormats[i].colorSpace), vkFormatString(surfaceFormats[i].format));
            
            if(surfaceFormats[i].colorSpace == spaceToScan && fromVulkanPixelFormat(surfaceFormats[i].format) != WGPUTextureFormat_Undefined){
                wgpuSurface->wgpuFormatCache[wgpuSurface->wgpuFormatCount++] = fromVulkanPixelFormat(surfaceFormats[i].format);
            }
        }
        wgpuSurface->formatCount = formatCount;
        RL_FREE(surfaceFormats);
    }

    // Present Modes
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physicalDevice, surface, &presentModeCount, NULL);
    if (presentModeCount != 0 && wgpuSurface->presentModeCache == NULL) {
        uint32_t wgpuCompatiblePresentModeCount = 0;
        VkPresentModeKHR presentModes[32] = {0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physicalDevice, surface, &presentModeCount, presentModes);

        for(size_t i = 0;i < presentModeCount;i++){
            wgpuCompatiblePresentModeCount += fromVulkanPresentMode(presentModes[i]) != (WGPUPresentMode)~0;
        }
        wgpuSurface->presentModeCache = (WGPUPresentMode*)RL_CALLOC(wgpuCompatiblePresentModeCount, sizeof(WGPUPresentMode));
        uint32_t insertIndex = 0;
        for(size_t i = 0;i < presentModeCount;i++){
            if(fromVulkanPresentMode(presentModes[i]) != (WGPUPresentMode)~0){
                wgpuSurface->presentModeCache[insertIndex++] = fromVulkanPresentMode(presentModes[i]);
            }
        }
        wgpuSurface->presentModeCount = wgpuCompatiblePresentModeCount;
    }
    wgpuSurface->capabilityCache.presentModeCount = wgpuSurface->presentModeCount;
    wgpuSurface->capabilityCache.formatCount = wgpuSurface->wgpuFormatCount;
    wgpuSurface->capabilityCache.formats = wgpuSurface->wgpuFormatCache;
    wgpuSurface->capabilityCache.presentModes = wgpuSurface->presentModeCache;
    wgpuSurface->capabilityCache.usages = fromVulkanWGPUTextureUsage(scap.supportedUsageFlags);
    wgpuSurface->capabilityCache.alphaModeCount = 0;
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR){
        ++wgpuSurface->capabilityCache.alphaModeCount;
    }
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR){
        ++wgpuSurface->capabilityCache.alphaModeCount;
    }
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR){
        ++wgpuSurface->capabilityCache.alphaModeCount;
    }
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR){
        ++wgpuSurface->capabilityCache.alphaModeCount;
    }
    WGPUCompositeAlphaMode* alphaModes = RL_CALLOC(wgpuSurface->capabilityCache.alphaModeCount, sizeof(WGPUCompositeAlphaMode));
    uint32_t amIndex = 0;
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR){
        alphaModes[amIndex++] = WGPUCompositeAlphaMode_Opaque;
    }
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR){
        alphaModes[amIndex++] = WGPUCompositeAlphaMode_Premultiplied;
    }
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR){
        alphaModes[amIndex++] = WGPUCompositeAlphaMode_Premultiplied;
    }
    if(scap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR){
        alphaModes[amIndex++] = WGPUCompositeAlphaMode_Inherit;
    }
    wgpuSurface->capabilityCache.alphaModes = alphaModes;
    *capabilities = wgpuSurface->capabilityCache;
    EXIT();
}



void wgpuSurfaceConfigure(WGPUSurface surface, const WGPUSurfaceConfiguration* config){
    ENTRY();
    if(surface->swapchain){
        wgpuSurfaceUnconfigure(surface);
    }
    WGPUDevice device = config->device;
    VkSurfaceCapabilitiesKHR vkCapabilities = {0};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->adapter->physicalDevice, surface->surface, &vkCapabilities);
    VkSwapchainCreateInfoKHR createInfo zeroinit;
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface->surface;
    uint32_t correctedWidth, correctedHeight;
    #define SWAPCHAIN_ICLAMP_TEMP(V, MINI, MAXI) ((V) < (MINI)) ? (MINI) : (((V) > (MAXI)) ? (MAXI) : (V))
    if(config->width < vkCapabilities.minImageExtent.width || config->width > vkCapabilities.maxImageExtent.width){
        correctedWidth = SWAPCHAIN_ICLAMP_TEMP(config->width, vkCapabilities.minImageExtent.width, vkCapabilities.maxImageExtent.width);
        char printbuf[1024] = {0};
        size_t pblength = snprintf(printbuf, 1024, "Invalid SurfaceConfiguration::width %u, adjusting to %u", config->width, correctedWidth);
        DeviceCallback(device, WGPUErrorType_Validation, (WGPUStringView){.data = printbuf, .length = pblength});
    }
    else{
        correctedWidth = config->width;
    }
    if(config->height < vkCapabilities.minImageExtent.height || config->height > vkCapabilities.maxImageExtent.height){
        correctedHeight = SWAPCHAIN_ICLAMP_TEMP(config->height, vkCapabilities.minImageExtent.height, vkCapabilities.maxImageExtent.height);
        char printbuf[1024] = {0};
        size_t pblength = snprintf(printbuf, 1024, "Invalid SurfaceConfiguration::height %u, adjusting to %u", config->height, correctedHeight);
        DeviceCallback(device, WGPUErrorType_Validation, (WGPUStringView){.data = printbuf, .length = pblength});
    }
    else{
        correctedHeight = config->height;
    }
    if(vkCapabilities.maxImageCount == 0){
        createInfo.minImageCount = vkCapabilities.minImageCount + 1;
    }
    else{
        createInfo.minImageCount = SWAPCHAIN_ICLAMP_TEMP(vkCapabilities.minImageCount + 1, vkCapabilities.minImageCount, vkCapabilities.maxImageCount);
    }
    #undef SWAPCHAIN_ICLAMP_TEMP
    
    
    createInfo.imageFormat = toVulkanPixelFormat(config->format);//swapchainImageFormat;
    surface->width  = correctedWidth;
    surface->height = correctedHeight;
    surface->device = config->device;
    VkExtent2D newExtent = {correctedWidth, correctedHeight};
    createInfo.imageExtent = newExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = toVulkanTextureUsage(config->usage, config->format);

    // Queue family indices
    uint32_t queueFamilyIndices[2] = {
        device->adapter->queueIndices.graphicsIndex, 
        device->adapter->queueIndices.transferIndex
    };

    if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    }
    createInfo.imageColorSpace = toVulkanColorSpace(surface->colorSpace, surface->tonemappingMode);
    createInfo.preTransform = vkCapabilities.currentTransform;
    createInfo.compositeAlpha = toVulkanCompositeAlphaMode(config->alphaMode);
    createInfo.presentMode = toVulkanPresentMode(config->presentMode); 
    createInfo.clipped = VK_TRUE;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkResult scCreateResult = device->functions.vkCreateSwapchainKHR(device->device, &createInfo, NULL, &(surface->swapchain));
    if (scCreateResult != VK_SUCCESS) {
        DeviceCallback(device, WGPUErrorType_Internal, STRVIEW("Failed to create swapchain"));
    } else {
        //TRACELOG(WGPU_LOG_INFO, "wgpuSurfaceConfigure(): Successfully created swap chain");
    }

    device->functions.vkGetSwapchainImagesKHR(device->device, surface->swapchain, &surface->imagecount, NULL);

    VkImage tmpImages[32] = {0};

    device->functions.vkGetSwapchainImagesKHR(device->device, surface->swapchain, &surface->imagecount, tmpImages);
    surface->images            = (WGPUTexture*)RL_CALLOC(surface->imagecount, sizeof(WGPUTexture));
    surface->presentSemaphores = (VkSemaphore*)RL_CALLOC(surface->imagecount, sizeof(VkSemaphore));
    for (uint32_t i = 0; i < surface->imagecount; i++) {
        surface->images[i] = RL_CALLOC(1, sizeof(WGPUTextureImpl));
        surface->images[i]->device = device;
        surface->images[i]->width = correctedWidth;
        surface->images[i]->height = correctedHeight;
        surface->images[i]->depthOrArrayLayers = 1;
        surface->images[i]->refCount = 1;
        surface->images[i]->sampleCount = 1;
        surface->images[i]->image = tmpImages[i];
        surface->images[i]->dimension = VK_IMAGE_TYPE_2D;
        surface->images[i]->format = createInfo.imageFormat;
        surface->images[i]->mipLevels = 1;
        surface->images[i]->depthOrArrayLayers = 1;
        surface->images[i]->refCount = 1;

        VkSemaphoreCreateInfo vci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkSemaphore createdSemaphore = VK_NULL_HANDLE;
        device->functions.vkCreateSemaphore(device->device, &vci, NULL, &createdSemaphore);
        surface->presentSemaphores[i] = createdSemaphore;
    }
    EXIT();
}

void wgpuSurfaceRelease(WGPUSurface surface){
    ENTRY();
    if(--surface->refCount == 0){
        if(surface->swapchain){
            wgpuSurfaceUnconfigure(surface);

            //RL_FREE((void*)surface->images);
            //RL_FREE(surface->formatCache);
            //RL_FREE(surface->wgpuFormatCache);
            //RL_FREE(surface->presentModeCache);
            surface->device->functions.vkDestroySwapchainKHR(surface->device->device, surface->swapchain, NULL);
        }
    }
    EXIT();
}

void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder cpe, uint32_t x, uint32_t y, uint32_t z){
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = cp_command_type_dispatch_workgroups,
        .dispatchWorkgroups = {x, y, z}
    };
    

    RenderPassCommandGenericVector_push_back(&cpe->bufferedCommands, insert);
    EXIT();
}

static void releaseRPSetCallback(WGPURenderPassEncoder rpEncoder, void* unused){
    wgpuRenderPassEncoderRelease(rpEncoder);
}

static void releaseCPSetCallback(WGPUComputePassEncoder cpEncoder, void* unused){
    wgpuComputePassEncoderRelease(cpEncoder);
}

static void releaseRTSetCallback(WGPURaytracingPassEncoder rtEncoder, void* unused){
    wgpuReleaseRaytracingPassEncoder(rtEncoder);
}

void wgpuCommandEncoderRelease(WGPUCommandEncoder commandEncoder) {
    ENTRY();
    WGPUCommandEncoder commandBuffer = commandEncoder;
    //wgvk_assert(commandEncoder->movedFrom, "Commandencoder still valid");
    if(--commandEncoder->refCount == 0){
        if(!commandEncoder->movedFrom){
            WGPURenderPassEncoderSet_for_each(&commandBuffer->referencedRPs, releaseRPSetCallback, NULL);
            WGPUComputePassEncoderSet_for_each(&commandBuffer->referencedCPs, releaseCPSetCallback, NULL);
            WGPURaytracingPassEncoderSet_for_each(&commandBuffer->referencedRTs, releaseRTSetCallback, NULL);
            
            releaseAllAndClear(&commandBuffer->resourceUsage);
            // The above performs ResourceUsage_free already!
            

            WGPURenderPassEncoderSet_free(&commandBuffer->referencedRPs);
            WGPUComputePassEncoderSet_free(&commandBuffer->referencedCPs);
            WGPURaytracingPassEncoderSet_free(&commandBuffer->referencedRTs);
        }
        if(commandEncoder->buffer){
            VkCommandBufferVector_push_back(
                &DeviceGetFIFCache(commandEncoder->device, commandEncoder->cacheIndex)->commandBuffers,
                commandEncoder->buffer
            );
        }
    }
    
    RL_FREE(commandEncoder);
    EXIT();
}

void wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer) {
    ENTRY();
    if(--commandBuffer->refCount == 0){
        WGPUDevice device = commandBuffer->device;
        WGPURenderPassEncoderSet_for_each(&commandBuffer->referencedRPs, releaseRPSetCallback, NULL);
        WGPUComputePassEncoderSet_for_each(&commandBuffer->referencedCPs, releaseCPSetCallback, NULL);
        WGPURaytracingPassEncoderSet_for_each(&commandBuffer->referencedRTs, releaseRTSetCallback, NULL);
        
        releaseAllAndClear(&commandBuffer->resourceUsage);
        // The above performs ResourceUsage_free already!
    

        WGPURenderPassEncoderSet_free(&commandBuffer->referencedRPs);
        WGPUComputePassEncoderSet_free(&commandBuffer->referencedCPs);
        WGPURaytracingPassEncoderSet_free(&commandBuffer->referencedRTs);
        
        PerframeCache* frameCache = DeviceGetFIFCache(commandBuffer->device, commandBuffer->cacheIndex);
        device->functions.vkFreeCommandBuffers(device->device, device->fifCache.frameCaches[commandBuffer->cacheIndex].commandPool, 1, &commandBuffer->buffer);
        //VkCommandBufferVector_push_back(&frameCache->commandBuffers, commandBuffer->buffer);
        if(commandBuffer->label.data){
            WGPUStringFree(commandBuffer->label);
        }
        RL_FREE(commandBuffer);
    }
    EXIT();
}
void wgpuRenderPassEncoderAddRef(WGPURenderPassEncoder rpenc) {
    ENTRY();
    ++rpenc->refCount;
    EXIT();
}

void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder rpenc) {
    ENTRY();
    if (--rpenc->refCount == 0) {
        releaseAllAndClear(&rpenc->resourceUsage);
        if(rpenc->frameBuffer){
            rpenc->device->functions.vkDestroyFramebuffer(rpenc->device->device, rpenc->frameBuffer, NULL);
        }
        ResourceUsage_free(&rpenc->resourceUsage);
        RenderPassCommandGenericVector_free(&rpenc->bufferedCommands);
        RL_FREE(rpenc);
    }
    EXIT();
}

void wgpuShaderModuleRelease(WGPUShaderModule module){
    ENTRY();
    if(--module->refCount == 0){
        if(module->vulkanModuleMultiEP){
            module->device->functions.vkDestroyShaderModule(module->device->device, module->vulkanModuleMultiEP, NULL);
        }
        else{
            for(size_t i = 0;i < 16;i++){
                if(module->modules[i].module){
                    module->device->functions.vkDestroyShaderModule(module->device->device, module->modules[i].module, NULL);
                }
            }
        }
        if(module->source->sType == WGPUSType_ShaderSourceSPIRV){
            RL_FREE((void*)((WGPUShaderSourceSPIRV*)module->source)->code);
        }
        if(module->source->sType == WGPUSType_ShaderSourceWGSL){
            RL_FREE((void*)((WGPUShaderSourceWGSL*)module->source)->code.data);
        }
        RL_FREE(module->source);
        RL_FREE(module);
    }
    EXIT();
}
void wgpuSamplerRelease(WGPUSampler sampler){
    ENTRY();
    if(--sampler->refCount == 0){
        sampler->device->functions.vkDestroySampler(sampler->device->device, sampler->sampler, NULL);
        RL_FREE(sampler);
    }
    EXIT();
}
void wgpuRenderPipelineRelease(WGPURenderPipeline pipeline){
    ENTRY();
    if(--pipeline->refCount == 0){
        wgpuPipelineLayoutRelease(pipeline->layout);
        pipeline->device->functions.vkDestroyPipeline(pipeline->device->device, pipeline->renderPipeline, NULL);
        RL_FREE(pipeline);
    }
    EXIT();
}

void wgpuComputePipelineRelease(WGPUComputePipeline pipeline){
    ENTRY();
    if(--pipeline->refCount == 0){
        wgpuPipelineLayoutRelease(pipeline->layout);
        pipeline->device->functions.vkDestroyPipeline(pipeline->device->device, pipeline->computePipeline, NULL);
        RL_FREE(pipeline);
    }
    EXIT();
}

void wgpuBufferRelease(WGPUBuffer buffer) {
    ENTRY();
    if (--buffer->refCount == 0) {
        if(buffer->latestFence){
            wgpuFenceRelease(buffer->latestFence);
            buffer->latestFence = NULL;
        }
        switch(buffer->allocationType){
            #if USE_VMA_ALLOCATOR
            case AllocationTypeVMA:
                vmaDestroyBuffer(buffer->device->allocator, buffer->buffer, buffer->vmaAllocation);
            break;
            #endif
            case AllocationTypeBuiltin:{
                buffer->device->functions.vkDestroyBuffer(buffer->device->device, buffer->buffer, NULL);
                wgvkAllocator_free(&buffer->builtinAllocation);
            }break;
            default:
            rg_unreachable();
        }
        RL_FREE(buffer);
    }
    EXIT();
}

WGPUBindGroupLayout wgpuBindGroupLayoutRelease_withReturn(WGPUBindGroupLayout bglayout){
    ENTRY();
    if(--bglayout->refCount == 0){
        WGPUDevice device = bglayout->device;
        for(uint32_t i = 0;i < framesInFlight;i++){
            PerframeCache* fci = DeviceGetFIFCache(bglayout->device, i);
            DescriptorSetAndPoolVector* dspVector = BindGroupCacheMap_get(&fci->bindGroupCache, bglayout);
            if(dspVector){
                for(size_t i = 0;i < dspVector->size;i++){
                    device->functions.vkDestroyDescriptorPool(device->device, dspVector->data[i].pool, NULL);
                }
                BindGroupCacheMap_erase(&fci->bindGroupCache, bglayout);
            }
        }
        device->functions.vkDestroyDescriptorSetLayout(bglayout->device->device, bglayout->layout, NULL);
        RL_FREE((void*)bglayout->entries);
        RL_FREE((void*)bglayout);
        return NULL;
    }
    EXIT();
    return bglayout;
}
void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bglayout){
    (void)wgpuBindGroupLayoutRelease_withReturn(bglayout);
}

void wgpuBindGroupRelease(WGPUBindGroup dshandle) {
    ENTRY();
    if (--dshandle->refCount == 0) {
        releaseAllAndClear(&dshandle->resourceUsage);

        WGPUBindGroupLayout stillThere = wgpuBindGroupLayoutRelease_withReturn(dshandle->layout);
        if(stillThere){
            BindGroupCacheMap* bgcm = &DeviceGetFIFCache(dshandle->device, dshandle->cacheIndex)->bindGroupCache;
            DescriptorSetAndPool insertValue = {
                .pool = dshandle->pool,
                .set = dshandle->set
            };
            DescriptorSetAndPoolVector* maybeAlreadyThere = BindGroupCacheMap_get(bgcm, dshandle->layout);
            if(maybeAlreadyThere == NULL){
                DescriptorSetAndPoolVector empty zeroinit;
                BindGroupCacheMap_put(bgcm, dshandle->layout, empty);
                maybeAlreadyThere = BindGroupCacheMap_get(bgcm, dshandle->layout);
                wgvk_assert(maybeAlreadyThere != NULL, "Still null after insert");
                DescriptorSetAndPoolVector_init(maybeAlreadyThere);
            }

            if(maybeAlreadyThere){
                DescriptorSetAndPoolVector_push_back(maybeAlreadyThere, insertValue);
            }
        }
        else{
            //dshandle->device->functions.vkFreeDescriptorSets(dshandle->device->device, dshandle->pool, 1, &dshandle->set);
            dshandle->device->functions.vkDestroyDescriptorPool(dshandle->device->device, dshandle->pool, NULL);
        }
        RL_FREE(dshandle->entries);

        // DONT delete them, they are cached
        // vkFreeDescriptorSets(dshandle->device->device, dshandle->pool, 1, &dshandle->set);
        // vkDestroyDescriptorPool(dshandle->device->device, dshandle->pool, NULL);
        
        RL_FREE(dshandle);
    }
    EXIT();
}
void wgpuInstanceAddRef                       (WGPUInstance instance){
    ENTRY();
    ++instance->refCount;
    EXIT();
}
void wgpuAdapterAddRef                        (WGPUAdapter adapter){
    ENTRY();
    ++adapter->refCount;
    EXIT();
}
void wgpuDeviceAddRef                         (WGPUDevice device){
    ENTRY();
    ++device->refCount;
    EXIT();
}
void wgpuQueueAddRef                          (WGPUQueue queue){
    ENTRY();
    ++queue->refCount;
    EXIT();
}

void wgpuInstanceRelease                      (WGPUInstance instance){
    ENTRY();
    if(--instance->refCount == 0){
        vkDestroyDebugUtilsMessengerEXT(instance->instance, instance->debugMessenger, NULL);
        vkDestroyInstance(instance->instance, NULL);
        FutureIDMap_free(&instance->g_futureIDMap);
        RL_FREE(instance);
    }
    EXIT();
}

void wgpuAdapterRelease(WGPUAdapter adapter){
    ENTRY();
    if(--adapter->refCount == 0){
        wgpuInstanceRelease(adapter->instance);
        RL_FREE(adapter);
    }
    EXIT();
}
void resetFenceAndReleaseBuffers(void* fence_, WGPUCommandBufferVector* cBuffers, void* wgpudevice);
void wgpuDeviceRelease(WGPUDevice device){
    ENTRY();
    if(--device->refCount == 0){
        WGPUCommandBufferDescriptor cbd = {
            .label = STRVIEW("PresubmitCache"),
        };
        wgvk_thread_pool_destroy(device->thread_pool);
        WGPUCommandBuffer cBuffer = wgpuCommandEncoderFinish(device->queue->presubmitCache, &cbd);
        wgpuCommandEncoderRelease(device->queue->presubmitCache);
        wgpuCommandBufferRelease(cBuffer);
        FIFCache_destroy(&device->fifCache);
        {  // Destroy PerframeCaches
            
            FenceCache_Destroy(&device->fenceCache);
            #if USE_VMA_ALLOCATOR == 1
            vmaDestroyPool(device->allocator, device->aligned_hostVisiblePool);
            vmaDestroyAllocator(device->allocator);
            #endif
            wgvkAllocator_destroy(&device->builtinAllocator);
        }
        device->functions.vkDestroyCommandPool(device->device, device->secondaryCommandPool, NULL);
        
        wgpuQueueRelease(device->queue);
        wgpuAdapterRelease(device->adapter);
        device->functions.vkDestroyDevice(device->device, NULL);
        
        // Still a lot to do
        RL_FREE(device->queue);
        RL_FREE(device);
    }
    EXIT();
}
void wgpuQueueRelease                         (WGPUQueue queue){
    ENTRY();
    if(--queue->refCount == 0){
        //wgpuDeviceRelease(queue->device);
    }
    EXIT();
}

WGPUComputePipeline wgpuDeviceCreateComputePipeline(WGPUDevice device, const WGPUComputePipelineDescriptor* descriptor){
    ENTRY();
    WGPUComputePipeline ret = RL_CALLOC(1, sizeof(WGPUComputePipelineImpl));
    ret->device = device;
    ret->refCount = 1;

    char namebuffer[512];
    wgvk_assert(descriptor->compute.entryPoint.length < 511, "griiindumehaue");
    memcpy(namebuffer, descriptor->compute.entryPoint.data, descriptor->compute.entryPoint.length);
    namebuffer[descriptor->compute.entryPoint.length] = 0;
    
    
    VkPipelineShaderStageCreateInfo computeStage = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        NULL,
        0,
        VK_SHADER_STAGE_COMPUTE_BIT,
        descriptor->compute.module->vulkanModuleMultiEP,
        namebuffer,
        NULL
    };

    VkComputePipelineCreateInfo cpci = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        NULL,
        0,
        computeStage,
        descriptor->layout->layout,
        0,
        0
    };
    device->functions.vkCreateComputePipelines(
        device->device,
        NULL,
        1, &cpci,
        NULL,
        &ret->computePipeline);
    ret->layout = descriptor->layout;
    wgpuPipelineLayoutAddRef(ret->layout);
    return ret;
    EXIT();
}


WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor) {
    ENTRY();
    WGPUDeviceImpl* deviceImpl = (WGPUDeviceImpl*)(device);
    WGPUPipelineLayout pl_layout = descriptor->layout;

    VkPipelineShaderStageCreateInfo shaderStages[16] = {
        (VkPipelineShaderStageCreateInfo){0}
    };
    uint32_t shaderStageInsertPos = 0;
    char nullTerminatedEntryPoints[16][64] = {0};

    const char* a = nullTerminatedEntryPoints[15];
    // Vertex Stage
    VkPipelineShaderStageCreateInfo vertShaderStageInfo zeroinit;
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.pName = descriptor->vertex.entryPoint.data; // Assuming null-terminated or careful length handling elsewhere
    if((descriptor->vertex.module)->vulkanModuleMultiEP){
        vertShaderStageInfo.module = (descriptor->vertex.module)->vulkanModuleMultiEP;
    }
    else{
        vertShaderStageInfo.module = (descriptor->vertex.module)->modules[WGPUShaderStageEnum_Vertex].module;
        // TODO (also below): handle all this stuff but this is mainly if a wrong entryPoint name is entered? idk
        // for(uint32_t i = 0;i < WGPUShaderStageEnum_EnumCount;i++){
        //     wgvk_assert(wgpuStrlen(descriptor->vertex.entryPoint) < 16, "Entry point name too long");
        //     size_t clength = wgpuStrlen(descriptor->vertex.entryPoint);
        //     
        // }
    }
    
    VkSpecializationInfo vertexSpecInfo = {0};
    double vertexConstantBuffer[32];
    VkSpecializationMapEntry vertexMapEntries[32];
    if(descriptor->vertex.constantCount){
        vertexSpecInfo.pData = vertexConstantBuffer;
        for(uint32_t i = 0;i < descriptor->vertex.constantCount;i++){
            vertexConstantBuffer[i] = descriptor->vertex.constants[i].value;
            vertexMapEntries[i].constantID = i;
            vertexMapEntries[i].offset = i * sizeof(double);
            vertexMapEntries[i].size = 8;
        }
        vertexSpecInfo.dataSize = descriptor->vertex.constantCount * sizeof(double);
        vertexSpecInfo.mapEntryCount = descriptor->vertex.constantCount;
        vertexSpecInfo.pMapEntries = vertexMapEntries;
        vertShaderStageInfo.pSpecializationInfo = &vertexSpecInfo;
    }
    // TODO: Handle constants if necessary via specialization constants
    // vertShaderStageInfo.pSpecializationInfo = ...;
    shaderStages[shaderStageInsertPos++] = vertShaderStageInfo;

    // Fragment Stage (Optional)
    VkPipelineShaderStageCreateInfo fragShaderStageInfo zeroinit;

    VkSpecializationInfo fragmentSpecInfo = {0};
    double fragmentConstantBuffer[32] ;
    VkSpecializationMapEntry fragmentMapEntries[32];
    if (descriptor->fragment) {
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

        fragShaderStageInfo.pName = descriptor->fragment->entryPoint.data;
        if((descriptor->fragment->module)->vulkanModuleMultiEP){
            fragShaderStageInfo.module = (descriptor->fragment->module)->vulkanModuleMultiEP;
        }
        else{
            // TODO: see above for the vertex case, same applies here
            fragShaderStageInfo.module = (descriptor->fragment->module)->modules[WGPUShaderStageEnum_Fragment].module;
        }
        if(descriptor->fragment->constantCount){
            fragmentSpecInfo.pData = fragmentConstantBuffer;
            for(uint32_t i = 0;i < descriptor->fragment->constantCount;i++){
                fragmentConstantBuffer[i] = descriptor->fragment->constants[i].value;
                fragmentMapEntries[i].constantID = i;
                fragmentMapEntries[i].offset = i * sizeof(double);
                fragmentMapEntries[i].size = sizeof(i);
            }
            fragmentSpecInfo.dataSize = descriptor->fragment->constantCount * sizeof(double);
            fragmentSpecInfo.mapEntryCount = descriptor->fragment->constantCount;
            fragmentSpecInfo.pMapEntries = fragmentMapEntries;
            fragShaderStageInfo.pSpecializationInfo = &fragmentSpecInfo;
        }
        shaderStages[shaderStageInsertPos++] = fragShaderStageInfo;
    }

    // Vertex Input State
    VkVertexInputBindingDescription   bindingDescriptions  [32] = {(VkVertexInputBindingDescription)  {0}};
    VkVertexInputAttributeDescription attributeDescriptions[32] = {(VkVertexInputAttributeDescription){0}};
    uint32_t attributeDescriptionCount = 0;
    uint32_t currentBinding = 0;
    for (size_t i = 0; i < descriptor->vertex.bufferCount; ++i) {
        const WGPUVertexBufferLayout* layout = &descriptor->vertex.buffers[i];
        bindingDescriptions[i].binding = currentBinding; // Assuming bindings are contiguous from 0, I think webgpu doesn't allow anything else
        bindingDescriptions[i].stride = (uint32_t)layout->arrayStride;
        bindingDescriptions[i].inputRate = toVulkanVertexStepMode(layout->stepMode);

        for (size_t j = 0; j < layout->attributeCount; ++j) {
            const WGPUVertexAttribute* attrib = &layout->attributes[j];
            
            attributeDescriptions[attributeDescriptionCount++] = (VkVertexInputAttributeDescription){
                .location = attrib->shaderLocation,
                .binding = currentBinding,
                .format = toVulkanVertexFormat(attrib->format),
                .offset = (uint32_t)attrib->offset
            };
        }
        currentBinding++;
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo zeroinit;
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = descriptor->vertex.bufferCount;
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions;
    vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptionCount;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    // Input Assembly State
    VkPipelineInputAssemblyStateCreateInfo inputAssembly zeroinit;
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = toVulkanPrimitiveTopology(descriptor->primitive.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE; // WGPU doesn't expose primitive restart? Assume false.
    // Strip index formats are for degenerate triangles/lines, only matters if topology is strip and primitiveRestart is enabled.
    // Vulkan handles this via primitiveRestartEnable flag usually. VkIndexType part is handled by vkCmdBindIndexBuffer.

    // Viewport State (Dynamic)
    VkPipelineViewportStateCreateInfo viewportState zeroinit;
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization State
    VkPipelineRasterizationStateCreateInfo rasterizer zeroinit;
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_TRUE; // Usually false unless specific features needed
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Default
    if(descriptor->primitive.nextInChain && descriptor->primitive.nextInChain->sType == WGPUSType_PrimitiveLineWidthInfo){
        rasterizer.lineWidth = (float)((WGPUPrimitiveLineWidthInfo*)descriptor->primitive.nextInChain)->lineWidth;
    }
    else{
        rasterizer.lineWidth = 1.0f;
    }
    rasterizer.cullMode =  toVulkanCullMode(descriptor->primitive.cullMode);
    rasterizer.frontFace = toVulkanFrontFace(descriptor->primitive.frontFace);
    rasterizer.depthBiasEnable = descriptor->depthStencil ? (descriptor->depthStencil->depthBias != 0 || descriptor->depthStencil->depthBiasSlopeScale != 0.0f || descriptor->depthStencil->depthBiasClamp != 0.0f) : VK_FALSE;
    rasterizer.depthBiasConstantFactor = descriptor->depthStencil ? (float)descriptor->depthStencil->depthBias : 0.0f;
    rasterizer.depthBiasClamp = descriptor->depthStencil ? descriptor->depthStencil->depthBiasClamp : 0.0f;
    rasterizer.depthBiasSlopeFactor = descriptor->depthStencil ? descriptor->depthStencil->depthBiasSlopeScale : 0.0f;
    // TODO: Handle descriptor->primitive.unclippedDepth (requires VK_EXT_depth_clip_enable or VK 1.3 feature)
    // If unclippedDepth is true, rasterizer.depthClampEnable should probably be VK_FALSE (confusingly).
    // Requires checking device features. Assume false for now.

    // Multisample State
    VkPipelineMultisampleStateCreateInfo multisampling zeroinit;
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE; // Basic case
    multisampling.rasterizationSamples = (VkSampleCountFlagBits)descriptor->multisample.count; // Assume direct mapping
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = &descriptor->multisample.mask;
    multisampling.alphaToCoverageEnable = descriptor->multisample.alphaToCoverageEnabled ? VK_TRUE : VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE; // Basic case

    // Depth Stencil State (Optional)
    bool stencilEnable = descriptor->depthStencil && 
    ((descriptor->depthStencil->format == WGPUTextureFormat_Depth24PlusStencil8) ||  
    (descriptor->depthStencil->format == WGPUTextureFormat_Depth32FloatStencil8));
    VkPipelineDepthStencilStateCreateInfo depthStencil zeroinit;
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    if (descriptor->depthStencil) {
        const WGPUDepthStencilState* ds = descriptor->depthStencil;
        depthStencil.depthTestEnable = VK_TRUE; // If struct exists, assume depth test is desired
        depthStencil.depthWriteEnable = ds->depthWriteEnabled ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = toVulkanCompareFunction(ds->depthCompare);
        depthStencil.depthBoundsTestEnable = VK_FALSE; // Not in WGPU descriptor
        depthStencil.minDepthBounds = 0.0f;
        depthStencil.maxDepthBounds = 1.0f;

        //bool stencilTestRequired =
        //    ds->stencilFront.compare != WGPUCompareFunction_Undefined || ds->stencilFront.failOp != WGPUStencilOperation_Undefined ||
        //    ds->stencilBack.compare != WGPUCompareFunction_Undefined || ds->stencilBack.failOp != WGPUStencilOperation_Undefined ||
        //    ds->stencilReadMask != 0 || ds->stencilWriteMask != 0;

        depthStencil.stencilTestEnable = stencilEnable ? VK_TRUE : VK_FALSE;
        if(stencilEnable) {
            depthStencil.front.failOp      = toVulkanStencilOperation(ds->stencilFront.failOp);
            depthStencil.front.passOp      = toVulkanStencilOperation(ds->stencilFront.passOp);
            depthStencil.front.depthFailOp = toVulkanStencilOperation(ds->stencilFront.depthFailOp);
            depthStencil.front.compareOp   = toVulkanCompareFunction(ds->stencilFront.compare);
            depthStencil.front.compareMask = ds->stencilReadMask;
            depthStencil.front.writeMask   = ds->stencilWriteMask;
            depthStencil.front.reference   = 0; // Dynamic state usually

            depthStencil.back.failOp       = toVulkanStencilOperation(ds->stencilBack.failOp);
            depthStencil.back.passOp       = toVulkanStencilOperation(ds->stencilBack.passOp);
            depthStencil.back.depthFailOp  = toVulkanStencilOperation(ds->stencilBack.depthFailOp);
            depthStencil.back.compareOp    = toVulkanCompareFunction(ds->stencilBack.compare);
            depthStencil.back.compareMask  = ds->stencilReadMask;
            depthStencil.back.writeMask    = ds->stencilWriteMask;
            depthStencil.back.reference    = 0; // Dynamic state usually
        }
    } else {
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
    }

    // Color Blend State (Requires fragment shader)
    VkPipelineColorBlendAttachmentState colorBlendAttachments[MAX_COLOR_ATTACHMENTS];
    VkPipelineColorBlendStateCreateInfo colorBlending zeroinit;
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.blendConstants[0] = 1.0f;
    colorBlending.blendConstants[1] = 1.0f;
    colorBlending.blendConstants[2] = 1.0f;
    colorBlending.blendConstants[3] = 1.0f;

    if (descriptor->fragment) {
        for (size_t i = 0; i < descriptor->fragment->targetCount; ++i) {
            const WGPUColorTargetState* target = &descriptor->fragment->targets[i];
            // Defaults for no blending
            if(target->writeMask == WGPUColorWriteMask_None){
                fprintf(stderr, "RenderPipelineDescriptor->fragment->target[%u] has an empty color write mask\n", (unsigned)i);
            }
            colorBlendAttachments[i].colorWriteMask =  toVulkanColorWriteMask(target->writeMask);
            colorBlendAttachments[i].blendEnable = VK_FALSE;
            colorBlendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachments[i].alphaBlendOp = VK_BLEND_OP_ADD;

            if (target->blend) {
                colorBlendAttachments[i].blendEnable = VK_TRUE; // Enable if blend state is provided
                colorBlendAttachments[i].srcColorBlendFactor = toVulkanBlendFactor(target->blend->color.srcFactor);
                colorBlendAttachments[i].dstColorBlendFactor = toVulkanBlendFactor(target->blend->color.dstFactor);
                colorBlendAttachments[i].colorBlendOp = toVulkanBlendOperation(target->blend->color.operation);
                colorBlendAttachments[i].srcAlphaBlendFactor = toVulkanBlendFactor(target->blend->alpha.srcFactor);
                colorBlendAttachments[i].dstAlphaBlendFactor = toVulkanBlendFactor(target->blend->alpha.dstFactor);
                colorBlendAttachments[i].alphaBlendOp = toVulkanBlendOperation(target->blend->alpha.operation);
            }
        }
        colorBlending.attachmentCount = descriptor->fragment->targetCount;
        colorBlending.pAttachments = colorBlendAttachments;
    } else {
        // No fragment shader means no color attachments needed? Check spec.
        // Typically, rasterizerDiscardEnable would be true if no fragment shader, but let's allow it.
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = NULL;
    }


    // Dynamic State
    uint32_t dynamicStateCount = 3;
    VkDynamicState dynamicStates[4] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE
    };
    if (depthStencil.stencilTestEnable) {
        dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    }
    


    VkPipelineDynamicStateCreateInfo dynamicState zeroinit;
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStateCount;
    dynamicState.pDynamicStates = dynamicStates;   
    #if VULKAN_USE_DYNAMIC_RENDERING == 1
    VkRenderPass rprp = NULL;
    VkFormat cAttachmentFormats[MAX_COLOR_ATTACHMENTS];
    for(uint32_t i = 0;i < descriptor->fragment->targetCount;i++){
        cAttachmentFormats[i] = toVulkanPixelFormat(descriptor->fragment->targets[i].format);
    }

    
    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = NULL,
        .colorAttachmentCount = descriptor->fragment->targetCount,
        .pColorAttachmentFormats = cAttachmentFormats,
        .depthAttachmentFormat = descriptor->depthStencil ? toVulkanPixelFormat(descriptor->depthStencil->format) : VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = stencilEnable ? toVulkanPixelFormat(descriptor->depthStencil->format) : VK_FORMAT_UNDEFINED
    };
    VkPipelineRenderingCreateInfo* pRenderingCreateInfo = &renderingCreateInfo;
    #else
    RenderPassLayout renderPassLayout = {0};
    for(uint32_t i = 0;i < descriptor->fragment->targetCount;i++){
        const WGPUColorTargetState* ctarget = descriptor->fragment->targets + i;
        renderPassLayout.colorAttachments[i] = (AttachmentDescriptor){
            .sampleCount = multisampling.rasterizationSamples,
            .format = toVulkanPixelFormat(ctarget->format),
            .loadop = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeop = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        };
    }
    renderPassLayout.colorAttachmentCount = descriptor->fragment->targetCount;
    if(descriptor->depthStencil){
        renderPassLayout.depthAttachmentPresent = 1;
        renderPassLayout.depthAttachment = (AttachmentDescriptor){
            .format = toVulkanPixelFormat(descriptor->depthStencil->format),
            .sampleCount = multisampling.rasterizationSamples,
            .loadop = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeop = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        };
    }
    LayoutedRenderPass lrp = LoadRenderPassFromLayout(device, renderPassLayout);
    VkRenderPass rprp = lrp.renderPass;
    VkPipelineRenderingCreateInfo* pRenderingCreateInfo = NULL;
    #endif
    const VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = pRenderingCreateInfo,
        .stageCount = shaderStageInsertPos,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = (descriptor->depthStencil) ? &depthStencil : NULL,
        .pColorBlendState = (descriptor->fragment) ? &colorBlending : NULL,
        .pDynamicState = &dynamicState,
        .layout = pl_layout->layout,
        .renderPass = rprp,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE, // Optional
        .basePipelineIndex = -1, // Optional
    };
    
    


    WGPURenderPipeline pipelineImpl = (WGPURenderPipeline)RL_CALLOC(1, sizeof(WGPURenderPipelineImpl));
    pipelineImpl->refCount = 1;
    if (!pipelineImpl) {
        // Handle allocation failure
        return NULL;
    }
    pipelineImpl->device = device;
    //wgpuDeviceAddRef(device);
    pipelineImpl->layout = pl_layout; // Store for potential use
    wgpuPipelineLayoutAddRef(pl_layout);
    VkResult result = device->functions.vkCreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipelineImpl->renderPipeline);

    if (result != VK_SUCCESS) {
        // Handle pipeline creation failure
        RL_FREE(pipelineImpl);
        // Optionally log error based on result code
        return NULL;
    }

    return pipelineImpl;
    EXIT();
}


void wgpuTextureRelease(WGPUTexture texture){
    ENTRY();
    --texture->refCount;
    if(texture->refCount == 0){
        WGPUDevice device = texture->device;
        Texture_ViewCache_kv_pair* viewCacheTable = texture->viewCache.table;
        texture->device->functions.vkDestroyImage(texture->device->device, texture->image, NULL);
        texture->device->functions.vkFreeMemory(texture->device->device, texture->memory, NULL);
        for(size_t i = 0;i < texture->viewCache.current_capacity;i++){
            if(viewCacheTable[i].key.format != VK_FORMAT_UNDEFINED){
                device->functions.vkDestroyImageView(device->device, viewCacheTable[i].value->view, NULL);
                RL_FREE(viewCacheTable[i].value);
            }
        }
        Texture_ViewCache_free(&texture->viewCache);
        RL_FREE(texture);
    }
    EXIT();
}
void wgpuTextureViewRelease(WGPUTextureView view){
    ENTRY();
    --view->refCount;
    if(view->refCount == 0){
        wgpuTextureRelease(view->texture);
        // This is commented out because views get cached in WGPUTexture
        // view->texture->device->functions.vkDestroyImageView(view->texture->device->device, view->view, NULL);
        // RL_FREE(view);
    }
    EXIT();
}

void wgpuCommandEncoderCopyBufferToBuffer  (WGPUCommandEncoder commandEncoder, WGPUBuffer source, uint64_t sourceOffset, WGPUBuffer destination, uint64_t destinationOffset, uint64_t size){
    ENTRY();
    ++commandEncoder->encodedCommandCount;
    ce_trackBuffer(
        commandEncoder,
        source,
        (BufferUsageSnap){
            .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_READ_BIT
        }
    );
    

    const VkBufferCopy copy = {
        .srcOffset = sourceOffset,
        .dstOffset = destinationOffset,
        .size = size
    };

    commandEncoder->device->functions.vkCmdCopyBuffer(commandEncoder->buffer, source->buffer, destination->buffer, 1, &copy);
    //if(destination->usage & (WGPUBufferUsage_MapWrite | WGPUBufferUsage_MapRead)){
    //    const VkMemoryBarrier memoryBarrier = {
    //        VK_STRUCTURE_TYPE_MEMORY_BARRIER,
    //        NULL,
    //        VK_ACCESS_TRANSFER_WRITE_BIT,
    //        VK_ACCESS_HOST_READ_BIT
    //    };
    //    commandEncoder->device->functions.vkCmdPipelineBarrier(
    //        commandEncoder->buffer,
    //        VK_PIPELINE_STAGE_TRANSFER_BIT,
    //        VK_PIPELINE_STAGE_HOST_BIT,
    //        0,
    //        1, &memoryBarrier, 
    //        0, NULL, 
    //        0, NULL
    //    );
    //}

    ce_trackBuffer(
        commandEncoder,
        destination,
        (BufferUsageSnap){
            .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_WRITE_BIT
        }
    );
    EXIT();
}
void wgpuCommandEncoderCopyBufferToTexture (WGPUCommandEncoder commandEncoder, const WGPUTexelCopyBufferInfo* source, const WGPUTexelCopyTextureInfo* destination, WGPUExtent3D const * copySize){
    ENTRY();
    
    ++commandEncoder->encodedCommandCount;
    
    const VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = source->layout.bytesPerRow / vkFormatSize(destination->texture->format),
        .bufferImageHeight = source->layout.rowsPerImage,
        .imageSubresource.aspectMask = toVulkanAspectMaskVk(destination->aspect, destination->texture->format),
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageOffset = CLITERAL(VkOffset3D){
            (int32_t)destination->origin.x,
            (int32_t)destination->origin.y,
            (int32_t)destination->origin.z,
        },
        .imageExtent = CLITERAL(VkExtent3D){
            copySize->width,
            copySize->height,
            copySize->depthOrArrayLayers
        },
    };
    
    ce_trackBuffer(commandEncoder, source->buffer, (BufferUsageSnap){VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT});
    ce_trackTexture(commandEncoder, destination->texture, (ImageUsageSnap){
        .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .access = VK_ACCESS_TRANSFER_WRITE_BIT,
        .subresource = {
            .aspectMask = destination->aspect,
            .baseMipLevel = destination->mipLevel,
            .levelCount = 1,
            .baseArrayLayer = destination->origin.z,
            .layerCount = 1
        }
    });

    commandEncoder->device->functions.vkCmdCopyBufferToImage(commandEncoder->buffer, source->buffer->buffer, destination->texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    EXIT();
}
void wgpuCommandEncoderCopyTextureToBuffer (WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyBufferInfo* destination, const WGPUExtent3D* copySize){
    ENTRY();
    ++commandEncoder->encodedCommandCount;
    ce_trackTexture(
        commandEncoder,
        source->texture,
        (ImageUsageSnap){
            .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .stage  = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_READ_BIT,
            .subresource = {
                .aspectMask     = toVulkanAspectMaskVk(source->aspect, source->texture->format),
                .baseMipLevel   = source->mipLevel,
                .baseArrayLayer = source->origin.z, // ?
                .layerCount     = 1,
                .levelCount     = 1,
            }
    });
    ce_trackBuffer(commandEncoder, destination->buffer, (BufferUsageSnap){VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT});
    
    VkBufferImageCopy region = {
        .bufferOffset = destination->layout.offset,
        .bufferRowLength = destination->layout.bytesPerRow / vkFormatSize(source->texture->format),
        .bufferImageHeight = destination->layout.rowsPerImage,
        .imageSubresource = {
            .aspectMask = toVulkanAspectMaskVk(source->aspect, source->texture->format),
            .baseArrayLayer = source->origin.z, // ?
            .mipLevel = source->mipLevel,
            .layerCount = 1,
        },
        .imageOffset = {
            .x = (int32_t)source->origin.x,
            .y = (int32_t)source->origin.y,
            .z = (int32_t)source->origin.z
        },
        .imageExtent = {
            .width  = copySize->width,
            .height = copySize->height,
            .depth  = copySize->depthOrArrayLayers
        }
    };
    commandEncoder->device->functions.vkCmdCopyImageToBuffer(
        commandEncoder->buffer,
        source->texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        destination->buffer->buffer,
        1, &region
    );
    EXIT();
}
void wgpuCommandEncoderCopyTextureToTexture(WGPUCommandEncoder commandEncoder, const WGPUTexelCopyTextureInfo* source, const WGPUTexelCopyTextureInfo* destination, const WGPUExtent3D* copySize){
    ENTRY();
    ++commandEncoder->encodedCommandCount;
    ce_trackTexture(
        commandEncoder,
        source->texture,
        (ImageUsageSnap){
            .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .stage  = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_READ_BIT,
            .subresource = {
                .aspectMask     = source->aspect,
                .baseMipLevel   = source->mipLevel,
                .baseArrayLayer = source->origin.z, // ?
                .layerCount     = 1,
                .levelCount     = 1,
            }
    });
    ce_trackTexture(
        commandEncoder,
        destination->texture,
        (ImageUsageSnap){
            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .stage  = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .access = VK_ACCESS_TRANSFER_WRITE_BIT,
            .subresource = {
                .aspectMask     = destination->aspect,
                .baseMipLevel   = destination->mipLevel,
                .baseArrayLayer = destination->origin.z, // ?
                .layerCount     = 1,
                .levelCount     = 1,
            }
    });

    VkImageBlit region = {
        .srcSubresource = {
            .aspectMask = source->aspect,
            .mipLevel = source->mipLevel,
            .baseArrayLayer = destination->origin.z, // ?
            .layerCount = 1,
        },
        .srcOffsets = {
            {source->origin.x, source->origin.y, source->origin.z},
            {source->origin.x + copySize->width, source->origin.y + copySize->height, source->origin.z + copySize->depthOrArrayLayers}
        },
        .dstSubresource = {
            .aspectMask = destination->aspect,
            .mipLevel = destination->mipLevel,
            .baseArrayLayer = destination->origin.z, // ?
            .layerCount = 1,
        },
        .dstOffsets[0] = {destination->origin.x,                   destination->origin.y,                    destination->origin.z},
        .dstOffsets[1] = {destination->origin.x + copySize->width, destination->origin.y + copySize->height, destination->origin.z + copySize->depthOrArrayLayers}
    };
    commandEncoder->device->functions.vkCmdBlitImage(
        commandEncoder->buffer,
        source->texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        destination->texture->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region,
        VK_FILTER_NEAREST
    );
    EXIT();
}
void RenderPassEncoder_PushCommand(WGPURenderPassEncoder encoder, const RenderPassCommandGeneric* cmd){
    if(cmd->type == rp_command_type_set_render_pipeline){
        encoder->lastLayout = cmd->setRenderPipeline.pipeline->layout;
    }
    RenderPassCommandGenericVector_push_back(&encoder->bufferedCommands, *cmd);
}

void ComputePassEncoder_PushCommand(WGPUComputePassEncoder encoder, const RenderPassCommandGeneric* cmd){
    if(cmd->type == cp_command_type_set_compute_pipeline){
        encoder->lastLayout = cmd->setComputePipeline.pipeline->layout;
    }
    RenderPassCommandGenericVector_push_back(&encoder->bufferedCommands, *cmd);
}

void RaytracingPassEncoder_PushCommand(WGPURaytracingPassEncoder encoder, const RenderPassCommandGeneric* cmd){
    if(cmd->type == rp_command_type_set_raytracing_pipeline){
        encoder->lastLayout = cmd->setRaytracingPipeline.pipeline->layout;
    }
    RenderPassCommandGenericVector_push_back(&encoder->bufferedCommands, *cmd);
}


// Implementation of RenderpassEncoderDraw
void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder rpe, uint32_t vertices, uint32_t instances, uint32_t firstvertex, uint32_t firstinstance) {
    ENTRY();
    wgvk_assert(rpe != NULL, "RenderPassEncoderHandle is null");
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_draw,
        .draw = {
            vertices,
            instances,
            firstvertex,
            firstinstance
        }
    };

    RenderPassEncoder_PushCommand(rpe, &insert);
    EXIT();
}

// Implementation of RenderpassEncoderDrawIndexed
void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder rpe, uint32_t indices, uint32_t instances, uint32_t firstindex, int32_t baseVertex, uint32_t firstinstance) {
    ENTRY();
    wgvk_assert(rpe != NULL, "RenderPassEncoderHandle is null");
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_draw_indexed,
        .drawIndexed = {
            indices,
            instances,
            firstindex,
            baseVertex,
            firstinstance
        }
    };

    // Buffer the indexed draw command into the command buffer
    RenderPassEncoder_PushCommand(rpe, &insert);
    
    //vkCmdDrawIndexed(rpe->secondaryCmdBuffer, indices, instances, firstindex, (int32_t)(baseVertex & 0x7fffffff), firstinstance);
    EXIT();
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder rpe, WGPURenderPipeline renderPipeline) {
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_render_pipeline,
        .setRenderPipeline = {
            renderPipeline
        }
    };
    RenderPassEncoder_PushCommand(rpe, &insert);
    ru_trackRenderPipeline(&rpe->resourceUsage, renderPipeline);
    EXIT();
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder rpe, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets) {
    ENTRY();
    wgvk_assert(rpe != NULL, "RenderPassEncoderHandle is null");
    wgvk_assert(group != NULL, "DescriptorSetHandle is null");

    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_bind_group,
        .setBindGroup = {
            groupIndex,
            group,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            dynamicOffsetCount,
            dynamicOffsets
        }
    };
    
    RenderPassEncoder_PushCommand(rpe, &insert);
    
    
    

    for(uint32_t i = 0;i < group->entryCount;i++){

        const WGPUBindGroupEntry* entry = &group->entries[i];

        if(entry->buffer){
            const VkAccessFlags accessFlags = extractVkAccessFlags(group->layout->entries + i);
            const VkPipelineStageFlags stage = toVulkanPipelineStageBits(group->layout->entries[i].visibility);
            ce_trackBuffer(rpe->cmdEncoder, entry->buffer, (BufferUsageSnap){
                .stage = stage,
                .access = accessFlags
            });
        }

        if(entry->textureView){
            const VkAccessFlags accessFlags = extractVkAccessFlags(group->layout->entries + i);
            const VkPipelineStageFlags stage = toVulkanPipelineStageBits(group->layout->entries[i].visibility) | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            
            //VkImageLayout layout = (extractVkDescriptorType(group->layout->entries + i) == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            ce_trackTextureView(rpe->cmdEncoder, entry->textureView, (ImageUsageSnap){
                .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .access = accessFlags,
                .stage = stage,
                .subresource = entry->textureView->subresourceRange
            });
        }
    }
    ru_trackBindGroup(&rpe->resourceUsage, group);
    EXIT();
}

void wgpuComputePassEncoderSetPipeline (WGPUComputePassEncoder cpe, WGPUComputePipeline computePipeline){
    ENTRY();
    
    RenderPassCommandGeneric insert = {
        .type = cp_command_type_set_compute_pipeline,
        .setComputePipeline = {
            computePipeline
        }
    };

    ComputePassEncoder_PushCommand(cpe, &insert);
    EXIT();
}
void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder cpe, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets){
    ENTRY();
    
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_bind_group,
        .setBindGroup = {
            groupIndex,
            group,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            dynamicOffsetCount,
            dynamicOffsets
        }
    };
    cpe->bindGroups[groupIndex] = group;
    
    //for(uint32_t i = 0;i < group->entryCount;i++){
    //    const WGPUBindGroupEntry* entry = &group->entries[i];
    //    if(entry->buffer){
    //        const VkAccessFlags accessFlags = extractVkAccessFlags(group->layout->entries + i);
    //        const VkPipelineStageFlags stage = toVulkanPipelineStageBits(group->layout->entries[i].visibility);
    //        ce_trackBuffer(cpe->cmdEncoder, entry->buffer, (BufferUsageSnap){
    //            .stage = stage,
    //            .access = accessFlags
    //        });
    //    }
    //}

    ComputePassEncoder_PushCommand(cpe, &insert);
    EXIT();
}

WGPUComputePassEncoder wgpuCommandEncoderBeginComputePass(WGPUCommandEncoder commandEncoder, const WGPUComputePassDescriptor* cpdesc){
    ENTRY();
    WGPUComputePassEncoder ret = RL_CALLOC(1, sizeof(WGPUComputePassEncoderImpl));
    ++commandEncoder->encodedCommandCount;
    ret->refCount = 2;
    WGPUComputePassEncoderSet_add(&commandEncoder->referencedCPs, ret);

    RenderPassCommandGenericVector_init(&ret->bufferedCommands);

    ret->cmdEncoder = commandEncoder;
    ret->device = commandEncoder->device;
    return ret;
    EXIT();
}
void wgpuComputePassEncoderEnd(WGPUComputePassEncoder commandEncoder){
    ENTRY();
    recordVkCommands(commandEncoder->cmdEncoder, commandEncoder->device, &commandEncoder->bufferedCommands, NULL);
    EXIT();
}
void wgpuComputePassEncoderRelease(WGPUComputePassEncoder cpenc){
    ENTRY();
    --cpenc->refCount;
    if(cpenc->refCount == 0){
        releaseAllAndClear(&cpenc->resourceUsage);
        RenderPassCommandGenericVector_free(&cpenc->bufferedCommands);
        RL_FREE(cpenc);
    }
    EXIT();
}

void wgpuReleaseRaytracingPassEncoder(WGPURaytracingPassEncoder rtenc){
    ENTRY();
    --rtenc->refCount;
    if(rtenc->refCount == 0){
        releaseAllAndClear(&rtenc->resourceUsage);
        RL_FREE(rtenc);
    }
    EXIT();
}

void pcmNonnullFlattenCallback(void* fence_, WGPUCommandBufferVector* key, void* userdata){
    WGPUFenceVector* vector = (WGPUFenceVector*)userdata;
    WGPUFence fence = (WGPUFence)fence_;
    if(fence != VK_NULL_HANDLE){
        WGPUFenceVector_push_back(vector, fence);
    }
}



WGPURaytracingPassEncoder wgpuCommandEncoderBeginRaytracingPass(WGPUCommandEncoder enc, const WGPURayTracingPassDescriptor* rtDesc){
    ENTRY();
    WGPURaytracingPassEncoder rtenc = RL_CALLOC(1, sizeof(WGPURaytracingPassEncoderImpl));
    rtenc->device = enc->device;
    rtenc->refCount = 2;
    RenderPassCommandGenericVector_init(&rtenc->bufferedCommands);
    rtenc->cmdEncoder = enc;
    rtenc->cmdBuffer = enc->buffer;
    EXIT();
    return rtenc;
}

void wgpuRaytracingPassEncoderEnd(WGPURaytracingPassEncoder rtPassEncoder){
    ENTRY();
    for(size_t i = 0;i < rtPassEncoder->bufferedCommands.size;i++){
        const RenderPassCommandGeneric* cmd = &rtPassEncoder->bufferedCommands.data[i];
        if(cmd->type == rp_command_type_set_bind_group){
            const RenderPassCommandSetBindGroup* cmdSetBindGroup = &cmd->setBindGroup;
            const WGPUBindGroup       group  = cmdSetBindGroup->group;
            const WGPUBindGroupLayout layout = group->layout;
            for(uint32_t bindingIndex = 0;bindingIndex < layout->entryCount;bindingIndex++){

                wgvk_assert(group->entries[bindingIndex].binding == layout->entries[bindingIndex].binding, "Mismatch between layout and group, this will cause bugs.");
                
                const WGPUBindGroupEntry*       groupEntry  = &group ->entries[bindingIndex];
                const WGPUBindGroupLayoutEntry* layoutEntry = &layout->entries[bindingIndex];

                //uniform_type eType = layout->entries[bindingIndex].type;
                if(layout->entries[bindingIndex].buffer.type != WGPUBufferBindingType_BindingNotUsed){
                    wgvk_assert(group->entries[bindingIndex].buffer, "Layout indicates buffer but no buffer passed");
                    WGPUShaderStage visibility = layout->entries[bindingIndex].visibility;
                    wgvk_assert(visibility, "Empty visibility goddamnit");
                    ce_trackBuffer(
                        rtPassEncoder->cmdEncoder,
                        group->entries[bindingIndex].buffer,
                        (BufferUsageSnap){
                            .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            //.access = access_to_vk[layout->entries[bindingIndex].access], //TODO
                            .stage = toVulkanPipelineStageBits(visibility)
                        }
                    );
                }

                else if(layout->entries[bindingIndex].texture.sampleType != WGPUTextureSampleType_BindingNotUsed){
                    WGPUShaderStage visibility = layout->entries[bindingIndex].visibility;
                    wgvk_assert(visibility, "Empty visibility goddamnit");
                    if(visibility == 0){ //TODO: Get rid of this hack
                        visibility = (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute);
                    }
                    ce_trackTextureView(
                        rtPassEncoder->cmdEncoder,
                        group->entries[bindingIndex].textureView,
                        (ImageUsageSnap){
                            .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            .access = VK_ACCESS_SHADER_READ_BIT,
                            .stage = toVulkanPipelineStageBits(visibility)
                        }
                    );
                }
                else if(layout->entries[bindingIndex].storageTexture.access != WGPUStorageTextureAccess_BindingNotUsed){
                    WGPUShaderStage visibility = layout->entries[bindingIndex].visibility;
                    wgvk_assert(visibility, "Empty visibility goddamnit");
                    if(visibility == 0){ //TODO: Get rid of this hack
                        visibility = (WGPUShaderStage_Vertex | WGPUShaderStage_Fragment | WGPUShaderStage_Compute);
                    }
                    ce_trackTextureView(
                        rtPassEncoder->cmdEncoder,
                        group->entries[bindingIndex].textureView,
                        (ImageUsageSnap){
                            .layout = VK_IMAGE_LAYOUT_GENERAL,
                            .access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                            .stage = toVulkanPipelineStageBits(visibility)
                        }
                    );
                }
            }
        }
    }
    recordVkCommands(rtPassEncoder->cmdEncoder, rtPassEncoder->device, &rtPassEncoder->bufferedCommands, NULL);
    EXIT();
}
void wgpuFenceReset(WGPUFence fence){
    wgvk_assert(atomic_load_explicit(&fence->state, memory_order_acquire) == WGPUFenceState_Finished, "Fence must be finished");
    fence->device->functions.vkResetFences(fence->device->device, 1, &fence->fence);
    atomic_store_explicit(&fence->state, memory_order_release, WGPUFenceState_Reset);
}


void resetFenceAndReleaseBuffers(void* fence_, WGPUCommandBufferVector* cBuffers, void* wgpudevice){
    WGPUDevice device = (WGPUDevice)wgpudevice;
    if(fence_){
        WGPUFence fence = fence_;
        device->functions.vkResetFences(device->device, 1, &fence->fence);
        fence->state = WGPUFenceState_Reset;
        wgpuFenceRelease(fence);
    }
    for(size_t i = 0;i < cBuffers->size;i++){
        WGPUCommandBuffer relBuffer = cBuffers->data[i];
        BufferUsageRecordMap* bmap = &relBuffer->resourceUsage.referencedBuffers;
        for(size_t bi = 0; bi < bmap->current_capacity;bi++){
            BufferUsageRecordMap_kv_pair* kvp = bmap->table + bi;
            if(kvp->key != PHM_EMPTY_SLOT_KEY){
                WGPUBuffer buffer = (WGPUBuffer)kvp->key;
                if(buffer->latestFence){
                    wgpuFenceRelease(buffer->latestFence);
                    buffer->latestFence = VK_NULL_HANDLE;
                }
            }
        }
        if(cBuffers->data[i]->label.data){
            volatile int x = 3;
        }
        wgpuCommandBufferRelease(cBuffers->data[i]);
    }
    WGPUCommandBufferVector_free(cBuffers);
}

void wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture* surfaceTexture){
    ENTRY();
    const size_t submittedframes = surface->device->submittedFrames;
    const uint32_t cacheIndex = surface->device->submittedFrames % framesInFlight;
    SyncState* syncState = DeviceGetSyncState(surface->device, cacheIndex);
    if(surface->swapchain){
        VkResult acquireResult = surface->device->functions.vkAcquireNextImageKHR(
            surface->device->device,
            surface->swapchain,
            UINT32_MAX,
            syncState->acquireImageSemaphore,
            VK_NULL_HANDLE,
            &surface->activeImageIndex
        );
        if(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR){
            syncState->acquireImageSemaphoreSignalled = true;
        }
        switch(acquireResult){
            case VK_ERROR_SURFACE_LOST_KHR:
                surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_Lost;
            break;
            case VK_SUBOPTIMAL_KHR:
                surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
            break;
            case VK_SUCCESS:
                surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal;
            break;
            case VK_TIMEOUT:
                surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_Timeout;
            break;
            case VK_ERROR_OUT_OF_DATE_KHR:
                surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_Outdated;
            break;
            default:
                surfaceTexture->status = WGPUSurfaceGetCurrentTextureStatus_Error;
            break;
        }
        if(surfaceTexture->status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal || surfaceTexture->status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal){
            surfaceTexture->texture = surface->images[surface->activeImageIndex];
            wgpuTextureAddRef(surface->images[surface->activeImageIndex]);
        }
    }
    else{
        DeviceCallback(surface->device, WGPUErrorType_Validation, STRVIEW("Surface is not configured"));
    }
    EXIT();
}

void wgpuSurfacePresent(WGPUSurface surface){
    ENTRY();
    WGPUDevice device = surface->device;
    uint32_t cacheIndex = surface->device->submittedFrames % framesInFlight;
    PerframeCache* frameCache = DeviceGetFIFCache(surface->device, cacheIndex);
    PendingCommandBufferMap* pcm = &frameCache->pendingCommandBuffers;
    SyncState* syncState = &frameCache->syncState;
    VkCommandBuffer transitionBuffer = frameCache->finalTransitionBuffer;
    
    VkCommandBufferBeginInfo transitionBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    device->functions.vkBeginCommandBuffer(transitionBuffer, &transitionBufferBeginInfo);

    VkImageMemoryBarrier finalBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        surface->images[surface->activeImageIndex]->layout,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        surface->device->adapter->queueIndices.graphicsIndex,
        surface->device->adapter->queueIndices.graphicsIndex,
        surface->images[surface->activeImageIndex]->image,
        {
            VK_IMAGE_ASPECT_COLOR_BIT,
            0, VK_REMAINING_MIP_LEVELS,
            0, VK_REMAINING_ARRAY_LAYERS
        }
    };
    device->functions.vkCmdPipelineBarrier(
        transitionBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &finalBarrier  
    );
    surface->images[surface->activeImageIndex]->layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    device->functions.vkEndCommandBuffer(transitionBuffer);

    VkPipelineStageFlags wsmask[2] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore waitSemaphores[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    uint32_t waitSemaphoreCount = 0;
    if(syncState->submits > 0){
        waitSemaphores[waitSemaphoreCount++] = syncState->semaphores.data[syncState->submits];
    }
    if(syncState->acquireImageSemaphoreSignalled){
        waitSemaphores[waitSemaphoreCount++] = syncState->acquireImageSemaphore;
        syncState->acquireImageSemaphoreSignalled = false;
    }
    const VkSubmitInfo cbsinfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &transitionBuffer,
        .signalSemaphoreCount = 1,
        .waitSemaphoreCount = waitSemaphoreCount,
        .pWaitDstStageMask = wsmask,
        .pWaitSemaphores = waitSemaphores,
        .pSignalSemaphores = surface->presentSemaphores + surface->activeImageIndex
    };
    
    
    WGPUFence finalTransitionFence = frameCache->finalTransitionFence;
    wgpuFenceAddRef(finalTransitionFence);
    //printf("Submitting %p with fence %p\n", transitionBuffer, finalTransitionFence);
    device->functions.vkQueueSubmit(surface->device->queue->graphicsQueue, 1, &cbsinfo, finalTransitionFence->fence);
    
    finalTransitionFence->state = WGPUFenceState_InUse;
    
    WGPUCommandBufferVector* cmdBuffers = PendingCommandBufferMap_get(pcm, (void*)finalTransitionFence);
    
    if(cmdBuffers == NULL){
        WGPUCommandBufferVector insert = {0};
        PendingCommandBufferMap_put(pcm, finalTransitionFence, insert);
        cmdBuffers = PendingCommandBufferMap_get(pcm, (void*)finalTransitionFence);
        WGPUCommandBufferVector_init(cmdBuffers);
    }

    VkPresentInfoKHR presentInfo  = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = surface->presentSemaphores + surface->activeImageIndex,
        .swapchainCount = 1,
        .pSwapchains = &surface->swapchain,
        .pImageIndices = &surface->activeImageIndex,
    };

    VkResult presentRes = device->functions.vkQueuePresentKHR(surface->device->queue->presentQueue, &presentInfo);
    if(presentRes != VK_SUCCESS && presentRes != VK_SUBOPTIMAL_KHR){
        fprintf(stderr, "vkQueuePresentKHR returned %s\n", vkErrorString(presentRes));
    }
    wgpuDeviceTick(surface->device);
    EXIT();
}
void wgpuDeviceTick(WGPUDevice device){
    ENTRY();
    WGPUQueue queue = device->queue;
    WGPUCommandBufferDescriptor cbd = {
        .label = STRVIEW("PresubmitCache"),
    };
    WGPUCommandBuffer buffer = wgpuCommandEncoderFinish(queue->presubmitCache, &cbd);
    wgpuCommandEncoderRelease(queue->presubmitCache);
    wgpuCommandBufferRelease(buffer);
    
    {
        const uint32_t toBeFinishedCacheIndex = device->submittedFrames % framesInFlight;
        PerframeCache* frameCachetbf = DeviceGetFIFCache(device, toBeFinishedCacheIndex);
        PendingCommandBufferMap* pcmtbf = &frameCachetbf->pendingCommandBuffers;
        SyncState* syncStatetbf = &frameCachetbf->syncState;

        const VkPipelineStageFlags waitmask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        const uint32_t tsubmits = syncStatetbf->submits;
        WGPUCommandBufferVector* pendingForFTF = PendingCommandBufferMap_get(pcmtbf, frameCachetbf->finalTransitionFence);
        
        if(pendingForFTF == NULL){
            VkSubmitInfo emptySubmit = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = VkSemaphoreVector_get(&syncStatetbf->semaphores, tsubmits),
                .pWaitDstStageMask = &waitmask
            };
            device->functions.vkQueueSubmit(device->queue->graphicsQueue, 1, &emptySubmit, frameCachetbf->finalTransitionFence->fence);
            wgpuFenceAddRef(frameCachetbf->finalTransitionFence);
            frameCachetbf->finalTransitionFence->state = WGPUFenceState_InUse;
            WGPUCommandBufferVector insert;
            WGPUCommandBufferVector_init(&insert);
            WGPUFence ftf = frameCachetbf->finalTransitionFence;

            WGPUCommandBufferVector* inserted = PendingCommandBufferMap_get(pcmtbf, ftf);
            if(inserted == NULL){
                PendingCommandBufferMap_put(pcmtbf, ftf, insert);
                inserted = PendingCommandBufferMap_get(pcmtbf, ftf);
            }
            //WGPUCommandBufferVector_push_back(inserted, WGPUCommandBuffer value)
            //WGPUCommandBufferVector_init(inserted);
        }
    }
    
    
    ++device->submittedFrames;
    #if USE_VMA_ALLOCATOR == 1
    vmaSetCurrentFrameIndex(device->allocator, device->submittedFrames % framesInFlight);
    #endif
    
    

    uint32_t cacheIndex = device->submittedFrames % framesInFlight;
    PerframeCache* frameCacheMew = DeviceGetFIFCache(device, cacheIndex);
    PendingCommandBufferMap* pcmNew = &frameCacheMew->pendingCommandBuffers;
    SyncState* syncStateNew = &frameCacheMew->syncState;
    size_t pcmSize = pcmNew->current_size;

    WGPUFenceVector fences;
    WGPUFenceVector_init(&fences);

    if(pcmNew->current_size > 0){
        PendingCommandBufferMap_for_each(pcmNew, pcmNonnullFlattenCallback, (void*)&fences);
        if(fences.size > 0){
            wgpuFencesWait(fences.data, fences.size, UINT32_MAX);
            //printf("Waiting for fences:\n");
            //for(uint32_t i = 0;i < fences.size;i++){
            //    printf("  %p\n", fences.data[i]);
            //}
        }
    }
    else{
        //TRACELOG(WGPU_LOG_INFO, "No fences!");
    }

    PendingCommandBufferMap_for_each(pcmNew, resetFenceAndReleaseBuffers, device);    
    WGPUFenceVector_free(&fences);

    WGPUBufferVector* usedBuffers = &frameCacheMew->usedBatchBuffers;
    WGPUBufferVector* unusedBuffers = &frameCacheMew->unusedBatchBuffers;
    if(unusedBuffers->capacity < unusedBuffers->size + usedBuffers->size){
        size_t newcap = (unusedBuffers->size + usedBuffers->size);
        WGPUBufferVector_reserve(unusedBuffers, newcap);
    }
    if(usedBuffers->size > 0){
        memcpy(unusedBuffers->data + unusedBuffers->size, usedBuffers->data, usedBuffers->size * sizeof(WGPUBuffer));
    }
    unusedBuffers->size += usedBuffers->size;
    WGPUBufferVector_clear(usedBuffers);//(WGPUBufferVector *dest, const WGPUBufferVector *source)
    

    VkCommandPool poolToClear = frameCacheMew->commandPool;

    // This line is currently commented out because they cause an 
    // issue with CommandEncoders living across wgpuDeviceTick (and wgpuSurfacePresent) calls
    //
    // device->functions.vkResetCommandPool(device->device, poolToClear, 0);

    for(size_t i = 0;i < pcmNew->current_capacity;i++){
        PendingCommandBufferMap_kv_pair* entry_pair = pcmNew->table + i;
        if(entry_pair->key){
            const WGPUCommandBufferVector* vector = &entry_pair->value;
            for(size_t j = 0;j < vector->size;j++){
                //device->functions.vkFreeCommandBuffers(device->device, poolToClear, 1, &vector->data[j]->buffer);
            }
        }
    }
    PendingCommandBufferMap_clear(pcmNew);

    WGPUCommandEncoderDescriptor cedesc zeroinit;
    device->queue->presubmitCache = wgpuDeviceCreateCommandEncoder(device, &cedesc);
    syncStateNew->submits = 0;
    EXIT();
}

WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor){
    ENTRY();
    WGPUSampler ret = RL_CALLOC(1, sizeof(WGPUSamplerImpl));
    ret->refCount = 1;
    ret->device = device;
    const VkSamplerCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .compareEnable = VK_FALSE,
        .maxLod = descriptor->lodMaxClamp,
        .minLod = descriptor->lodMinClamp,
        .addressModeU = toVulkanAddressMode(descriptor->addressModeU),
        .addressModeV = toVulkanAddressMode(descriptor->addressModeV),
        .addressModeW = toVulkanAddressMode(descriptor->addressModeW),
        .mipmapMode = ((descriptor->mipmapFilter == WGPUMipmapFilterMode_Linear) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST),
        .anisotropyEnable = false,
        .maxAnisotropy = descriptor->maxAnisotropy,
        .magFilter = ((descriptor->magFilter == WGPUFilterMode_Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST),
        .minFilter = ((descriptor->minFilter == WGPUFilterMode_Linear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST),
    };
    VkResult result = device->functions.vkCreateSampler(device->device, &sci, NULL, &(ret->sampler));
    return ret;
    EXIT();
}
void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder rpe, uint32_t binding, WGPUBuffer buffer, size_t offset, uint64_t size) {
    ENTRY();
    wgvk_assert(rpe != NULL, "RenderPassEncoderHandle is null");
    wgvk_assert(buffer != NULL, "BufferHandle is null");
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_vertex_buffer,
        .setVertexBuffer = {
            binding,
            buffer,
            offset,
        }
    };

    RenderPassEncoder_PushCommand(rpe, &insert);
    
    ce_trackBuffer(rpe->cmdEncoder, buffer, (BufferUsageSnap){
        .access =  VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 
        .stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
    });
    EXIT();
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, size_t bundleCount, const WGPURenderBundle* bundles) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    wgvk_assert(renderPassEncoder != NULL, "RenderPassEncoderHandle is null");
    for(uint32_t i = 0;i < bundleCount;i++){
        RenderPassCommandGeneric insert = {
            .type = rp_command_type_execute_renderbundle,
            .executeRenderBundles = {
                .renderBundle = bundles[i],
            }
        };
        RenderPassCommandGenericVector_push_back(&renderPassEncoder->bufferedCommands, insert);
        ru_trackRenderBundle(&renderPassEncoder->resourceUsage, bundles[i]);
    }
    EXIT();
}


void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_draw_indexed_indirect,
        .drawIndexedIndirect = {
            indirectBuffer,
            indirectOffset
        }
    };
    RenderPassEncoder_PushCommand(renderPassEncoder, &insert);
    EXIT();
}
void wgpuRenderPassEncoderDrawIndirect           (WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_draw_indirect,
        .drawIndirect = {
            indirectBuffer,
            indirectOffset
        }
    };
    RenderPassEncoder_PushCommand(renderPassEncoder, &insert);
    EXIT();
}
void wgpuRenderPassEncoderSetBlendConstant       (WGPURenderPassEncoder renderPassEncoder, const WGPUColor* color) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_blend_constant,
        .setBlendConstant = {
            *color
        }
    };
    RenderPassEncoder_PushCommand(renderPassEncoder, &insert);
    EXIT();
}
void wgpuRenderPassEncoderSetViewport            (WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_viewport,
        .setViewport = {
            .x = x,
            .y = y + height,
            .width = width,
            .height = -height,
            .minDepth = minDepth,
            .maxDepth = maxDepth
        }
    };
    RenderPassEncoder_PushCommand(renderPassEncoder, &insert);
    EXIT();
}
void wgpuRenderPassEncoderSetScissorRect         (WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height) WGPU_FUNCTION_ATTRIBUTE{
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_scissor_rect,
        .setScissorRect = {
            x, y, width, height
        }
    };
    RenderPassEncoder_PushCommand(renderPassEncoder, &insert);
    EXIT();
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder rpe, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size) {
    ENTRY();
    wgvk_assert(rpe != NULL, "RenderPassEncoderHandle is null");
    wgvk_assert(buffer != NULL, "BufferHandle is null");
    
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_set_index_buffer,
        .setIndexBuffer = {
            .buffer = buffer,
            .format = format,
            .offset = offset,
            .size = buffer->capacity
        }
    };

    RenderPassEncoder_PushCommand(rpe, &insert);
    
    ce_trackBuffer(rpe->cmdEncoder, buffer, (BufferUsageSnap){
        .stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 
        .access = VK_ACCESS_INDEX_READ_BIT
    });
    EXIT();
}
void wgpuTextureViewAddRef(WGPUTextureView textureView){
    ENTRY();
    ++textureView->refCount;
    EXIT();
}
void wgpuTextureAddRef(WGPUTexture texture){
    ENTRY();
    ++texture->refCount;
    EXIT();
}
void wgpuBufferAddRef(WGPUBuffer buffer){
    ENTRY();
    ++buffer->refCount;
    EXIT();
}
void wgpuBindGroupAddRef(WGPUBindGroup bindGroup){
    ENTRY();
    ++bindGroup->refCount;
    EXIT();
}
void wgpuBindGroupLayoutAddRef(WGPUBindGroupLayout bindGroupLayout){
    ENTRY();
    ++bindGroupLayout->refCount;
    EXIT();
}
void wgpuPipelineLayoutAddRef(WGPUPipelineLayout pipelineLayout){
    ENTRY();
    ++pipelineLayout->refCount;
    EXIT();
}
void wgpuSamplerAddRef(WGPUSampler sampler){
    ENTRY();
    ++sampler->refCount;
    EXIT();
}
void wgpuRenderPipelineAddRef(WGPURenderPipeline rpl){
    ENTRY();
    ++rpl->refCount;
    EXIT();
}
void wgpuComputePipelineAddRef(WGPUComputePipeline cpl){
    ENTRY();
    ++cpl->refCount;
    EXIT();
}
void wgpuShaderModuleAddRef(WGPUShaderModule module){
    ENTRY();
    ++module->refCount;
    EXIT();
}

RGAPI void ru_trackBuffer(ResourceUsage* resourceUsage, WGPUBuffer buffer, BufferUsageRecord brecord){
    if(BufferUsageRecordMap_put(&resourceUsage->referencedBuffers, (void*)buffer, brecord)){
        ++buffer->refCount;
    }
}

RGAPI void ru_trackTexture(ResourceUsage* resourceUsage, WGPUTexture texture, ImageUsageRecord record){
    if(ImageUsageRecordMap_put(&resourceUsage->referencedTextures, texture, record)){
        ++texture->refCount;
    }
}

RGAPI void ru_trackTextureView     (ResourceUsage* resourceUsage, WGPUTextureView view){
    if(ImageViewUsageSet_add(&resourceUsage->referencedTextureViews, view)){
        ++view->refCount;
    }
}
RGAPI void ru_trackRaytracingPipeline(ResourceUsage* resourceUsage, WGPURaytracingPipeline raytracingPipeline){
    if(RaytracingPipelineUsageSet_add(&resourceUsage->referencedRaytracingPipelines, raytracingPipeline)){
        ++raytracingPipeline->refCount;
    }
}

RGAPI void ru_trackRenderPipeline(ResourceUsage* resourceUsage, WGPURenderPipeline renderPipeline){
    if(RenderPipelineUsageSet_add(&resourceUsage->referencedRenderPipelines, renderPipeline)){
        ++renderPipeline->refCount;
    }
}
RGAPI void ru_trackComputePipeline(ResourceUsage* resourceUsage, WGPUComputePipeline computePipeline){
    if(ComputePipelineUsageSet_add(&resourceUsage->referencedComputePipelines, computePipeline)){
        ++computePipeline->refCount;
    }
}
RGAPI void ru_trackQuerySet        (ResourceUsage* resourceUsage, WGPUQuerySet computePipeline){
    if(QuerySetUsageSet_add(&resourceUsage->referencedQuerySets, computePipeline)){
        ++computePipeline->refCount;
    }
}
RGAPI void ru_trackRenderBundle    (ResourceUsage* resourceUsage, WGPURenderBundle computePipeline){
if(RenderBundleUsageSet_add(&resourceUsage->referencedRenderBundles, computePipeline)){
        ++computePipeline->refCount;
    }
}
RGAPI void ru_trackBindGroup(ResourceUsage* resourceUsage, WGPUBindGroup bindGroup){
    if(BindGroupUsageSet_add(&resourceUsage->referencedBindGroups, (void*)bindGroup)){
        ++bindGroup->refCount;
    }
}

RGAPI void ru_trackBindGroupLayout (ResourceUsage* resourceUsage, WGPUBindGroupLayout bindGroupLayout){
    if(BindGroupLayoutUsageSet_add(&resourceUsage->referencedBindGroupLayouts, bindGroupLayout)){
        wgpuBindGroupLayoutAddRef(bindGroupLayout);
    }
}

RGAPI void ru_trackSampler         (ResourceUsage* resourceUsage, WGPUSampler sampler){
    if(SamplerUsageSet_add(&resourceUsage->referencedSamplers, sampler)){
        ++sampler->refCount;
    }
}

RGAPI void ru_trackAccelerationStructure(ResourceUsage* resourceUsage, WGPURayTracingAccelerationContainer accelerationStructure){
    if(WGPURayTracingAccelerationContainerSet_add(&resourceUsage->referencedAccelerationStructures, accelerationStructure)){
        ++accelerationStructure->refCount;
    }
}
typedef enum barrierType{
    bt_no_barrier = 0,
    bt_buffer_barrier = 1,
    bt_memory_barrier = 2,
    bt_image_barrier = 3,
    bt_force32 = 0x7fffffff
} barrierType;

typedef struct OptionalBarrier{
    barrierType type;
    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;
    union{
        VkBufferMemoryBarrier bufferBarrier;
        VkMemoryBarrier memoryBarrier;
        VkImageMemoryBarrier imageBarrier;
    };
}OptionalBarrier;

static OptionalBarrier ru_trackTextureAndEmit(ResourceUsage* resourceUsage, WGPUTexture texture, ImageUsageSnap usage){
    ImageUsageRecord* alreadyThere = ImageUsageRecordMap_get(&resourceUsage->referencedTextures, texture);
    if(alreadyThere){
        const VkImageMemoryBarrier barr = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            alreadyThere->lastAccess,
            usage.access,
            alreadyThere->lastLayout,
            usage.layout,
            texture->device->adapter->queueIndices.graphicsIndex,
            texture->device->adapter->queueIndices.graphicsIndex,
            texture->image,
            usage.subresource
        };
        alreadyThere->lastStage  = usage.stage;
        alreadyThere->lastAccess = usage.access;
        alreadyThere->lastLayout = usage.layout;
        const OptionalBarrier ret = {
            .srcStage = alreadyThere->lastStage,
            .dstStage = usage.stage,
            .type = bt_image_barrier,
            .imageBarrier = barr
        };
        return ret;
    }
    else{
        const ImageUsageRecord record = {
            .initialAccess = usage.access,
            .initialStage = usage.stage,
            .initialLayout = usage.layout,
            .lastLayout = usage.layout,
            .lastStage = usage.stage,
            .lastAccess = usage.access,
            .initiallyAccessedSubresource = usage.subresource,
            .lastAccessedSubresource = usage.subresource
        };
        int newEntry = ImageUsageRecordMap_put(&resourceUsage->referencedTextures, texture, record);
        wgvk_assert(newEntry != 0, "_get failed, but _put did not return 1");
        if(newEntry)
            ++texture->refCount;

        return (OptionalBarrier){
            .type = bt_no_barrier
        };
    }
}

static OptionalBarrier ru_trackTextureViewAndEmit(ResourceUsage* resourceUsage, WGPUTextureView view, ImageUsageSnap usage){
    ru_trackTextureView(resourceUsage, view);
    
    ImageUsageRecord* alreadyThere = ImageUsageRecordMap_get(&resourceUsage->referencedTextures, view->texture);
    if(alreadyThere){
        const VkImageMemoryBarrier barr = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            alreadyThere->lastAccess,
            usage.access,
            alreadyThere->lastLayout,
            usage.layout, 
            view->texture->device->adapter->queueIndices.graphicsIndex,
            view->texture->device->adapter->queueIndices.graphicsIndex,
            view->texture->image,
            view->subresourceRange
        };
        alreadyThere->lastStage  = usage.stage;
        alreadyThere->lastAccess = usage.access;
        alreadyThere->lastLayout = usage.layout;
        const OptionalBarrier ret = {
            .srcStage = alreadyThere->lastStage,
            .dstStage = usage.stage,
            .type = bt_image_barrier,
            .imageBarrier = barr
        };
        return ret;
    }
    else{
        const ImageUsageRecord record = {
            .initialAccess = usage.access,
            .initialStage = usage.stage,
            .initialLayout = usage.layout,
            .lastLayout = usage.layout,
            .lastStage = usage.stage,
            .lastAccess = usage.access,
            .initiallyAccessedSubresource = view->subresourceRange,
            .lastAccessedSubresource = view->subresourceRange
        };
        int newEntry = ImageUsageRecordMap_put(&resourceUsage->referencedTextures, view->texture, record);
        wgvk_assert(newEntry != 0, "_get failed, but _put did not return 1");
        if(newEntry)
            ++view->texture->refCount;

        return (OptionalBarrier){
            .type = bt_no_barrier
        };
    }
}
static OptionalBarrier ru_trackBufferAndEmit(ResourceUsage* resourceUsage, WGPUBuffer buffer, BufferUsageSnap usage){
    BufferUsageRecord* rec = BufferUsageRecordMap_get(&resourceUsage->referencedBuffers, buffer);
    
    if(rec != NULL){
        const VkBufferMemoryBarrier bufferBarrier = {
            VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            NULL,
            rec->lastAccess,
            usage.access, 
            buffer->device->adapter->queueIndices.graphicsIndex,
            buffer->device->adapter->queueIndices.graphicsIndex,
            buffer->buffer,
            0,
            VK_WHOLE_SIZE
        };
        const OptionalBarrier ret = {
            .type = bt_buffer_barrier,
            .bufferBarrier = bufferBarrier,
            .srcStage = rec->lastStage,
            .dstStage = usage.stage,
        };
        rec->lastAccess = usage.access;
        rec->lastStage = usage.stage;
        rec->everWrittenTo |= isWritingAccess(usage.access);
        
        return ret;
    }
    else{
        const BufferUsageRecord record = {
            .initialStage = usage.stage,
            .initialAccess = usage.access,
            .lastStage = usage.stage,
            .lastAccess = usage.access,
            .everWrittenTo = isWritingAccess(usage.access)
        };
        ru_trackBuffer(resourceUsage, buffer, record);
        return (OptionalBarrier){bt_no_barrier};
    }
}

static void encoderOptionalBarrierVk(VkCommandBuffer buffer, PFN_vkCmdPipelineBarrier barrier_fn, OptionalBarrier barrier){
    uint32_t bufferBarriers = 0;
    uint32_t memoryBarriers = 0;
    uint32_t imageBarriers  = 0;
    VkBufferMemoryBarrier* bufferBarrier = NULL;
    VkMemoryBarrier*       memoryBarrier = NULL;
    VkImageMemoryBarrier*  imageBarrier  = NULL;
    switch(barrier.type){
        case bt_buffer_barrier:{
            bufferBarrier = &barrier.bufferBarrier;
            ++bufferBarriers;
        }break;
        case bt_image_barrier:{
            imageBarrier = &barrier.imageBarrier;
            ++imageBarriers;
        }break;
        case bt_memory_barrier:{
            memoryBarrier = &barrier.memoryBarrier;
            ++memoryBarriers;
        }break;

        default: return;
    }
    barrier_fn(
        buffer,
        barrier.srcStage ? barrier.srcStage : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        barrier.dstStage,
        0,
        memoryBarriers, memoryBarrier,
        bufferBarriers, bufferBarrier,
        imageBarriers,  imageBarrier
    );
}
static void encoderOptionalBarrier(WGPUCommandEncoder encoder, OptionalBarrier barrier){
    encoderOptionalBarrierVk(encoder->buffer, encoder->device->functions.vkCmdPipelineBarrier, barrier);
}
static void ru_trackAndEncodeTexture(WGPUCommandEncoder encoder, ResourceUsage* resourceUsage, WGPUTexture texture, ImageUsageSnap usage){
    OptionalBarrier barrier = ru_trackTextureAndEmit(resourceUsage, texture, usage);
    encoderOptionalBarrier(encoder, barrier);
}

static void ru_trackAndEncodeTextureView(WGPUCommandEncoder encoder, ResourceUsage* resourceUsage, WGPUTextureView view, ImageUsageSnap usage){
    OptionalBarrier barrier = ru_trackTextureViewAndEmit(resourceUsage, view, usage);
    encoderOptionalBarrier(encoder, barrier);
}

static void ru_trackAndEncodeBuffer(WGPUCommandEncoder encoder, ResourceUsage* resourceUsage, WGPUBuffer buffer, BufferUsageSnap usage){
    OptionalBarrier barrier = ru_trackBufferAndEmit(resourceUsage, buffer, usage);
    encoderOptionalBarrier(encoder, barrier);
}

static void ru_trackAndEncodeTextureVk(VkCommandBuffer buffer, ResourceUsage* resourceUsage, WGPUTexture texture, ImageUsageSnap usage){
    OptionalBarrier barrier = ru_trackTextureAndEmit(resourceUsage, texture, usage);
    encoderOptionalBarrierVk(buffer, texture->device->functions.vkCmdPipelineBarrier, barrier);
}

static void ru_trackAndEncodeTextureViewVk(VkCommandBuffer buffer, ResourceUsage* resourceUsage, WGPUTextureView view, ImageUsageSnap usage){
    OptionalBarrier barrier = ru_trackTextureViewAndEmit(resourceUsage, view, usage);
    encoderOptionalBarrierVk(buffer, view->texture->device->functions.vkCmdPipelineBarrier, barrier);
}

static void ru_trackAndEncodeBufferVk(VkCommandBuffer buffer, ResourceUsage* resourceUsage, WGPUBuffer wbuffer, BufferUsageSnap usage){
    OptionalBarrier barrier = ru_trackBufferAndEmit(resourceUsage, wbuffer, usage);
    encoderOptionalBarrierVk(buffer, wbuffer->device->functions.vkCmdPipelineBarrier, barrier);
}

RGAPI void ce_trackTexture(WGPUCommandEncoder encoder, WGPUTexture texture, ImageUsageSnap usage){
    ru_trackAndEncodeTexture(encoder, &encoder->resourceUsage, texture, usage);
}

RGAPI void ce_trackTextureView(WGPUCommandEncoder encoder, WGPUTextureView view, ImageUsageSnap usage){
    
    //ru_trackTextureView(&enc->resourceUsage, view); TODO verify if that is actually taken care of
    
    ru_trackAndEncodeTextureView(encoder, &encoder->resourceUsage, view, usage);
}
RGAPI void ce_trackBuffer(WGPUCommandEncoder encoder, WGPUBuffer buffer, BufferUsageSnap usage){
    
    ru_trackAndEncodeBuffer(encoder, &encoder->resourceUsage, buffer, usage);
}


static inline void bufferReleaseCallback(void* buffer, BufferUsageRecord* bu_record, void* unused){
    wgpuBufferRelease(buffer);
}
static inline void textureReleaseCallback(void* texture, ImageUsageRecord* iur, void* unused){
    (void)unused;
    wgpuTextureRelease(texture);
}
static inline void textureViewReleaseCallback(WGPUTextureView textureView, void* unused){
    (void)unused;
    wgpuTextureViewRelease(textureView);
}
static inline void bindGroupReleaseCallback(WGPUBindGroup bindGroup, void* unused){
    wgpuBindGroupRelease(bindGroup);
}
static inline void bindGroupLayoutReleaseCallback(WGPUBindGroupLayout bgl, void* unused){
    wgpuBindGroupLayoutRelease(bgl);
}
static inline void samplerReleaseCallback(WGPUSampler sampler, void* unused){
    wgpuSamplerRelease(sampler);
}
static inline void computePipelineReleaseCallback(WGPUComputePipeline computePipeline, void* unused){
    wgpuComputePipelineRelease(computePipeline);
}
static inline void renderPipelineReleaseCallback(WGPURenderPipeline renderPipeline, void* unused){
    wgpuRenderPipelineRelease(renderPipeline);
}
static inline void renderBundleReleaseCallback(WGPURenderBundle renderPipeline, void* unused){
    wgpuRenderBundleRelease(renderPipeline);
}
static inline void querySetReleaseCallback(WGPUQuerySet renderPipeline, void* unused){
    wgpuQuerySetRelease(renderPipeline);
}



static WGPUTextureViewDimension spvDimToWGPUViewDimension(SpvDim dim, bool arrayed) {
    switch (dim) {
        case SpvDim1D: return WGPUTextureViewDimension_1D; // WebGPU doesn't have 1D array textures
        case SpvDim2D: return arrayed ? WGPUTextureViewDimension_2DArray : WGPUTextureViewDimension_2D;
        case SpvDim3D: return WGPUTextureViewDimension_3D; // 3D textures cannot be arrayed
        case SpvDimCube: return arrayed ? WGPUTextureViewDimension_CubeArray : WGPUTextureViewDimension_Cube;
        default: return WGPUTextureViewDimension_Undefined;
    }
}

static WGPUTextureSampleType spvImageTypeToWGPUSampleType(const SpvImageFormat type_desc, bool is_depth_image) {
    if (is_depth_image) {
        return WGPUTextureSampleType_Depth;
    }
    //if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
    //    // SPIRV-Reflect doesn't directly distinguish filterable vs unfilterable for float.
    //    // This often depends on the format and usage. Assume filterable for simplicity here.
    //    // You might need more sophisticated logic based on SpvImageFormat.
    //    return WGPUTextureSampleType_Float;
    //}
    //if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_INT) {
    //    if (type_desc->traits.numeric.scalar.signedness) {
    //        return WGPUTextureSampleType_Sint;
    //    } else {
    //        return WGPUTextureSampleType_Uint;
    //    }
    //}
    return WGPUTextureSampleType_Float;
}

static WGPUTextureFormat spvImageFormatToWGPUFormat(SpvImageFormat spv_format) {
    switch (spv_format) {
        case SpvImageFormatRgba32f:         return WGPUTextureFormat_RGBA32Float;
        case SpvImageFormatRgba16f:         return WGPUTextureFormat_RGBA16Float;
        case SpvImageFormatR32f:            return WGPUTextureFormat_R32Float;
        case SpvImageFormatRgba8:           return WGPUTextureFormat_RGBA8Unorm;
        case SpvImageFormatRgba8Snorm:      return WGPUTextureFormat_RGBA8Snorm;
        case SpvImageFormatRg32f:           return WGPUTextureFormat_RG32Float;
        case SpvImageFormatRg16f:           return WGPUTextureFormat_RG16Float;
        case SpvImageFormatR11fG11fB10f:    return WGPUTextureFormat_RG11B10Ufloat;
        case SpvImageFormatR16f:            return WGPUTextureFormat_R16Float;
        case SpvImageFormatRgba16:          return WGPUTextureFormat_Undefined; // WGPUTextureFormat_RGBA16Unorm missing
        case SpvImageFormatRgb10A2:         return WGPUTextureFormat_RGB10A2Unorm;
        case SpvImageFormatRg16:            return WGPUTextureFormat_Undefined; // WGPUTextureFormat_RG16Unorm missing
        case SpvImageFormatRg8:             return WGPUTextureFormat_RG8Unorm;
        case SpvImageFormatR16:             return WGPUTextureFormat_Undefined; // WGPUTextureFormat_R16Unorm missing
        case SpvImageFormatR8:              return WGPUTextureFormat_R8Unorm;
        case SpvImageFormatRgba16Snorm:     return WGPUTextureFormat_Undefined; // WGPUTextureFormat_RGBA16Snorm missing
        case SpvImageFormatRg16Snorm:       return WGPUTextureFormat_Undefined; // WGPUTextureFormat_RG16Snorm missing
        case SpvImageFormatRg8Snorm:        return WGPUTextureFormat_RG8Snorm;
        case SpvImageFormatR16Snorm:        return WGPUTextureFormat_Undefined; // WGPUTextureFormat_R16Snorm missing
        case SpvImageFormatR8Snorm:         return WGPUTextureFormat_R8Snorm;
        case SpvImageFormatRgba32i:         return WGPUTextureFormat_RGBA32Sint;
        case SpvImageFormatRgba16i:         return WGPUTextureFormat_RGBA16Sint;
        case SpvImageFormatRgba8i:          return WGPUTextureFormat_RGBA8Sint;
        case SpvImageFormatR32i:            return WGPUTextureFormat_R32Sint;
        case SpvImageFormatRg32i:           return WGPUTextureFormat_RG32Sint;
        case SpvImageFormatRg16i:           return WGPUTextureFormat_RG16Sint;
        case SpvImageFormatRg8i:            return WGPUTextureFormat_RG8Sint;
        case SpvImageFormatR16i:            return WGPUTextureFormat_R16Sint;
        case SpvImageFormatR8i:             return WGPUTextureFormat_R8Sint;
        case SpvImageFormatRgba32ui:        return WGPUTextureFormat_RGBA32Uint;
        case SpvImageFormatRgba16ui:        return WGPUTextureFormat_RGBA16Uint;
        case SpvImageFormatRgba8ui:         return WGPUTextureFormat_RGBA8Uint;
        case SpvImageFormatR32ui:           return WGPUTextureFormat_R32Uint;
        case SpvImageFormatRgb10a2ui:       return WGPUTextureFormat_RGB10A2Uint;
        case SpvImageFormatRg32ui:          return WGPUTextureFormat_RG32Uint;
        case SpvImageFormatRg16ui:          return WGPUTextureFormat_RG16Uint;
        case SpvImageFormatRg8ui:           return WGPUTextureFormat_RG8Uint;
        case SpvImageFormatR16ui:           return WGPUTextureFormat_R16Uint;
        case SpvImageFormatR8ui:            return WGPUTextureFormat_R8Uint;
        case SpvImageFormatR64ui:           return WGPUTextureFormat_Undefined; // No 64-bit formats in WGPUTextureFormat
        case SpvImageFormatR64i:            return WGPUTextureFormat_Undefined; // No 64-bit formats in WGPUTextureFormat
        case SpvImageFormatUnknown:
        // SpvImageFormatMax is not typically used as an input format value
        default:
            return WGPUTextureFormat_Undefined;
    }
}

static WGPUStorageTextureAccess getStorageTextureAccess(const SpvReflectDescriptorBinding* entry) {
    bool non_readable = (entry->decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE);
    bool non_writable = (entry->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);

    if (non_readable && !non_writable) { // WriteOnly
        return WGPUStorageTextureAccess_WriteOnly;
    } else if (!non_readable && non_writable) { // ReadOnly
        return WGPUStorageTextureAccess_ReadOnly;
    } else if (!non_readable && !non_writable) { // ReadWrite
        // SPIR-V default for storage image is read_write.
        // WebGPU requires explicit access. If using for typical compute output, WriteOnly is common.
        // If truly read-write is intended and supported by format, this could be ReadWrite.
        // For safety and common WebGPU patterns, often WriteOnly is expected for storage textures unless specified.
        // However, based on SPIR-V, ReadWrite is more accurate if no flags.
        // Let's assume a choice that aligns with user's previous note:
        // "return WGPUStorageTextureAccess_WriteOnly; // Common default for WebGPU pipeline creation."
        return WGPUStorageTextureAccess_WriteOnly;
    } else { // non_readable && non_writable
        wgvk_assert(false, "Storage image cannot be both non-readable and non-writable");
        return WGPUStorageTextureAccess_Undefined;
    }
}

WGPUGlobalReflectionInfo* getGlobalRI(SpvReflectShaderModule mod, uint32_t* count){
    wgvk_assert(count != NULL, "count may not be NULL here");
    *count = 0;

    uint32_t descriptorSetCount = 0;
    SpvReflectResult result = spvReflectEnumerateDescriptorSets(&mod, &descriptorSetCount, NULL);
    wgvk_assert(result == SPV_REFLECT_RESULT_SUCCESS, "Failed to enumerate descriptor sets (count)");
    if (descriptorSetCount == 0) {
        return NULL;
    }

    SpvReflectDescriptorSet** descriptorSets = (SpvReflectDescriptorSet**)RL_CALLOC(descriptorSetCount, sizeof(SpvReflectDescriptorSet*));
    wgvk_assert(descriptorSets != NULL, "Failed to allocate memory for descriptor set pointers");
    result = spvReflectEnumerateDescriptorSets(&mod, &descriptorSetCount, descriptorSets);
    wgvk_assert(result == SPV_REFLECT_RESULT_SUCCESS, "Failed to enumerate descriptor sets (pointers)");
    
    uint32_t totalGlobalCount = 0;
    for(uint32_t i = 0; i < descriptorSetCount; i++){
        totalGlobalCount += descriptorSets[i]->binding_count;
    }

    if (totalGlobalCount == 0) {
        RL_FREE((void*)descriptorSets); // Free the pointers array if no bindings.
        return NULL;
    }

    WGPUGlobalReflectionInfo* allGlobals = (WGPUGlobalReflectionInfo*)RL_CALLOC(totalGlobalCount, sizeof(WGPUGlobalReflectionInfo));
    wgvk_assert(allGlobals != NULL, "Failed to allocate memory for global reflection info array");
    *count = totalGlobalCount;

    uint32_t globalInsertIndex = 0;
    for(uint32_t bindGroupIndex = 0; bindGroupIndex < descriptorSetCount; bindGroupIndex++){
        const SpvReflectDescriptorSet* set = descriptorSets[bindGroupIndex];
        for(uint32_t entryIndex = 0; entryIndex < set->binding_count; entryIndex++){
            const SpvReflectDescriptorBinding* entry = set->bindings[entryIndex];
            WGPUGlobalReflectionInfo insert = {0}; // Initialize to zero

            insert.bindGroup = set->set; // Use the actual set number from reflection
            insert.binding = entry->binding;
            
            const char* entry_name_ptr = entry->name ? entry->name : (entry->type_description->type_name ? entry->type_description->type_name : "");
            insert.name.data = entry_name_ptr;
            insert.name.length = strlen(entry_name_ptr);
            // insert.stageFlags = entry->shader_stage_flags; // If WGPUGlobalReflectionInfo has stage flags

            switch(entry->descriptor_type){    
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    // insert.resourceType = WGPUBindingResourceType_Buffer; // If using a type enum for a union
                    insert.buffer.type = WGPUBufferBindingType_Uniform;
                    insert.buffer.minBindingSize = entry->block.size; // Size of the UBO block
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                    // insert.resourceType = WGPUBindingResourceType_Buffer;
                    insert.buffer.type = WGPUBufferBindingType_Storage; // Or ReadOnlyStorage if distinguishable and needed
                    insert.buffer.minBindingSize = entry->block.size; // Size of the SSBO block
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                    // insert.resourceType = WGPUBindingResourceType_Sampler;
                    insert.sampler.type = WGPUSamplerBindingType_Filtering; // Default, see notes
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    // insert.resourceType = WGPUBindingResourceType_Texture;
                    insert.texture.sampleType = spvImageTypeToWGPUSampleType(entry->image.image_format, (entry->image.depth == 1));
                    insert.texture.viewDimension = spvDimToWGPUViewDimension(entry->image.dim, entry->image.arrayed);
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                    // insert.resourceType = WGPUBindingResourceType_StorageTexture;
                    insert.storageTexture.access = getStorageTextureAccess(entry);
                    insert.storageTexture.format = spvImageFormatToWGPUFormat(entry->image.image_format);
                    insert.storageTexture.viewDimension = spvDimToWGPUViewDimension(entry->image.dim, entry->image.arrayed);
                    break;
                case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    // WebGPU requires separate texture and sampler.
                    // This reflection info might need to create two entries or have a special type.
                    // For now, matches your original wgvk_assert.
                    wgvk_assert(false, "SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER not handled/allowed.");
                    break;
                // Handle other types like SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT if necessary
                default:
                    wgvk_assert(false, "Unhandled or illegal descriptor type in SPIR-V reflection.");
            }
            allGlobals[globalInsertIndex++] = insert;
        }
    }
    
    RL_FREE((void*)descriptorSets); // Free the array of pointers (not the content they point to)
    return allGlobals;
}


static inline WGPUShaderStage toWGPUShaderStage(SpvReflectShaderStageFlagBits stageBits){
    WGPUShaderStage ret = 0;

    if(stageBits & SPV_REFLECT_SHADER_STAGE_VERTEX_BIT){
        ret |= WGPUShaderStage_Vertex;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT){
        ret |= WGPUShaderStage_TessControl;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT){
        ret |= WGPUShaderStage_TessEvaluation;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT){
        ret |= WGPUShaderStage_Geometry;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT){
        ret |= WGPUShaderStage_Fragment;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT){
        ret |= WGPUShaderStage_Compute;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV){
        ret |= WGPUShaderStage_Task;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT){
        ret |= WGPUShaderStage_Task;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV){
        ret |= WGPUShaderStage_Mesh;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT){
        ret |= WGPUShaderStage_Mesh;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR){
        ret |= WGPUShaderStage_RayGen;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR){
        ret |= WGPUShaderStage_AnyHit;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR){
        ret |= WGPUShaderStage_ClosestHit;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR){
        ret |= WGPUShaderStage_Miss;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR){
        ret |= WGPUShaderStage_Intersect;
    }
    if(stageBits & SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR){
        ret |= WGPUShaderStage_Callable;
    }
    return ret;
}

struct wgpuShaderModuleGetReflectionInfo_sync_userdata{
    WGPUShaderModule module;
    WGPUReflectionInfoCallbackInfo callbackInfo;
};
WGPUReflectionAttribute spvReflectToWGPUReflectAttrib(SpvReflectInterfaceVariable* spvAttrib){
    
    WGPUReflectionAttribute result = {0};
    result.location = spvAttrib->location;
    switch(spvAttrib->format){
        case SPV_REFLECT_FORMAT_R32_SFLOAT:
            result.compositionType = WGPUReflectionCompositionType_Scalar;
            result.componentType = WGPUReflectionComponentType_Float32;
            break;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
            result.compositionType = WGPUReflectionCompositionType_Vec2;
            result.componentType = WGPUReflectionComponentType_Float32;
            break;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
            result.compositionType = WGPUReflectionCompositionType_Vec3;
            result.componentType = WGPUReflectionComponentType_Float32;
            break;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
            result.compositionType = WGPUReflectionCompositionType_Vec4;
            result.componentType = WGPUReflectionComponentType_Float32;
            break;
        case SPV_REFLECT_FORMAT_R32_UINT:
            result.compositionType = WGPUReflectionCompositionType_Scalar;
            result.componentType = WGPUReflectionComponentType_Uint32;
            break;
        case SPV_REFLECT_FORMAT_R32G32_UINT:
            result.compositionType = WGPUReflectionCompositionType_Vec2;
            result.componentType = WGPUReflectionComponentType_Uint32;
            break;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:
            result.compositionType = WGPUReflectionCompositionType_Vec3;
            result.componentType = WGPUReflectionComponentType_Uint32;
            break;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
            result.compositionType = WGPUReflectionCompositionType_Vec4;
            result.componentType = WGPUReflectionComponentType_Uint32;
            break;
        case SPV_REFLECT_FORMAT_R32_SINT:
            result.compositionType = WGPUReflectionCompositionType_Scalar;
            result.componentType = WGPUReflectionComponentType_Sint32;
            break;
        case SPV_REFLECT_FORMAT_R32G32_SINT:
            result.compositionType = WGPUReflectionCompositionType_Vec2;
            result.componentType = WGPUReflectionComponentType_Sint32;
            break;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:
            result.compositionType = WGPUReflectionCompositionType_Vec3;
            result.componentType = WGPUReflectionComponentType_Sint32;
            break;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
            result.compositionType = WGPUReflectionCompositionType_Vec4;
            result.componentType = WGPUReflectionComponentType_Sint32;
            break;
        default: break; //wgvk_assert(false, "Unhandled Spirv-reflect attribute type");
    }
    return result;
}
static void wgpuShaderModuleGetReflectionInfo_sync(void* userdata_){
    ENTRY();
    struct wgpuShaderModuleGetReflectionInfo_sync_userdata* userdata = (struct wgpuShaderModuleGetReflectionInfo_sync_userdata*)userdata_;
    WGPUShaderModule module = userdata->module;
    
    wgvk_assert(module,         "shaderModule is NULL");
    wgvk_assert(module->source, "shaderModule->source is NULL");

    switch(module->source->sType){
        case WGPUSType_ShaderSourceSPIRV:{
            WGPUShaderSourceSPIRV* spirvSource = (WGPUShaderSourceSPIRV*)module->source;

            SpvReflectShaderModule mod zeroinit;
            SpvReflectResult result = spvReflectCreateShaderModule(spirvSource->codeSize, spirvSource->code, &mod);
            
            if(result == SPV_REFLECT_RESULT_SUCCESS){
                SpvReflectDescriptorSet* descriptorSets = NULL;
                
                WGPUReflectionInfo reflectionInfo zeroinit;
                reflectionInfo.globals = getGlobalRI(mod,  &reflectionInfo.globalCount);
                
                SpvReflectInterfaceVariable** input_vars = NULL;
                SpvReflectInterfaceVariable** output_vars = NULL;

                uint32_t input_var_count = 0;
                uint32_t output_var_count = 0;
                spvReflectEnumerateInputVariables(&mod, &input_var_count, NULL);
                spvReflectEnumerateOutputVariables(&mod, &output_var_count, NULL);
                input_vars = (SpvReflectInterfaceVariable**)RL_CALLOC(input_var_count, sizeof(uintptr_t));
                output_vars = (SpvReflectInterfaceVariable**)RL_CALLOC(output_var_count, sizeof(uintptr_t));

                spvReflectEnumerateInputVariables(&mod, &input_var_count, input_vars);
                spvReflectEnumerateOutputVariables(&mod, &output_var_count, output_vars);
                
                WGPUAttributeReflectionInfo input_attribute_info = {0};
                WGPUAttributeReflectionInfo output_attribute_info = {0};

                input_attribute_info.attributeCount = input_var_count;
                output_attribute_info.attributeCount = output_var_count;
                input_attribute_info.attributes = RL_CALLOC(input_attribute_info.attributeCount, sizeof(WGPUReflectionAttribute));
                output_attribute_info.attributes = RL_CALLOC(output_attribute_info.attributeCount, sizeof(WGPUReflectionAttribute));
                
                for(uint32_t i = 0;i < input_var_count;i++){
                    SpvReflectInterfaceVariable* input_var_i = input_vars[i];
                    input_attribute_info.attributes[i] = spvReflectToWGPUReflectAttrib(input_vars[i]);
                }
                for(uint32_t i = 0;i < output_var_count;i++){
                    SpvReflectInterfaceVariable* output_var_i = output_vars[i];
                    output_attribute_info.attributes[i] = spvReflectToWGPUReflectAttrib(output_vars[i]);
                }

                reflectionInfo.inputAttributes = &input_attribute_info;

                userdata->callbackInfo.callback(
                    WGPUReflectionInfoRequestStatus_Success,
                    &reflectionInfo,
                    userdata->callbackInfo.userdata1,
                    userdata->callbackInfo.userdata2
                );

                RL_FREE(descriptorSets);
                RL_FREE(input_attribute_info.attributes);
                RL_FREE(output_attribute_info.attributes);
                RL_FREE((void*)input_vars);
                RL_FREE((void*)output_vars);
                RL_FREE((void*)reflectionInfo.globals);
                spvReflectDestroyShaderModule(&mod);
            }
        }
        break;
        #if SUPPORT_WGSL == 1
        case WGPUSType_ShaderSourceWGSL:{
            WGPUShaderSourceWGSL* wgslSource = (WGPUShaderSourceWGSL*)module->source;
            WGPUReflectionInfo reflectionInfo = reflectionInfo_wgsl_sync(wgslSource->code);
            userdata->callbackInfo.callback(
                WGPUReflectionInfoRequestStatus_Success,
                &reflectionInfo,
                userdata->callbackInfo.userdata1,
                userdata->callbackInfo.userdata2
            );
            reflectionInfo_wgsl_free(&reflectionInfo);
        }break;
        #else
        case WGPUSType_ShaderSourceWGSL:{
            wgvk_assert(false, "Passed WGSL source without support, recompile with -DSUPPORT_WGSL=1");
        }break;
        #endif
        default:
        wgvk_assert(false, "Invalid sType for source");
        rg_unreachable();
    }
    
    EXIT();
}

WGPUFuture wgpuShaderModuleGetReflectionInfo(WGPUShaderModule shaderModule, WGPUReflectionInfoCallbackInfo callbackInfo){
    ENTRY();
    struct wgpuShaderModuleGetReflectionInfo_sync_userdata* udff = RL_CALLOC(1, sizeof(struct wgpuShaderModuleGetReflectionInfo_sync_userdata));
    WGPUInstance instance = shaderModule->device->adapter->instance;
    udff->callbackInfo = callbackInfo;
    udff->module = shaderModule;
    WGPUFutureImpl ret = {
        .functionCalledOnWaitAny = wgpuShaderModuleGetReflectionInfo_sync,
        .userdataForFunction = udff,
        .freeUserData = RL_FREE
    };
    WGPUFuture rete = { 
        atomic_fetch_add_explicit(&shaderModule->device->adapter->instance->currentFutureId, 1, memory_order_relaxed)
    };
    FutureIDMap_put(&instance->g_futureIDMap, rete.id, ret);
    return rete;
    EXIT();
}

void wgpuAdapterInfoFreeMembers(WGPUAdapterInfo value) {
    ENTRY();
    (void)value;
    EXIT();
}
WGPUStatus wgpuGetInstanceCapabilities(WGPUInstanceCapabilities * capabilities) {
    ENTRY();
    EXIT();
    return WGPUStatus_Error;
}
WGPUProc wgpuGetProcAddress(WGPUStringView procName) {
    ENTRY();
    EXIT();
    return NULL;
}
void wgpuSupportedFeaturesFreeMembers(WGPUSupportedFeatures value) {
    ENTRY();
    if(value.features){
        RL_FREE((void*)value.features);
    }
    EXIT();
}
void wgpuSupportedWGSLLanguageFeaturesFreeMembers(WGPUSupportedWGSLLanguageFeatures value) {
    ENTRY();
    (void)value;
    EXIT();
}
void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities value) {
    ENTRY();
    (void)value;
    EXIT();
}

// Stubs for missing Methods of Adapter
void wgpuAdapterGetFeatures(WGPUAdapter adapter, WGPUSupportedFeatures* features) {
    ENTRY();
    if (!adapter || !features) {
        if (features) {
            features->featureCount = 0;
            // As per the API contract for output arrays, we do not modify features->features on invalid input.
        }
        return;
    }

    // Query all necessary Vulkan features using a pNext chain.
    // This is more efficient than separate calls.
    VkPhysicalDeviceFeatures2 features2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceVulkan12Features features12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features2.pNext = &features12;
    vkGetPhysicalDeviceFeatures2(adapter->physicalDevice, &features2);

    // Query subgroup properties separately as they are in VkPhysicalDeviceProperties.
    VkPhysicalDeviceSubgroupProperties subgroupProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };
    VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    properties2.pNext = &subgroupProperties;
    vkGetPhysicalDeviceProperties2(adapter->physicalDevice, &properties2);

    WGPUFeatureName supported_features[64] = {0};
    size_t count = 0;

    // --- Map Vulkan capabilities to WGPU features ---

    if (features2.features.depthClamp) {
        supported_features[count++] = WGPUFeatureName_DepthClipControl;
    }
    //if (features2.features) {
        // A full implementation would also check that queueFamilyProperties.timestampValidBits > 0
        // for at least one queue family, which is typically done during adapter enumeration.
        //supported_features[count++] = WGPUFeatureName_TimestampQuery;
    //}
    if (features2.features.textureCompressionBC) {
        supported_features[count++] = WGPUFeatureName_TextureCompressionBC;
    }
    if (features2.features.textureCompressionETC2) {
        supported_features[count++] = WGPUFeatureName_TextureCompressionETC2;
    }
    if (features2.features.textureCompressionASTC_LDR) {
        supported_features[count++] = WGPUFeatureName_TextureCompressionASTC;
    }
    if (features2.features.drawIndirectFirstInstance) {
        supported_features[count++] = WGPUFeatureName_IndirectFirstInstance;
    }
    if (features12.shaderFloat16) {
        supported_features[count++] = WGPUFeatureName_ShaderF16;
    }
    if (features2.features.shaderClipDistance) {
        supported_features[count++] = WGPUFeatureName_ClipDistances;
    }
    if (features2.features.dualSrcBlend) {
        supported_features[count++] = WGPUFeatureName_DualSourceBlending;
    }

    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_D32_SFLOAT_S8_UINT, &props);
    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
        supported_features[count++] = WGPUFeatureName_Depth32FloatStencil8;
    }
    
    vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_B10G11R11_UFLOAT_PACK32, &props);
    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
        supported_features[count++] = WGPUFeatureName_RG11B10UfloatRenderable;
    }

    vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_B8G8R8A8_UNORM, &props);
    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
        supported_features[count++] = WGPUFeatureName_BGRA8UnormStorage;
    }

    vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_R32_SFLOAT, &props);
    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) {
        supported_features[count++] = WGPUFeatureName_Float32Filterable;
    }
    
    vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT, &props);
    if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) {
        supported_features[count++] = WGPUFeatureName_Float32Blendable;
    }

    // Check subgroup support
    if ((subgroupProperties.supportedStages & VK_SHADER_STAGE_COMPUTE_BIT) &&
        (subgroupProperties.supportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT)) {
        supported_features[count++] = WGPUFeatureName_Subgroups;
    }

    // Check YCbCr support
    if (adapter->deviceInfoCache.knobs.samplerYCbCrConversionFeatures.samplerYcbcrConversion) {
        vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_G8_B8R8_2PLANE_420_UNORM, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            supported_features[count++] = WGPUFeatureName_TextureFormatNV12;
        }
        vkGetPhysicalDeviceFormatProperties(adapter->physicalDevice, VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            supported_features[count++] = WGPUFeatureName_TextureFormatP010;
        }
    }

    supported_features[count++] = WGPUFeatureName_CoreFeaturesAndLimits;
    features->featureCount = count;
    features->features = RL_CALLOC(count, sizeof(WGPUFeatureName));
    memcpy((void*)features->features, supported_features, count * sizeof(WGPUFeatureName));
    EXIT();
}

WGPUStatus wgpuAdapterGetInfo(WGPUAdapter adapter, WGPUAdapterInfo* info) {
    ENTRY();

    if (adapter == NULL || info == NULL || adapter->physicalDevice == VK_NULL_HANDLE) {
        EXIT();
        return WGPUStatus_Error;
    }
    if (adapter == NULL || info == NULL || adapter->physicalDevice == VK_NULL_HANDLE) {
        return WGPUStatus_Error;
    }

    const VkPhysicalDeviceProperties*       deviceProps = &adapter->deviceInfoCache.properties.properties;
    const VkPhysicalDeviceDriverProperties* driverProps = &adapter->deviceInfoCache.driverProperties;

    info->vendorID = adapter->deviceInfoCache.properties.properties.vendorID;
    info->deviceID = adapter->deviceInfoCache.properties.properties.deviceID;

    info->device = (WGPUStringView){
        adapter->deviceInfoCache.properties.properties.deviceName,
        WGPU_STRLEN
    };
    info->description = (WGPUStringView){
        adapter->cachedDeviceDescription,
        WGPU_STRLEN
    };
    info->architecture = (WGPUStringView){
        .length = WGPU_STRLEN,
        .data = PhysicalDeviceArchitecture_ToString(GetArchitecture(deviceProps->vendorID, deviceProps->deviceID)),
    };

    // Map the Vulkan device type to the corresponding WGPU adapter type.
    switch (deviceProps->deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: info->adapterType = WGPUAdapterType_IntegratedGPU; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   info->adapterType = WGPUAdapterType_DiscreteGPU;   break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            info->adapterType = WGPUAdapterType_CPU;           break;
        default:                                     info->adapterType = WGPUAdapterType_Unknown;       break;
    }

    EXIT();
    return WGPUStatus_Success;
}

WGPUBool wgpuAdapterHasFeature(WGPUAdapter adapter, WGPUFeatureName feature) {
    ENTRY();
    EXIT();
    return 0;
}

// Stubs for missing Methods of BindGroup
void wgpuBindGroupSetLabel(WGPUBindGroup bindGroup, WGPUStringView label) {
    ENTRY();

    EXIT();
}

// Stubs for missing Methods of BindGroupLayout
void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, WGPUStringView label) {
     ENTRY();

     EXIT();
}

// Stubs for missing Methods of Buffer
void wgpuBufferDestroy(WGPUBuffer buffer) {
    ENTRY();
    EXIT();
}
const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
    ENTRY();
    EXIT();
    return (const void*)((char*)buffer->mappedRange + offset);
}
void* wgpuBufferGetMappedRange(WGPUBuffer buffer, size_t offset, size_t size) {
    ENTRY();
    return (void*)((char*)buffer->mappedRange + offset);
    EXIT();
}
WGPUBufferMapState wgpuBufferGetMapState(WGPUBuffer buffer) {
    ENTRY();
    return buffer->mapState;
    EXIT();
}
WGPUBufferUsage wgpuBufferGetUsage(WGPUBuffer buffer) {
    ENTRY();
    EXIT();
    return buffer->usage;
}
WGPUStatus wgpuBufferReadMappedRange(WGPUBuffer buffer, size_t offset, void * data, size_t size) {
    ENTRY();
    EXIT();
    return WGPUStatus_Error;
}
void wgpuBufferSetLabel(WGPUBuffer buffer, WGPUStringView label) {
    ENTRY();
    EXIT();
}
WGPUStatus wgpuBufferWriteMappedRange(WGPUBuffer buffer, size_t offset, const void* data, size_t size) {
    ENTRY();
    EXIT();
    return WGPUStatus_Error;
}

// Stubs for missing Methods of CommandBuffer
void wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer, WGPUStringView label) {
    ENTRY();
    
    EXIT();
}
void wgpuCommandBufferAddRef(WGPUCommandBuffer commandBuffer) {
    ENTRY();
    ++commandBuffer->refCount;
    EXIT();
}

// Stubs for missing Methods of CommandEncoder
void wgpuCommandEncoderClearBuffer(WGPUCommandEncoder commandEncoder, WGPUBuffer buffer, uint64_t offset, uint64_t size) {
    ENTRY();

    EXIT();
}
void wgpuCommandEncoderInsertDebugMarker(WGPUCommandEncoder commandEncoder, WGPUStringView markerLabel) {
    ENTRY();

    EXIT();
}
void wgpuCommandEncoderPopDebugGroup(WGPUCommandEncoder commandEncoder) {
    ENTRY();

    EXIT();
}
void wgpuCommandEncoderPushDebugGroup(WGPUCommandEncoder commandEncoder, WGPUStringView groupLabel) {
    ENTRY();

    EXIT();
}
void wgpuCommandEncoderResolveQuerySet(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t firstQuery, uint32_t queryCount, WGPUBuffer destination, uint64_t destinationOffset) {
    ENTRY();
    const BufferUsageSnap usage = {
        .access = VK_ACCESS_TRANSFER_WRITE_BIT,
        .stage = VK_PIPELINE_STAGE_TRANSFER_BIT
    };
    ce_trackBuffer(commandEncoder, destination, usage);
    ru_trackQuerySet(&commandEncoder->resourceUsage, querySet);
    commandEncoder->device->functions.vkCmdCopyQueryPoolResults(
        commandEncoder->buffer,
        querySet->queryPool,
        firstQuery,
        queryCount, 
        destination->buffer,
        destinationOffset,
        8,
        VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT
    );
    EXIT();
}
void wgpuCommandEncoderSetLabel(WGPUCommandEncoder commandEncoder, WGPUStringView label) {
     ENTRY();

     EXIT();
}
void wgpuCommandEncoderWriteTimestamp(WGPUCommandEncoder commandEncoder, WGPUQuerySet querySet, uint32_t queryIndex) {
    ENTRY();
    ru_trackQuerySet(&commandEncoder->resourceUsage, querySet);
    commandEncoder->device->functions.vkCmdWriteTimestamp(commandEncoder->buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, querySet->queryPool, queryIndex);
    EXIT();
}
void wgpuCommandEncoderAddRef(WGPUCommandEncoder commandEncoder) {
    ENTRY();
    ++commandEncoder->refCount;
    EXIT();
}

// Stubs for missing Methods of ComputePassEncoder
void wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset) {
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = cp_command_type_dispatch_workgroups_indirect,
        .dispatchWorkgroupsIndirect = {
            .buffer = indirectBuffer,
            .offset = indirectOffset
        }
    };
    ComputePassEncoder_PushCommand(computePassEncoder, &insert);
    EXIT();
}
void wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, WGPUStringView markerLabel) {
    ENTRY();

    EXIT();
}
void wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder) {
    ENTRY();

    EXIT();
}
void wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, WGPUStringView groupLabel) {
    ENTRY();

    EXIT();
}
void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, WGPUStringView label) {
    ENTRY();

    EXIT();
}
void wgpuComputePassEncoderAddRef(WGPUComputePassEncoder computePassEncoder) {
    ENTRY();

    EXIT();
}

// Stubs for missing Methods of ComputePipeline
WGPUBindGroupLayout wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex) {
    ENTRY();
    return computePipeline->layout->bindGroupLayouts[groupIndex];
    EXIT();
}
void wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, WGPUStringView label) {
    ENTRY();

    EXIT();
}

typedef struct CreateComputePipelineAsyncState{
    wgvk_thread_t thread;
    WGPUDevice device;
    WGPUComputePipelineDescriptor cpdesc;
    WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo;
    Atomar(WGPUComputePipeline) computePipeline;
    Atomar(uint32_t) completed;
}CreateComputePipelineAsyncState;

static void* wgpuDeviceCreateComputePipelineAsync_sync(void* _crps){
    CreateComputePipelineAsyncState* crps = (CreateComputePipelineAsyncState*)_crps;
    WGPUDevice device = crps->device;
    WGPUComputePipeline temporary = wgpuDeviceCreateComputePipeline(device, &crps->cpdesc);
    atomic_store_explicit(&crps->computePipeline, temporary, memory_order_release);
    if(crps->callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous){
        if(crps->computePipeline){
            crps->callbackInfo.callback(WGPUCreatePipelineAsyncStatus_Success, crps->computePipeline, (WGPUStringView){"", 0}, crps->callbackInfo.userdata1, crps->callbackInfo.userdata2);
        }
        else{
            crps->callbackInfo.callback(WGPUCreatePipelineAsyncStatus_InternalError, crps->computePipeline, (WGPUStringView){"", 0}, crps->callbackInfo.userdata1, crps->callbackInfo.userdata2);
        }
    }
    atomic_store_explicit(&crps->completed, 1, memory_order_release);
    return _crps;
}

WGPUFuture wgpuDeviceCreateComputePipelineAsync(WGPUDevice device, WGPUComputePipelineDescriptor const * descriptor, WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo) {
    ENTRY();
    uint64_t futureID = ++device->adapter->instance->currentFutureId;
    WGPUFutureImpl futureImpl = {0}; // = RL_CALLOC(1, sizeof(WGPUFutureImpl));
    FutureIDMap_put(&device->adapter->instance->g_futureIDMap, futureID, futureImpl);
    CreateComputePipelineAsyncState* crps = RL_CALLOC(1, sizeof(CreateComputePipelineAsyncState));
    crps->callbackInfo = callbackInfo;
    crps->completed = 0;
    crps->device = device;
    crps->cpdesc = *descriptor;
    if(callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous){
        wgvk_job_enqueue(device->thread_pool, wgpuDeviceCreateComputePipelineAsync_sync, (void*)crps);
    }
    return (WGPUFuture){
        .id = futureID
    };
    EXIT();
}
WGPUQuerySet wgpuDeviceCreateQuerySet(WGPUDevice device, const WGPUQuerySetDescriptor* descriptor) {
    ENTRY();
    WGPUQuerySet ret = RL_CALLOC(1, sizeof(WGPUQuerySetImpl));
    ret->device = device;
    ret->type = descriptor->type;

    VkQueryPoolCreateInfo qpci = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = toVulkanQueryType(descriptor->type),
        .queryCount = descriptor->count,
    };
    
    VkResult qcr = device->functions.vkCreateQueryPool(device->device, &qpci, NULL, &ret->queryPool);
    if(qcr != VK_SUCCESS){
        RL_FREE(ret);
        return NULL;
    }
    EXIT();
    return ret; 
}

typedef struct CreateRenderPipelineAsyncState{
    wgvk_thread_t thread;
    WGPUDevice device;
    WGPURenderPipelineDescriptor* rpdesc;
    WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo;
    Atomar(WGPURenderPipeline) renderPipeline;
    Atomar(uint32_t) completed;
}CreateRenderPipelineAsyncState;

static WGPURenderPipelineDescriptor* copyRenderPipelineDescriptor(const WGPURenderPipelineDescriptor* desc);

static WGPUVertexState* copyVertexState(const WGPUVertexState* vertexState) {
    if (!vertexState) {
        return NULL;
    }

    WGPUVertexState* newVertexState = (WGPUVertexState*)RL_MALLOC(sizeof(WGPUVertexState));
    if (!newVertexState) {
        return NULL;
    }
    memcpy(newVertexState, vertexState, sizeof(WGPUVertexState));

    if (vertexState->entryPoint.data) {
        char* newEntryPoint = (char*)RL_MALLOC(vertexState->entryPoint.length + 1);
        if (!newEntryPoint) {
            RL_FREE(newVertexState);
            return NULL;
        }
        memcpy(newEntryPoint, vertexState->entryPoint.data, vertexState->entryPoint.length);
        newEntryPoint[vertexState->entryPoint.length] = '\0';
        newVertexState->entryPoint.data = newEntryPoint;
    }

    if (vertexState->constantCount > 0 && vertexState->constants) {
        WGPUConstantEntry* newConstants = (WGPUConstantEntry*)RL_MALLOC(sizeof(WGPUConstantEntry) * vertexState->constantCount);
        if (!newConstants) {
            if (newVertexState->entryPoint.data) RL_FREE((void*)newVertexState->entryPoint.data);
            RL_FREE(newVertexState);
            return NULL;
        }
        memcpy(newConstants, vertexState->constants, sizeof(WGPUConstantEntry) * vertexState->constantCount);
        newVertexState->constants = newConstants;
    }

    if (vertexState->bufferCount > 0 && vertexState->buffers) {
        WGPUVertexBufferLayout* newBuffers = (WGPUVertexBufferLayout*)RL_MALLOC(sizeof(WGPUVertexBufferLayout) * vertexState->bufferCount);
        if (!newBuffers) {
            if (newVertexState->entryPoint.data) RL_FREE((void*)newVertexState->entryPoint.data);
            if (newVertexState->constants) RL_FREE((void*)newVertexState->constants);
            RL_FREE(newVertexState);
            return NULL;
        }
        for (size_t i = 0; i < vertexState->bufferCount; ++i) {
            memcpy(&newBuffers[i], &vertexState->buffers[i], sizeof(WGPUVertexBufferLayout));
            if (vertexState->buffers[i].attributeCount > 0 && vertexState->buffers[i].attributes) {
                WGPUVertexAttribute* newAttributes = (WGPUVertexAttribute*)RL_MALLOC(sizeof(WGPUVertexAttribute) * vertexState->buffers[i].attributeCount);
                if (!newAttributes) {
                    // Cleanup allocated memory
                    for (size_t j = 0; j < i; ++j) {
                        if (newBuffers[j].attributes) RL_FREE((void*)newBuffers[j].attributes);
                    }
                    RL_FREE(newBuffers);
                    if (newVertexState->entryPoint.data) RL_FREE((void*)newVertexState->entryPoint.data);
                    if (newVertexState->constants) RL_FREE((void*)newVertexState->constants);
                    RL_FREE(newVertexState);
                    return NULL;
                }
                memcpy(newAttributes, vertexState->buffers[i].attributes, sizeof(WGPUVertexAttribute) * vertexState->buffers[i].attributeCount);
                newBuffers[i].attributes = newAttributes;
            }
        }
        newVertexState->buffers = newBuffers;
    }

    return newVertexState;
}

WGPUDepthStencilState* copyDepthStencilState(const WGPUDepthStencilState* depthStencilState) {
    if (!depthStencilState) {
        return NULL;
    }

    WGPUDepthStencilState* newDepthStencilState = (WGPUDepthStencilState*)RL_MALLOC(sizeof(WGPUDepthStencilState));
    if (!newDepthStencilState) {
        return NULL;
    }
    memcpy(newDepthStencilState, depthStencilState, sizeof(WGPUDepthStencilState));
    return newDepthStencilState;
}

WGPUFragmentState* copyFragmentState(const WGPUFragmentState* fragmentState) {
    if (!fragmentState) {
        return NULL;
    }

    WGPUFragmentState* newFragmentState = (WGPUFragmentState*)RL_MALLOC(sizeof(WGPUFragmentState));
    if (!newFragmentState) {
        return NULL;
    }
    memcpy(newFragmentState, fragmentState, sizeof(WGPUFragmentState));

    if (fragmentState->entryPoint.data) {
        char* newEntryPoint = (char*)RL_MALLOC(fragmentState->entryPoint.length + 1);
        if (!newEntryPoint) {
            RL_FREE(newFragmentState);
            return NULL;
        }
        memcpy(newEntryPoint, fragmentState->entryPoint.data, fragmentState->entryPoint.length);
        newEntryPoint[fragmentState->entryPoint.length] = '\0';
        newFragmentState->entryPoint.data = newEntryPoint;
    }

    if (fragmentState->constantCount > 0 && fragmentState->constants) {
        WGPUConstantEntry* newConstants = (WGPUConstantEntry*)RL_MALLOC(sizeof(WGPUConstantEntry) * fragmentState->constantCount);
        if (!newConstants) {
            if (newFragmentState->entryPoint.data) RL_FREE((void*)newFragmentState->entryPoint.data);
            RL_FREE(newFragmentState);
            return NULL;
        }
        memcpy(newConstants, fragmentState->constants, sizeof(WGPUConstantEntry) * fragmentState->constantCount);
        newFragmentState->constants = newConstants;
    }

    if (fragmentState->targetCount > 0 && fragmentState->targets) {
        WGPUColorTargetState* newTargets = (WGPUColorTargetState*)RL_MALLOC(sizeof(WGPUColorTargetState) * fragmentState->targetCount);
        if (!newTargets) {
            if (newFragmentState->entryPoint.data) RL_FREE((void*)newFragmentState->entryPoint.data);
            if (newFragmentState->constants) RL_FREE((void*)newFragmentState->constants);
            RL_FREE(newFragmentState);
            return NULL;
        }
        memcpy(newTargets, fragmentState->targets, sizeof(WGPUColorTargetState) * fragmentState->targetCount);
        for (size_t i = 0; i < fragmentState->targetCount; ++i) {
            if (fragmentState->targets[i].blend) {
                WGPUBlendState* newBlend = (WGPUBlendState*)RL_MALLOC(sizeof(WGPUBlendState));
                if (!newBlend) {
                    // In a real-world scenario, you would need more robust error handling and cleanup.
                    RL_FREE(newTargets);
                    if (newFragmentState->entryPoint.data) RL_FREE((void*)newFragmentState->entryPoint.data);
                    if (newFragmentState->constants) RL_FREE((void*)newFragmentState->constants);
                    RL_FREE(newFragmentState);
                    return NULL;
                }
                memcpy(newBlend, fragmentState->targets[i].blend, sizeof(WGPUBlendState));
                newTargets[i].blend = newBlend;
            }
        }
        newFragmentState->targets = newTargets;
    }

    return newFragmentState;
}

WGPURenderPipelineDescriptor* copyRenderPipelineDescriptor(const WGPURenderPipelineDescriptor* desc) {
    if (!desc) {
        return NULL;
    }
    WGPURenderPipelineDescriptor* newDesc = (WGPURenderPipelineDescriptor*)RL_CALLOC(1, sizeof(WGPURenderPipelineDescriptor));
    if (!newDesc) {
        return NULL;
    }
    memcpy(newDesc, desc, sizeof(WGPURenderPipelineDescriptor));
    if (desc->label.data) {
        char* newLabel = (char*)RL_MALLOC(desc->label.length + 1);
        if (!newLabel) {
            RL_FREE(newDesc);
            return NULL;
        }
        memcpy(newLabel, desc->label.data, desc->label.length);
        newLabel[desc->label.length] = '\0';
        newDesc->label.data = newLabel;
    }
    newDesc->vertex = *copyVertexState(&desc->vertex);
    newDesc->primitive = desc->primitive;
    if (desc->depthStencil) {
        newDesc->depthStencil = copyDepthStencilState(desc->depthStencil);
    }
    if (desc->fragment) {
        newDesc->fragment = copyFragmentState(desc->fragment);
    }

    return newDesc;
}
void freeRenderPipelineDescriptor(WGPURenderPipelineDescriptor* desc) {
    if (!desc) {
        return;
    }
    if (desc->label.data) {
        RL_FREE((void*)desc->label.data);
    }
    if (desc->vertex.entryPoint.data) RL_FREE((void*)desc->vertex.entryPoint.data);
    if (desc->vertex.constants) RL_FREE((void*)desc->vertex.constants);
    if (desc->vertex.buffers) {
        for (size_t i = 0; i < desc->vertex.bufferCount; ++i) {
            if (desc->vertex.buffers[i].attributes) {
                RL_FREE((void*)desc->vertex.buffers[i].attributes);
            }
        }
        RL_FREE((void*)desc->vertex.buffers);
    }
    if (desc->depthStencil) {
        RL_FREE((void*)desc->depthStencil);
    }
    if (desc->fragment) {
        if (desc->fragment->entryPoint.data) RL_FREE((void*)desc->fragment->entryPoint.data);
        if (desc->fragment->constants) RL_FREE((void*)desc->fragment->constants);
        if (desc->fragment->targets) {
            for (size_t i = 0; i < desc->fragment->targetCount; ++i) {
                if (desc->fragment->targets[i].blend) {
                    RL_FREE((void*)desc->fragment->targets[i].blend);
                }
            }
            RL_FREE((void*)desc->fragment->targets);
        }
        RL_FREE((void*)desc->fragment);
    }
    RL_FREE(desc);
}



static void* wgpuDeviceCreateRenderPipelineAsync_sync(void* _crps){
    CreateRenderPipelineAsyncState* crps = (CreateRenderPipelineAsyncState*)_crps;
    crps->renderPipeline = wgpuDeviceCreateRenderPipeline(crps->device, crps->rpdesc);
    freeRenderPipelineDescriptor(crps->rpdesc);
    if(crps->callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous){
        if(crps->renderPipeline){
            crps->callbackInfo.callback(WGPUCreatePipelineAsyncStatus_Success, crps->renderPipeline, (WGPUStringView){"", 0}, crps->callbackInfo.userdata1, crps->callbackInfo.userdata2);
        }
        else{
            crps->callbackInfo.callback(WGPUCreatePipelineAsyncStatus_InternalError, crps->renderPipeline, (WGPUStringView){"", 0}, crps->callbackInfo.userdata1, crps->callbackInfo.userdata2);
        }
    }
    atomic_store_explicit(&crps->completed, 1, memory_order_release);
    return _crps;
}
static void wgpuDeviceCreateRenderPipelineAsync_wait(void* _crps){
    CreateRenderPipelineAsyncState* crps = (CreateRenderPipelineAsyncState*)_crps;
    do{
        volatile int x = 0;
    }while(atomic_load_explicit(&crps->completed, memory_order_acquire) == 0);
    if(crps->callbackInfo.mode == WGPUCallbackMode_WaitAnyOnly){
        if(crps->renderPipeline){
            crps->callbackInfo.callback(WGPUCreatePipelineAsyncStatus_Success, crps->renderPipeline, (WGPUStringView){"", 0}, crps->callbackInfo.userdata1, crps->callbackInfo.userdata2);
        }
        else{
            crps->callbackInfo.callback(WGPUCreatePipelineAsyncStatus_InternalError, crps->renderPipeline, (WGPUStringView){"", 0}, crps->callbackInfo.userdata1, crps->callbackInfo.userdata2);
        }
    }
}

WGPUFuture wgpuDeviceCreateRenderPipelineAsync(WGPUDevice device, const WGPURenderPipelineDescriptor* descriptor, WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo) {
    ENTRY();
    uint64_t futureID = atomic_fetch_add_explicit(&device->adapter->instance->currentFutureId, 1, memory_order_relaxed);
    CreateRenderPipelineAsyncState* crps = RL_CALLOC(1, sizeof(CreateRenderPipelineAsyncState));
    crps->callbackInfo = callbackInfo;
    crps->completed = 0;
    crps->device = device;
    crps->rpdesc = copyRenderPipelineDescriptor(descriptor);
    //if(callbackInfo.mode == WGPUCallbackMode_AllowSpontaneous){
    wgvk_thread_create(&crps->thread, wgpuDeviceCreateRenderPipelineAsync_sync, (void*)crps);
    //}

    WGPUFutureImpl* futureImpl = RL_CALLOC(1, sizeof(WGPUFutureImpl));
    futureImpl->freeUserData = RL_FREE;

    futureImpl->functionCalledOnWaitAny = wgpuDeviceCreateRenderPipelineAsync_wait;


    EXIT();
    return (WGPUFuture){ futureID };                                                                                                                                                                              
}
void wgpuDeviceDestroy(WGPUDevice device) {
    ENTRY();
    EXIT();
}
void wgpuDeviceGetFeatures(WGPUDevice device, WGPUSupportedFeatures * features) {
     ENTRY();

     EXIT();
}
WGPUStatus wgpuDeviceGetLimits(WGPUDevice device, WGPULimits * limits) {
    ENTRY();
    EXIT();
    return WGPUStatus_Error;
}
WGPUFuture wgpuDeviceGetLostFuture(WGPUDevice device) {
    ENTRY();
    EXIT();
    return (WGPUFuture){0};
}
WGPUBool wgpuDeviceHasFeature(WGPUDevice device, WGPUFeatureName feature) {
    ENTRY();
    EXIT();
    return 0;
}
WGPUFuture wgpuDevicePopErrorScope(WGPUDevice device, WGPUPopErrorScopeCallbackInfo callbackInfo) {
    ENTRY();
    EXIT();
    return (WGPUFuture){0};
}
void wgpuDevicePushErrorScope(WGPUDevice device, WGPUErrorFilter filter) {
    ENTRY();

    EXIT();
}
void wgpuDeviceSetLabel(WGPUDevice device, WGPUStringView label) {
    ENTRY();

    EXIT();
}

// Stubs for missing Methods of Instance
WGPUStatus wgpuInstanceGetWGSLLanguageFeatures(WGPUInstance instance, WGPUSupportedWGSLLanguageFeatures * features) {
    ENTRY();
    EXIT();
    return WGPUStatus_Error;
}
WGPUBool wgpuInstanceHasWGSLLanguageFeature(WGPUInstance instance, WGPUWGSLLanguageFeatureName feature) {
    ENTRY();
    EXIT();
    return 0;
}
void wgpuInstanceProcessEvents(WGPUInstance instance) {
    ENTRY()
    EXIT();
}

// Stubs for missing Methods of PipelineLayout
void wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, WGPUStringView label) {
    ENTRY();
    EXIT();
}

// Stubs for missing Methods of QuerySet
void wgpuQuerySetDestroy(WGPUQuerySet querySet) {
    ENTRY();

    EXIT();
}
uint32_t wgpuQuerySetGetCount(WGPUQuerySet querySet) {
    ENTRY();
    EXIT();
    return 0;
}
WGPUQueryType wgpuQuerySetGetType(WGPUQuerySet querySet) {
    ENTRY();
    EXIT();
    return querySet->type;
}
void wgpuQuerySetSetLabel(WGPUQuerySet querySet, WGPUStringView label) {
    ENTRY();

    EXIT();
}
void wgpuQuerySetAddRef(WGPUQuerySet querySet) {
    ENTRY();

    EXIT();
}
void wgpuQuerySetRelease(WGPUQuerySet querySet) {
    ENTRY();

    EXIT();
}


static void processWorkDoneFuture(void* userdata) {
    WorkDoneFutureState* state = (WorkDoneFutureState*)userdata;

    wgpuFenceWait(state->fence, UINT64_MAX);

    WGPUFenceState finalState = atomic_load_explicit(&state->fence->state, memory_order_acquire);
    
    if (finalState == WGPUFenceState_Finished) {
        if (state->callbackInfo.callback) {
            state->callbackInfo.callback(WGPUQueueWorkDoneStatus_Success, 
                                         state->callbackInfo.userdata1, 
                                         state->callbackInfo.userdata2);
        }
    } else {
        // Something went terribly wrong
        if (state->callbackInfo.callback) {
            state->callbackInfo.callback(WGPUQueueWorkDoneStatus_Error, 
                                         state->callbackInfo.userdata1, 
                                         state->callbackInfo.userdata2);
        }
    }
}

static void freeWorkDoneFutureState(void* userdata) {
    if (!userdata) return;
    WorkDoneFutureState* state = (WorkDoneFutureState*)userdata;
    
    wgpuFenceRelease(state->fence);
    
    RL_FREE(state);
}

// The main implementation
WGPUFuture wgpuQueueOnSubmittedWorkDone(WGPUQueue queue, WGPUQueueWorkDoneCallbackInfo callbackInfo) {
    ENTRY();

    WGPUFence fence = wgpuDeviceCreateFence(queue->device);
    if (!fence) {
        if (callbackInfo.callback) {
            callbackInfo.callback(WGPUQueueWorkDoneStatus_Error, callbackInfo.userdata1, callbackInfo.userdata2);
        }
        return (WGPUFuture){0};
    }

    
    // TODO: This function works correctly as-is, but it is suboptimal so submit an empty batch
    // Just to get a fence. One could also reuse the latest fence used from that queue
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 0,    // No command buffers
        .pCommandBuffers = NULL,
        .waitSemaphoreCount = 0,
        .signalSemaphoreCount = 0,
    };

    VkResult result = queue->device->functions.vkQueueSubmit(queue->graphicsQueue, 1, &submitInfo, fence->fence);

    if (result != VK_SUCCESS) {
        wgpuFenceRelease(fence);
        if (callbackInfo.callback) {
            callbackInfo.callback(WGPUQueueWorkDoneStatus_Error, callbackInfo.userdata1, callbackInfo.userdata2);
        }
        return (WGPUFuture){0};
    }
    
    atomic_store_explicit(&fence->state, WGPUFenceState_InUse, memory_order_release);
    WorkDoneFutureState* futureState = RL_CALLOC(1, sizeof(WorkDoneFutureState));
    futureState->fence = fence;
    futureState->callbackInfo = callbackInfo;
    futureState->device = queue->device;

    // 4. Create and register the WGPUFuture.
    WGPUInstance instance = queue->device->adapter->instance;
    WGPUFutureImpl futureImpl = {
        .userdataForFunction = futureState,
        .functionCalledOnWaitAny = processWorkDoneFuture,
        .freeUserData = freeWorkDoneFutureState
    };
    
    uint64_t futureID = atomic_fetch_add_explicit(&instance->currentFutureId, 1, memory_order_relaxed);
    FutureIDMap_put(&instance->g_futureIDMap, futureID, futureImpl);
    
    EXIT();
    return (WGPUFuture){ .id = futureID };
}

void wgpuQueueSetLabel(WGPUQueue queue, WGPUStringView label) {
    ENTRY();

    EXIT();
}

// Stubs for missing Methods of RenderBundle
void wgpuRenderBundleSetLabel(WGPURenderBundle renderBundle, WGPUStringView label) {
    ENTRY();

    EXIT();
}
void wgpuRenderBundleAddRef(WGPURenderBundle renderBundle) {
    ENTRY();

    EXIT();
}
void wgpuRenderBundleRelease(WGPURenderBundle renderBundle) {
    ENTRY();

    EXIT();
}

// Stubs for missing Methods of RenderBundleEncoder
void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView markerLabel) {
    ENTRY();

    EXIT();
}
void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder) {
    ENTRY();

    EXIT();
}
void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView groupLabel) {
    ENTRY();

    EXIT();
}
void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, WGPUStringView label) {
    ENTRY();

    EXIT();
}

// Stubs for missing Methods of RenderPassEncoder
void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex) {
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_begin_occlusion_query,
        .beginOcclusionQuery = {
            .queryIndex = queryIndex
        }
    };
    RenderPassCommandGenericVector_push_back(&renderPassEncoder->bufferedCommands, insert);
    EXIT();
}
void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder) {
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_end_occlusion_query
    };
    RenderPassCommandGenericVector_push_back(&renderPassEncoder->bufferedCommands, insert);
    EXIT();
}

static inline size_t WGPUStringView_length(WGPUStringView view){
    return (view.length == WGPU_STRLEN) ? strlen(view.data) : view.length; 
}

void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, WGPUStringView markerLabel) {
    ENTRY();
    
    size_t length = WGPUStringView_length(markerLabel);
    wgvk_assert(length <= 30, "Debug marker labels can't be longer than 30 chars");

    RenderPassCommandGeneric insert = {
        .type = rp_command_type_insert_debug_marker
    };
    memcpy(insert.insertDebugMarker.text, markerLabel.data, length);
    insert.insertDebugMarker.length = length;
    RenderPassCommandGenericVector_push_back(&renderPassEncoder->bufferedCommands, insert);
    EXIT();
}

void wgpuRenderPassEncoderMultiDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPU_NULLABLE WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) {
    ENTRY();
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_multi_draw_indexed_indirect,
        .multiDrawIndexedIndirect = {
            .indirectBuffer = indirectBuffer,
            .indirectOffset = indirectOffset,
            .maxDrawCount = maxDrawCount,
            .drawCountBuffer = drawCountBuffer,
            .drawCountBufferOffset = drawCountBufferOffset
        }
    };
    RenderPassCommandGenericVector_push_back(&renderPassEncoder->bufferedCommands, insert);
    EXIT();
}

void wgpuRenderPassEncoderMultiDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset, uint32_t maxDrawCount, WGPU_NULLABLE WGPUBuffer drawCountBuffer, uint64_t drawCountBufferOffset) {
    ENTRY();
    // Assumes RenderPassCommandMultiDrawIndirect has fields for all parameters.
    RenderPassCommandGeneric insert = {
        .type = rp_command_type_multi_draw_indirect,
        .multiDrawIndirect = {
            .indirectBuffer = indirectBuffer,
            .indirectOffset = indirectOffset,
            .maxDrawCount = maxDrawCount,
            .drawCountBuffer = drawCountBuffer,
            .drawCountBufferOffset = drawCountBufferOffset
        }
    };
    RenderPassCommandGenericVector_push_back(&renderPassEncoder->bufferedCommands, insert);
    EXIT();
}

void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder) {
    ENTRY();
    // Implemented as a no-op.
    (void)renderPassEncoder;
    EXIT();
}

void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, WGPUStringView groupLabel) {
    ENTRY();
    // Implemented as a no-op.
    (void)renderPassEncoder;
    (void)groupLabel;
    EXIT();
}

void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, WGPUStringView label) {
    ENTRY();
    // This typically sets a debug label on the encoder object itself and is not a recorded command.
    // The WGPURenderPassEncoder struct definition is not provided, so this is a no-op.
    // An actual implementation might be: `renderPassEncoder->label = strdup(label);`
    (void)renderPassEncoder;
    (void)label;
    EXIT();
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference) {
    ENTRY();
    RenderPassCommandSetStencilReference setStencilReference = {
        .reference = reference
    };
    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_set_stencil_reference,
        .setStencilReference = setStencilReference,
    };
    RenderPassEncoder_PushCommand(renderPassEncoder, &cmd);
    // This sets state on the encoder for subsequent draws. It is not a recorded command.
    // Assumes the WGPURenderPassEncoder struct has a 'stencilReference' member.
    //renderPassEncoder->stencilReference = reference;
    EXIT();
}
// Stubs for missing Methods of RenderPipeline
WGPUBindGroupLayout wgpuRenderPipelineGetBindGroupLayout(WGPURenderPipeline renderPipeline, uint32_t groupIndex) {
    ENTRY();
    return renderPipeline->layout->bindGroupLayouts[groupIndex];
    EXIT();
}
void wgpuRenderPipelineSetLabel(WGPURenderPipeline renderPipeline, WGPUStringView label) {
    ENTRY();
    (void)renderPipeline;
    (void)label;
    EXIT();
}

// Stubs for missing Methods of Sampler
void wgpuSamplerSetLabel(WGPUSampler sampler, WGPUStringView label) {
    ENTRY();
    (void)sampler;
    (void)label;
    EXIT();
}

// Stubs for missing Methods of ShaderModule
WGPUFuture wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallbackInfo callbackInfo) {
    ENTRY();
    return (WGPUFuture){0};
    EXIT();
}
void wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, WGPUStringView label) {
    ENTRY();
    (void)shaderModule;
    (void)label;
    EXIT();
}

// Stubs for missing Methods of Surface
void wgpuSurfaceSetLabel(WGPUSurface surface, WGPUStringView label) {
    ENTRY();
    (void)surface;
    (void)label;
    EXIT();
}
void wgpuSurfaceUnconfigure(WGPUSurface surface) {
    ENTRY();
    WGPUDevice device = surface->device;
    uint32_t cacheIndex = surface->device->submittedFrames % framesInFlight;
    PerframeCache* fcache = DeviceGetFIFCache(device, cacheIndex);
    SyncState* syncState = DeviceGetSyncState(device, cacheIndex);

    device->functions.vkDeviceWaitIdle(device->device);
    if(syncState->acquireImageSemaphoreSignalled){
        VkPipelineStageFlags wm = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
        VkSubmitInfo sinfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = NULL,
            .pWaitDstStageMask = &wm,
            .pWaitSemaphores = &syncState->acquireImageSemaphore,
            .waitSemaphoreCount = 1
        };
        device->functions.vkQueueSubmit(surface->device->queue->graphicsQueue, 1, &sinfo, fcache->finalTransitionFence->fence);
        syncState->acquireImageSemaphoreSignalled = false;
        device->functions.vkWaitForFences(surface->device->device, 1, &fcache->finalTransitionFence->fence, VK_TRUE, UINT64_MAX);
        device->functions.vkResetFences(surface->device->device, 1, &fcache->finalTransitionFence->fence);
    }
    if(surface->presentSemaphores){
        for (uint32_t i = 0; i < surface->imagecount; i++) {
            device->functions.vkDestroySemaphore(surface->device->device, surface->presentSemaphores[i], NULL);
        }
        RL_FREE((void*)surface->presentSemaphores);
        surface->presentSemaphores = NULL;
    }

    if(surface->swapchain){
        for(uint32_t i = 0;i < surface->imagecount;i++){
            WGPUTexture swapchainTexture = surface->images[i];
            for(size_t j = 0;j < swapchainTexture->viewCache.current_capacity;j++){
                if(swapchainTexture->viewCache.table[j].key.format != VK_FORMAT_UNDEFINED){
                    WGPUTextureView cacheEntry = swapchainTexture->viewCache.table[j].value;
                    device->functions.vkDestroyImageView(cacheEntry->texture->device->device, cacheEntry->view, NULL);
                }

                //wgpuTextureViewRelease(swapchainTexture->viewCache.table[j].value);
            }
        }
        RL_FREE((void*)surface->images);
        device->functions.vkDestroySwapchainKHR(device->device, surface->swapchain, NULL);
        surface->swapchain = NULL;
    }
    EXIT();
}
void wgpuSurfaceAddRef(WGPUSurface surface) {
    ENTRY();
    ++surface->refCount;
    EXIT();
}

// Stubs for missing Methods of Texture
void wgpuTextureDestroy(WGPUTexture texture) {
    ENTRY();
    (void)texture;
    EXIT();
}
void wgpuTextureSetLabel(WGPUTexture texture, WGPUStringView label) {
    ENTRY();
    (void)texture;
    (void)label;
    EXIT();
}

// Stubs for missing Methods of TextureView
void wgpuTextureViewSetLabel(WGPUTextureView textureView, WGPUStringView label) {
    ENTRY();
    (void)textureView;
    (void)label;
    EXIT();
}



// =============================================================================
// Allocator implementation
// =============================================================================
static void allocator_destroy(VirtualAllocator* allocator) {
    if (!allocator) return;
    free(allocator->level0);
    free(allocator->level1);
    free(allocator->level2);
    memset(allocator, 0, sizeof(VirtualAllocator));
}

static bool allocator_create(VirtualAllocator* allocator, size_t size) {
    memset(allocator, 0, sizeof(VirtualAllocator));
    allocator->size_in_bytes = size;
    allocator->total_blocks = size / ALLOCATOR_GRANULARITY;
    allocator->l2_word_count = (allocator->total_blocks + BITS_PER_WORD - 1) / BITS_PER_WORD;
    allocator->l1_word_count = (allocator->l2_word_count + BITS_PER_WORD - 1) / BITS_PER_WORD;
    allocator->l0_word_count = (allocator->l1_word_count + BITS_PER_WORD - 1) / BITS_PER_WORD;

    allocator->level2 = calloc(allocator->l2_word_count, sizeof(uint64_t));
    allocator->level1 = calloc(allocator->l1_word_count, sizeof(uint64_t));
    allocator->level0 = calloc(allocator->l0_word_count, sizeof(uint64_t));

    if (!allocator->level2 || !allocator->level1 || !allocator->level0) {
        allocator_destroy(allocator);
        return false;
    }
    return true;
}

static size_t allocator_alloc(VirtualAllocator* allocator, size_t size, size_t alignment) {
    if (size == 0) return 0;
    if (size > allocator->size_in_bytes) return OUT_OF_SPACE;

    {
        size_t min_align = ALLOCATOR_GRANULARITY;
        if (alignment < min_align) alignment = min_align;
        size_t a = alignment - 1;
        a |= a >> 1;
        a |= a >> 2;
        a |= a >> 4;
        a |= a >> 8;
        a |= a >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
        a |= a >> 32;
#endif
        alignment = a + 1;
    }

    const size_t num_blocks = (size + ALLOCATOR_GRANULARITY - 1) / ALLOCATOR_GRANULARITY;
    if (num_blocks == 0) return 0;
    if (num_blocks > allocator->total_blocks) return OUT_OF_SPACE;

    const size_t last_start_block =
        allocator->total_blocks - num_blocks; // only these starts can ever fit

    for (size_t i = 0; i <= last_start_block; ) {
        size_t l2_word_idx = i / BITS_PER_WORD;
        size_t bit_idx     = i % BITS_PER_WORD;

        // Skip if current start block is occupied.
        if (((allocator->level2[l2_word_idx] >> bit_idx) & 1ULL) != 0) {
            ++i;
            continue;
        }

        // Enforce alignment in bytes, then convert to blocks.
        size_t offset         = i * ALLOCATOR_GRANULARITY;
        size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
        if (aligned_offset != offset) {
            size_t next_i = aligned_offset / ALLOCATOR_GRANULARITY;
            if (next_i <= i) ++i; else i = next_i;
            if (i > last_start_block) break; // cannot fit anymore
            continue;
        }

        // Probe forward for a contiguous free run.
        bool possible = true;
        for (size_t j = 0; j < num_blocks; ++j) {
            size_t block_to_check = i + j;
            // If this start would run past the end, stop the outer loop.
            if (block_to_check > last_start_block + (num_blocks - 1)) {
                i = last_start_block + 1; // force exit
                possible = false;
                break;
            }
            size_t check_l2_word = block_to_check / BITS_PER_WORD;
            size_t check_bit_idx = block_to_check % BITS_PER_WORD;
            if (((allocator->level2[check_l2_word] >> check_bit_idx) & 1ULL) != 0) {
                i = block_to_check + 1; // jump just past first conflict
                if (i > last_start_block) { possible = false; }
                else { possible = false; }
                break;
            }
        }

        if (!possible) {
            continue;
        }

        // Claim the run.
        size_t start_block_index = i;
        for (size_t j = 0; j < num_blocks; ++j) {
            size_t current_block     = start_block_index + j;
            size_t current_l2_word   = current_block / BITS_PER_WORD;
            size_t current_bit_idx   = current_block % BITS_PER_WORD;
            allocator->level2[current_l2_word] |= (1ULL << current_bit_idx);
        }

        // Update summary levels.
        size_t first_l2 = start_block_index / BITS_PER_WORD;
        size_t last_l2  = (start_block_index + num_blocks - 1) / BITS_PER_WORD;
        for (size_t l2_idx = first_l2; l2_idx <= last_l2; ++l2_idx) {
            if (allocator->level2[l2_idx] != 0) {
                size_t l1_idx = l2_idx / BITS_PER_WORD;
                size_t l1_bit = l2_idx % BITS_PER_WORD;
                allocator->level1[l1_idx] |= (1ULL << l1_bit);

                size_t l0_idx = l1_idx / BITS_PER_WORD;
                size_t l0_bit = l1_idx % BITS_PER_WORD;
                allocator->level0[l0_idx] |= (1ULL << l0_bit);
            }
        }
        return start_block_index * ALLOCATOR_GRANULARITY;
    }

    return OUT_OF_SPACE;
}


static void allocator_free(VirtualAllocator* allocator, size_t offset, size_t size) {
    if (size == 0) return;

    const size_t num_blocks = (size + ALLOCATOR_GRANULARITY - 1) / ALLOCATOR_GRANULARITY;
    const size_t start_block_index = offset / ALLOCATOR_GRANULARITY;

    size_t first_l2_word = start_block_index / BITS_PER_WORD;
    size_t last_l2_word = (start_block_index + num_blocks - 1) / BITS_PER_WORD;

    for (size_t i = 0; i < num_blocks; ++i) {
        size_t current_block = start_block_index + i;
        if (current_block >= allocator->total_blocks) break;
        size_t l2_word_idx = current_block / BITS_PER_WORD;
        size_t bit_idx = current_block % BITS_PER_WORD;
        allocator->level2[l2_word_idx] &= ~(1ULL << bit_idx);
    }

    for (size_t l2_idx = first_l2_word; l2_idx <= last_l2_word; ++l2_idx) {
        if (l2_idx >= allocator->l2_word_count) continue;
        if (allocator->level2[l2_idx] == 0) {
            size_t l1_idx = l2_idx / BITS_PER_WORD;
            size_t l1_bit = l2_idx % BITS_PER_WORD;
            allocator->level1[l1_idx] &= ~(1ULL << l1_bit);

            if (allocator->level1[l1_idx] == 0) {
                size_t l0_idx = l1_idx / BITS_PER_WORD;
                size_t l0_bit = l1_idx % BITS_PER_WORD;
                allocator->level0[l0_idx] &= ~(1ULL << l0_bit);
            }
        }
    }
}

static VkResult wgvkDeviceMemoryPool_create_chunk(WgvkDeviceMemoryPool* pool, size_t size) {
    if (pool->chunk_count == pool->chunk_capacity) {
        uint32_t new_capacity = pool->chunk_capacity == 0 ? 4 : pool->chunk_capacity * 2;
        WgvkMemoryChunk* new_chunks = realloc(pool->chunks, new_capacity * sizeof(WgvkMemoryChunk));
        if (!new_chunks) return VK_ERROR_OUT_OF_HOST_MEMORY;
        pool->chunks = new_chunks;
        pool->chunk_capacity = new_capacity;
    }

    WgvkMemoryChunk* new_chunk = &pool->chunks[pool->chunk_count];
    memset(new_chunk, 0, sizeof(WgvkMemoryChunk));
    if (!allocator_create(&new_chunk->allocator, size)) {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }

    VkMemoryAllocateFlagsInfo flagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    };

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = size,
        .memoryTypeIndex = pool->memoryTypeIndex,
    };
    if (pool->device->capabilities.shaderDeviceAddress) {
        allocInfo.pNext = &flagsInfo;
    }
    VkResult result = pool->pFunctions->vkAllocateMemory(pool->device->device, &allocInfo, NULL, &new_chunk->memory);
    if (result != VK_SUCCESS) {
        allocator_destroy(&new_chunk->allocator);
        return result;
    }

    pool->chunk_count++;
    return VK_SUCCESS;
}

static bool wgvkDeviceMemoryPool_alloc(WgvkDeviceMemoryPool* pool, size_t size, size_t alignment, wgvkAllocation* out_allocation) {
    // Harden alignment at the callsite as well.
    {
        size_t min_align = ALLOCATOR_GRANULARITY;
        if (alignment < min_align) alignment = min_align;
        size_t a = alignment - 1;
        a |= a >> 1;
        a |= a >> 2;
        a |= a >> 4;
        a |= a >> 8;
        a |= a >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
        a |= a >> 32;
#endif
        alignment = a + 1;
    }

    for (uint32_t i = 0; i < pool->chunk_count; ++i) {
        WgvkMemoryChunk* chunk = &pool->chunks[i];
        size_t offset = allocator_alloc(&chunk->allocator, size, alignment);
        if (offset != OUT_OF_SPACE) {
            out_allocation->pool = pool;
            out_allocation->offset = offset;
            out_allocation->size = size;
            out_allocation->chunk_index = i;
            out_allocation->memory = chunk->memory;
            return true;
        }
    }

    size_t last_chunk_size = pool->chunk_count > 0 ? pool->chunks[pool->chunk_count - 1].allocator.size_in_bytes : MIN_CHUNK_SIZE / 2;
    size_t new_chunk_size = last_chunk_size * 2;
    if (size > new_chunk_size) new_chunk_size = size;
    if (new_chunk_size < MIN_CHUNK_SIZE) new_chunk_size = MIN_CHUNK_SIZE;

    VkResult result = wgvkDeviceMemoryPool_create_chunk(pool, new_chunk_size);
    if (result != VK_SUCCESS) return false;

    uint32_t new_chunk_index = pool->chunk_count - 1;
    WgvkMemoryChunk* new_chunk = &pool->chunks[new_chunk_index];
    size_t offset = allocator_alloc(&new_chunk->allocator, size, alignment);

    if (offset != OUT_OF_SPACE) {
        out_allocation->pool = pool;
        out_allocation->offset = offset;
        out_allocation->size = size;
        out_allocation->chunk_index = new_chunk_index;
        out_allocation->memory = new_chunk->memory;
        return true;
    }
    return false;
}

static void wgvkDeviceMemoryPool_free(const wgvkAllocation* allocation) {
    if (!allocation || !allocation->pool || allocation->chunk_index >= allocation->pool->chunk_count) {
        return;
    }
    WgvkMemoryChunk* chunk = &allocation->pool->chunks[allocation->chunk_index];
    allocator_free(&chunk->allocator, allocation->offset, allocation->size);
}

static void wgvkDeviceMemoryPool_destroy(WgvkDeviceMemoryPool* pool) {
    if (!pool) return;
    for (uint32_t i = 0; i < pool->chunk_count; ++i) {
        WgvkMemoryChunk* chunk = &pool->chunks[i];
        vkFreeMemory(pool->device->device, chunk->memory, NULL);
        allocator_destroy(&chunk->allocator);
    }
    free(pool->chunks);
}

RGAPI VkResult wgvkAllocator_init(WgvkAllocator* allocator, VkPhysicalDevice physicalDevice, WGPUDevice device, struct VolkDeviceTable* dtable) {
    memset(allocator, 0, sizeof(WgvkAllocator));
    allocator->device = device;
    allocator->physicalDevice = physicalDevice;
    allocator->pFunctions = dtable;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &allocator->memoryProperties);
    return VK_SUCCESS;
}

RGAPI void wgvkAllocator_destroy(WgvkAllocator* allocator) {
    if (!allocator) return;
    for (uint32_t i = 0; i < allocator->pool_count; ++i) {
        wgvkDeviceMemoryPool_destroy(&allocator->pools[i]);
    }
    free(allocator->pools);
    memset(allocator, 0, sizeof(WgvkAllocator));
}

RGAPI bool wgvkAllocator_alloc(WgvkAllocator* allocator, const VkMemoryRequirements* requirements, VkMemoryPropertyFlags propertyFlags, wgvkAllocation* out_allocation) {
    // Normalized alignment used everywhere below.
    size_t norm_alignment = requirements->alignment;
    {
        size_t min_align = ALLOCATOR_GRANULARITY;
        if (norm_alignment < min_align) norm_alignment = min_align;
        size_t a = norm_alignment - 1;
        a |= a >> 1;
        a |= a >> 2;
        a |= a >> 4;
        a |= a >> 8;
        a |= a >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
        a |= a >> 32;
#endif
        norm_alignment = a + 1;
    }

    for (uint32_t i = 0; i < allocator->memoryProperties.memoryTypeCount; ++i) {
        if (!((requirements->memoryTypeBits >> i) & 1)) continue;
        if ((allocator->memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) != propertyFlags) continue;

        WgvkDeviceMemoryPool* found_pool = NULL;
        for (uint32_t j = 0; j < allocator->pool_count; ++j) {
            if (allocator->pools[j].memoryTypeIndex == i) {
                found_pool = &allocator->pools[j];
                break;
            }
        }

        if (found_pool) {
            if (wgvkDeviceMemoryPool_alloc(found_pool, requirements->size, norm_alignment, out_allocation)) {
                return true;
            }
        }
    }

    for (uint32_t i = 0; i < allocator->memoryProperties.memoryTypeCount; ++i) {
        if (!((requirements->memoryTypeBits >> i) & 1)) continue;
        if ((allocator->memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) != propertyFlags) continue;

        if (allocator->pool_count == allocator->pool_capacity) {
            uint32_t new_capacity = allocator->pool_capacity == 0 ? 4 : allocator->pool_capacity * 2;
            WgvkDeviceMemoryPool* new_pools = RL_REALLOC(allocator->pools, new_capacity * sizeof(WgvkDeviceMemoryPool));
            if (!new_pools) return false;
            allocator->pools = new_pools;
            allocator->pool_capacity = new_capacity;
        }

        WgvkDeviceMemoryPool* new_pool = &allocator->pools[allocator->pool_count];
        memset(new_pool, 0, sizeof(WgvkDeviceMemoryPool));
        new_pool->device = allocator->device;
        new_pool->physicalDevice = allocator->physicalDevice;
        new_pool->memoryTypeIndex = i;
        new_pool->pFunctions = allocator->pFunctions;

        allocator->pool_count++;

        if (wgvkDeviceMemoryPool_alloc(new_pool, requirements->size, norm_alignment, out_allocation)) {
            return true;
        } else {
            allocator->pool_count--;
        }
    }

    return false;
}

RGAPI void wgvkAllocator_free(const wgvkAllocation* allocation) {
    wgvkDeviceMemoryPool_free(allocation);
}

// =============================================================================
// Threads implementation
// =============================================================================

#define _POSIX_C_SOURCE 200809L

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
    #define WGVK_OS_WINDOWS 1
    #include <windows.h>
#else
    #define WGVK_OS_POSIX 1
    #include <pthread.h>
    #include <sched.h>
    #include <unistd.h>
#endif


/**
 * @brief Portable "yield" function
 * 
 */
static inline void wgvk_cpu_relax(void) {
#if defined(WGVK_OS_WINDOWS)
    YieldProcessor();              /* maps to PAUSE/YIELD on target */
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();        /* GCC/Clang x86 */
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield"); /* ARM */
#else
    atomic_signal_fence(memory_order_seq_cst); /* portable fallback */
#endif
}



/* ------------------------ thread implementation ------------------------ */

#if defined(WGVK_OS_WINDOWS)

typedef struct win32_thread_entry_t{
    HANDLE handle;
    void* return_value;
    int in_use;
} win32_thread_entry_t;

typedef struct win32_thread_data_t{
    wgvk_thread_func_t func;
    void* arg;
    HANDLE thread_handle;
} win32_thread_data_t;


DEFINE_PTR_HASH_MAP_ERASABLE(static inline, Win32ThreadHandleRegistry, win32_thread_entry_t)

static Win32ThreadHandleRegistry global_win32_thread_registry;
static CRITICAL_SECTION registry_lock;
static int win32thread_registry_initialized = 0;

static void store_return_value(HANDLE handle, void* retval) {
    wgvk_assert(win32thread_registry_initialized, "Thread registry not initialized");

    EnterCriticalSection(&registry_lock);
    
    win32_thread_entry_t* entry_for_handle = Win32ThreadHandleRegistry_get(&global_win32_thread_registry, (void*)handle);
    wgvk_assert(entry_for_handle != NULL, "Thread handle nonexistent in global_win32_thread_registry");
    if(entry_for_handle){
        entry_for_handle->return_value = retval;
    }
    
    LeaveCriticalSection(&registry_lock);
}

static void init_registry(void) {
    if (!win32thread_registry_initialized) {
        InitializeCriticalSection(&registry_lock);
        Win32ThreadHandleRegistry_init(&global_win32_thread_registry);
        win32thread_registry_initialized = 1;
    }
}


static int register_thread(HANDLE handle) {
    init_registry();
    EnterCriticalSection(&registry_lock);
    
    win32_thread_entry_t value = {
        .handle = handle,
        .return_value = NULL,
        .in_use = 1,
    };
    Win32ThreadHandleRegistry_put(&global_win32_thread_registry, handle, value);
    
    LeaveCriticalSection(&registry_lock);
    return -1;  // Registry full
}

static DWORD WINAPI wgvk_thread_trampoline(LPVOID arg) {

    win32_thread_data_t* data = (win32_thread_data_t*)arg;
    void* retval = data->func(data->arg);
    
    store_return_value(data->thread_handle, retval);
    free(data);
    return 0;
}

int wgvk_thread_create(wgvk_thread_t* thread, wgvk_thread_func_t func, void* arg) {
    
    win32_thread_data_t* data = malloc(sizeof(win32_thread_data_t));
    if (!data) return -1;
    
    data->func = func;
    data->arg = arg;
    
    HANDLE handle = CreateThread(NULL, 0, wgvk_thread_trampoline, data, CREATE_SUSPENDED, NULL);
    if (!handle) {
        free(data);
        return -1;
    }
    
    data->thread_handle = handle;
    
    if (register_thread(handle) < 0) {
        TerminateThread(handle, 0);
        CloseHandle(handle);
        free(data);
        return -1;
    }
    
    ResumeThread(handle);  // Start the thread
    thread->handle = handle;
    return 0;
}

static void* get_return_value(HANDLE handle) {
    wgvk_assert(win32thread_registry_initialized, "Thread registry not initialized");
    void* retval = NULL;
    
    EnterCriticalSection(&registry_lock);
    win32_thread_entry_t* entry_for_handle = Win32ThreadHandleRegistry_get(&global_win32_thread_registry, handle);
    wgvk_assert(entry_for_handle != NULL, "Thread handle nonexistent in global_win32_thread_registry");
    
    retval = entry_for_handle->return_value;
    Win32ThreadHandleRegistry_erase(&global_win32_thread_registry, handle);
    
    LeaveCriticalSection(&registry_lock);
    return retval;
}

int wgvk_thread_join(wgvk_thread_t* thread, void** result) {

    DWORD retCode = WaitForSingleObject(thread, INFINITE);
    if (retCode != WAIT_OBJECT_0) return -1;
    
    if (result) {
        *result = get_return_value(thread);
    } else {
        get_return_value(thread);  // Still need to clean up registry
    }
    
    CloseHandle(thread);
    return 0;
}

int wgvk_thread_detach(wgvk_thread_t* thread) {
    if (!thread || !thread->handle) { errno = EINVAL; return -1; }
    CloseHandle(thread->handle);
    thread->handle = NULL;
    return 0;
}

void wgvk_thread_yield(void) {
    SwitchToThread();
}

#else /* POSIX */

void wgvk_thread_yield(void) {
    sched_yield();
}

int wgvk_thread_create(wgvk_thread_t* thread, wgvk_thread_func_t func, void* arg) {
    if (!thread || !func) { errno = EINVAL; return -1; }
    int e = pthread_create(&thread->handle, NULL, func, arg);
    return e ? -1 : 0;
}

int wgvk_thread_join(wgvk_thread_t* thread, void** result) {
    if (!thread) { errno = EINVAL; return -1; }
    int e = pthread_join(thread->handle, result);
    return e ? -1 : 0;
}

int wgvk_thread_detach(wgvk_thread_t* thread) {
    if (!thread) { errno = EINVAL; return -1; }
    int e = pthread_detach(thread->handle);
    return e ? -1 : 0;
}

#endif

/* ------------------------ opaque mutex / cond types ------------------------ */

/* mutex: kernel (pthread_mutex / CRITICAL_SECTION) OR spin (atomic_flag) */
struct wgvk_mutex {
    wgvk_locktype backend;
#if defined(WGVK_OS_POSIX)
    union {
        pthread_mutex_t pm;
        atomic_flag     spin;
    } u;
#else
    union {
        CRITICAL_SECTION cs;
        atomic_flag      spin;
    } u;
#endif
};

/**
 * @brief condition_variable type, either kernel (pthread/win32 threads)
 * or purely busy spinwait based
 */
struct wgvk_cond {
    wgvk_locktype backend;
#if defined(WGVK_OS_POSIX)
    union {
        pthread_cond_t pc; /* kernel */
        struct {           /* spin */
            atomic_uint tokens;   /* produced wakeups */
            atomic_uint waiters;  /* threads currently in wait */
        } s;
    } u;
#else
    union {
        CONDITION_VARIABLE cv; /* kernel */
        struct {               /* spin */
            atomic_uint tokens;
            atomic_uint waiters;
        } s;
    } u;
#endif
};


/* ------------------------ mutex API ------------------------ */

wgvk_mutex_t* wgvk_mutex_create(wgvk_locktype backend) {
    wgvk_mutex_t* m = malloc(sizeof(*m));
    if (!m) { errno = ENOMEM; return NULL; }
    m->backend = backend;
#if defined(WGVK_OS_POSIX)
    if (backend == wgvk_locktype_kernel) {
        if (pthread_mutex_init(&m->u.pm, NULL) != 0) { free(m); return NULL; }
    } else {
        atomic_flag_clear(&m->u.spin);
    }
#else /* Windows */
    if (backend == wgvk_locktype_kernel) {
        /* prefer InitializeCriticalSectionAndSpinCount for better perf */
        if (!InitializeCriticalSectionAndSpinCount(&m->u.cs, 0x00000400)) {
            free(m); errno = ENOMEM; return NULL;
        }
    } else {
        atomic_flag_clear(&m->u.spin);
    }
#endif
    return m;
}

int wgvk_mutex_destroy(wgvk_mutex_t* m) {
    if (!m) return EINVAL;
#if defined(WGVK_OS_POSIX)
    if (m->backend == wgvk_locktype_kernel) {
        int e = pthread_mutex_destroy(&m->u.pm);
        free(m);
        return e;
    } else {
        free(m);
        return 0;
    }
#else
    if (m->backend == wgvk_locktype_kernel) {
        DeleteCriticalSection(&m->u.cs);
        free(m);
        return 0;
    } else {
        free(m);
        return 0;
    }
#endif
}

int wgvk_mutex_lock(wgvk_mutex_t* m) {
    if (!m) return EINVAL;
#if defined(WGVK_OS_POSIX)
    if (m->backend == wgvk_locktype_kernel) {
        return pthread_mutex_lock(&m->u.pm);
    } else {
        while (atomic_flag_test_and_set_explicit(&m->u.spin, memory_order_acquire)) {
            /* polite backoff */
            sched_yield();
        }
        return 0;
    }
#else
    if (m->backend == wgvk_locktype_kernel) {
        EnterCriticalSection(&m->u.cs);
        return 0;
    } else {
        while (atomic_flag_test_and_set_explicit(&m->u.spin, memory_order_acquire)) {
            Sleep(0);
        }
        return 0;
    }
#endif
}

int wgvk_mutex_try_lock(wgvk_mutex_t* m) {
    if (!m) {
        return EINVAL;
    }

#if defined(WGVK_OS_POSIX)
    if (m->backend == wgvk_locktype_kernel) {
        // pthread_mutex_trylock returns 0 on success, or an error code (like EBUSY) on failure
        return pthread_mutex_trylock(&m->u.pm);
    } else {
        // atomic_flag_test_and_set returns the *previous* state.
        // If false (clear), we acquired the lock. If true (set), it was already locked
        if (atomic_flag_test_and_set_explicit(&m->u.spin, memory_order_acquire)) {
            return EBUSY;
        }
        return 0;
    }
#else /* Windows */
    if (m->backend == wgvk_locktype_kernel) {
        // TryEnterCriticalSection returns a non-zero value on success and 0 on failure.
        if (TryEnterCriticalSection(&m->u.cs)) {
            return 0; // Success
        } else {
            return EBUSY; // Return a POSIX-style error code for "busy"
        }
    } else {
        // The logic for the atomic spinlock is the same as on POSIX.
        if (atomic_flag_test_and_set_explicit(&m->u.spin, memory_order_acquire)) {
            return EBUSY;
        }
        return 0;
    }
#endif
}

int wgvk_mutex_unlock(wgvk_mutex_t* m) {
    if (!m) return EINVAL;
#if defined(WGVK_OS_POSIX)
    if (m->backend == wgvk_locktype_kernel) {
        return pthread_mutex_unlock(&m->u.pm);
    } else {
        atomic_flag_clear_explicit(&m->u.spin, memory_order_release);
        return 0;
    }
#else
    if (m->backend == wgvk_locktype_kernel) {
        LeaveCriticalSection(&m->u.cs);
        return 0;
    } else {
        atomic_flag_clear_explicit(&m->u.spin, memory_order_release);
        return 0;
    }
#endif
}

/* ------------------------ cond API ------------------------ */

wgvk_cond_t* wgvk_cond_create(wgvk_locktype backend) {
    wgvk_cond_t* c = malloc(sizeof(*c));
    if (!c) { errno = ENOMEM; return NULL; }
    c->backend = backend;
#if defined(WGVK_OS_POSIX)
    if (backend == wgvk_locktype_kernel) {
        if (pthread_cond_init(&c->u.pc, NULL) != 0) { free(c); return NULL; }
    } else {
        atomic_init(&c->u.s.tokens, 0u);
        atomic_init(&c->u.s.waiters, 0u);
    }
#else
    if (backend == wgvk_locktype_kernel) {
        InitializeConditionVariable(&c->u.cv);
    } else {
        atomic_init(&c->u.s.tokens, 0u);
        atomic_init(&c->u.s.waiters, 0u);
    }
#endif
    return c;
}


int wgvk_cond_destroy(wgvk_cond_t* c) {
    if (!c) return EINVAL;
#if defined(WGVK_OS_POSIX)
    if (c->backend == wgvk_locktype_kernel) {
        int e = pthread_cond_destroy(&c->u.pc);
        free(c);
        return e;
    } else {
        free(c);
        return 0;
    }
#else
    if (c->backend == wgvk_locktype_kernel) {
        /* no destroy for CONDITION_VARIABLE */
        free(c);
        return 0;
    } else {
        free(c);
        return 0;
    }
#endif
}

/* ----------------------------- wgvk_cond_wait ----------------------------- */
/* m must be a spin mutex when c is spin-backed. */
int wgvk_cond_wait(wgvk_cond_t* c, wgvk_mutex_t* m) {
    if (!c || !m) return EINVAL;

    if (c->backend == wgvk_locktype_kernel) {
#if defined(WGVK_OS_POSIX)
        if (m->backend != wgvk_locktype_kernel) return EINVAL;
        return pthread_cond_wait(&c->u.pc, &m->u.pm);
#else
        if (m->backend != wgvk_locktype_kernel) return EINVAL;
        BOOL ok = SleepConditionVariableCS(&c->u.cv, &m->u.cs, INFINITE);
        return ok ? 0 : -1;
#endif
    }

    if (m->backend != wgvk_locktype_spin) return EINVAL;

    /* Register as waiter before releasing the lock to avoid lost wakeups. */
    atomic_fetch_add_explicit(&c->u.s.waiters, 1u, memory_order_acq_rel);

    /* Release spin mutex. */
    atomic_flag_clear_explicit(&m->u.spin, memory_order_release);

    /* Busy-wait until we consume one token. */
    for (;;) {
        unsigned t = atomic_load_explicit(&c->u.s.tokens, memory_order_acquire);
        if (t > 0) {
            if (atomic_compare_exchange_weak_explicit(&c->u.s.tokens, &t, t - 1,
                                                      memory_order_acq_rel,
                                                      memory_order_relaxed)) {
                break; /* token consumed */
            }
        }
        wgvk_cpu_relax();
    }

    /* We are no longer a waiter. */
    atomic_fetch_sub_explicit(&c->u.s.waiters, 1u, memory_order_acq_rel);

    /* Reacquire spin mutex. */
    while (atomic_flag_test_and_set_explicit(&m->u.spin, memory_order_acquire))
        wgvk_cpu_relax();

    return 0;
}

/* ---------------------------- wgvk_cond_signal ---------------------------- */
/* Produces exactly one token if at least one thread is waiting. */
int wgvk_cond_signal(wgvk_cond_t* c) {
    if (!c) return EINVAL;

    if (c->backend == wgvk_locktype_kernel) {
#if defined(WGVK_OS_POSIX)
        return pthread_cond_signal(&c->u.pc);
#else
        WakeConditionVariable(&c->u.cv);
        return 0;
#endif
    }

    unsigned w = atomic_load_explicit(&c->u.s.waiters, memory_order_acquire);
    if (w == 0) return 0;

    /* Avoid accumulating surplus tokens. */
    for (;;) {
        unsigned t = atomic_load_explicit(&c->u.s.tokens, memory_order_acquire);
        if (t >= w) break; /* enough tokens outstanding */
        if (atomic_compare_exchange_weak_explicit(&c->u.s.tokens, &t, t + 1,
                                                  memory_order_release,
                                                  memory_order_relaxed)) {
            break;
        }
    }
    return 0;
}

/* -------------------------- wgvk_cond_broadcast --------------------------- */
/* Produces one token per observed waiter. */
int wgvk_cond_broadcast(wgvk_cond_t* c) {
    if (!c) return EINVAL;

    if (c->backend == wgvk_locktype_kernel) {
#if defined(WGVK_OS_POSIX)
        return pthread_cond_broadcast(&c->u.pc);
#else
        WakeAllConditionVariable(&c->u.cv);
        return 0;
#endif
    }

    unsigned w = atomic_load_explicit(&c->u.s.waiters, memory_order_acquire);
    if (w == 0) return 0;

    /* Bring tokens up to at least waiters. */
    for (;;) {
        unsigned t = atomic_load_explicit(&c->u.s.tokens, memory_order_acquire);
        if (t >= w) break;
        if (atomic_compare_exchange_weak_explicit(&c->u.s.tokens, &t, w,
                                                  memory_order_release,
                                                  memory_order_relaxed)) {
            break;
        }
    }
    return 0;
}

/* ------------------------ thread-pool (uses kernel primitives) ------------------------ */

/* worker */
static void* wgvk_thread_pool_worker(void* arg) {
    wgvk_thread_pool_t* pool = (wgvk_thread_pool_t*)arg;
    while (1) {
        wgvk_mutex_lock(pool->queue_mutex);
        while (pool->head == NULL && !pool->stop) {
            wgvk_cond_wait(pool->queue_cond, pool->queue_mutex);
        }
        if (pool->stop && pool->head == NULL) {
            wgvk_mutex_unlock(pool->queue_mutex);
            break;
        }
        wgvk_job_t* job = pool->head;
        pool->head = pool->head->next;
        if (pool->head == NULL) pool->tail = NULL;
        wgvk_mutex_unlock(pool->queue_mutex);

        /* update status and run */
        wgvk_mutex_lock(job->status_mutex);
        job->status = WGVK_JOB_RUNNING;
        wgvk_mutex_unlock(job->status_mutex);

        job->result = job->func(job->arg);

        wgvk_mutex_lock(job->status_mutex);
        job->status = WGVK_JOB_COMPLETED;
        wgvk_cond_signal(job->status_cond);
        wgvk_mutex_unlock(job->status_mutex);
    }
    return NULL;
}

wgvk_thread_pool_t* wgvk_thread_pool_create(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;
    wgvk_thread_pool_t* pool = malloc(sizeof(*pool));
    if (!pool) { errno = ENOMEM; return NULL; }
    pool->num_threads = num_threads;
    pool->workers = calloc(num_threads, sizeof(wgvk_thread_t));
    if (!pool->workers) { free(pool); return NULL; }

    pool->head = pool->tail = NULL;
    pool->stop = 0;

    /* queue sync: always kernel-backed to avoid spinlock parking complexity */
    pool->queue_mutex = wgvk_mutex_create(wgvk_locktype_kernel);
    pool->queue_cond  = wgvk_cond_create(wgvk_locktype_kernel);
    if (!pool->queue_mutex || !pool->queue_cond) {
        if (pool->queue_mutex) wgvk_mutex_destroy(pool->queue_mutex);
        if (pool->queue_cond)  wgvk_cond_destroy(pool->queue_cond);
        free(pool->workers); free(pool);
        return NULL;
    }

    for (size_t i = 0; i < num_threads; ++i) {
        wgvk_thread_create(&pool->workers[i], wgvk_thread_pool_worker, pool);
    }
    return pool;
}

void wgvk_thread_pool_destroy(wgvk_thread_pool_t* pool) {
    if (!pool) return;
    wgvk_mutex_lock(pool->queue_mutex);
    pool->stop = 1;
    wgvk_cond_broadcast(pool->queue_cond);
    wgvk_mutex_unlock(pool->queue_mutex);

    for (size_t i = 0; i < pool->num_threads; ++i) {
        wgvk_thread_join(&pool->workers[i], NULL);
    }

    while (pool->head) {
        wgvk_job_t* tmp = pool->head;
        pool->head = pool->head->next;
        wgvk_job_destroy(tmp);
    }

    wgvk_mutex_destroy(pool->queue_mutex);
    wgvk_cond_destroy(pool->queue_cond);
    free(pool->workers);
    free(pool);
}

wgvk_job_t* wgvk_job_enqueue(wgvk_thread_pool_t* pool, wgvk_thread_func_t func, void* arg) {
    if (!pool || !func) { errno = EINVAL; return NULL; }
    wgvk_job_t* job = malloc(sizeof(*job));
    if (!job) { errno = ENOMEM; return NULL; }
    job->func = func;
    job->arg = arg;
    job->result = NULL;
    job->status = WGVK_JOB_PENDING;
    job->next = NULL;
    /* per-job sync use kernel primitives */
    job->status_mutex = wgvk_mutex_create(wgvk_locktype_kernel);
    job->status_cond  = wgvk_cond_create(wgvk_locktype_kernel);
    if (!job->status_mutex || !job->status_cond) {
        if (job->status_mutex) wgvk_mutex_destroy(job->status_mutex);
        if (job->status_cond)  wgvk_cond_destroy(job->status_cond);
        free(job);
        return NULL;
    }

    wgvk_mutex_lock(pool->queue_mutex);
    if (pool->tail) pool->tail->next = job;
    else pool->head = job;
    pool->tail = job;
    wgvk_cond_signal(pool->queue_cond);
    wgvk_mutex_unlock(pool->queue_mutex);

    return job;
}

int wgvk_job_wait(wgvk_job_t* job, void** result) {
    if (!job) return EINVAL;
    wgvk_mutex_lock(job->status_mutex);
    while (job->status != WGVK_JOB_COMPLETED) {
        wgvk_cond_wait(job->status_cond, job->status_mutex);
    }
    if (result) *result = job->result;
    wgvk_mutex_unlock(job->status_mutex);
    return 0;
}

void wgvk_job_destroy(wgvk_job_t* job) {
    if (!job) return;
    wgvk_mutex_destroy(job->status_mutex);
    wgvk_cond_destroy(job->status_cond);
    free(job);
}



RGAPI void releaseAllAndClear(ResourceUsage* resourceUsage){
    BufferUsageRecordMap_for_each(&resourceUsage->referencedBuffers, bufferReleaseCallback, NULL); 
    ImageUsageRecordMap_for_each(&resourceUsage->referencedTextures, textureReleaseCallback, NULL); 
    ImageViewUsageSet_for_each(&resourceUsage->referencedTextureViews, textureViewReleaseCallback, NULL); 
    BindGroupUsageSet_for_each(&resourceUsage->referencedBindGroups, bindGroupReleaseCallback, NULL);
    BindGroupLayoutUsageSet_for_each(&resourceUsage->referencedBindGroupLayouts, bindGroupLayoutReleaseCallback, NULL);
    SamplerUsageSet_for_each(&resourceUsage->referencedSamplers, samplerReleaseCallback, NULL);
    ComputePipelineUsageSet_for_each(&resourceUsage->referencedComputePipelines, computePipelineReleaseCallback, NULL);
    RenderPipelineUsageSet_for_each(&resourceUsage->referencedRenderPipelines, renderPipelineReleaseCallback, NULL);
    RenderBundleUsageSet_for_each(&resourceUsage->referencedRenderBundles, renderBundleReleaseCallback, NULL);
    QuerySetUsageSet_for_each(&resourceUsage->referencedQuerySets, querySetReleaseCallback, NULL);

    BufferUsageRecordMap_free(&resourceUsage->referencedBuffers);
    ImageUsageRecordMap_free(&resourceUsage->referencedTextures);
    ImageViewUsageSet_free(&resourceUsage->referencedTextureViews);
    BindGroupUsageSet_free(&resourceUsage->referencedBindGroups);
    BindGroupLayoutUsageSet_free(&resourceUsage->referencedBindGroupLayouts);
    SamplerUsageSet_free(&resourceUsage->referencedSamplers);
    ComputePipelineUsageSet_free(&resourceUsage->referencedComputePipelines);
    RenderPipelineUsageSet_free(&resourceUsage->referencedRenderPipelines);
    RenderBundleUsageSet_free(&resourceUsage->referencedRenderBundles);
    QuerySetUsageSet_free(&resourceUsage->referencedQuerySets);
}







// WGPU Raytracing Begin =======================



//DEFINE_VECTOR(static inline, VkAccelerationStructureBuildGeometryInfoKHR, VkAccelerationStructureBuildGeometryInfoKHRVector)
static inline VkRayTracingShaderGroupTypeKHR toVulkanShaderGroupType(WGPURayTracingShaderBindingTableGroupType type){
    switch(type){
        case WGPURayTracingShaderBindingTableGroupType_General:{
            return VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        }break;
        case WGPURayTracingShaderBindingTableGroupType_TrianglesHitGroup:{
            return VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        }break;
        case WGPURayTracingShaderBindingTableGroupType_ProceduralHitGroup:{
            return VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
        }break;
        default: rg_unreachable();
        return (VkRayTracingShaderGroupTypeKHR)~0;
    }
}
static  WGPUShaderStageEnum wgpuShaderStageToEnum(WGPUShaderStage stage){
    ENTRY();
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
    EXIT();
}
WGPURayTracingShaderBindingTable wgpuDeviceCreateRayTracingShaderBindingTable(WGPUDevice device, const WGPURayTracingShaderBindingTableDescriptor* descriptor){
    ENTRY();
    WGPURayTracingShaderBindingTable ret = RL_CALLOC(1, sizeof(WGPURayTracingShaderBindingTableImpl));
    ret->refCount = 1;
    ret->shaderGroups = RL_CALLOC(descriptor->groupCount, sizeof(VkRayTracingShaderGroupCreateInfoKHR));
    ret->shaderGroupCount = descriptor->groupCount;
    for(uint32_t i = 0;i < descriptor->groupCount;i++){
        const WGPURayTracingShaderBindingTableGroupDescriptor* group_i = descriptor->groups + i;
        const VkRayTracingShaderGroupTypeKHR vkShaderGroupType = toVulkanShaderGroupType(group_i->type);
        
        VkRayTracingShaderGroupCreateInfoKHR insert = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = vkShaderGroupType,
            .generalShader = group_i->generalIndex,
            .closestHitShader = group_i->closestHitIndex,
            .anyHitShader = group_i->anyHitIndex,
            .intersectionShader = group_i->intersectionIndex,
        };

        //if(vkShaderGroupType == VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR){
        //    insert.intersectionShader = VK_SHADER_UNUSED_KHR;
        //    insert.anyHitShader = VK_SHADER_UNUSED_KHR;
        //}
        //else if(vkShaderGroupType == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR){
        //    insert.anyHitShader = VK_SHADER_UNUSED_KHR;
        //    insert.closestHitShader = VK_SHADER_UNUSED_KHR;
        //    insert.intersectionShader = VK_SHADER_UNUSED_KHR;
        //}

        ret->shaderGroups[i] = insert;
    }
    ret->shaderStageCount = descriptor->stageCount;
    ret->shaderStages = RL_CALLOC(descriptor->stageCount, sizeof(VkPipelineShaderStageCreateInfo));
    for(uint32_t i = 0;i < descriptor->stageCount;i++){
        ret->shaderStages[i] = CLITERAL(VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = toVulkanShaderStageBits(descriptor->stages[i].stage),
            .module = descriptor->stages[i].module->modules[wgpuShaderStageToEnum(descriptor->stages[i].stage)].module,
            //.pName = descriptor->stages[i].module->modules[wgpuShaderStageToEnum(descriptor->stages[i].stage)].epName,
            .pName = "main",
        };
    }
    return ret;
    EXIT();
}

static inline uint32_t roundup_to_multiple(uint32_t x, uint32_t multipleOf){
    if(multipleOf == 0){
        return x;
    }
    return (x + multipleOf - 1) & ~(multipleOf - 1);
}

WGPURaytracingPipeline wgpuDeviceCreateRayTracingPipeline(WGPUDevice device, const WGPURayTracingPipelineDescriptor* descriptor){
    ENTRY();
    WGPURaytracingPipeline ret = RL_CALLOC(1, sizeof(WGPURaytracingPipelineImpl));
    ret->layout = descriptor->layout;
    wgpuPipelineLayoutAddRef(ret->layout);

    const VkRayTracingPipelineCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .maxPipelineRayRecursionDepth = descriptor->rayTracingState.maxRecursionDepth,
        .layout = descriptor->layout->layout,
        .groupCount = descriptor->rayTracingState.shaderBindingTable->shaderGroupCount,
        .pGroups    = descriptor->rayTracingState.shaderBindingTable->shaderGroups,
        .stageCount = descriptor->rayTracingState.shaderBindingTable->shaderStageCount,
        .pStages    = descriptor->rayTracingState.shaderBindingTable->shaderStages,
    };
    
    VkResult plCreateResult = device->functions.vkCreateRayTracingPipelinesKHR(device->device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, NULL, &ret->raytracingPipeline);
    
    if(plCreateResult != VK_SUCCESS){
        RL_FREE(ret);
        return NULL;
    }

    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR* rtProperties = &device->adapter->rayTracingPipelineProperties;
    const uint32_t handleSize = rtProperties->shaderGroupHandleSize;
    const uint32_t baseAlignment = rtProperties->shaderGroupBaseAlignment;
    const uint32_t groupCount = descriptor->rayTracingState.shaderBindingTable->shaderGroupCount;
    const uint32_t sbtStride = roundup_to_multiple(handleSize, baseAlignment); 

    uint32_t packedSize = groupCount * handleSize;
    uint8_t* tempPackedHandles = (uint8_t*)RL_CALLOC(1, packedSize);
    
    VkResult result = device->functions.vkGetRayTracingShaderGroupHandlesKHR(
        device->device,
        ret->raytracingPipeline,
        0,
        groupCount,
        packedSize,
        tempPackedHandles
    );

    if(result != VK_SUCCESS){
        RL_FREE(tempPackedHandles);
        RL_FREE(ret);
        return NULL;
    }

    VkDeviceSize sbtTotalSize = (VkDeviceSize)groupCount * sbtStride;
    ret->totalSbtSize = sbtTotalSize;

    WGPUBufferDescriptor sbtBufferDesc = {
        .size = sbtTotalSize,
        .usage = WGPUBufferUsage_Raytracing | WGPUBufferUsage_ShaderDeviceAddress | WGPUBufferUsage_CopyDst,
        .mappedAtCreation = true 
    };
    
    ret->sbtBuffer = wgpuDeviceCreateBuffer(device, &sbtBufferDesc);
    
    uint8_t* mappedSbtData = (uint8_t*)wgpuBufferGetMappedRange(ret->sbtBuffer, 0, sbtTotalSize);
    
    for(size_t i = 0; i < groupCount; i++) {
        uint8_t* src = tempPackedHandles + (i * (size_t)handleSize);
        uint8_t* dst = mappedSbtData + (i * (size_t)sbtStride);
        memcpy(dst, src, handleSize);
    }

    wgpuBufferUnmap(ret->sbtBuffer);
    
    RL_FREE(tempPackedHandles);
    EXIT();
    return ret;
}

WGPURayTracingAccelerationContainer wgpuDeviceCreateRayTracingAccelerationContainer(WGPUDevice device, const WGPURayTracingAccelerationContainerDescriptor* descriptor){
    ENTRY();
    WGPURayTracingAccelerationContainer ret = RL_CALLOC(1, sizeof(WGPURayTracingAccelerationContainerImpl));
    ret->level = descriptor->level;
    ret->device = device;
    
    // For BLAS, geometryCount is the number of meshes.
    // For TLAS, geometryCount is 1 (A single geometry of type INSTANCES containing N primitives).
    uint32_t geometryCount = (descriptor->level == WGPURayTracingAccelerationContainerLevel_Bottom) ? descriptor->geometryCount : 1;
    
    ret->geometryCount = geometryCount;
    ret->primitiveCounts = RL_CALLOC(geometryCount, sizeof(uint32_t));
    uint32_t* maxPrimitiveCounts = ret->primitiveCounts;
    
    // Allocate geometries and range infos based on the corrected geometryCount
    VkAccelerationStructureGeometryKHR* geometries = RL_CALLOC(geometryCount, sizeof(VkAccelerationStructureGeometryKHR));
    ret->buildRangeInfos = (VkAccelerationStructureBuildRangeInfoKHR*)RL_CALLOC(geometryCount, sizeof(VkAccelerationStructureBuildRangeInfoKHR));
    
    if (descriptor->level == WGPURayTracingAccelerationContainerLevel_Bottom) {
        ret->inputGeometryBuffers = (WGPUBuffer*)RL_CALLOC(geometryCount, sizeof(WGPUBuffer));

        for (uint32_t i = 0; i < geometryCount; i++) {
            geometries[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometries[i].flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            
            switch (descriptor->geometries[i].type) {
                case WGPURayTracingAccelerationGeometryType_Triangles: {
                    geometries[i].geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                    geometries[i].geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                    geometries[i].geometry.triangles.vertexFormat = toVulkanVertexFormat(descriptor->geometries[i].vertex.format);
                    uint64_t address = descriptor->geometries[i].vertex.buffer->address;
                    geometries[i].geometry.triangles.vertexData.deviceAddress = address;
                    geometries[i].geometry.triangles.vertexStride = descriptor->geometries[i].vertex.stride;
                    geometries[i].geometry.triangles.maxVertex = descriptor->geometries[i].vertex.count;
                    
                    if (descriptor->geometries[i].index.buffer) {
                        geometries[i].geometry.triangles.indexType = toVulkanIndexFormat(descriptor->geometries[i].index.format);
                        ret->inputGeometryBuffers[i] = descriptor->geometries[i].index.buffer;
                        geometries[i].geometry.triangles.indexData.deviceAddress = descriptor->geometries[i].index.buffer->address;
                        maxPrimitiveCounts[i] = descriptor->geometries[i].index.count / 3;
                        ret->buildRangeInfos[i].primitiveOffset = descriptor->geometries[i].index.offset;
                        ret->buildRangeInfos[i].firstVertex = 0;
                    } else {
                        geometries[i].geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
                        ret->inputGeometryBuffers[i] = descriptor->geometries[i].vertex.buffer;
                        ret->buildRangeInfos[i].primitiveOffset = descriptor->geometries[i].vertex.offset;
                        maxPrimitiveCounts[i] = descriptor->geometries[i].vertex.count / 3;
                        ret->buildRangeInfos[i].firstVertex = 0;
                    }
                    wgpuBufferAddRef(ret->inputGeometryBuffers[i]);

                    ret->buildRangeInfos[i].primitiveCount = maxPrimitiveCounts[i];
                    ret->buildRangeInfos[i].transformOffset = 0;
                    break;
                }
                case WGPURayTracingAccelerationGeometryType_AABBs: {
                    geometries[i].geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
                    geometries[i].geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
                    geometries[i].geometry.aabbs.data.deviceAddress = descriptor->geometries[i].aabb.buffer->address;
                    geometries[i].geometry.aabbs.stride = descriptor->geometries[i].aabb.stride;
                    ret->inputGeometryBuffers[i] = descriptor->geometries[i].aabb.buffer;
                    wgpuBufferAddRef(ret->inputGeometryBuffers[i]);
                    maxPrimitiveCounts[i] = descriptor->geometries[i].aabb.count;
                    ret->buildRangeInfos[i].primitiveCount = maxPrimitiveCounts[i];
                    ret->buildRangeInfos[i].primitiveOffset = descriptor->geometries[i].aabb.offset;
                    ret->buildRangeInfos[i].firstVertex = 0;
                    ret->buildRangeInfos[i].transformOffset = 0;
                    break;
                }
                default:{
                    //rg_unreachable();
                    (void)0;
                }
            }
        }
    }
    else if(descriptor->level == WGPURayTracingAccelerationContainerLevel_Top){
        // Allocate host-side instance data to populate buffer
        VkAccelerationStructureInstanceKHR* vulkanInstances = RL_CALLOC(descriptor->instanceCount, sizeof(VkAccelerationStructureInstanceKHR));

        WGPUBufferDescriptor vfbDesc = {
            .size = descriptor->instanceCount * sizeof(VkAccelerationStructureInstanceKHR),
            .usage = WGPUBufferUsage_Raytracing | WGPUBufferUsage_ShaderDeviceAddress,
        };

        ret->instanceBuffer = wgpuDeviceCreateBuffer(device, &vfbDesc);
        
        // 1. Loop through instances to fill the buffer data
        for(uint32_t i = 0; i < descriptor->instanceCount; i++){
            const WGPURayTracingAccelerationInstanceDescriptor* wgpuInstance = descriptor->instances + i;
            VkAccelerationStructureInstanceKHR* vulkanInstance = vulkanInstances + i;
            
            memcpy(&vulkanInstance->transform, &wgpuInstance->transformMatrix, sizeof(VkTransformMatrixKHR));
            
            vulkanInstance->instanceCustomIndex = wgpuInstance->instanceId;
            vulkanInstance->mask = wgpuInstance->mask;
            vulkanInstance->instanceShaderBindingTableRecordOffset = wgpuInstance->instanceOffset;
            vulkanInstance->flags = 0;
            
            if (wgpuInstance->usage & WGPURayTracingAccelerationInstanceUsage_TriangleCullDisable) {
                vulkanInstance->flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            }

            VkAccelerationStructureDeviceAddressInfoKHR getASDeviceAddressInfo = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = wgpuInstance->geometryContainer->accelerationStructure,
            };
            vulkanInstance->accelerationStructureReference = device->functions.vkGetAccelerationStructureDeviceAddressKHR(device->device, &getASDeviceAddressInfo);
        }
        
        // Write the instance data to GPU memory
        wgpuQueueWriteBuffer(device->queue, ret->instanceBuffer, 0, vulkanInstances, vfbDesc.size);
        RL_FREE(vulkanInstances); // Clean up temp host memory

        // 2. Configure the SINGLE geometry entry for the TLAS
        // The instance count is handled by primitiveCount, not geometryCount.
        maxPrimitiveCounts[0] = descriptor->instanceCount;

        geometries[0].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometries[0].geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometries[0].geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometries[0].geometry.instances.data.deviceAddress = ret->instanceBuffer->address;
        geometries[0].geometry.instances.arrayOfPointers = VK_FALSE;

        ret->buildRangeInfos[0].primitiveCount = descriptor->instanceCount; 
        ret->buildRangeInfos[0].primitiveOffset = 0;
        ret->buildRangeInfos[0].firstVertex = 0;
        ret->buildRangeInfos[0].transformOffset = 0;
    }
    
    ret->geometries = geometries;

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfoVulkan = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pGeometries = geometries,
        .geometryCount = geometryCount, // This is 1 for TLAS
        .type = descriptor->level == WGPURayTracingAccelerationContainerLevel_Bottom ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .dstAccelerationStructure = VK_NULL_HANDLE
    };

    VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    
    device->functions.vkGetAccelerationStructureBuildSizesKHR(
        device->device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_OR_DEVICE_KHR,
        &geometryInfoVulkan,
        maxPrimitiveCounts, // For TLAS, this array has length 1, and the value is instanceCount
        &buildSizesInfo
    );
    
    ret->accelerationStructureBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = buildSizesInfo.accelerationStructureSize,
        .usage = WGPUBufferUsage_Raytracing
    });

    if(buildSizesInfo.updateScratchSize){
        ret->updateScratchBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
            .size = buildSizesInfo.updateScratchSize,
            .usage = WGPUBufferUsage_Raytracing
        });
    }

    ret->buildScratchBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = buildSizesInfo.buildScratchSize,
        .usage = WGPUBufferUsage_Raytracing
    });


    VkAccelerationStructureCreateInfoKHR accelStructureCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .type = toVulkanAccelerationStructureLevel(descriptor->level),
        .size = buildSizesInfo.accelerationStructureSize,
        .buffer = ret->accelerationStructureBuffer->buffer,
        .createFlags = 0,
    };

    VkResult casResult = device->functions.vkCreateAccelerationStructureKHR(
        device->device,
        &accelStructureCreateInfo,
        NULL,
        &ret->accelerationStructure
    );

    if(casResult != VK_SUCCESS){
        return NULL;
    }
    
    // Update the geometry info with the created AS handle for future use (e.g. building)
    geometryInfoVulkan.dstAccelerationStructure = ret->accelerationStructure;
    // NOTE: Do not overwrite geometryCount with descriptor->geometryCount here, 
    // strictly keep the calculated variable which is 1 for TLAS.
    // geometryInfoVulkan.geometryCount = geometryCount; 
    
    wgpuDeviceAddRef(device);
    return ret;
    EXIT();
}

void wgpuCommandEncoderBuildRayTracingAccelerationContainer(WGPUCommandEncoder encoder, WGPURayTracingAccelerationContainer container){
    ENTRY();
    
    WGPUDevice device = encoder->device;
    
    if(container->level == WGPURayTracingAccelerationContainerLevel_Top){
        if (container->instanceBuffer) {
            BufferUsageSnap instanceSnap = {
                .access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
                .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
            };
            ce_trackBuffer(encoder, container->instanceBuffer, instanceSnap);
        }
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .dstAccelerationStructure = container->accelerationStructure,
            .type = toVulkanAccelerationStructureLevel(container->level),
            .geometryCount = container->geometryCount,
            .pGeometries = container->geometries,
            .scratchData = {
                .deviceAddress = container->buildScratchBuffer->address
            }
        };

        device->functions.vkCmdBuildAccelerationStructuresKHR(
            encoder->buffer,
            1, &buildInfo,
            (const VkAccelerationStructureBuildRangeInfoKHR**)&container->buildRangeInfos
        );
    }

    else if(container->level == WGPURayTracingAccelerationContainerLevel_Bottom){
        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .dstAccelerationStructure = container->accelerationStructure,
            .type = toVulkanAccelerationStructureLevel(container->level),
            .pGeometries = container->geometries,
            .geometryCount = container->geometryCount,
            .scratchData = {
                .deviceAddress = container->buildScratchBuffer->address
            }
        };

        BufferUsageSnap inBufferSnap = {
            .access = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
            .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
        };

        BufferUsageSnap asBufferSnap = {
            .access = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
            .stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
        };

        for(uint32_t i = 0;i < container->geometryCount;i++){
            ce_trackBuffer(encoder, container->inputGeometryBuffers[i], inBufferSnap);
        }
        ce_trackBuffer(encoder, container->accelerationStructureBuffer, asBufferSnap);

        device->functions.vkCmdBuildAccelerationStructuresKHR(
            encoder->buffer,
            1, &buildInfo,
            (const VkAccelerationStructureBuildRangeInfoKHR**)&container->buildRangeInfos
        );
    }
    EXIT();
}


void wgpuCommandEncoderCopyRayTracingAccelerationContainer(WGPUCommandEncoder encoder, WGPURayTracingAccelerationContainer source, WGPURayTracingAccelerationContainer dest){
    ENTRY();

    EXIT();
}
void wgpuCommandEncoderUpdateRayTracingAccelerationContainer(WGPUCommandEncoder encoder, WGPURayTracingAccelerationContainer container){
    ENTRY();

    EXIT();
}

void wgpuRaytracingPassEncoderSetPipeline     (WGPURaytracingPassEncoder rte, WGPURaytracingPipeline raytracingPipeline){
    ENTRY();

    ru_trackRaytracingPipeline(&rte->resourceUsage, raytracingPipeline);
    RenderPassCommandGeneric setPipeline = {
        .type = rp_command_type_set_raytracing_pipeline,
        .setRaytracingPipeline = {
            .pipeline = raytracingPipeline,
        }
    };
    RaytracingPassEncoder_PushCommand(rte, &setPipeline);
    EXIT();
}
void wgpuRaytracingPassEncoderSetBindGroup    (WGPURaytracingPassEncoder cpe, uint32_t groupIndex, WGPUBindGroup bindGroup, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets){
    ENTRY();
    RenderPassCommandSetBindGroup setBindGroup = {
        .groupIndex = groupIndex,
        .group = bindGroup,
        .bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        .dynamicOffsetCount = dynamicOffsetCount,
        .dynamicOffsets = dynamicOffsets
    };

    RenderPassCommandGeneric cmd = {
        .type = rp_command_type_set_bind_group,
        .setBindGroup = {
            .groupIndex = groupIndex,
            .group = bindGroup,
            .bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            .dynamicOffsetCount = dynamicOffsetCount,
            .dynamicOffsets = dynamicOffsets
        }
    };
    RaytracingPassEncoder_PushCommand(cpe, &cmd);
    cpe->bindGroups[groupIndex] = bindGroup;
    EXIT();
}

void wgpuRaytracingPassEncoderTraceRays       (WGPURaytracingPassEncoder cpe, uint32_t rayGenerationOffset, uint32_t rayHitOffset, uint32_t rayMissOffset, uint32_t width, uint32_t height, uint32_t depth){
    ENTRY();
    RenderPassCommandGeneric command = {
        .type = rt_command_type_trace_rays,
        .traceRays = {
            .rayGenerationOffset = rayGenerationOffset,
            .rayHitOffset = rayHitOffset,
            .rayMissOffset = rayMissOffset,
            .width = width,
            .height = height,
            .depth = depth,
        }
    };
    RaytracingPassEncoder_PushCommand(cpe, &command);
    EXIT();
}



// WGPU Raytracing End =======================

size_t vkFormatSize(VkFormat format){
    switch(format){
        default: return UINT32_MAX;

        // 4/5/6-bit packed (uncompressed)
        case VK_FORMAT_R4G4_UNORM_PACK8:               return 1;  // 8 bits
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:          return 2;  // 16 bits
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16:          return 2;  // 16 bits
        case VK_FORMAT_R5G6B5_UNORM_PACK16:            return 2;  // 16 bits
        case VK_FORMAT_B5G6R5_UNORM_PACK16:            return 2;  // 16 bits
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16:          return 2;  // 16 bits
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16:          return 2;  // 16 bits
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16:          return 2;  // 16 bits

        // 8-bit channels (uncompressed)
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_USCALED:
        case VK_FORMAT_R8_SSCALED:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:                        return 1;
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_USCALED:
        case VK_FORMAT_R8G8_SSCALED:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R8G8_SRGB:                      return 2;
        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SNORM:
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
        case VK_FORMAT_R8G8B8_SRGB:                    return 3;
        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SNORM:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
        case VK_FORMAT_B8G8R8_SRGB:                    return 3;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:                  return 4;
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SNORM:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
        case VK_FORMAT_B8G8R8A8_SRGB:                  return 4;
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:           return 4;

        // 10/11-bit packed (uncompressed)
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_UINT_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:        return 4;  // 32 bits
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_UINT_PACK32:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:        return 4;  // 32 bits

        // 16-bit channels (uncompressed)
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_USCALED:
        case VK_FORMAT_R16_SSCALED:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT:                     return 2;
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_USCALED:
        case VK_FORMAT_R16G16_SSCALED:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16_SFLOAT:                  return 4;
        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SNORM:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
        case VK_FORMAT_R16G16B16_SFLOAT:               return 6;
        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_USCALED:
        case VK_FORMAT_R16G16B16A16_SSCALED:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:            return 8;

        // 32-bit channels (uncompressed)
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT:                     return 4;
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT:                  return 8;
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT:               return 12;
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT:            return 16;

        // 64-bit channels (uncompressed)
        case VK_FORMAT_R64_UINT:
        case VK_FORMAT_R64_SINT:
        case VK_FORMAT_R64_SFLOAT:                     return 8;
        case VK_FORMAT_R64G64_UINT:
        case VK_FORMAT_R64G64_SINT:
        case VK_FORMAT_R64G64_SFLOAT:                  return 16;
        case VK_FORMAT_R64G64B64_UINT:
        case VK_FORMAT_R64G64B64_SINT:
        case VK_FORMAT_R64G64B64_SFLOAT:               return 24;
        case VK_FORMAT_R64G64B64A64_UINT:
        case VK_FORMAT_R64G64B64A64_SINT:
        case VK_FORMAT_R64G64B64A64_SFLOAT:            return 32;

        // Special packed float
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:        return 4;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:         return 4;

        // Depth/Stencil (uncompressed; sizes include padding where applicable)
        case VK_FORMAT_D16_UNORM:                      return 2;
        case VK_FORMAT_X8_D24_UNORM_PACK32:            return 4;
        case VK_FORMAT_D32_SFLOAT:                     return 4;
        case VK_FORMAT_S8_UINT:                        return 1;
        case VK_FORMAT_D16_UNORM_S8_UINT:              return 4;  // 24 used + 8 stencil
        case VK_FORMAT_D24_UNORM_S8_UINT:              return 4;
        case VK_FORMAT_D32_SFLOAT_S8_UINT:             return 8;  // 32 depth + 8 stencil (24 unused)

        // BC block-compressed: return bytes per 4x4 block
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:            return 8;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:                 return 16;
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK:                return 8;
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:                return 16;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK:                 return 16;

        // ETC2/EAC block-compressed: bytes per 4x4 block
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:       return 8;
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:       return 16;
        case VK_FORMAT_EAC_R11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11_SNORM_BLOCK:            return 8;
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:         return 16;

        // ASTC LDR/HDR: bytes per block (various footprints)
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
        case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
        case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:        return 16;

        // 4:2:2 packed Y′CbCr (bytes per 2×1 block = 4; => 2 bytes/texel equivalently)
        case VK_FORMAT_G8B8G8R8_422_UNORM:              return 4;  // per 2×1 block
        case VK_FORMAT_B8G8R8G8_422_UNORM:              return 4;  // per 2×1 block

        // Y′CbCr multi-plane 8-bit (return bytes per format block)
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:       return 6;  // per 2×2 block
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:        return 6;  // per 2×2 block
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:       return 4;  // per 2×1 block
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:        return 4;  // per 2×1 block
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:       return 3;  // per 1×1 block

        // 10-bit “X6” packed/YCbCr (sizes follow Vulkan block definitions)
        case VK_FORMAT_R10X6_UNORM_PACK16:              return 2;
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:        return 4;
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: return 8;
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: return 4; // per 2×1 block
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: return 4; // per 2×1 block
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: return 12; // per 2×2 block
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:  return 12; // per 2×2 block
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: return 8;  // per 2×1 block
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:  return 8;  // per 2×1 block
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: return 6;  // per 1×1 block

        // 12-bit “X4” packed/YCbCr (stored in 16 bits per sample)
        case VK_FORMAT_R12X4_UNORM_PACK16:              return 2;
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:        return 4;
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: return 8;
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: return 4; // per 2×1 block
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: return 4; // per 2×1 block
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: return 12; // per 2×2 block
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:  return 12; // per 2×2 block
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: return 8;  // per 2×1 block
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:  return 8;  // per 2×1 block
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: return 6;  // per 1×1 block

        // 16-bit Y′CbCr variants
        case VK_FORMAT_G16B16G16R16_422_UNORM:          return 8;  // per 2×1 block
        case VK_FORMAT_B16G16R16G16_422_UNORM:          return 8;  // per 2×1 block
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:    return 24; // per 2×2 block (4*2 + 2 + 2)*2 bytes
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:     return 24; // per 2×2 block
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:    return 16; // per 2×1 block
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:     return 16; // per 2×1 block
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:    return 12; // per 1×1 block

        // Additional 2-plane 444 variants
        case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM:        return 3;  // per 1×1 block (1Y + 2 chroma)
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16: return 6; // per 1×1 block
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16: return 6; // per 1×1 block
        case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM:     return 6; // per 1×1 block

        // Misc packed
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16:           return 2;
        case VK_FORMAT_A4B4G4R4_UNORM_PACK16:           return 2;
        case VK_FORMAT_A1B5G5R5_UNORM_PACK16:           return 2;
        case VK_FORMAT_A8_UNORM:                        return 1;

        // PVRTC (bytes per block)
        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:     return 8;  // 8×4 block
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:     return 8;  // 4×4 block
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:     return 8;  // 8×4 block
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:     return 8;  // 4×4 block
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:      return 8;
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:      return 8;
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:      return 8;
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:      return 8;

        // NV extension (assume 16-bit per component fixed => 4 bytes total)
        case VK_FORMAT_R16G16_SFIXED5_NV:               return 4;
    }
}

const char* vkErrorString(int code){
    
    switch(code){
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_EVENT_SET: return "VK_EVENT_SET";
        case VK_EVENT_RESET: return "VK_EVENT_RESET";
        case VK_INCOMPLETE: return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
        case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
        case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
        case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
        case VK_ERROR_NOT_PERMITTED: return "VK_ERROR_NOT_PERMITTED";
        case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
        case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
        case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR: return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR: return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
        case VK_THREAD_IDLE_KHR: return "VK_THREAD_IDLE_KHR";
        case VK_THREAD_DONE_KHR: return "VK_THREAD_DONE_KHR";
        case VK_OPERATION_DEFERRED_KHR: return "VK_OPERATION_DEFERRED_KHR";
        case VK_OPERATION_NOT_DEFERRED_KHR: return "VK_OPERATION_NOT_DEFERRED_KHR";
        case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR: return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT: return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
        case VK_INCOMPATIBLE_SHADER_BINARY_EXT: return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
        case VK_PIPELINE_BINARY_MISSING_KHR: return "VK_PIPELINE_BINARY_MISSING_KHR";
        case VK_ERROR_NOT_ENOUGH_SPACE_KHR: return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
        default: return "<Unknown VkResult enum>";
    }
}
