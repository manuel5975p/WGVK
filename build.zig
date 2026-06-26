const std = @import("std");
const AndroidSdk = @import("zig_android").Sdk;

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const use_vma = b.option(bool, "vma", "Use GPUOpen's VMA allocator (Requires C++)") orelse false;
    const support_drm = b.option(bool, "drm", "Support Direct Rendering Infrastructure Surfaces (Linux)") orelse false;
    const enable_x11 = b.option(bool, "x11", "Enable X11 support") orelse true;
    const enable_wayland = b.option(bool, "wayland", "Enable Wayland support") orelse true;
    const wgvk_options = WgvkOptions{
        .target = target,
        .optimize = optimize,
        .use_vma = use_vma,
        .support_drm = support_drm,
        .enable_x11 = enable_x11,
        .enable_wayland = enable_wayland,
    };

    const wgvk_lib = try buildLib(b, wgvk_options);
    b.installArtifact(wgvk_lib);

    const examples_step = b.step("examples", "Build examples");
    const examples: []const []const u8 = &.{
        "asynchronous_loading",
        "basic_compute",
        "glfw_surface",
        "multi_submit",
        "rgfw_surface",
    };
    for (examples) |src| {
        const example_output = buildExample(b, wgvk_options, wgvk_lib, src) catch continue;
        examples_step.dependOn(&example_output.step);
    }

    const build_all_step = b.step("all", "Build all targets");

    const ios_min_version: std.Target.Query.OsVersion = .{
        .semver = .{ .major = 13, .minor = 0, .patch = 0 },
    };

    const build_targets: []const std.Target.Query = &.{
        .{ .cpu_arch = .aarch64, .os_tag = .windows },
        .{ .cpu_arch = .aarch64, .os_tag = .macos },
        .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .gnu },
        .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .musl },
        .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .android },
        .{ .cpu_arch = .x86_64, .os_tag = .windows },
        .{ .cpu_arch = .x86_64, .os_tag = .macos },
        .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .gnu },
        .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .musl },
        .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .android },
        .{ .cpu_arch = .aarch64, .os_tag = .ios, .os_version_min = ios_min_version },
        .{ .cpu_arch = .aarch64, .os_tag = .ios, .abi = .simulator, .os_version_min = ios_min_version },
        .{ .cpu_arch = .x86_64, .os_tag = .ios, .abi = .simulator, .os_version_min = ios_min_version },
    };
    for (build_targets) |t| {
        const resolved_target = b.resolveTargetQuery(t);
        const lib = buildLib(b, .{
            .target = resolved_target,
            .optimize = optimize,
            .use_vma = use_vma,
            .support_drm = support_drm,
            .enable_x11 = enable_x11,
            .enable_wayland = enable_wayland,
        }) catch |err| switch (err) {
            error.UnsupportedTarget => continue,
            else => |e| return e,
        };
        const target_output = b.addInstallArtifact(lib, .{
            .dest_dir = .{
                .override = .{
                    .custom = try t.zigTriple(b.allocator),
                },
            },
        });
        build_all_step.dependOn(&target_output.step);
    }

    // Android APK example
    const android_step = b.step("android-example", "Build Android example APK");
    var sdk = AndroidSdk.init(b, 29);
    const app = try sdk.createApp(.{
        .manifest = .{
            .package = "com.wgvk.example",
            .uses_sdk = .{
                .android_minSdkVersion = 29,
                .android_targetSdkVersion = 35,
            },
            .application = .{
                .android_label = "WGVK Clear",
                .android_hasCode = false,
                .activity = &.{.{
                    .android_name = "android.app.NativeActivity",
                    .android_exported = true,
                    .android_configChanges = &.{ .orientation, .keyboardHidden, .screenSize },
                    .meta_data = &.{.{
                        .android_name = "android.app.lib_name",
                        .android_value = "wgvk_android_clear",
                    }},
                    .intent_filter = &.{AndroidSdk.Application.Manifest.IntentFilter.main_launcher},
                }},
            },
        },
    });

    const android_targets: []const std.Target.Query = &.{
        .{ .cpu_arch = .aarch64, .os_tag = .linux, .abi = .android },
        .{ .cpu_arch = .x86_64, .os_tag = .linux, .abi = .android },
    };
    for (android_targets) |t| {
        const resolved = b.resolveTargetQuery(t);
        const wgvk_android = buildLib(b, .{
            .target = resolved,
            .optimize = optimize,
            .use_vma = use_vma,
            .support_drm = false,
            .enable_x11 = false,
            .enable_wayland = false,
        }) catch continue;

        const lib = app.addLibrary(.{
            .name = "wgvk_android_clear",
            .root_module = b.createModule(.{
                .target = resolved,
                .optimize = optimize,
                .link_libc = true,
            }),
        });
        lib.root_module.addIncludePath(b.path("include"));
        lib.root_module.addCSourceFile(.{
            .file = b.path("examples/android_clear.c"),
            .flags = &.{"-std=c11"},
        });
        lib.root_module.linkLibrary(wgvk_android);
    }

    const apk = app.addInstallApk(".");
    android_step.dependOn(&apk.step);
}

const WgvkOptions = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    use_vma: bool,
    support_drm: bool = false,
    enable_x11: bool,
    enable_wayland: bool,
};

fn buildLib(b: *std.Build, options: WgvkOptions) !*std.Build.Step.Compile {
    const wgvk_mod = b.createModule(.{
        .target = options.target,
        .optimize = options.optimize,
        .link_libc = true,
    });

    wgvk_mod.addCMacro("_POSIX_C_SOURCE", "200809L");
    wgvk_mod.addIncludePath(b.path("./include/"));
    wgvk_mod.addCSourceFiles(.{
        .files = &.{
            "src/wgvk.c",
        },
        .flags = &.{
            "-std=c11",
        },
    });
    if (options.use_vma) {
        wgvk_mod.addCMacro("USE_VMA_ALLOCATOR", "1");
        wgvk_mod.addCSourceFiles(.{
            .files = &.{"src/vma_impl.cpp"},
            .flags = &.{"-std=c++17"},
        });
        wgvk_mod.link_libcpp = true;
    }

    var is_android = false;
    switch (options.target.result.os.tag) {
        .windows => {
            wgvk_mod.addCMacro("SUPPORT_WIN32_SURFACE", "1");
        },
        .macos, .ios => {
            if (b.lazyDependency("xcode_frameworks", .{})) |frameworks| {
                wgvk_mod.addSystemFrameworkPath(frameworks.path("Frameworks"));
                wgvk_mod.addSystemIncludePath(frameworks.path("include"));
                wgvk_mod.addLibraryPath(frameworks.path("lib"));
            }
            wgvk_mod.addCMacro("SUPPORT_METAL_SURFACE", "1");
            wgvk_mod.linkFramework("Metal", .{});
            wgvk_mod.linkFramework("QuartzCore", .{});
        },
        else => {
            is_android = options.target.result.abi.isAndroid();
            if (is_android) {
                wgvk_mod.addCMacro("SUPPORT_ANDROID_SURFACE", "1");
            } else {
                if (options.support_drm) {
                    wgvk_mod.addCMacro("SUPPORT_DRM_SURFACE", "1");
                }

                if (options.enable_x11) if (b.lazyDependency("x11_headers", .{})) |x11| {
                    wgvk_mod.addCMacro("SUPPORT_XLIB_SURFACE", "1");
                    wgvk_mod.linkLibrary(x11.artifact("x11-headers"));
                };
                if (options.enable_wayland) if (b.lazyDependency("wayland_headers", .{})) |wayland| {
                    wgvk_mod.addCMacro("SUPPORT_WAYLAND_SURFACE", "1");
                    wgvk_mod.addIncludePath(wayland.path("wayland"));
                };
            }
        },
    }

    const wgvk_lib =
        if (!is_android)
            b.addLibrary(.{
                .name = "wgvk",
                .root_module = wgvk_mod,
            })
        else blk: {
            var android = AndroidSdk.init(b, 29);
            break :blk android.addLibrary(.{
                .name = "wgvk",
                .root_module = wgvk_mod,
            });
        };

    wgvk_lib.installHeadersDirectory(b.path("./include/webgpu"), "webgpu", .{});
    wgvk_lib.installHeader(b.path("./include/wgvk.h"), "wgvk.h");
    wgvk_lib.installHeader(b.path("./include/wgvk_config.h"), "wgvk_config.h");

    return wgvk_lib;
}

fn buildExample(
    b: *std.Build,
    options: WgvkOptions,
    wgvk_lib: *std.Build.Step.Compile,
    example: []const u8,
) !*std.Build.Step.InstallArtifact {
    const example_exe = b.addExecutable(.{
        .name = example,
        .root_module = b.createModule(.{
            .target = options.target,
            .optimize = options.optimize,
        }),
    });
    example_exe.root_module.addCMacro("_POSIX_C_SOURCE", "200809L");
    example_exe.root_module.addIncludePath(b.path("include"));
    const file_path = b.path(b.fmt("examples/{s}.c", .{example}));
    const language: std.Build.Module.CSourceLanguage = switch (options.target.result.os.tag) {
        .macos => if (std.mem.eql(u8, example, "rgfw_surface")) .c else .objective_c,
        else => .c,
    };

    if (options.target.result.os.tag == .macos) {
        example_exe.root_module.addCMacro("_DARWIN_C_SOURCE", "1");
    }
    example_exe.root_module.addCSourceFile(.{
        .file = file_path,
        .language = language,
    });
    example_exe.root_module.linkLibrary(wgvk_lib);

    if (b.lazyDependency("glfw", .{
        .target = options.target,
        .optimize = options.optimize,
        .x11 = options.enable_x11,
        .wayland = options.enable_wayland,
    })) |glfw| {
        example_exe.root_module.linkLibrary(glfw.artifact("glfw"));
    }

    switch (options.target.result.os.tag) {
        .windows => {
            example_exe.root_module.addCMacro("SUPPORT_WIN32_SURFACE", "1");
        },
        .macos => {
            if (b.lazyDependency("xcode_frameworks", .{})) |frameworks| {
                example_exe.root_module.addSystemFrameworkPath(frameworks.path("Frameworks"));
                example_exe.root_module.addSystemIncludePath(frameworks.path("include"));
                example_exe.root_module.addLibraryPath(frameworks.path("lib"));
            }
            example_exe.root_module.addCMacro("SUPPORT_METAL_SURFACE", "1");
            example_exe.root_module.linkFramework("Metal", .{});
            example_exe.root_module.linkFramework("QuartzCore", .{});
            example_exe.root_module.linkFramework("CoreVideo", .{});
            example_exe.root_module.linkFramework("Cocoa", .{});
            example_exe.root_module.linkFramework("OpenGL", .{});
            example_exe.root_module.linkFramework("IOKit", .{});
        },
        .linux => {
            const is_android = options.target.result.abi.isAndroid();
            if (!is_android and options.enable_x11) if (b.lazyDependency("x11_headers", .{})) |x11| {
                example_exe.root_module.addCMacro("SUPPORT_XLIB_SURFACE", "1");
                example_exe.root_module.linkLibrary(x11.artifact("x11-headers"));
                example_exe.root_module.linkSystemLibrary("X11", .{});
                example_exe.root_module.linkSystemLibrary("Xrandr", .{});
            };
            if (!is_android and options.enable_wayland) if (b.lazyDependency("wayland_headers", .{})) |wayland| {
                example_exe.root_module.addCMacro("SUPPORT_WAYLAND_SURFACE", "1");
                example_exe.root_module.addIncludePath(wayland.path("wayland"));
                example_exe.root_module.linkSystemLibrary("wayland-client", .{});
            };
        },
        .ios => {
            return error.UnsupportedPlatform;
        },
        else => {
            return error.UnsupportedPlatform;
        },
    }

    const example_output = b.addInstallArtifact(example_exe, .{
        .dest_dir = .{
            .override = .{
                .custom = "examples",
            },
        },
    });

    return example_output;
}
