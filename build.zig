const std = @import("std");

const GRAVITY_SRC_PATH: []const u8 = "src/";

const COMPILER_DIR: []const u8 = GRAVITY_SRC_PATH ++ "compiler/";
const RUNTIME_DIR: []const u8 = GRAVITY_SRC_PATH ++ "runtime/";
const SHARED_DIR: []const u8 = GRAVITY_SRC_PATH ++ "shared/";
const UTILS_DIR: []const u8 = GRAVITY_SRC_PATH ++ "utils/";
const OPT_DIR: []const u8 = GRAVITY_SRC_PATH ++ "optionals/";
const SRC: []const []const u8 = &.{
    // COMPILER_DIR/*.c
    COMPILER_DIR ++ "gravity_ast.c",
    COMPILER_DIR ++ "gravity_codegen.c",
    COMPILER_DIR ++ "gravity_compiler.c",
    COMPILER_DIR ++ "gravity_ircode.c",
    COMPILER_DIR ++ "gravity_lexer.c",
    COMPILER_DIR ++ "gravity_optimizer.c",
    COMPILER_DIR ++ "gravity_optimizer.c",
    COMPILER_DIR ++ "gravity_parser.c",
    COMPILER_DIR ++ "gravity_semacheck1.c",
    COMPILER_DIR ++ "gravity_semacheck2.c",
    COMPILER_DIR ++ "gravity_symboltable.c",
    COMPILER_DIR ++ "gravity_token.c",
    COMPILER_DIR ++ "gravity_visitor.c",
    // RUNTIME_DIR/*.c
    RUNTIME_DIR ++ "gravity_core.c",
    RUNTIME_DIR ++ "gravity_vm.c",
    // SHARED_DIR/*.c
    SHARED_DIR ++ "gravity_hash.c",
    SHARED_DIR ++ "gravity_memory.c",
    SHARED_DIR ++ "gravity_value.c",
    // UTILS_DIR/*.c
    UTILS_DIR ++ "gravity_debug.c",
    UTILS_DIR ++ "gravity_json.c",
    UTILS_DIR ++ "gravity_utils.c",
    // OPT_DIR/*.c
    OPT_DIR ++ "gravity_opt_env.c",
    OPT_DIR ++ "gravity_opt_file.c",
    OPT_DIR ++ "gravity_opt_json.c",
    OPT_DIR ++ "gravity_opt_math.c",
};
const INCLUDE: []const []const u8 = &.{
    "-I", COMPILER_DIR,
    "-I", RUNTIME_DIR,
    "-I", SHARED_DIR,
    "-I", UTILS_DIR,
    "-I", OPT_DIR,
};
const CFLAGS: []const []const u8 = @as([]const []const u8, &.{
    "-std=gnu99",
    "-fgnu89-inline",
    "-fPIC",
    "-fno-sanitize=undefined",
});

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const shared = b.option(bool, "shared", "Whether to only build shared or static library, default: null/build both");

    const lib_s = b.addStaticLibrary(.{
        .name = if (shared) |_| "gravity" else "gravity_s",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    inline for (INCLUDE) |I| {
        if (comptime std.mem.eql(u8, I, "-I"))
            continue;
        lib_s.installHeadersDirectory(b.path(I), ".", .{});
    }
    lib_s.linkSystemLibrary("m");
    switch (target.result.os.tag) {
        .windows => lib_s.linkSystemLibrary("Shlwapi"),
        .openbsd, .freebsd, .netbsd, .dragonfly => {},
        else => if (!target.result.isDarwin()) lib_s.linkSystemLibrary("rt"),
    }
    lib_s.addCSourceFiles(.{
        .root = b.path("."),
        .files = SRC,
        .flags = INCLUDE ++ CFLAGS ++ @as([]const []const u8, &.{"-DBUILD_GRAVITY_API"}),
    });

    const lib = b.addSharedLibrary(.{
        .name = "gravity",
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    inline for (INCLUDE) |I| {
        if (comptime std.mem.eql(u8, I, "-I"))
            continue;
        lib.installHeadersDirectory(b.path(I), ".", .{});
    }
    lib.linkSystemLibrary("m");
    switch (target.result.os.tag) {
        .windows => lib.linkSystemLibrary("Shlwapi"),
        .openbsd, .freebsd, .netbsd, .dragonfly => {},
        else => if (!target.result.isDarwin()) lib.linkSystemLibrary("rt"),
    }
    lib.addCSourceFiles(.{
        .root = b.path("."),
        .files = SRC,
        .flags = INCLUDE ++ CFLAGS ++ @as([]const []const u8, &.{ "-DBUILD_GRAVITY_API", "-DGRAVITY_DLL" }),
    });

    if (shared) |s| {
        if (s)
            b.installArtifact(lib)
        else
            b.installArtifact(lib_s);
    } else {
        b.installArtifact(lib);
        b.installArtifact(lib_s);
    }

    const exe = b.addExecutable(.{
        .name = "gravity",
        .root_source_file = b.path("bootstrap.zig"),
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibrary(lib_s);
    exe.addCSourceFiles(.{
        .root = b.path("."),
        .files = &.{"src/cli/gravity.c"},
        .flags = CFLAGS ++ @as([]const []const u8, &.{"-DZIG_BOOTSTRAP"}),
    });
    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    run_cmd.addArgs(b.args orelse &.{});

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
