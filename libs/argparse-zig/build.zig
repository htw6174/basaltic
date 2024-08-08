const std = @import("std");

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

    const upstream = b.dependency("argparse", .{});
    const lib = b.addStaticLibrary(.{
        .name = "argparse",
        .link_libc = true,
        .target = target,
        .optimize = optimize,
    });
    lib.addCSourceFile(.{ .file = upstream.path("argparse.c") });
    lib.installHeadersDirectory(upstream.path(""), "", .{});

    b.installArtifact(lib);
}
