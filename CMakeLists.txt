cmake_minimum_required(VERSION 3.19)
project(wgvk)
include(FetchContent)

option(WGVK_BUILD_EXAMPLES "Build some WGVK examples" OFF)
option(WGVK_BUILD_GLSL_SUPPORT "Build the GLSL->SPIRV compiler (glslang)" OFF)
option(WGVK_BUILD_WGSL_SUPPORT "Build the WGSL->SPIRV compiler (tint)" OFF)
option(WGVK_USE_VMA "Use GPUOpen's VMA allocator (Requires C++)" OFF)
if(EMSCRIPTEN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --use-port=emdawnwebgpu")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --use-port=emdawnwebgpu")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sASYNCIFY=2 -sUSE_GLFW=3")
endif()

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

  add_library(${output_target} ${all_objects})
  add_dependencies(${output_target} ${ARGN})
endfunction()

# Lock in static for now
if(EMSCRIPTEN)
  set(WGVK_CORE_SRC_LIST "src/dummy.c")
else()
  set(WGVK_CORE_SRC_LIST "src/wgvk.c")
endif()
if(SANITIZE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${SANITIZE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=${SANITIZE}")
endif()
if(WGVK_USE_VMA)
  list(APPEND WGVK_CORE_SRC_LIST "src/vma_impl.cpp")
endif()
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-DNDEBUG -g3 -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-DNDEBUG -g3 -O3")

# We define a variable to hold the name of the main library target.
# This avoids repeating `if/else` logic for setting properties.
if(WGVK_BUILD_GLSL_SUPPORT OR WGVK_BUILD_WGSL_SUPPORT)
  set(WGVK_PRIMARY_TARGET_NAME "wgvk_core")
else()
  set(WGVK_PRIMARY_TARGET_NAME "wgvk")
endif()
  add_library(${WGVK_PRIMARY_TARGET_NAME} STATIC ${WGVK_CORE_SRC_LIST})
set(BUNDLE_LIST "")

if(WGVK_BUILD_WGSL_SUPPORT)
  
  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC RG_STATIC=1 SUPPORT_WGSL=1 WGVK_DISABLE_ASSERT=1)
  #if(NOT EMSCRIPTEN)
  target_include_directories(${WGVK_PRIMARY_TARGET_NAME} PUBLIC include)
  #endif()
  option(DAWN_ENABLE_DESKTOP_GL "" OFF)
  option(DAWN_ENABLE_OPENGLES "" OFF)
  option(DAWN_ENABLE_VULKAN "" OFF)
  option(DAWN_ENABLE_D3D11 "" OFF)
  option(DAWN_ENABLE_D3D12 "" OFF)
  option(DAWN_ENABLE_NULL "" OFF)
  option(DAWN_ENABLE_SPIRV_VALIDATION "" OFF)
  option(DAWN_BUILD_SAMPLES "" OFF)
  option(DAWN_USE_GLFW "" OFF)
  option(DAWN_USE_X11 "" OFF)
  option(DAWN_USE_WAYLAND "" OFF)
  option(TINT_BUILD_GLSL_WRITER "" OFF)
  option(TINT_BUILD_CMD_TOOLS "" OFF)
  option(TINT_BUILD_SPV_READER "" OFF)
  set(TINT_BUILD_SPV_WRITER ON CACHE BOOL "" FORCE)
  option(TINT_BUILD_WGSL_WRITER "" OFF)
  option(TINT_BUILD_TESTS "" OFF)
  option(TINT_BUILD_GLSL_VALIDATOR "" OFF)
  option(TINT_ENABLE_IR_VALIDATION "" OFF)
  FetchContent_Declare(
    dawn
    URL https://github.com/manuel5975p/dawn_monorepo/archive/refs/tags/v7281.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP True
  )
  FetchContent_MakeAvailable(dawn)
  set(TINT_BUILD_SPV_WRITER ON CACHE BOOL "" FORCE)
  add_library(tint_c_api STATIC "src/tint_c_api.cpp")
  target_include_directories(tint_c_api PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
  target_include_directories(tint_c_api PRIVATE "${dawn_SOURCE_DIR}")
  target_compile_features(tint_c_api PRIVATE cxx_std_20)
  target_compile_options(tint_c_api PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-fno-rtti>
    $<$<CXX_COMPILER_ID:Clang>:-fno-rtti>
    $<$<CXX_COMPILER_ID:AppleClang>:-fno-rtti>
    $<$<CXX_COMPILER_ID:MSVC>:/GR->  # disable RTTI in MSVC
  )
  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_WGSL=1)
  
  # Bundle wgvk_core and its dependencies into the final `wgvk` library.
  list(APPEND BUNDLE_LIST ${WGVK_PRIMARY_TARGET_NAME} tint_lang_wgsl_reader tint_lang_spirv_writer tint_lang_wgsl_inspector tint_c_api)
endif()

if(WGVK_BUILD_GLSL_SUPPORT)
  #find_package(glslang REQUIRED)
  if(NOT TARGET glslang::glslang)
    set(ENABLE_OPT OFF)
    FetchContent_Declare(
      glslang
      URL https://github.com/KhronosGroup/glslang/archive/refs/tags/15.4.0.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(glslang)
  endif()
  add_library(glslang_c_api STATIC "${CMAKE_CURRENT_LIST_DIR}/src/glslang_c_api.cpp")
  target_link_libraries(glslang_c_api PUBLIC glslang)
  target_include_directories(glslang_c_api PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")

  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_GLSL=1)
  list(APPEND BUNDLE_LIST ${WGVK_PRIMARY_TARGET_NAME} glslang glslang_c_api)
  target_compile_options(glslang_c_api PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-fno-rtti>
    $<$<CXX_COMPILER_ID:Clang>:-fno-rtti>
    $<$<CXX_COMPILER_ID:AppleClang>:-fno-rtti>
    $<$<CXX_COMPILER_ID:MSVC>:/GR->  # disable RTTI in MSVC
  )
endif()

if(WGVK_BUILD_GLSL_SUPPORT OR WGVK_BUILD_WGSL_SUPPORT)
  wgvk_bundle_libraries(wgvk ${WGVK_PRIMARY_TARGET_NAME} ${BUNDLE_LIST})
  target_link_libraries(wgvk INTERFACE ${WGVK_PRIMARY_TARGET_NAME})
endif()
if(BUILD_SHARED_LIBS)
  set_target_properties(wgvk_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(wgvk PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(tint_c_api PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Now, we apply all remaining properties to the primary target using the variable.
# Because we used PUBLIC on the primary target and then propagated its interface
# to the final `wgvk` target, these properties will be correctly inherited.
if(WGVK_USE_VMA)
  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC USE_VMA_ALLOCATOR=1)
endif()

target_include_directories(${WGVK_PRIMARY_TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# Platform-specific surface support
if(WIN32)
  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_WIN32_SURFACE=1)
elseif(APPLE)
  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_METAL_SURFACE=1)
  target_link_libraries(${WGVK_PRIMARY_TARGET_NAME} PUBLIC "-framework Cocoa" "-framework Metal" "-framework QuartzCore")
elseif(UNIX AND NOT ANDROID)
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
if(ANDROID)
  target_compile_definitions(${WGVK_PRIMARY_TARGET_NAME} PUBLIC SUPPORT_ANDROID_SURFACE=1)
endif()


if(WGVK_BUILD_EXAMPLES)
  if(NOT EMSCRIPTEN)
    FetchContent_Declare(
      glfw
      URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP True
    )
    FetchContent_MakeAvailable(glfw)
  endif()
  add_executable(glfw_surface "examples/glfw_surface.c")
  add_executable(basic_compute "examples/basic_compute.c")
  #add_executable(rgfw_surface "examples/rgfw_surface.c")
  add_executable(basic_wgsl_shader "examples/basic_wgsl_shader.c")
  add_executable(basic_glsl_shader "examples/basic_glsl_shader.c")
  add_executable(multi_submit "examples/multi_submit.c")
  add_executable(raytracing "examples/raytracing.c")
  
  # Examples always link against the final `wgvk` target. Thanks to the fix above,
  # they will now correctly inherit all necessary compile definitions.
  target_link_libraries(glfw_surface PUBLIC wgvk glfw)
  target_link_libraries(basic_wgsl_shader PUBLIC wgvk glfw m)
  target_link_libraries(basic_glsl_shader PUBLIC wgvk glfw m)
  target_link_libraries(raytracing PUBLIC wgvk glfw)
  target_link_libraries(basic_compute PUBLIC wgvk)
  target_link_libraries(multi_submit PUBLIC wgvk)
  #target_link_libraries(rgfw_surface PUBLIC wgvk Xrandr)
endif()
