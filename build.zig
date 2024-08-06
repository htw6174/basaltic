const std = @import("std");

/// NOTE: Will recurse directories
pub fn addAllCSources(b: *std.Build, compile: *std.Build.Step.Compile, root: []const u8) void {
    var sources = std.ArrayList([]const u8).init(b.allocator);

    // Search for all C/C++ files in `src` and add them
    {
        var dir = std.fs.cwd().openDir(root, .{ .iterate = true }) catch @panic("Directory not found");

        var walker = dir.walk(b.allocator) catch @panic("OOM");
        defer walker.deinit();

        const allowed_exts = [_][]const u8{ ".c", ".cpp", ".cxx", ".c++", ".cc" };
        while (walker.next() catch null) |entry| {
            const ext = std.fs.path.extension(entry.basename);
            const include_file = for (allowed_exts) |e| {
                if (std.mem.eql(u8, ext, e))
                    break true;
            } else false;
            if (include_file) {
                // we have to clone the path as walker.next() or walker.deinit() will override/kill it
                sources.append(b.dupe(entry.path)) catch @panic("OOM");
            }
        }
    }
    compile.addCSourceFiles(.{ .root = b.path(root), .files = sources.items });
}

// Although this function looks imperative, note that its job is to
// declaratively construct a build graph that will be executed by an external
// runner.
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

    const libhtw_dep = b.dependency("libhtw", .{ .target = target, .optimize = optimize });
    const libhtw = libhtw_dep.artifact("libhtw");

    const cimgui_dep = b.dependency("cimgui", .{ .target = target, .optimize = optimize });
    const cimgui = cimgui_dep.artifact("cimgui");

    // cimgui TEMP: disabled until the rest of the build system works
    // const cimgui_build = b.addSystemCommand(&[_][]const u8{
    //     "cmake",
    //     "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
    //     "-DIMPL_SDL=yes",
    //     "-DIMPL_OPENGL3=yes",
    //     "-DSDL_PATH=\"\"",
    //     "../cimgui_build",
    // });
    // cimgui_build.setCwd(b.path("libs/cimgui"));

    // const pre_build = b.step("pre-build", "Build non-zig libraries from source");
    // pre_build.dependOn(libhtw_build);
    // pre_build.dependOn(cimgui_build);

    // flecs
    const flecs = b.addStaticLibrary(.{
        .name = "flecs",
        .link_libc = true,
        .target = target,
        .optimize = optimize,
    });
    flecs.addCSourceFile(.{ .file = b.path("libs/flecs.c") });
    // NOTE: installHeadersDirectory is used instead of installHeader, even though there is only one header required here.
    // This is because installHeader emits only an "include" (no extension) file with the contents of the provided header, instead of a file with the same name as the original
    // This may be a bug with zig 0.13, because anything linked against a lib built this way can't find the expected header
    flecs.installHeadersDirectory(b.path("libs"), "", .{ .include_extensions = &.{"flecs.h"} });

    // Model library
    const model = b.addSharedLibrary(.{
        .name = "basaltic_model",
        .link_libc = true,
        .target = target,
        .optimize = optimize,
    });
    model.addCSourceFiles(.{ .root = b.path("src/model"), .files = &.{
        "basaltic_model.c",
        "basaltic_worldGen.c",
    } });
    // Glob match to add all files under model/ecs
    addAllCSources(b, model, "src/model/ecs");
    model.installHeadersDirectory(b.path("src/model"), "", .{ .include_extensions = &.{"basaltic_model.h"} });
    model.addIncludePath(b.path("src/include"));
    // TODO: should put model- and view-specific header dependencies in the respective modules
    model.addIncludePath(b.path("src/model/include"));
    // TODO: consider a better organization for ecs modules that doesn't require so much relative header location knowledge
    model.addIncludePath(b.path("src/model/ecs"));
    model.addIncludePath(b.path("src/model/ecs/components"));
    model.linkLibrary(libhtw);
    model.linkLibrary(flecs);

    // View Library
    const view = b.addSharedLibrary(.{
        .name = "basaltic_view",
        .link_libc = true,
        .target = target,
        .optimize = optimize,
    });
    view.addCSourceFiles(.{ .root = b.path("src/view"), .files = &.{
        "basaltic_view.c",
        "basaltic_sokol_gfx.c",
        "basaltic_uiState.c",
    } });
    addAllCSources(b, view, "src/view/ecs");
    view.installHeadersDirectory(b.path("src/view"), "", .{ .include_extensions = &.{"basaltic_view.h"} });
    view.addIncludePath(b.path("src/include"));
    view.addIncludePath(b.path("src/view/include"));
    // TODO: sokol is only used in view module, should localize dependency
    view.addIncludePath(b.path("libs/sokol"));
    // TODO: same as model, but further: view also wants to know about all of the model's ecs components
    view.addIncludePath(b.path("src/view/ecs"));
    view.addIncludePath(b.path("src/view/ecs/components"));
    view.addIncludePath(b.path("src/model/ecs"));
    view.addIncludePath(b.path("src/model/ecs/components"));
    // TODO: remove khash include from component module header. No need to expose it everywhere.
    view.addIncludePath(b.path("src/model/include"));
    view.linkSystemLibrary("SDL2");
    view.linkSystemLibrary("OpenGL");
    view.linkLibrary(libhtw);
    view.linkLibrary(cimgui);
    view.linkLibrary(flecs);
    view.linkLibrary(model);

    const exe = b.addExecutable(.{
        .name = "basaltic",
        // When the entry point is a c file, add it later instead of setting a root source
        //.root_source_file = b.path("src/main.zig"),
        // When a build has c source files that use the c standard library, must link libc
        // Note that this isn't needed when using the Zig standard library in zig source files
        .link_libc = true,
        .target = target,
        .optimize = optimize,
    });
    // Add c source files after creating the exe: Build.Step.Compile
    // can also specify .flags to apply to this source file
    // If adding multiple c files with the same flags, use addCSourceFiles
    exe.addCSourceFile(.{ .file = b.path("src/main.c") });
    exe.addCSourceFiles(.{ .root = b.path("src"), .files = &[_][]const u8{
        "basaltic_super.c",
        "basaltic_window.c",
        "basaltic_editor_base.c",
        "bc_flecs_utils.c",
    } });
    exe.addIncludePath(b.path("src/include"));
    exe.addIncludePath(b.path("libs"));
    // exe.addIncludePath(b.path("libs/cimgui"));
    // TODO: move general-purpose flecs modules out of model; only need this for components_common
    exe.addIncludePath(b.path("src/model/ecs/components"));

    exe.linkSystemLibrary("SDL2");
    exe.linkLibrary(libhtw);
    exe.linkLibrary(cimgui);
    exe.linkLibrary(model);
    exe.linkLibrary(view);

    // This declares intent for the executable to be installed into the
    // standard location when the user invokes the "install" step (the default
    // step when running `zig build`).
    b.installArtifact(exe);

    // TODO: copy licenses to install dir

    // This *creates* a Run step in the build graph, to be executed when another
    // step is evaluated that depends on it. The next line below will establish
    // such a dependency.
    const run_cmd = b.addRunArtifact(exe);

    // set working directory relative to build path instead of setting build directory with args
    // run_cmd.addArg("-d " ++ data_path); // Default location is correct for a "real" install, for dev need to point to the project's data dir
    run_cmd.setCwd(b.path("data"));
    run_cmd.addArg("-n 3 3"); // New world, 3x3 chunks
    // FIXME: why does removing this switch cause an early crash?
    run_cmd.addArg("-e"); // Show editor on start

    // By making the run step depend on the install step, it will be run from the
    // installation directory rather than directly from within the cache directory.
    // This is not necessary, however, if the application depends on other installed
    // files, this ensures they will be present and in the expected location.
    run_cmd.step.dependOn(b.getInstallStep());

    // This allows the user to pass arguments to the application in the build
    // command itself, like this: `zig build run -- arg1 arg2 etc`
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    // This creates a build step. It will be visible in the `zig build --help` menu,
    // and can be selected like this: `zig build run`
    // This will evaluate the `run` step rather than the default, which is "install".
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
