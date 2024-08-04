const std = @import("std");

// This build file will first build the required c/c++ libraries htw-libs and cimgui with their existing cmake toolchains,
// then add the emitted files as library dependencies

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

    // Libraries from git submodules
    // TODO: set this up so that these commands are run only if:
    // - The expected output files do not exist OR
    // - A build arg is specified to force re-run external toolchains

    // Personal library
    const htw_libs_build = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "../htw_libs_build",
    });
    htw_libs_build.setCwd(b.path("libs/htw-libs"));

    // cimgui
    const cimgui_build = b.addSystemCommand(&[_][]const u8{
        "cmake",
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
        "-DIMPL_SDL=yes",
        "-DIMPL_OPENGL3=yes",
        "-DSDL_PATH=\"\"",
        "../cimgui_build",
    });
    cimgui_build.setCwd(b.path("libs/cimgui"));

    const pre_build = b.step("pre-build", "Build non-zig libraries from source");
    pre_build.dependOn(htw_libs_build);
    pre_build.dependOn(cimgui_build);

    // flecs
    const flecs = b.addStaticLibrary(.{ .name = "flecs" });
    flecs.addCSourceFile(.{ .path = "libs/flecs.c" });
    //flecs.addIncludePath(b.path("libs"));

    // Model library
    const model = b.addSharedLibrary(.{ .name = "basaltic_model" });
    model.addCSourceFiles(.{ .root = "src/model", .files = &[_][]const u8{
        "basaltic_model.c",
    } });
    model.step.dependOn(pre_build);
    model.linkLibrary(flecs);

    // View Library
    const view = b.addSharedLibrary(.{ .name = "basaltic_view" });
    view.addCSourceFiles(.{ .root = "src/view", .files = &[_][]const u8{
        "basaltic_view.c",
    } });
    view.step.dependOn(pre_build);
    view.linkLibrary(flecs);
    view.linkLibrary(model);
    view.addLibraryPath(b.path("libs/cimgui_build"));
    view.linkSystemLibrary("cimgui");

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
    exe.addIncludePath(b.path("libs/htw-libs/include"));
    exe.addIncludePath(b.path("libs/cimgui"));

    exe.linkSystemLibrary("SDL2");
    // Library linking searches user-added paths first
    exe.addLibraryPath(b.path("libs"));
    exe.linkSystemLibrary("htw_libs");
    exe.linkSystemLibrary("cimgui");
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

    // TODO: should automatically get good path for data dir (using lazy paths?)
    const data_path = "../../data/";
    run_cmd.addArg("-d " ++ data_path); // Default location is correct for a "real" install, for dev need to point to the project's data dir
    run_cmd.addArg("-n 3 3"); // New world, 3x3 chunks
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
