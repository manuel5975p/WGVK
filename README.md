# WGVK

[![CMake on multiple platforms](https://github.com/manuel5975p/WGVK/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/manuel5975p/WGVK/actions/workflows/cmake-multi-platform.yml)
[![Unittests](https://github.com/manuel5975p/WGVK/actions/workflows/unittests.yml/badge.svg)](https://github.com/manuel5975p/WGVK/actions/workflows/unittests.yml)

<a href="https://opensource.org/licenses/MIT">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT" height="25">
</a>
<a href="https://en.wikipedia.org/wiki/C11">
  <img src="https://img.shields.io/badge/Language-C11-blue.svg" alt="Language" height="25">
</a>
<a href="https://cmake.org/">
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20Android-lightgreen.svg" alt="Platform" height="25">
</a>


A standalone, single-file, fully feature-conformant implementation WebGPU API in C11, built on top of Vulkan 1.1. WGVK is designed for easy integration into C/C++ projects, providing a modern graphics and compute API without the steep learning curve of raw Vulkan.
## Implementation progress

- ▓▓▓▓▓▓▓▓▓▓ 100% WebGPU Conformity
- ▓▓▓▓▓▓▓▓░░ 80% WGSL Reflection
- ▓▓▓▓▓░░░░░ 50% HW Raytracing according to [Dawn-RT](https://github.com/maierfelix/dawn-ray-tracing/blob/master/RT_SPEC.md#GPURayTracingAccelerationInstanceDescriptor)

For more detailed insights, join us on [Discord](https://discord.gg/dK7FqBx7q8)

## Features

*   **Full** WebGPU API: A modern interface for graphics and compute pipelines.
*   **Single Implementation File**: Simply drop `wgvk.c` and the `include` directory into your project.
*   **Cross-Platform Window System Integration**:
    *   Windows (Win32)
    *   macOS (Metal)
    *   Linux (X11 & Wayland)
*   **Optional Memory Allocator**: Integrates with the excellent [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) for optimized memory management (requires C++).
*   **CMake Build System**: Easy to configure and build the library and examples.
*   **Dependency-Free Core**: The core library has no external dependencies for basic operation. GLFW is fetched by CMake for the examples.

## Building
[wgvk.c](https://github.com/manuel5975p/WGVK/blob/master/src/wgvk.c) contains all implementations. Adding it to your compilation command line is already enough. 
```bash
gcc src/wgvk.c your_file.c -I include
```

However, the recommended way to build is with CMake, which will also build the examples and configures platform-specific windowing options.

### Prerequisites

*   A C99 compatible compiler (GCC, Clang, MSVC)
*   A Vulkan 1.1+ Driver (the SDK is **not** required) 
*   CMake (version 3.19 or newer)
*   **Linux**: `pkg-config` and development libraries for X11 (`libx11-dev`) and/or Wayland (`libwayland-dev`).
*   **MacOS**: MoltenVK capable of Vulkan 1.1.

### CMake Build (Recommended)

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/manuel5975p/WGVK
    cd wgvk
    ```

2.  **Configure and build with CMake:**
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build .
    ```
    The example binaries will be located in the `build` directory.

3.  **CMake Options:**
    *   Enable VMA for better memory management: `cmake .. -DWGVK_USE_VMA=ON`
    *   Disable building examples: `cmake .. -DWGVK_BUILD_EXAMPLES=OFF`

### Manual Build (Simple Example)

For a quick test without dependencies, you can compile a basic example directly with GCC/Clang:

```bash
# Note: This simple command does not link against platform-specific libraries
# for windowing and is best for compute-only examples.
gcc examples/basic_compute.c src/wgvk.c -O3 -I include -o basic_compute
```

Or, if you are building with Visual Studio:

```bash
cl examples\basic_compute.c src\wgvk.c -O2 -DNDEBUG -I include /std:c11 /experimental:c11atomics
cl examples\rgfw_surface.c src\wgvk.c -O2 -DNDEBUG -I include /std:c11 /experimental:c11atomics
```


## Examples

The `examples/` directory contains sample code to get you started:

*   `basic_compute.c`: Demonstrates a simple compute shader workflow.
*   `glfw_surface.c`: Shows how to create a window with GLFW and render a triangle.
*   `rgfw_surface.c`: Shows how to create a window with [RGFW](https://github.com/ColleagueRiley/RGFW) and render a triangle,

Run them from the `build` directory after compiling:
```bash
./basic_compute
./glfw_surface
```

## Integrating into Your Project

You can use WGVK in two main ways:

1.  **As a Library**: Use CMake to build `wgvk` as a static library and link it into your project.
2.  **Source Inclusion**: Copy `src/wgvk.c` and the `include/` directory into your project's source tree and compile them alongside your own code.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
