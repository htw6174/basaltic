// See also this project, which attempts to maintain a more complete zig module for imgui with c bindings:
// https://github.com/tiawl/cimgui.zig
const std = @import("std");

const root = "cimgui/";

pub const Renderer = enum {
    OpenGL3,
    WebGPU,
    Vulkan,
};

pub const Platform = enum {
    SDL2,
    GLFW,
};

pub fn build(b: *std.Build) void {
    // Standard target options allows the person running `zig build` to choose
    // what target to build for. Here we do not override the defaults, which
    // means any target is allowed, and the default is native. Other options
    // for restricting supported target set are available.
    const target = b.standardTargetOptions(.{});

    // Standard optimization options allow the person running `zig build` to select
    // between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall. Here we do not
    // set a preferred release mode, allowing the user to decide how to optimize.
    const optimize = b.standardOptimizeOption(.{});

    const flags = &.{
        "-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1",
        "-DIMGUI_LIBRARIES",
    };

    // TODO: option to compile as static library instead
    const lib = b.addSharedLibrary(.{
        .name = "cimgui",
        .target = target,
        .optimize = optimize,
    });
    lib.linkLibCpp();
    lib.addCSourceFile(.{ .file = b.path(root ++ "cimgui.cpp"), .flags = flags });
    lib.addCSourceFiles(.{ .root = b.path(root ++ "imgui"), .files = &.{
        "imgui.cpp",
        "imgui_demo.cpp",
        "imgui_draw.cpp",
        "imgui_tables.cpp",
        "imgui_widgets.cpp",
    }, .flags = flags });

    lib.root_module.addCMacro("IMGUI_IMPL_API", "extern \"C\"");

    const renderer = b.option(Renderer, "renderer", "Specify the renderer backend") orelse Renderer.OpenGL3;
    switch (renderer) {
        .OpenGL3 => {
            lib.root_module.addCMacro("IMGUI_IMPL_OPENGL_LOADER_GL3W", "");
            lib.root_module.addCMacro("CIMGUI_USE_OPENGL3", "");
            lib.addCSourceFile(.{ .file = b.path(root ++ "imgui/backends/imgui_impl_opengl3.cpp"), .flags = flags });
            lib.linkSystemLibrary("OpenGL");
        },
        .WebGPU => {
            @panic("WebGPU not supported");
        },
        .Vulkan => {
            @panic("Vulkan not supported");
        },
    }

    const platform = b.option(Platform, "platform", "Specify the platform backend") orelse Platform.SDL2;
    switch (platform) {
        .SDL2 => {
            lib.root_module.addCMacro("IMGUI_SOURCES_sdl", "");
            lib.root_module.addCMacro("CIMGUI_USE_SDL2", "");
            lib.addCSourceFile(.{ .file = b.path(root ++ "imgui/backends/imgui_impl_sdl2.cpp"), .flags = flags });
            lib.linkSystemLibrary("SDL2");
        },
        .GLFW => {
            @panic("GLFW not supported");
        },
    }

    lib.addIncludePath(b.path(root ++ "imgui"));
    lib.addIncludePath(b.path(root ++ "imgui/backends"));
    lib.installHeadersDirectory(b.path(root), "", .{ .include_extensions = &.{"cimgui.h"} });

    b.installArtifact(lib);
}
