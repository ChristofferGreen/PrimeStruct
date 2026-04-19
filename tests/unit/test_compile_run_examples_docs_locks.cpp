#include "test_compile_run_helpers.h"

#include "test_compile_run_examples_helpers.h"

TEST_SUITE_BEGIN("primestruct.compile.run.examples");

TEST_CASE("contributor doctest guardrails stay source locked") {
  std::filesystem::path agentsPath = std::filesystem::path("..") / "AGENTS.md";
  if (!std::filesystem::exists(agentsPath)) {
    agentsPath = std::filesystem::current_path() / "AGENTS.md";
  }
  REQUIRE(std::filesystem::exists(agentsPath));

  const std::string agents = readFile(agentsPath.string());
  CHECK(agents.find("**Doctest size guardrail:**") != std::string::npos);
  CHECK(agents.find("beyond 10 `SUBCASE` blocks or equivalent subtests") != std::string::npos);
  CHECK(agents.find("multiple focused `TEST_CASE`s or suite shards") != std::string::npos);
  CHECK(agents.find("**Doctest runtime guardrail:**") != std::string::npos);
  CHECK(agents.find("multiple subcases takes more than 5 seconds") != std::string::npos);
  CHECK(agents.find("single-focus doctest still takes more than 5 seconds") != std::string::npos);
  CHECK(agents.find("optimize it or add a brief justification") != std::string::npos);
}

TEST_CASE("stdlib style boundary docs stay source locked") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path agentsPath = std::filesystem::path("..") / "AGENTS.md";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(agentsPath)) {
    agentsPath = std::filesystem::current_path() / "AGENTS.md";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(agentsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string agents = readFile(agentsPath.string());

  CHECK(codeExamples.find("## Stdlib Style Boundary") != std::string::npos);
  CHECK(codeExamples.find("Style-aligned surface code:") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/math/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/maybe/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/file/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/image/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/ui/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/map.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/errors.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/soa_vector_conversions.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx/gfx.prime`") != std::string::npos);
  CHECK(codeExamples.find("Intentionally canonical or substrate-oriented code:") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/bench_non_math/*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/collections.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections/experimental_*`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx/experimental.prime`") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/collections` is intentionally mixed") != std::string::npos);
  CHECK(codeExamples.find("`stdlib/std/gfx` is intentionally mixed") != std::string::npos);

  CHECK(primeStructDoc.find("### Stdlib Surface-Style Boundary") != std::string::npos);
  CHECK(primeStructDoc.find("This boundary is the scope reference for the stdlib surface-style cleanup lane in") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/vector.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/collections.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections/experimental_*`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/gfx/experimental.prime`") != std::string::npos);
  CHECK(primeStructDoc.find("`stdlib/std/collections` and `stdlib/std/gfx`") != std::string::npos);

  CHECK(agents.find("For stdlib style work, treat `stdlib/std/math`, `maybe`, `file`, `image`,") !=
        std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/collections.prime`,") != std::string::npos);
  CHECK(agents.find("`stdlib/std/collections/experimental_*`, and") != std::string::npos);
  CHECK(agents.find("`stdlib/std/gfx/experimental.prime` as canonical/bridge code") !=
        std::string::npos);
}

TEST_CASE("software renderer command list docs stay source locked" * doctest::skip(true)) {
  std::filesystem::path graphicsDocPath = std::filesystem::path("..") / "docs" / "Graphics_API_Design.md";
  std::filesystem::path specDocPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  if (!std::filesystem::exists(graphicsDocPath)) {
    graphicsDocPath = std::filesystem::current_path() / "docs" / "Graphics_API_Design.md";
  }
  if (!std::filesystem::exists(specDocPath)) {
    specDocPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  REQUIRE(std::filesystem::exists(graphicsDocPath));
  REQUIRE(std::filesystem::exists(specDocPath));

  const std::string graphicsDoc = readFile(graphicsDocPath.string());
  const std::string specDoc = readFile(specDocPath.string());

  CHECK(graphicsDoc.find("The initial stdlib prototype lives under `/std/ui/*`") != std::string::npos);
  CHECK(graphicsDoc.find("`Rgba8`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_text(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_rounded_rect(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.push_clip(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.pop_clip()`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.clip_depth()`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree`") != std::string::npos);
  CHECK(graphicsDoc.find("`LoginFormNodes`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_root_column(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_column(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_leaf(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.append_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.measure()`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.arrange(x, y, width, height)`") != std::string::npos);
  CHECK(graphicsDoc.find("`LayoutTree.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.begin_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.end_panel()`") != std::string::npos);
  CHECK(graphicsDoc.find("`CommandList.draw_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_panel(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_label(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_button(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_input(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.bind_event(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.emit_login_form(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`HtmlCommandList.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_move(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_down(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_pointer_up(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_key_down(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_key_up(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_ime_preedit(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_ime_commit(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_resize(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_focus_gained(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.push_focus_lost(...)`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.event_count()`") != std::string::npos);
  CHECK(graphicsDoc.find("`UiEventStream.serialize() -> vector<i32>`") != std::string::npos);
  CHECK(graphicsDoc.find("First word: format version (`1`)") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `draw_text`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `draw_rounded_rect`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `push_clip`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `pop_clip`") != std::string::npos);
  CHECK(graphicsDoc.find("Current prototype serialization format for `/std/ui/HtmlCommandList.serialize()`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`1` = `emit_panel`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `emit_label`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `emit_button`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `emit_input`") != std::string::npos);
  CHECK(graphicsDoc.find("`5` = `bind_event`") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `click`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `input`") != std::string::npos);
  CHECK(graphicsDoc.find("Current prototype serialization format for `/std/ui/UiEventStream.serialize()`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`1` = `pointer_move`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `pointer_down`") != std::string::npos);
  CHECK(graphicsDoc.find("`3` = `pointer_up`") != std::string::npos);
  CHECK(graphicsDoc.find("`4` = `key_down`") != std::string::npos);
  CHECK(graphicsDoc.find("`5` = `key_up`") != std::string::npos);
  CHECK(graphicsDoc.find("`6` = `ime_preedit`") != std::string::npos);
  CHECK(graphicsDoc.find("`7` = `ime_commit`") != std::string::npos);
  CHECK(graphicsDoc.find("`8` = `resize`") != std::string::npos);
  CHECK(graphicsDoc.find("`9` = `focus_gained`") != std::string::npos);
  CHECK(graphicsDoc.find("`10` = `focus_lost`") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `shift`, `2` = `control`, `4` = `alt`, `8` = `meta`") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`pop_clip()` at depth `0` is a deterministic no-op") != std::string::npos);
  CHECK(graphicsDoc.find("Single-root flat tree; nodes are appended in parent-before-child") != std::string::npos);
  CHECK(graphicsDoc.find("`measure()` walks reverse insertion order") != std::string::npos);
  CHECK(graphicsDoc.find("`arrange(x, y, width, height)` assigns the root rectangle") != std::string::npos);
  CHECK(graphicsDoc.find("`1` = `leaf`") != std::string::npos);
  CHECK(graphicsDoc.find("`2` = `column`") != std::string::npos);
  CHECK(graphicsDoc.find("`append_label(...)` creates a leaf whose measured width is") != std::string::npos);
  CHECK(graphicsDoc.find("`append_button(...)` creates a leaf whose measured size is the label") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_input(...)` creates a leaf whose measured height matches the") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_button(...)` emits one rounded rect followed by one text command") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_input(...)` emits one rounded rect followed by one text command") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_panel(...)` creates a column-backed container node") != std::string::npos);
  CHECK(graphicsDoc.find("`begin_panel(...)` emits one rounded rect for the panel background") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`end_panel()` emits one balancing `pop_clip()`") != std::string::npos);
  CHECK(graphicsDoc.find("`append_login_form(...) -> LoginFormNodes` is the first composite widget") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_login_form(...)` emits only through `begin_panel`, `draw_label`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Composite widget helpers in this prototype must not call raw") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`draw_text`, `draw_rounded_rect`, `push_clip`, `pop_clip`, `append_leaf`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`emit_login_form(...)` emits only through `emit_panel`, `emit_label`,") !=
        std::string::npos);
  CHECK(graphicsDoc.find("Composite HTML adapter helpers in this prototype must not call raw") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`append_word`, `append_color`, or `append_string` directly.") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_pointer_move(...)`, `push_pointer_down(...)`, and `push_pointer_up(...)` normalize through one pointer event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_key_down(...)` and `push_key_up(...)` normalize through one key event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_ime_preedit(...)` and `push_ime_commit(...)` normalize through one IME event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`push_resize(...)`, `push_focus_gained(...)`, and `push_focus_lost(...)` normalize through one view event record shape") !=
        std::string::npos);
  CHECK(graphicsDoc.find("can upload a deterministic BGRA8 software surface into a shared Metal") !=
        std::string::npos);
  CHECK(graphicsDoc.find("`--software-surface-demo`") != std::string::npos);
  CHECK(graphicsDoc.find("[CommandList mut] commands{CommandList()}") != std::string::npos);
  CHECK(graphicsDoc.find("tree.append_root_column(2i32, 3i32, 10i32, 4i32)") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_label(root, 10i32, \"Hi\"utf8)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_button(") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_panel(root, 2i32, 1i32, 20i32, 12i32)") != std::string::npos);
  CHECK(graphicsDoc.find("commands.begin_panel(layout, panel, 4i32") != std::string::npos);
  CHECK(graphicsDoc.find("layout.append_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("commands.draw_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[HtmlCommandList mut] html{HtmlCommandList()}") != std::string::npos);
  CHECK(graphicsDoc.find("html.emit_login_form(") != std::string::npos);
  CHECK(graphicsDoc.find("[UiEventStream mut] events{UiEventStream()}") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_pointer_down(login.submitButton, 7i32, 1i32, 20i32, 30i32)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_key_down(login.usernameInput, 13i32, 3i32, 1i32)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_ime_preedit(login.usernameInput, 1i32, 4i32, \"al|\"utf8)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_ime_commit(login.usernameInput, \"alice\"utf8)") !=
        std::string::npos);
  CHECK(graphicsDoc.find("events.push_resize(login.panel, 40i32, 57i32)") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_focus_gained(login.usernameInput)") != std::string::npos);
  CHECK(graphicsDoc.find("events.push_focus_lost(login.usernameInput)") != std::string::npos);
  CHECK(specDoc.find("the first `/std/ui/*` foundation now includes deterministic command-list rendering, a two-pass layout tree contract, basic control emission, a basic panel container primitive, and the first composite widget helper") !=
        std::string::npos);
  CHECK(specDoc.find("`draw_label`, `draw_button`, `draw_input`, `begin_panel`, `end_panel`, `draw_login_form`, `HtmlCommandList`, `emit_panel`, `emit_label`, `emit_button`, `emit_input`, `bind_event`, `emit_login_form`, `UiEventStream`, `push_pointer_move`, `push_pointer_down`, `push_pointer_up`, `push_key_down`, `push_key_up`, `push_ime_preedit`, `push_ime_commit`, `push_resize`, `push_focus_gained`, `push_focus_lost`, `LayoutTree`, `LoginFormNodes`, `append_root_column`, `append_column`, `append_leaf`, `append_label`, `append_button`, `append_input`, `append_panel`, `append_login_form`, `measure`, `arrange`, deterministic `serialize()` output") != std::string::npos);
  CHECK(specDoc.find("blit a deterministic BGRA8 software surface through the native window presenter") !=
        std::string::npos);
  CHECK(specDoc.find("shared widget/layout model can also emit deterministic HTML/backend adapter records") !=
        std::string::npos);
  CHECK(specDoc.find("normalize pointer, keyboard, IME, resize, and focus input into deterministic UI event-stream records") !=
        std::string::npos);
}

TEST_CASE("image api docs and stdlib stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string imageStdlib = readFile(imageStdlibPath.string());

  CHECK(primeStructDoc.find("the shared image file-I/O API currently lives under `/std/image/*`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(width, height, pixels, path) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(path, width, height, pixels) -> Result<ImageError>`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`pixels` is a flat `vector<i32>` in RGB byte") != std::string::npos);
  CHECK(primeStructDoc.find("image file I/O follows `File<...>` behavior: `ppm.read(...)` and `png.read(...)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("require `effects(file_read, heap_alloc)` because they reset/materialize the pixel buffer") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(...)` require `effects(file_write)`") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`file_write` also implies `file_read` for compatibility") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.read(...)` currently parses ASCII `P3` and binary `P6` PPM files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("read contract now compiles through target validation") !=
        std::string::npos);
  CHECK(primeStructDoc.find("overflowed read-side size arithmetic") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ppm.write(...)` now emits ASCII `P3` PPM files in") !=
        std::string::npos);
  CHECK(primeStructDoc.find("invalid dimensions, payload mismatches, overflowed write-side size") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.read(...)` now validates PNG signatures/chunks,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("current PNG read subset for both non-interlaced and Adam7-interlaced images:") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The shared decoder accepts a single") !=
        std::string::npos);
  CHECK(primeStructDoc.find("dynamic-Huffman reads") !=
        std::string::npos);
  CHECK(primeStructDoc.find("reads materialize the public flat RGB buffer") !=
        std::string::npos);
  CHECK(primeStructDoc.find("Malformed or missing PNGs,") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`png.write(...)` now emits non-interlaced 8-bit RGB PNG files in VM/native/Wasm") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`ImageError.why()` currently returns") !=
        std::string::npos);
  CHECK(primeStructDoc.find("`/ContainerError/why([ContainerError] err)` wrapper keeps explicit type-owned error strings") !=
        std::string::npos);
  CHECK(primeStructDoc.find("The image stdlib layer also defines `/ImageError/why([ImageError] err)` as the public wrapper over the") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(ppm.read(width, height, pixels, \"input.ppm\"utf8)))") !=
        std::string::npos);
  CHECK(primeStructDoc.find("print_line(Result.why(png.write(\"output.png\"utf8, width, height, pixels)))") !=
        std::string::npos);

  CHECK(imageStdlib.find("[public struct]\n  ImageError()") != std::string::npos);
  CHECK(imageStdlib.find("ImageError(1i32)") != std::string::npos);
  CHECK(imageStdlib.find("ImageError(2i32)") != std::string::npos);
  CHECK(imageStdlib.find("ImageError(3i32)") != std::string::npos);
  CHECK(imageStdlib.find("\"image_read_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_write_unsupported\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("\"image_invalid_operation\"utf8") != std::string::npos);
  CHECK(imageStdlib.find("namespace ppm") != std::string::npos);
  CHECK(imageStdlib.find("namespace png") != std::string::npos);
  CHECK(imageStdlib.find("ppmReadAsciiInt") != std::string::npos);
  CHECK(imageStdlib.find("ppmReadBinaryRasterLead") != std::string::npos);
  CHECK(imageStdlib.find("imageRgbWritePixelCount") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteInputValid") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteHeader") != std::string::npos);
  CHECK(imageStdlib.find("ppmWriteComponent") != std::string::npos);
  CHECK(imageStdlib.find("pngInflateFixedHuffmanBlock") != std::string::npos);
  CHECK(imageStdlib.find("pngBuildFixedDistanceLengths") != std::string::npos);
  CHECK(imageStdlib.find("pngInflateCopyFromOutput") != std::string::npos);
  CHECK(imageStdlib.find("pngPackedSampleAt") != std::string::npos);
  CHECK(imageStdlib.find("pngScanlineBytesValid") != std::string::npos);
  CHECK(imageStdlib.find("pngScalePackedSampleToByte") != std::string::npos);
  CHECK(imageStdlib.find("pngScaleU16SampleToByte") != std::string::npos);
  CHECK(imageStdlib.find("pngScanlineChannelByte") != std::string::npos);
  CHECK(imageStdlib.find("pngAdam7PassStartX") != std::string::npos);
  CHECK(imageStdlib.find("pngDecodeRows") != std::string::npos);
  CHECK(imageStdlib.find("pngChunkIsPlte(") != std::string::npos);
  CHECK(imageStdlib.find("pngChunkCrcMatches(") != std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_read, heap_alloc)]\n    read([i32 mut] width, [i32 mut] height, [vector<i32> mut] pixels, [string] path)") !=
        std::string::npos);
  CHECK(imageStdlib.find("[public return<Result<ImageError>> effects(file_write)]\n    write([string] path, [i32] width, [i32] height, [vector<i32>] pixels)") !=
        std::string::npos);

  const size_t ppmStart = imageStdlib.find("namespace ppm");
  const size_t pngStart = imageStdlib.find("namespace png");
  REQUIRE(ppmStart != std::string::npos);
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngStart > ppmStart);
  const std::string ppmBody = imageStdlib.substr(ppmStart, pngStart - ppmStart);
  CHECK(ppmBody.find("readImpl(") != std::string::npos);
  CHECK(ppmBody.find("writeImpl(") != std::string::npos);
  CHECK(ppmBody.find("File<Read>(path)?") != std::string::npos);
  CHECK(ppmBody.find("File<Write>(path)?") != std::string::npos);
  CHECK(ppmBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(ppmBody.find("return(Result.ok())") != std::string::npos);
  CHECK(ppmBody.find("return(value - 48i32)") != std::string::npos);
  CHECK(ppmBody.find("value = pendingByte") != std::string::npos);
  CHECK(ppmBody.find("value = multiply(value, 10i32) + ppmDigitValue(byte)") != std::string::npos);
  CHECK(ppmBody.find("pixelCount = convert<i32>(pixelCountWide)") != std::string::npos);
  CHECK(ppmBody.find("status = ppmReadAsciiInt(file, hasPending, pendingByte, parsedWidth)") != std::string::npos);
  CHECK(ppmBody.find("status = ppmWriteComponent(file, pixels[index])") != std::string::npos);
  CHECK(ppmBody.find("assign(") == std::string::npos);
  CHECK(ppmBody.find("plus(") == std::string::npos);
  CHECK(ppmBody.find("minus(") == std::string::npos);
  CHECK(ppmBody.find("return(unsupported_write())") == std::string::npos);

  const std::string pngBody = imageStdlib.substr(pngStart);
  CHECK(pngBody.find("return(unsupported_read())") != std::string::npos);
  CHECK(pngBody.find("readImpl(") != std::string::npos);
  CHECK(pngBody.find("writeImpl(") != std::string::npos);
  CHECK(pngBody.find("pngValidateSignature") != std::string::npos);
  CHECK(pngBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngBody.find("pngReadChunkType") != std::string::npos);
  CHECK(pngBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngBody.find("pngDecodeScanlines") != std::string::npos);
  CHECK(pngBody.find("pngWriteSignature") != std::string::npos);
  CHECK(pngBody.find("pngWriteIdatChunk") != std::string::npos);
  CHECK(pngBody.find("pngWriteSizingValid") != std::string::npos);
  CHECK(pngBody.find("File<Read>(path)?") != std::string::npos);
  CHECK(pngBody.find("File<Write>(path)?") != std::string::npos);
  CHECK(pngBody.find("pngAppendBytes") != std::string::npos);
  const size_t pngDecodeStart = imageStdlib.find("pngPaethPredictor");
  REQUIRE(pngDecodeStart != std::string::npos);
  REQUIRE(pngDecodeStart > pngStart);
  const std::string pngPreludeBody = imageStdlib.substr(pngStart, pngDecodeStart - pngStart);
  CHECK(pngPreludeBody.find("pngReadU32Be") != std::string::npos);
  CHECK(pngPreludeBody.find("pngReadIhdr") != std::string::npos);
  CHECK(pngPreludeBody.find("pngChunkCrc") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteChunkOpen") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteSizingValid") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteIdatChunk") != std::string::npos);
  CHECK(pngPreludeBody.find("pngWriteIendChunk") != std::string::npos);
  CHECK(pngPreludeBody.find("value = multiply(value, 256i32) + byte") != std::string::npos);
  CHECK(pngPreludeBody.find("remaining = remaining - 1i32") != std::string::npos);
  CHECK(pngPreludeBody.find("width = parsedWidth") != std::string::npos);
  CHECK(pngPreludeBody.find("return(value - multiply(divide(value, divisor), divisor))") != std::string::npos);
  CHECK(pngPreludeBody.find("crc = pngCrc32UpdateByte(crc, typeA)") != std::string::npos);
  CHECK(pngPreludeBody.find("return(divide(rawByteCount + 65534i32, 65535i32))") != std::string::npos);
  CHECK(pngPreludeBody.find("rawByteCount = convert<i32>(rawByteCountWide)") != std::string::npos);
  CHECK(pngPreludeBody.find("a = pngMod(a + byte, 65521i32)") != std::string::npos);
  CHECK(pngPreludeBody.find("pixelOffset = pixelOffset + 3i32") != std::string::npos);
  CHECK(pngPreludeBody.find("assign(") == std::string::npos);
  CHECK(pngPreludeBody.find("plus(") == std::string::npos);
  CHECK(pngPreludeBody.find("minus(") == std::string::npos);
  const size_t pngInflateStart = imageStdlib.find("pngZlibHeaderValid");
  REQUIRE(pngInflateStart != std::string::npos);
  REQUIRE(pngInflateStart > pngDecodeStart);
  const std::string pngScanlineBody = imageStdlib.substr(pngDecodeStart, pngInflateStart - pngDecodeStart);
  CHECK(pngScanlineBody.find("pngPaethPredictor") != std::string::npos);
  CHECK(pngScanlineBody.find("pngDecodeRows") != std::string::npos);
  CHECK(pngScanlineBody.find("[i32] predictor{left + up - upLeft}") != std::string::npos);
  CHECK(pngScanlineBody.find("return(divide(multiply(pngColorTypeSamplesPerPixel(colorType), bitDepth) + 7i32, 8i32))") != std::string::npos);
  CHECK(pngScanlineBody.find("scanlineBytes = 0i32") != std::string::npos);
  CHECK(pngScanlineBody.find("reconstructed = pngMod(reconstructed + leftByte, 256i32)") != std::string::npos);
  CHECK(pngScanlineBody.find("paletteBytes[paletteOffset + 1i32]") != std::string::npos);
  CHECK(pngScanlineBody.find("pixelByteIndex = pixelByteIndex + bytesPerPixel") != std::string::npos);
  CHECK(pngScanlineBody.find("offset = offset + scanlineBytes") != std::string::npos);
  CHECK(pngScanlineBody.find("assign(") == std::string::npos);
  CHECK(pngScanlineBody.find("plus(") == std::string::npos);
  CHECK(pngScanlineBody.find("minus(") == std::string::npos);
  const size_t pngInflateExecStart = imageStdlib.find("pngInflateCopyFromOutput");
  REQUIRE(pngInflateExecStart != std::string::npos);
  REQUIRE(pngInflateExecStart > pngInflateStart);
  const std::string pngBitstreamBody = imageStdlib.substr(pngInflateStart, pngInflateExecStart - pngInflateStart);
  CHECK(pngBitstreamBody.find("pngReadBits") != std::string::npos);
  CHECK(pngBitstreamBody.find("pngReadDynamicHuffmanLengths") != std::string::npos);
  CHECK(pngBitstreamBody.find("pngLengthInfo") != std::string::npos);
  CHECK(pngBitstreamBody.find("return(equal(pngMod(multiply(cmf, 256i32) + flg, 31i32), 0i32))") != std::string::npos);
  CHECK(pngBitstreamBody.find("value = value + multiply(pngMod(shifted, 2i32), factor)") != std::string::npos);
  CHECK(pngBitstreamBody.find("codeLengthLengths[codeLengthSymbol] = lengthValue") != std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] totalCodeCount{literalCount + distanceCount}") != std::string::npos);
  CHECK(pngBitstreamBody.find("[i32] repeatCount{11i32 + repeatExtra}") != std::string::npos);
  CHECK(pngBitstreamBody.find("baseOut = 3i32 + (symbol - 257i32)") != std::string::npos);
  CHECK(pngBitstreamBody.find("if(equal(symbol - currentSymbol, 1i32),") != std::string::npos);
  CHECK(pngBitstreamBody.find("assign(") == std::string::npos);
  CHECK(pngBitstreamBody.find("plus(") == std::string::npos);
  CHECK(pngBitstreamBody.find("minus(") == std::string::npos);
  const size_t pngReadStart = imageStdlib.find("pngDecodeScanlines");
  REQUIRE(pngReadStart != std::string::npos);
  REQUIRE(pngReadStart > pngInflateExecStart);
  const std::string pngInflateExecBody = imageStdlib.substr(pngInflateExecStart, pngReadStart - pngInflateExecStart);
  CHECK(pngInflateExecBody.find("pngInflateStoredBlock") != std::string::npos);
  CHECK(pngInflateExecBody.find("pngInflateDynamicHuffmanBlock") != std::string::npos);
  CHECK(pngInflateExecBody.find("pngInflateDeflateBlocks") != std::string::npos);
  CHECK(pngInflateExecBody.find("output[count(output) - distance]") != std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] trailerStart{count(compressed) - 4i32}") != std::string::npos);
  CHECK(pngInflateExecBody.find("byteIndex = byteIndex + 4i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("[i32] matchLength{lengthBase + lengthExtraValue}") != std::string::npos);
  CHECK(pngInflateExecBody.find("code = code + multiply(bit, factor)") != std::string::npos);
  CHECK(pngInflateExecBody.find("symbol = 280i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("hclenBits + 4i32") != std::string::npos);
  CHECK(pngInflateExecBody.find("inflateStatus = pngInflateStoredBlock(compressed, byteIndex, bitIndex, output)") != std::string::npos);
  CHECK(pngInflateExecBody.find("if(not_equal(compressedCount - byteIndex, 4i32),") != std::string::npos);
  CHECK(pngInflateExecBody.find("assign(") == std::string::npos);
  CHECK(pngInflateExecBody.find("plus(") == std::string::npos);
  CHECK(pngInflateExecBody.find("minus(") == std::string::npos);
  const size_t pngWriteStart = imageStdlib.rfind(
      "[effects(file_write), return<int> on_error<FileError, /std/image/ignoreFileError>]\n    writeImpl");
  REQUIRE(pngWriteStart != std::string::npos);
  REQUIRE(pngWriteStart > pngReadStart);
  const std::string pngReadBody = imageStdlib.substr(pngReadStart, pngWriteStart - pngReadStart);
  CHECK(pngReadBody.find("pixels[targetOffset] = passPixels[sourceOffset]") != std::string::npos);
  CHECK(pngReadBody.find("pixels[targetOffset + 1i32] = passPixels[sourceOffset + 1i32]") != std::string::npos);
  CHECK(pngReadBody.find("sawIhdr = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawPlte = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawPostIdatChunk = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("sawIdat = 1i32") != std::string::npos);
  CHECK(pngReadBody.find("width = parsedWidth") != std::string::npos);
  CHECK(pngReadBody.find("height = parsedHeight") != std::string::npos);
  CHECK(pngReadBody.find("assign(") == std::string::npos);
  CHECK(pngReadBody.find("plus(") == std::string::npos);
  CHECK(pngReadBody.find("minus(") == std::string::npos);
  CHECK(pngBody.find("return(invalidOperation())") != std::string::npos);
  CHECK(pngBody.find("return(unsupported_write())") == std::string::npos);
}

TEST_CASE("file read_byte docs and helpers stay source locked") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path preludePath = std::filesystem::path("..") / "src" / "emitter" / "EmitterEmitPrelude.h";
  std::filesystem::path lowererPath = std::filesystem::path("..") / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(preludePath)) {
    preludePath = std::filesystem::current_path() / "src" / "emitter" / "EmitterEmitPrelude.h";
  }
  if (!std::filesystem::exists(lowererPath)) {
    lowererPath = std::filesystem::current_path() / "src" / "ir_lowerer" / "IrLowererFileWriteHelpers.cpp";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(preludePath));
  REQUIRE(std::filesystem::exists(lowererPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string prelude = readFile(preludePath.string());
  const std::string lowerer = readFile(lowererPath.string());

  CHECK(primeStructDoc.find("`read_byte([i32 mut] value)`") != std::string::npos);
  CHECK(primeStructDoc.find("`read_byte(...)` reports deterministic end-of-file as `EOF`") != std::string::npos);
  CHECK(primeStructDoc.find("read-only file operations require `effects(file_read)`") != std::string::npos);

  CHECK(prelude.find("static inline uint32_t ps_file_read_byte") != std::string::npos);
  CHECK(prelude.find("return \" << FileReadEofCode << \"u;") != std::string::npos);
  CHECK(prelude.find("std::string_view(\\\"EOF\\\")") != std::string::npos);

  CHECK(lowerer.find("read_byte requires exactly one argument") != std::string::npos);
  CHECK(lowerer.find("read_byte requires mutable integer binding") != std::string::npos);
  CHECK(lowerer.find("emitInstruction(IrOpcode::FileReadByte") != std::string::npos);
}

TEST_CASE("maybe stdlib control flow stays source locked to surface if syntax") {
  std::filesystem::path primeStructPath = std::filesystem::path("..") / "docs" / "PrimeStruct.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  if (!std::filesystem::exists(primeStructPath)) {
    primeStructPath = std::filesystem::current_path() / "docs" / "PrimeStruct.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  REQUIRE(std::filesystem::exists(primeStructPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));

  const std::string primeStructDoc = readFile(primeStructPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());

  CHECK(primeStructDoc.find("Destroy() {\n      if(not(this.empty)) {\n        drop(this.value)\n      }\n    }") !=
        std::string::npos);
  CHECK(primeStructDoc.find("clear() {\n      if(not(this.empty)) {\n        drop(this.value)\n      }\n      this.empty = true\n    }") !=
        std::string::npos);
  CHECK(maybeStdlib.find("if(not(this.empty)) {\n        drop(this.value)\n      }") != std::string::npos);
  CHECK(maybeStdlib.find("this.empty = true") != std::string::npos);
  CHECK(maybeStdlib.find("this.empty = false") != std::string::npos);
  CHECK(maybeStdlib.find("ref.empty = false") != std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, true)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(this.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("assign(ref.empty, false)") == std::string::npos);
  CHECK(maybeStdlib.find("if(not(this.empty), then() { drop(this.value) }, else() { })") ==
        std::string::npos);
}

TEST_CASE("small stdlib wrappers stay source locked to inferred locals") {
  std::filesystem::path codeExamplesPath = std::filesystem::path("..") / "docs" / "CodeExamples.md";
  std::filesystem::path maybeStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "maybe" / "maybe.prime";
  std::filesystem::path vectorStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "vector.prime";
  std::filesystem::path mapStdlibPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "map.prime";
  std::filesystem::path soaConversionsPath =
      std::filesystem::path("..") / "stdlib" / "std" / "collections" / "soa_vector_conversions.prime";
  if (!std::filesystem::exists(codeExamplesPath)) {
    codeExamplesPath = std::filesystem::current_path() / "docs" / "CodeExamples.md";
  }
  if (!std::filesystem::exists(maybeStdlibPath)) {
    maybeStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "maybe" / "maybe.prime";
  }
  if (!std::filesystem::exists(vectorStdlibPath)) {
    vectorStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "vector.prime";
  }
  if (!std::filesystem::exists(mapStdlibPath)) {
    mapStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "collections" / "map.prime";
  }
  if (!std::filesystem::exists(soaConversionsPath)) {
    soaConversionsPath =
        std::filesystem::current_path() / "stdlib" / "std" / "collections" / "soa_vector_conversions.prime";
  }
  REQUIRE(std::filesystem::exists(codeExamplesPath));
  REQUIRE(std::filesystem::exists(maybeStdlibPath));
  REQUIRE(std::filesystem::exists(vectorStdlibPath));
  REQUIRE(std::filesystem::exists(mapStdlibPath));
  REQUIRE(std::filesystem::exists(soaConversionsPath));

  const std::string codeExamples = readFile(codeExamplesPath.string());
  const std::string maybeStdlib = readFile(maybeStdlibPath.string());
  const std::string vectorStdlib = readFile(vectorStdlibPath.string());
  const std::string mapStdlib = readFile(mapStdlibPath.string());
  const std::string soaConversions = readFile(soaConversionsPath.string());

  CHECK(codeExamples.find("[mut] current{start}") != std::string::npos);
  CHECK(codeExamples.find("limit{5}") != std::string::npos);

  CHECK(maybeStdlib.find("out{take(this.value)}") != std::string::npos);
  CHECK(maybeStdlib.find("[mut] out{Maybe<T>()}") != std::string::npos);
  CHECK(maybeStdlib.find("[mut] ref{location(out)}") != std::string::npos);
  CHECK(maybeStdlib.find("[T] out{take(this.value)}") == std::string::npos);
  CHECK(maybeStdlib.find("[Maybe<T> mut] out{Maybe<T>()}") == std::string::npos);
  CHECK(maybeStdlib.find("[Reference<Maybe<T>> mut] ref{location(out)}") == std::string::npos);

  CHECK(vectorStdlib.find("[mut] out{/std/collections/experimental_vector/vector<T>()}") !=
        std::string::npos);
  CHECK(vectorStdlib.find("valueCount{count(values)}") != std::string::npos);
  CHECK(vectorStdlib.find("[mut] index{0i32}") != std::string::npos);
  CHECK(vectorStdlib.find("[Vector<T> mut] out{/std/collections/experimental_vector/vector<T>()}") ==
        std::string::npos);
  CHECK(vectorStdlib.find("[i32] valueCount{count(values)}") == std::string::npos);
  CHECK(vectorStdlib.find("[i32 mut] index{0i32}") == std::string::npos);

  CHECK(mapStdlib.find("[mut] out{/std/collections/experimental_map/mapNew<K, V>()}") !=
        std::string::npos);
  CHECK(mapStdlib.find("entryCount{count(entries)}") != std::string::npos);
  CHECK(mapStdlib.find("[mut] index{0i32}") != std::string::npos);
  CHECK(mapStdlib.find("current{entries[index]}") != std::string::npos);
  CHECK(mapStdlib.find("[map<K, V> mut] out{/std/collections/experimental_map/mapNew<K, V>()}") ==
        std::string::npos);
  CHECK(mapStdlib.find("[i32] entryCount{count(entries)}") == std::string::npos);
  CHECK(mapStdlib.find("[Entry<K, V>] current{entries[index]}") == std::string::npos);

  CHECK(soaConversions.find("valueCount{/std/collections/soa_vector/count<T>(values)}") !=
        std::string::npos);
  CHECK(soaConversions.find("valueCount{/std/collections/soa_vector/count_ref<T>(values)}") !=
        std::string::npos);
  CHECK(soaConversions.find("[mut] out{vector<T>()}") != std::string::npos);
  CHECK(soaConversions.find("[mut] index{0i32}") != std::string::npos);
  CHECK(soaConversions.find("[i32] valueCount{/std/collections/soa_vector/count<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[i32] valueCount{/std/collections/soa_vector/count_ref<T>(values)}") ==
        std::string::npos);
  CHECK(soaConversions.find("[vector<T> mut] out{vector<T>()}") == std::string::npos);
  CHECK(soaConversions.find("[i32 mut] index{0i32}") == std::string::npos);
}

TEST_CASE("ppm image workflows stay source locked to inferred locals") {
  std::filesystem::path imageStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "image" / "image.prime";
  if (!std::filesystem::exists(imageStdlibPath)) {
    imageStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "image" / "image.prime";
  }
  REQUIRE(std::filesystem::exists(imageStdlibPath));

  const std::string imageStdlib = readFile(imageStdlibPath.string());
  const size_t helperStart = imageStdlib.find("ppmSkipComment(");
  const size_t pngStart = imageStdlib.find("namespace png");
  REQUIRE(helperStart != std::string::npos);
  REQUIRE(pngStart != std::string::npos);
  REQUIRE(pngStart > helperStart);
  const std::string ppmHelpersBody = imageStdlib.substr(helperStart, pngStart - helperStart);

  CHECK(ppmHelpersBody.find("[mut] byte{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("status{ppmReadByteStatus(file, byte)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] started{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("commentStatus{ppmSkipComment(file)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] sawWhitespace{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("pixelCountWide{multiply(multiply(convert<i64>(width), convert<i64>(height)), 3i64)}") !=
        std::string::npos);
  CHECK(ppmHelpersBody.find("pixelCount{count(pixels)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] expectedPixelCount{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("component{pixels[index]}") != std::string::npos);
  CHECK(ppmHelpersBody.find("file{File<Read>(path)?}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] formatByte{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] status{ppmNextByte(file, hasPending, pendingByte, byte)}") !=
        std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] parsedWidth{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("[mut] pixelCount{0i32}") != std::string::npos);
  CHECK(ppmHelpersBody.find("status{readImpl(width, height, pixels, path)}") != std::string::npos);
  CHECK(ppmHelpersBody.find("file{File<Write>(path)?}") != std::string::npos);

  CHECK(ppmHelpersBody.find("[i32] status{ppmReadByteStatus(file, byte)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] started{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] commentStatus{ppmSkipComment(file)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] sawWhitespace{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i64] pixelCountWide{multiply(multiply(convert<i64>(width), convert<i64>(height)), 3i64)}") ==
        std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] pixelCount{count(pixels)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] expectedPixelCount{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] component{pixels[index]}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[File<Read>] file{File<Read>(path)?}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] formatByte{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] status{ppmNextByte(file, hasPending, pendingByte, byte)}") ==
        std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] parsedWidth{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32 mut] pixelCount{0i32}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[i32] status{readImpl(width, height, pixels, path)}") == std::string::npos);
  CHECK(ppmHelpersBody.find("[File<Write>] file{File<Write>(path)?}") == std::string::npos);
}

TEST_CASE("gfx stdlib wrappers stay source locked to inferred locals") {
  std::filesystem::path gfxStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "gfx.prime";
  if (!std::filesystem::exists(gfxStdlibPath)) {
    gfxStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "gfx.prime";
  }
  REQUIRE(std::filesystem::exists(gfxStdlibPath));

  const std::string gfxStdlib = readFile(gfxStdlibPath.string());

  CHECK(gfxStdlib.find("config{\n        SubstrateSwapchainConfig(") != std::string::npos);
  CHECK(gfxStdlib.find("swapchainToken{GraphicsSubstrate.createSwapchain(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("vertexCount{count(vertices)}") != std::string::npos);
  CHECK(gfxStdlib.find("indexCount{count(indices)}") != std::string::npos);
  CHECK(gfxStdlib.find("meshToken{GraphicsSubstrate.createMesh(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("pipelineToken{GraphicsSubstrate.createPipeline(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("frameToken{GraphicsSubstrate.acquireFrame(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("renderPassToken{GraphicsSubstrate.openRenderPass(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("drawToken{GraphicsSubstrate.drawMesh(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("endToken{GraphicsSubstrate.endRenderPass(config)}") != std::string::npos);
  CHECK(gfxStdlib.find("window{Window([token] 1i32, [width] 1i32, [height] 1i32)}") != std::string::npos);
  CHECK(gfxStdlib.find("deviceToken{GraphicsSubstrate.createDevice(config)?}") != std::string::npos);
  CHECK(gfxStdlib.find("queueToken{GraphicsSubstrate.createQueue(config)?}") != std::string::npos);

  CHECK(gfxStdlib.find("[SubstrateSwapchainConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] swapchainToken{GraphicsSubstrate.createSwapchain(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] vertexCount{count(vertices)}") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] indexCount{count(indices)}") == std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateMeshConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] meshToken{GraphicsSubstrate.createMesh(config)?}") == std::string::npos);
  CHECK(gfxStdlib.find("[SubstratePipelineConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] pipelineToken{GraphicsSubstrate.createPipeline(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateFrameConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] frameToken{GraphicsSubstrate.acquireFrame(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateRenderPassConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] renderPassToken{GraphicsSubstrate.openRenderPass(config)}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateDrawMeshConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] drawToken{GraphicsSubstrate.drawMesh(config)}") == std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateRenderPassEndConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] endToken{GraphicsSubstrate.endRenderPass(config)}") == std::string::npos);
  CHECK(gfxStdlib.find("[Window] window{Window([token] 1i32, [width] 1i32, [height] 1i32)}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[SubstrateDeviceConfig] config{") == std::string::npos);
  CHECK(gfxStdlib.find("[i32] deviceToken{GraphicsSubstrate.createDevice(config)?}") ==
        std::string::npos);
  CHECK(gfxStdlib.find("[i32] queueToken{GraphicsSubstrate.createQueue(config)?}") ==
        std::string::npos);
}

TEST_CASE("ui stdlib workflows stay source locked to inferred locals") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  CHECK(source.find("[mut] records{self.records}") != std::string::npos);
  CHECK(source.find("len{count(text)}") != std::string::npos);
  CHECK(source.find("for([mut] index{0i32}, less_than(index, len), ++index)") != std::string::npos);
  CHECK(source.find("[mut] words{vector<i32>()}") != std::string::npos);
  CHECK(source.find("[mut] kinds{self.kinds}") != std::string::npos);
  CHECK(source.find("panel{self.append_panel(parentIndex, panelPaddingPx, panelGapPx, 0i32, 0i32)}") !=
        std::string::npos);
  CHECK(source.find("contentWidth{widget_text_width(textSizePx, text) + multiply(paddingPx, 2i32)}") !=
        std::string::npos);
  CHECK(source.find("[mut] nodeId{count(self.kinds) - 1i32}") != std::string::npos);
  CHECK(source.find("totalNodes{count(self.kinds)}") != std::string::npos);
  CHECK(source.find("paddingPx{self.paddingPxs[nodeId]}") != std::string::npos);
  CHECK(source.find("[mut] childY{innerY}") != std::string::npos);

  CHECK(source.find("[vector<i32> mut] records{self.records}") == std::string::npos);
  CHECK(source.find("[i32] len{count(text)}") == std::string::npos);
  CHECK(source.find("[vector<i32> mut] words{vector<i32>()}") == std::string::npos);
  CHECK(source.find("[vector<i32> mut] kinds{self.kinds}") == std::string::npos);
  CHECK(source.find("[i32] panel{self.append_panel(parentIndex, panelPaddingPx, panelGapPx, 0i32, 0i32)}") ==
        std::string::npos);
  CHECK(source.find("[i32] contentWidth{widget_text_width(textSizePx, text) + multiply(paddingPx, 2i32)}") ==
        std::string::npos);
  CHECK(source.find("[i32 mut] nodeId{count(self.kinds) - 1i32}") == std::string::npos);
  CHECK(source.find("[i32] totalNodes{count(self.kinds)}") == std::string::npos);
  CHECK(source.find("[i32] paddingPx{self.paddingPxs[nodeId]}") == std::string::npos);
  CHECK(source.find("[i32 mut] childY{innerY}") == std::string::npos);
}

TEST_CASE("gfx stdlib wrapper arithmetic stays source locked to surface operators") {
  std::filesystem::path gfxStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "gfx.prime";
  std::filesystem::path gfxExperimentalPath =
      std::filesystem::path("..") / "stdlib" / "std" / "gfx" / "experimental.prime";
  if (!std::filesystem::exists(gfxStdlibPath)) {
    gfxStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "gfx.prime";
  }
  if (!std::filesystem::exists(gfxExperimentalPath)) {
    gfxExperimentalPath = std::filesystem::current_path() / "stdlib" / "std" / "gfx" / "experimental.prime";
  }
  REQUIRE(std::filesystem::exists(gfxStdlibPath));
  REQUIRE(std::filesystem::exists(gfxExperimentalPath));

  const std::string gfxStdlib = readFile(gfxStdlibPath.string());
  const std::string gfxExperimental = readFile(gfxExperimentalPath.string());

  CHECK(gfxStdlib.find("return(Queue([token] this.token + 1i32))") != std::string::npos);
  CHECK(gfxStdlib.find("[swapchainToken] this.token + window.token + 1i32") != std::string::npos);
  CHECK(gfxStdlib.find("[meshToken] this.token + vertexCount + indexCount") != std::string::npos);
  CHECK(gfxStdlib.find("[pipelineToken] shader.value + this.token + 5i32") != std::string::npos);
  CHECK(gfxStdlib.find("[drawToken] this.token + mesh.token + material.token") != std::string::npos);
  CHECK(gfxStdlib.find("if(not_equal(queueToken, deviceToken + 1i32))") != std::string::npos);
  CHECK(gfxStdlib.find("plus(") == std::string::npos);
  CHECK(gfxExperimental.find("plus(") != std::string::npos);
}

TEST_CASE("ui stdlib arithmetic and assignment stay source locked to surface operators") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  CHECK(source.find("assign(") == std::string::npos);
  CHECK(source.find("plus(") == std::string::npos);
  CHECK(source.find("minus(") == std::string::npos);
  CHECK(source.find("self.commandCount = self.commandCount + 1i32") != std::string::npos);
  CHECK(source.find("self.records = records") != std::string::npos);
  CHECK(source.find("for([mut] index{0i32}, less_than(index, len), ++index)") != std::string::npos);
  CHECK(source.find("[mut] nodeId{count(self.kinds) - 1i32}") != std::string::npos);
  CHECK(source.find("childY = childY + self.measuredHeights[childId] + self.gapPxs[nodeId]") !=
        std::string::npos);
}

TEST_CASE("software renderer composite widgets stay source locked to basic widgets") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t drawLoginStart = source.find("draw_login_form(");
  const size_t pushClipStart = source.find("\n    [public effects(heap_alloc), return<void>]\n    push_clip(");
  REQUIRE(drawLoginStart != std::string::npos);
  REQUIRE(pushClipStart != std::string::npos);
  REQUIRE(pushClipStart > drawLoginStart);
  const std::string drawLoginBody = source.substr(drawLoginStart, pushClipStart - drawLoginStart);
  CHECK(drawLoginBody.find("self.begin_panel(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_label(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_input(") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_button(") != std::string::npos);
  CHECK(drawLoginBody.find("self.end_panel()") != std::string::npos);
  CHECK(drawLoginBody.find("self.draw_text(") == std::string::npos);
  CHECK(drawLoginBody.find("self.draw_rounded_rect(") == std::string::npos);
  CHECK(drawLoginBody.find("self.push_clip(") == std::string::npos);
  CHECK(drawLoginBody.find("self.pop_clip(") == std::string::npos);

  const size_t appendLoginStart = source.find("append_login_form(");
  const size_t appendLeafStart = source.find("\n    [public effects(heap_alloc), return<i32>]\n    append_leaf(");
  REQUIRE(appendLoginStart != std::string::npos);
  REQUIRE(appendLeafStart != std::string::npos);
  REQUIRE(appendLeafStart > appendLoginStart);
  const std::string appendLoginBody = source.substr(appendLoginStart, appendLeafStart - appendLoginStart);
  CHECK(appendLoginBody.find("self.append_panel(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_label(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_input(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_button(") != std::string::npos);
  CHECK(appendLoginBody.find("self.append_leaf(") == std::string::npos);
  CHECK(appendLoginBody.find("self.append_column(") == std::string::npos);
  CHECK(appendLoginBody.find("self.append_node(") == std::string::npos);
}

TEST_CASE("software renderer html adapter stays source locked to shared widgets") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t emitLoginStart = source.find("emit_login_form(");
  const size_t serializeStart =
      source.find("\n    [public effects(heap_alloc), return<vector<i32>>]\n    serialize(", emitLoginStart);
  REQUIRE(emitLoginStart != std::string::npos);
  REQUIRE(serializeStart != std::string::npos);
  REQUIRE(serializeStart > emitLoginStart);
  const std::string emitLoginBody = source.substr(emitLoginStart, serializeStart - emitLoginStart);
  CHECK(emitLoginBody.find("self.emit_panel(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_label(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_input(") != std::string::npos);
  CHECK(emitLoginBody.find("self.emit_button(") != std::string::npos);
  CHECK(emitLoginBody.find("self.append_word(") == std::string::npos);
  CHECK(emitLoginBody.find("self.append_color(") == std::string::npos);
  CHECK(emitLoginBody.find("self.append_string(") == std::string::npos);
}

TEST_CASE("software renderer ui event stream stays source locked to normalized helpers") {
  std::filesystem::path uiStdlibPath = std::filesystem::path("..") / "stdlib" / "std" / "ui" / "ui.prime";
  if (!std::filesystem::exists(uiStdlibPath)) {
    uiStdlibPath = std::filesystem::current_path() / "stdlib" / "std" / "ui" / "ui.prime";
  }
  REQUIRE(std::filesystem::exists(uiStdlibPath));

  const std::string source = readFile(uiStdlibPath.string());

  const size_t pointerMoveStart = source.find("push_pointer_move(");
  const size_t keyDownStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_key_down(", pointerMoveStart);
  REQUIRE(pointerMoveStart != std::string::npos);
  REQUIRE(keyDownStart != std::string::npos);
  REQUIRE(keyDownStart > pointerMoveStart);
  const std::string pointerBody = source.substr(pointerMoveStart, keyDownStart - pointerMoveStart);
  CHECK(pointerBody.find("self.append_pointer_event(1i32, targetNodeId, pointerId, -1i32, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_pointer_event(2i32, targetNodeId, pointerId, button, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_pointer_event(3i32, targetNodeId, pointerId, button, x, y)") !=
        std::string::npos);
  CHECK(pointerBody.find("self.append_word(") == std::string::npos);

  const size_t eventCountStart =
      source.find("\n    [public return<i32>]\n    event_count(", keyDownStart);
  const size_t imeStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_ime_preedit(", keyDownStart);
  REQUIRE(imeStart != std::string::npos);
  REQUIRE(imeStart > keyDownStart);
  REQUIRE(eventCountStart != std::string::npos);
  const size_t resizeStart =
      source.find("\n    [public effects(heap_alloc), return<void>]\n    push_resize(", imeStart);
  REQUIRE(resizeStart != std::string::npos);
  REQUIRE(resizeStart > imeStart);
  REQUIRE(eventCountStart > resizeStart);
  const std::string keyBody = source.substr(keyDownStart, imeStart - keyDownStart);
  CHECK(keyBody.find("self.append_key_event(4i32, targetNodeId, keyCode, modifierMask, isRepeat)") !=
        std::string::npos);
  CHECK(keyBody.find("self.append_key_event(5i32, targetNodeId, keyCode, modifierMask, 0i32)") !=
        std::string::npos);
  CHECK(keyBody.find("self.append_word(") == std::string::npos);

  const std::string imeBody = source.substr(imeStart, resizeStart - imeStart);
  CHECK(imeBody.find("self.append_ime_event(6i32, targetNodeId, selectionStart, selectionEnd, text)") !=
        std::string::npos);
  CHECK(imeBody.find("self.append_ime_event(7i32, targetNodeId, -1i32, -1i32, text)") !=
        std::string::npos);
  CHECK(imeBody.find("self.append_word(") == std::string::npos);
  CHECK(imeBody.find("self.append_string(") == std::string::npos);

  const std::string viewBody = source.substr(resizeStart, eventCountStart - resizeStart);
  CHECK(viewBody.find("self.append_view_event(8i32, targetNodeId, width, height)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_view_event(9i32, targetNodeId, 0i32, 0i32)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_view_event(10i32, targetNodeId, 0i32, 0i32)") !=
        std::string::npos);
  CHECK(viewBody.find("self.append_word(") == std::string::npos);
}


TEST_SUITE_END();
