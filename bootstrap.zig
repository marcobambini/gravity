const std = @import("std");
const builtin = @import("builtin");

extern fn c_main(argc: c_int, argv: [*c]const [*c]const u8) c_int;

const DEBUG = builtin.mode == .Debug;
const BACK_ALLOCATOR = std.heap.c_allocator;

pub fn main() u8 {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){ .backing_allocator = BACK_ALLOCATOR };
    defer if (DEBUG) {
        _ = gpa.deinit();
    };
    const allocator = if (DEBUG) gpa.allocator() else BACK_ALLOCATOR;

    var args_list = std.ArrayList([*c]const u8).init(allocator);
    defer {
        for (args_list.items) |arg| {
            allocator.free(std.mem.span(arg));
        }
        args_list.deinit();
    }

    var args_it = std.process.argsWithAllocator(allocator) catch return 1;
    defer args_it.deinit();

    while (args_it.next()) |arg| {
        args_list.append(@ptrCast(allocator.dupeZ(u8, arg) catch return 1)) catch return 1;
    }

    args_list.ensureTotalCapacity(args_list.items.len + 1) catch return 1;
    args_list.append(null) catch unreachable;

    const argv: [*c]const [*c]const u8 = @ptrCast(args_list.allocatedSlice());
    return @intCast(c_main(@intCast(args_list.items.len), argv) & 0xFF);
}
