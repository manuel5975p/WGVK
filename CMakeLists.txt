cmake_minimum_required(VERSION 3.19)
project(wgvk)
include(FetchContent)

option(WGVK_BUILD_EXAMPLES "Build some WGVK examples" OFF)
option(WGVK_BUILD_WGSL_SUPPORT "Build the WGSL->SPIRV compiler" OFF)
option(WGVK_USE_VMA "Use GPUOpen's VMA allocator (Requires C++)" OFF)


function(wgvk_bundle_libraries output_target)
  function(get_dependencies input_target)
    get_target_property(alias ${input_target} ALIASED_TARGET)
    if(TARGET ${alias})
      set(input_target ${alias})
    endif()
    if(${input_target} IN_LIST all_dependencies)
      return()
    endif()
    list(APPEND all_dependencies ${input_target})
    get_target_property(link_libraries ${input_target} LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()
    get_target_property(link_libraries ${input_target} INTERFACE_LINK_LIBRARIES)
    foreach(dependency IN LISTS link_libraries)
      if(TARGET ${dependency})
        get_dependencies(${dependency})
      endif()
    endforeach()
    set(all_dependencies ${all_dependencies} PARENT_SCOPE)
  endfunction()
  foreach(input_target IN LISTS ARGN)
    get_dependencies(${input_target})
  endforeach()
  foreach(dependency IN LISTS all_dependencies)
    get_target_property(type ${dependency} TYPE)
    if(${type} STREQUAL "STATIC_LIBRARY")
      list(APPEND all_objects $<TARGET_OBJECTS:${dependency}>)
    elseif(${type} STREQUAL "OBJECT_LIBRARY")
      list(APPEND all_objects $<TARGET_OBJECTS:${dependency}>)
    endif()
  endforeach()

  add_library(${output_target} STATIC ${all_objects})
  add_dependencies(${output_target} ${ARGN})
endfunction()

# Lock in static for now
set(WGVK_CORE_SRC_LIST "src/wgvk.c")
if(SANITIZE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
endif()
if(WGVK_USE_VMA)
    list(APPEND WGVK_CORE_SRC_LIST "src/vma_impl.cpp")
endif()


# We define a variable to hold the name of the main library target.
# This avoids repeating `if/else` logic for setting properties.
set(WGVK_PRIMARY_TARGET_NAME "")

if(WGVK_BUILD_WGSL_SUPPORT)
    set(WGVK_PRIMARY_TARGET_NAME "wgvk_core")

    add_library(${WGVK_PRIMARY_TARGET_NAME} STATIC ${WGVK_CORE_SRC_LIST})
    # Add all properties as PUBLIC. This means they are used to build wgvk_core itself,
    # and they are part of its public interface to be inherited by consumers.
    target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC RG_STATIC=1 WGVK_DISABLE_ASSERT=1)
    target_include_directories(${WGVK_PRIMARY_TARGET_NAME} PUBLIC include)

    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(DAWN_ENABLE_VULKAN OFF)
    set(DAWN_ENABLE_SPIRV_VALIDATION OFF)
    set(DAWN_BUILD_SAMPLES OFF)
    set(DAWN_USE_GLFW OFF)
    set(DAWN_USE_X11 OFF)
    set(DAWN_USE_WAYLAND OFF)
    set(TINT_BUILD_GLSL_WRITER OFF)
    set(TINT_BUILD_CMD_TOOLS OFF)
    set(TINT_BUILD_SPV_READER OFF)
    set(TINT_BUILD_SPV_WRITER ON)
    set(TINT_BUILD_WGSL_WRITER OFF)
    set(TINT_BUILD_TESTS OFF)
    set(TINT_BUILD_GLSL_VALIDATOR OFF)
    FetchContent_Declare(
        dawn
        URL https://github.com/manuel5975p/dawn_monorepo/archive/refs/tags/v7252.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(dawn)
    add_library(tint_c_api STATIC "src/tint_c_api.cpp")
    target_include_directories(tint_c_api PRIVATE "include")
    target_include_directories(tint_c_api PRIVATE "${dawn_SOURCE_DIR}")
    target_compile_features(tint_c_api PRIVATE cxx_std_20)
    
    target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_WGSL=1)
    
    # Bundle wgvk_core and its dependencies into the final `wgvk` library.
    wgvk_bundle_libraries(wgvk ${WGVK_PRIMARY_TARGET_NAME} tint_lang_wgsl_reader tint_lang_spirv_writer tint_lang_wgsl_inspector tint_c_api)

    # ---- FIX: PROPAGATE USAGE REQUIREMENTS ----
    # The `wgvk` library is made of object files, so it doesn't inherit CMake
    # properties from `wgvk_core` automatically.
    # We use `target_link_libraries` with `INTERFACE` to manually propagate all
    # PUBLIC/INTERFACE properties (includes, definitions, etc.) from `wgvk_core`
    # to `wgvk`. This makes them available to anything that links against `wgvk`.
    target_link_libraries(wgvk INTERFACE ${WGVK_PRIMARY_TARGET_NAME})

else()
    set(WGVK_PRIMARY_TARGET_NAME "wgvk")

    add_library(${WGVK_PRIMARY_TARGET_NAME} STATIC ${WGVK_CORE_SRC_LIST})
    # When not bundling, `wgvk` is the primary target. We use PUBLIC so that
    # any consumers (like the examples) inherit these properties correctly.
    target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC RG_STATIC=1 WGVK_DISABLE_ASSERT=1)
    target_include_directories(${WGVK_PRIMARY_TARGET_NAME} PUBLIC include)
endif()

# Now, we apply all remaining properties to the primary target using the variable.
# Because we used PUBLIC on the primary target and then propagated its interface
# to the final `wgvk` target, these properties will be correctly inherited.
if(WGVK_USE_VMA)
    target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC USE_VMA_ALLOCATOR=1)
endif()

# Platform-specific surface support
if(WIN32)
    target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_WIN32_SURFACE=1)
elseif(APPLE)
    target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_METAL_SURFACE=1)
    target_link_libraries(${WGVK_PRIMARY_TARGET_NAME} PUBLIC "-framework Cocoa" "-framework Metal" "-framework QuartzCore")
elseif(UNIX)
    find_package(PkgConfig QUIET)
        
    pkg_check_modules(WAYLAND_CLIENT QUIET wayland-client)
    if(WAYLAND_CLIENT_FOUND)
        message(STATUS "Wayland found")
        set(GLFW_BUILD_WAYLAND ON)
        target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_WAYLAND_SURFACE=1)
        target_link_libraries(${WGVK_PRIMARY_TARGET_NAME} PUBLIC wayland-client)
    endif()

    find_package(X11 QUIET)
    if(X11_FOUND)
        set(GLFW_BUILD_X11 ON)
        target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_XLIB_SURFACE=1)
        target_link_libraries(${WGVK_PRIMARY_TARGET_NAME} PUBLIC X11::X11)
    endif()
endif()


if(WGVK_BUILD_EXAMPLES)
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(glfw)

    add_executable(glfw_surface "examples/glfw_surface.c")
    add_executable(basic_compute "examples/basic_compute.c")
    add_executable(rgfw_surface "examples/rgfw_surface.c")
    add_executable(cpp_api "examples/cpp_api.cpp")
    
    # Examples always link against the final `wgvk` target. Thanks to the fix above,
    # they will now correctly inherit all necessary compile definitions.
    target_link_libraries(glfw_surface PUBLIC wgvk glfw)
    target_link_libraries(basic_compute PUBLIC wgvk glfw)
    target_link_libraries(rgfw_surface PUBLIC wgvk)
    target_link_libraries(cpp_api PUBLIC wgvk)
endif()