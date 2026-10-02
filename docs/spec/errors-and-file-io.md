# Error Handling and File I/O

> Part of the [PrimeStruct specification index](../PrimeStruct.md). Classification: **normative (draft)**.

### Error Handling (draft)
- **`Result` propagation:** the postfix `?` operator unwraps `Result<T, Error>` in-place; on error, it invokes a local
  `on_error` handler and then returns the error from the current function.
  - **Monadic view:** `value?` is equivalent to binding the success value and early-returning the error; it matches
    `Result.and_then` semantics and is the recommended shorthand for fallible sequencing.
  - **Graphics entry flow:** `?` is also valid inside `return<int>` definitions with a matching
    `on_error<ErrorType, Handler>(...)`; on error, the handler runs and the definition returns the raw error code.
  - **Current stdlib progress:** import `/std/file/*` to use `.prime`-authored `fileReadEof()`,
    `fileErrorIsEof(err)`, the type-owned `FileError.status(err)` / `FileError.result<T>(err)` namespace surface
    instead of hand-packing `FileError` results or hard-coding the EOF status code, plus receiver-style
    `err.status()` / `err.result<T>()` and the type-owned `FileError.why(err)` / `FileError.eof()` /
    `FileError.isEof(err)` helpers for explicit access to the current stdlib FileError helper surface. The same
    import also exposes
    `.prime`-authored `/File/openRead(...)`, `/File/openWrite(...)`, and `/File/openAppend(...)` wrappers so the
    legacy constructor-shaped `File<Mode>(path)` compatibility surface can resolve through stdlib-owned helpers while
    the host file substrate remains builtin and effect-gated underneath.
    Import `/std/collections/*` to use `.prime`-authored `containerErrorStatus(err)` /
    `containerErrorResult<T>(err)` compatibility helpers, the type-owned `ContainerError.status(err)` /
    `ContainerError.result<T>(err)` namespace surface, the canonical constructor helpers
    `ContainerError.missingKey()` / `ContainerError.indexOutOfBounds()` / `ContainerError.empty()` /
    `ContainerError.capacityExceeded()`, or the public `/ContainerError/status(err)` /
    `/ContainerError/result<T>(err)` wrappers plus `/ContainerError/why([ContainerError] err)`,
    `/ContainerError/missingKey()`, `/ContainerError/indexOutOfBounds()`, `/ContainerError/empty()`, and
    `/ContainerError/capacityExceeded()` wrappers. Compatibility wrappers keep the older snake_case root spellings for
    migration, and receiver-style `err.why()` / `err.status()` / `err.result<T>()` stay available instead of
    hand-packing container error codes or reaching through namespace-private helper paths.
    Import `/std/image/*` to use `.prime`-authored `imageReadUnsupported()`, `imageWriteUnsupported()`,
    `imageInvalidOperation()`, `imageErrorStatus(err)`, and `imageErrorResult<T>(err)` compatibility helpers, the
    type-owned `ImageError.status(err)` / `ImageError.result<T>(err)` namespace surface, or the public
    `/ImageError/status(err)` / `/ImageError/result<T>(err)` wrappers instead of hand-packing `ImageError` result
    codes, and load the public `/ImageError/why([ImageError] err)`, `/ImageError/read_unsupported()`,
    `/ImageError/write_unsupported()`, and `/ImageError/invalid_operation()` wrappers plus receiver-style
    `err.why()` / `err.status()` / `err.result<T>()` for explicit type-owned access to the current stdlib error
    strings and constructors. Import `/std/gfx/*` to use the type-owned `GfxError.why(err)` /
    `GfxError.status(err)` namespace surface, the public `windowCreateFailed()` / `deviceCreateFailed()` /
    `swapchainCreateFailed()` / `meshCreateFailed()` / `pipelineCreateFailed()` / `materialCreateFailed()` /
    `frameAcquireFailed()` / `queueSubmitFailed()` / `framePresentFailed()` wrappers, plus receiver-style
    `err.why()` / `err.status()` / `err.result<T>()`. Import `/std/gfx/experimental/*` only when preserving legacy
    compatibility imports; that namespace now acts as a compatibility shim over the canonical `/std/gfx/*` helper
    layer rather than as a peer public graphics contract.
- **Local handlers:** error handling is explicit and local to the scope that declares it.
  - `on_error<ErrorType, Handler>(args...)` is a semantic transform that attaches an error handler to a definition or
    block body.
  - Handlers do **not** flow into nested blocks; any nested block using `?` must declare its own `on_error`.
  - A missing `on_error` for a `?` usage is a compile-time error.
  - The handler signature is `Handler(ErrorType err, args...)`.
  - Bound arguments are evaluated when the error is raised, not at declaration time.

### File I/O (draft)
- **RAII object:** `File<Mode>` is the owning file handle with automatic close on scope exit (`Destroy`).
  - `File<Mode>` is a hybrid surface: host file open/read/write/close behavior remains effect-gated runtime substrate,
    while the user-facing helper layer should live in stdlib `.prime`.
  - `File<Mode>` is move-only; `Clone` is a compile-time error.
  - `close()` disarms the handle so `Destroy` becomes a no-op.
- **Modes:** `Read`, `Write`, `Append`.
- **Construction:** `/File/openRead(path)`, `/File/openWrite(path)`, and `/File/openAppend(path)` return
  `Result<File<Mode>, FileError>`. The imported constructor-shaped `File<Mode>(path)` form is legacy compatibility
  helper syntax that routes through the same mode-specific open helpers; it is not value construction syntax.
  - `path` is a `string` (string literal or literal-backed binding in VM/native).
- **Methods (all return `Result<FileError>`):**
  - `readByte([i32 mut] value)` (reads one byte into `value`; leaves `value` unchanged on error)
  - `write(values...)` (variadic text write)
  - `writeLine(values...)` (variadic + newline)
  - `writeByte(u8_value)`
  - `writeBytes(array<u8>)`
  - `flush()`
  - `close()`
- **Error type:** `FileError` carries `why()` (owned `string`).
  - `readByte(...)` reports deterministic end-of-file as `EOF`.
  - Import `/std/file/*` for the current stdlib-authored file helper layer:
    `fileReadEof()`, `fileErrorIsEof(err)`,
    `FileError.why(err)`, `FileError.eof()`, `FileError.isEof(err)`,
    `/File/openRead(...)`, `/File/openWrite(...)`, `/File/openAppend(...)`,
    `/File/readByte(...)`, zero-to-nine-value heterogenous `/File/write(...)` and
    `/File/writeLine(...)` overload families,
    `/File/writeByte(...)`, `/File/writeBytes(...)`, `/File/flush(...)`, and
    `/File/close<Mode>(...)`.
    Compatibility wrappers keep the older snake_case spellings (`open_read`, `read_byte`, `write_line`,
    `write_byte`, `write_bytes`, `is_eof`) available for migration-only callers.
    Imported legacy constructor-shaped `File<Mode>(path)` calls now route through those `.prime`
    mode-specific open wrappers, and imported method/free-call sugar now prefers the same stdlib
    layer for `readByte`, `write`, `writeLine`, `writeByte`, `writeBytes`, `flush`, and
    `close` across that current zero-to-nine-value overload family. Once `write(...)` or
    `writeLine(...)` exceeds the fixed wrapper ladder, imported broader slash-calls and method
    sugar now fall back to the builtin variadic `/file/*` surface instead of extending the stdlib
    overload family further.
  - The stdlib file layer defines `FileError.why(err)` as the public type-owned wrapper over the intrinsic
    file-error string mapping, so direct `err.why()` and `Result.why(...)` can route through stdlib-owned helper
    surface while platform-specific code-to-string translation stays builtin substrate. It also defines
    `FileError.eof()` so EOF values can be constructed from the same type-owned stdlib surface, plus
    `FileError.isEof(err)` so EOF classification can use that same surface via direct calls or `err.isEof()`.
- **Effect requirement:** read-only file operations require `effects(file_read)` and write/append operations require
  `effects(file_write)`. `file_write` also implies `file_read` for compatibility.
- **Example:**
  ```
  [return<Result<FileError>> effects(file_write, io_err)
   on_error<FileError, /log_file_error>(path)]
  write_ppm([string] path, [i32] width, [i32] height) {
    [File<Write>] file{ File<Write>(path)? }
    file.writeLine("P3")?
    file.writeLine(width, " ", height)?
    file.writeLine("255")?
    file.writeLine(255, " ", 0, " ", 0)?
    return(Result.ok())
  }

  [effects(io_err)]
  /log_file_error([FileError] err, [string] path) {
    print_line_error(path)
    print_line_error(": "utf8)
    print_line_error(err.why())
  }
  ```

### Image File I/O (draft)
- **Shared stdlib surface:** the shared image file-I/O API currently lives under `/std/image/*`.
- **Current prototype surface:**
  - `ImageError`
  - `ppm.read(width, height, pixels, path) -> Result<ImageError>`
  - `ppm.write(path, width, height, pixels) -> Result<ImageError>`
  - `png.read(width, height, pixels, path) -> Result<ImageError>`
  - `png.write(path, width, height, pixels) -> Result<ImageError>`
- **Buffer contract:** `width` and `height` are `i32` out-parameters, and `pixels` is a flat `vector<i32>` in RGB byte
  order (`r, g, b, r, g, b, ...`).
- **Current effect requirement:** image file I/O follows `File<...>` behavior: `ppm.read(...)` and `png.read(...)`
  require `effects(file_read, heap_alloc)` because they reset/materialize the pixel buffer, while `ppm.write(...)` and
  `png.write(...)` require `effects(file_write)`. `file_write` still implies `file_read` for compatibility with mixed
  read/write entrypoints.
- **Current backend contract:** `ppm.read(...)` currently parses ASCII `P3` and binary `P6` PPM files in VM/native/Wasm
  (and C++ emitter flows via the shared stdlib implementation). Missing files, malformed headers, overflowed read-side
  size arithmetic, unsupported max values, non-positive dimensions, missing binary-raster separators, truncated
  payloads, and out-of-range ASCII component values deterministically return `image_invalid_operation`. On Wasm-wasi,
  the current `effects(file_read, heap_alloc)` read contract now compiles through target validation instead of failing
  on the shared heap-backed pixel-buffer materialization path. `ppm.write(...)` now emits ASCII `P3` PPM files in
  VM/native/Wasm for strictly positive `width`/`height` pairs with an exact `width * height * 3` RGB payload; invalid
  dimensions, payload mismatches, overflowed write-side size arithmetic, out-of-range components, and file-open/write
  failures deterministically return `image_invalid_operation`. `png.read(...)` now validates PNG signatures/chunks,
  including CRCs for critical chunks and stricter `PLTE`/`IDAT` ordering for the current subset, and fully decodes the
  current PNG read subset for both non-interlaced and Adam7-interlaced images: 1/2/4/8/16-bit grayscale, 1/2/4/8-bit
  indexed-color, 8/16-bit grayscale+alpha, and 8/16-bit RGB/RGBA inputs whose `IDAT` payload uses stored/no-compression
  deflate blocks, fixed-Huffman deflate blocks, or dynamic-Huffman deflate blocks, with filter-`0`, filter-`1` (`Sub`),
  filter-`2` (`Up`), filter-`3` (`Average`), and filter-`4` (`Paeth`) scanlines. The shared decoder accepts a single
  `PLTE` chunk before the `IDAT` run when present, indexed-color inputs require that palette before decode, and
  multi-chunk `IDAT` payloads must stay consecutive once decoding data begins. Fixed-Huffman reads now cover both
  literal-only payloads and length/distance backreferences with overlapping copy semantics, and dynamic-Huffman reads
  now cover both literal-only payloads and length/distance backreferences with explicit code-length tables. Successful
  reads materialize the public flat RGB buffer, reconstructing Adam7 passes into image order while expanding packed
  grayscale samples to full-range RGB, downscaling 16-bit channel samples into RGB bytes, expanding palette indexes into
  RGB, and dropping alpha when decoding 8/16-bit grayscale+alpha or 8/16-bit RGBA inputs. Malformed or missing PNGs,
  including critical-chunk CRC mismatches, overflowed read-side size arithmetic, invalid `PLTE`/`IDAT` ordering,
  malformed Adam7 scanline payloads, and indexed palette overruns, deterministically return `image_invalid_operation`.
  `png.write(...)` now emits non-interlaced 8-bit RGB PNG files in VM/native/Wasm (and C++ emitter flows via the shared
  stdlib implementation) using a single `IHDR`/`IDAT`/`IEND` layout, stored/no-compression deflate blocks, and
  filter-`0` scanlines for the current write subset; invalid dimensions, payload mismatches, overflowed write-side size
  arithmetic, out-of-range components, and file-open/write failures deterministically return `image_invalid_operation`.
- **Error strings:** `ImageError.why()` currently returns `image_read_unsupported`, `image_write_unsupported`, or
  `image_invalid_operation`.
  - Import `/std/image/*` for the current stdlib-authored ImageError helper layer:
    `imageReadUnsupported()`, `imageWriteUnsupported()`, `imageInvalidOperation()`,
    `imageErrorStatus(err)`, and `imageErrorResult<T>(err)`.
  - The image stdlib layer also defines `/ImageError/why([ImageError] err)` as the public wrapper over the
    current `ImageError.why(err)` mapping so explicit type-owned calls stay on the stdlib surface. It also
    defines `/ImageError/read_unsupported()`, `/ImageError/write_unsupported()`, and
    `/ImageError/invalid_operation()` so ImageError constructor values can come from that same public
    type-owned stdlib surface instead of only from package-level image helpers.
- **Example:**
  ```
  import /std/image/*

  [effects(heap_alloc, io_out, file_write), return<int>]
  main() {
    [i32 mut] width{0i32}
    [i32 mut] height{0i32}
    [vector<i32> mut] pixels{vector<i32>{}}
    print_line(Result.why(ppm.read(width, height, pixels, "input.ppm"utf8)))
    print_line(Result.why(ppm.write("output.ppm"utf8, width, height, pixels)))
    print_line(Result.why(png.read(width, height, pixels, "input.png"utf8)))
    print_line(Result.why(png.write("output.png"utf8, width, height, pixels)))
    return(plus(width, height))
  }
  ```
