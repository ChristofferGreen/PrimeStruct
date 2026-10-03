#pragma once

// Spelling tables for the fixed (non-discovered) stdlib surfaces; consumed by
// StdlibSurfaceRegistry.cpp.

#include <array>
#include <string_view>

namespace primec::stdlib_surface_tables {

inline constexpr auto FileHelperMembers = std::to_array<std::string_view>({
    "openRead",
    "openWrite",
    "openAppend",
    "open_read",
    "open_write",
    "open_append",
    "readByte",
    "read_byte",
    "write",
    "writeLine",
    "write_line",
    "writeByte",
    "write_byte",
    "writeBytes",
    "write_bytes",
    "flush",
    "close",
});

inline constexpr auto FileHelperImportAliases = std::to_array<std::string_view>({
    "/std/file/File",
    "/File",
    "File",
});

inline constexpr auto FileHelperCompatibilitySpellings = std::to_array<std::string_view>({
    "/file/read_byte",
    "/file/write_byte",
    "/file/write_bytes",
    "/file/flush",
});

inline constexpr auto FileHelperLoweringSpellings = std::to_array<std::string_view>({
    "/std/file/File/open_read",
    "/std/file/File/open_write",
    "/std/file/File/open_append",
    "/std/file/File/write",
    "/std/file/File/write_line",
    "/file/write",
    "/file/write_line",
    "/file/read_byte",
    "/file/write_byte",
    "/file/write_bytes",
    "/file/flush",
    "/file/close",
});

inline constexpr auto FileErrorHelperMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "eof",
    "isEof",
    "is_eof",
    "result",
});

inline constexpr auto FileErrorImportAliases = std::to_array<std::string_view>({
    "/std/file/FileError",
    "/FileError",
    "FileError",
});

inline constexpr auto FileErrorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/file/fileReadEof",
    "/std/file/fileErrorIsEof",
    "/std/file/fileErrorStatus",
    "/std/file/fileErrorResult",
});

inline constexpr auto FileErrorLoweringSpellings = std::to_array<std::string_view>({
    "/std/file/FileError/why",
    "/std/file/FileError/status",
    "/std/file/FileError/eof",
    "/std/file/FileError/is_eof",
    "/std/file/FileError/result",
    "/file_error/why",
});

inline constexpr auto CollectionsContainerErrorMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "missing_key",
    "index_out_of_bounds",
    "empty",
    "capacity_exceeded",
    "result",
});

inline constexpr auto CollectionsContainerErrorImportAliases = std::to_array<std::string_view>({
    "/std/collections/ContainerError",
    "/ContainerError",
    "ContainerError",
});

inline constexpr auto CollectionsContainerErrorCompatibilitySpellings = std::to_array<std::string_view>({
    "/ContainerError/why",
    "/ContainerError/status",
    "/ContainerError/missing_key",
    "/ContainerError/index_out_of_bounds",
    "/ContainerError/empty",
    "/ContainerError/capacity_exceeded",
    "/ContainerError/result",
    "/std/collections/containerMissingKey",
    "/std/collections/containerIndexOutOfBounds",
    "/std/collections/containerEmpty",
    "/std/collections/containerCapacityExceeded",
    "/std/collections/containerErrorStatus",
    "/std/collections/containerErrorResult",
});

inline constexpr auto CollectionsContainerErrorLoweringSpellings = std::to_array<std::string_view>({
    "/std/collections/ContainerError/why",
    "/std/collections/ContainerError/status",
    "/std/collections/ContainerError/missing_key",
    "/std/collections/ContainerError/index_out_of_bounds",
    "/std/collections/ContainerError/empty",
    "/std/collections/ContainerError/capacity_exceeded",
    "/std/collections/ContainerError/result",
});

inline constexpr auto GfxBufferHelperMembers = std::to_array<std::string_view>({
    "count",
    "empty",
    "is_valid",
    "readback",
    "load",
    "store",
    "allocate",
    "upload",
});

inline constexpr auto GfxBufferImportAliases = std::to_array<std::string_view>({
    "/std/gfx/Buffer",
    "/Buffer",
    "Buffer",
});

inline constexpr auto GfxBufferCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/gfx/experimental/Buffer",
    "/std/gfx/experimental/Buffer/count",
    "/std/gfx/experimental/Buffer/empty",
    "/std/gfx/experimental/Buffer/is_valid",
    "/std/gfx/experimental/Buffer/readback",
    "/std/gfx/experimental/Buffer/load",
    "/std/gfx/experimental/Buffer/store",
    "/std/gfx/experimental/Buffer/allocate",
    "/std/gfx/experimental/Buffer/upload",
});

inline constexpr auto GfxBufferLoweringSpellings = std::to_array<std::string_view>({
    "/std/gfx/Buffer/count",
    "/std/gfx/Buffer/empty",
    "/std/gfx/Buffer/is_valid",
    "/std/gfx/Buffer/readback",
    "/std/gfx/Buffer/load",
    "/std/gfx/Buffer/store",
    "/std/gfx/Buffer/allocate",
    "/std/gfx/Buffer/upload",
    "/std/gpu/readback",
    "/std/gpu/buffer_load",
    "/std/gpu/buffer_store",
    "/std/gpu/buffer",
    "/std/gpu/upload",
});

inline constexpr auto GfxErrorHelperMembers = std::to_array<std::string_view>({
    "why",
    "status",
    "window_create_failed",
    "device_create_failed",
    "swapchain_create_failed",
    "mesh_create_failed",
    "pipeline_create_failed",
    "material_create_failed",
    "frame_acquire_failed",
    "queue_submit_failed",
    "frame_present_failed",
    "result",
});

inline constexpr auto GfxErrorImportAliases = std::to_array<std::string_view>({
    "/std/gfx/GfxError",
    "/GfxError",
    "GfxError",
});

inline constexpr auto GfxErrorCompatibilitySpellings = std::to_array<std::string_view>({
    "/std/gfx/experimental/GfxError",
    "/std/gfx/experimental/ignoreGfxError",
});

inline constexpr auto GfxErrorLoweringSpellings = std::to_array<std::string_view>({
    "/std/gfx/GfxError/why",
    "/std/gfx/GfxError/status",
    "/std/gfx/GfxError/result",
    "/std/gfx/ignoreGfxError",
    "/std/gfx/experimental/GfxError/result",
});


} // namespace primec::stdlib_surface_tables
