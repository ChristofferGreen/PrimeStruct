#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <string>
#include <vector>

using primec::embed::ScriptEngine;

// Differential suite for the owned `String` (docs/spec/strings-and-views.md, Parity with `std::string`): the same
// operations run on the VM, through `--emit=optexe`, and on a C++ `std::string` oracle, and the printed checksums
// of the text must match exactly.
TEST_SUITE_BEGIN("primestruct.backend.string_conformance");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("string_conformance");
  std::filesystem::create_directories(dir);
  return (dir / name).string();
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string shellQuote(const std::string &text) { return "'" + text + "'"; }

int run(const std::string &command) {
  const int status = std::system(command.c_str());
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// What the VM prints for the program.
std::string vmOutput(const std::string &source) {
  ScriptEngine engine;
  const auto script = engine.compileSource("/string_conformance.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  std::string out;
  primec::embed::RunOptions options;
  options.output = [&out](int fd, std::string_view chunk) {
    if (fd == 1) {
      out += chunk;
    }
  };
  const auto result = script.run({}, options);
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  CHECK(result.exitCode == 0);
  return out;
}

// What the program compiled with `--emit=optexe` prints.
std::string compiledOutput(const std::string &kind, const std::string &name, const std::string &source) {
  const std::string file = scratch(name + ".prime");
  std::ofstream(file, std::ios::binary) << source;
  const std::string exe = scratch(name + "_" + kind);
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=" + kind + " " + shellQuote(file) + " -o " + shellQuote(exe) + " > " +
                         shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const std::string out = exe + ".out";
  CHECK(run(shellQuote(exe) + " > " + shellQuote(out)) == 0);
  return readFile(out);
}

std::string optexeOutput(const std::string &name, const std::string &source) { return compiledOutput("optexe", name, source); }

// Whether the arm64 native backend runs byte memory and the String heap in this build.
constexpr bool NativeRunsBytes =
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    true;
#else
    false;
#endif

// 32-bit wrapping arithmetic, as the language defines it.
int32_t wrapMul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }
int32_t wrapAdd(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }

int32_t pick(int32_t r, int32_t m) {
  const int32_t v = r - r / m * m;
  return v < 0 ? v + m : v;
}

int32_t foldText(const std::string &text, int32_t h) {
  for (const unsigned char ch : text) {
    h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(ch));
  }
  return h;
}

// Expands to the loop that folds the bytes of `String` variable `var` into `h`.
std::string foldCode(const std::string &var, const std::string &indent) {
  return indent + "for([i32 mut] fi{0i32}, fi < " + var + ".count(), fi++) {\n" + indent + "  h = h * 31i32 + convert<i32>(" + var +
         ".byteAt(fi))\n" + indent + "}\n";
}

const char *Short = "ab";
const char *Long = "hello world, this is a longer chunk of text";

constexpr int Steps = 300;
const std::vector<int32_t> Seeds = {1, 7, 12345, -99, 2026};

// The edit program: a seeded stream of append / push / insert / erase / replace / substr / reserve / shrink steps.
std::string editProgram() {
  std::string seeds;
  for (size_t i = 0; i < Seeds.size(); ++i) {
    seeds += "  seed = " + std::to_string(Seeds[i]) + "i32\n  h = 17i32\n";
    seeds += "  for([i32 mut] step{0i32}, step < " + std::to_string(Steps) + "i32, step++) {\n";
    seeds +=
        "    seed = seed * 1103515245i32 + 12345i32\n"
        "    [i32] op{pick(seed / 65536i32, 9i32)}\n"
        "    seed = seed * 1103515245i32 + 12345i32\n"
        "    [i32] a{pick(seed / 65536i32, s.count() + 1i32)}\n"
        "    seed = seed * 1103515245i32 + 12345i32\n"
        "    [i32] b{a + pick(seed / 65536i32, s.count() - a + 1i32)}\n"
        "    seed = seed * 1103515245i32 + 12345i32\n"
        "    [i32] c{pick(seed / 65536i32, 26i32)}\n"
        "    if(op == 0i32) {\n      s.append(\"ab\")\n    }\n"
        "    if(op == 1i32) {\n      s.append(\"hello world, this is a longer chunk of text\")\n    }\n"
        "    if(op == 2i32) {\n      s.pushByte(convert<u8>(97i32 + c))\n    }\n"
        "    if(op == 3i32) {\n      s.insert(a, \"XY\")\n    }\n"
        "    if(op == 4i32) {\n      s.erase(a, b)\n    }\n"
        "    if(op == 5i32) {\n      s.replace(a, b, \"<>zz\")\n    }\n"
        "    if(op == 6i32) {\n      [String] t{s.substr(a, b)}\n" +
        foldCode("t", "      ") +
        "    }\n"
        "    if(op == 7i32) {\n      if(s.count() > 2000i32) {\n        s.clear()\n      }\n    }\n"
        "    if(op == 8i32) {\n      s.reserve(s.count() * 2i32)\n      s.shrinkToFit()\n    }\n"
        "    h = h * 31i32 + s.count()\n"
        "  }\n";
    seeds += foldCode("s", "  ");
    seeds += "  print_line(h)\n  s.clear()\n";
  }
  return "import /std/text/*\nimport /std/text/view/*\n\n"
         "[return<int>]\npick([i32] r, [i32] m) {\n  [i32] v{r - r / m * m}\n"
         "  return(if(v < 0i32, then() { v + m }, else() { v }))\n}\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  [String mut] s{/std/text/stringFrom(\"\")}\n  [i32 mut] seed{0i32}\n  [i32 mut] h{17i32}\n" +
         seeds + "  return(0i32)\n}\n";
}

std::string editOracle() {
  std::string out;
  for (const int32_t startSeed : Seeds) {
    std::string s;
    int32_t seed = startSeed;
    int32_t h = 17;
    const auto next = [&seed]() { seed = wrapAdd(wrapMul(seed, 1103515245), 12345); return seed / 65536; };
    for (int step = 0; step < Steps; ++step) {
      const int32_t op = pick(next(), 9);
      const int32_t a = pick(next(), static_cast<int32_t>(s.size()) + 1);
      const int32_t b = a + pick(next(), static_cast<int32_t>(s.size()) - a + 1);
      const int32_t c = pick(next(), 26);
      switch (op) {
      case 0: s += Short; break;
      case 1: s += Long; break;
      case 2: s.push_back(static_cast<char>(97 + c)); break;
      case 3: s.insert(static_cast<size_t>(a), "XY"); break;
      case 4: s.erase(static_cast<size_t>(a), static_cast<size_t>(b - a)); break;
      case 5: s.replace(static_cast<size_t>(a), static_cast<size_t>(b - a), "<>zz"); break;
      case 6: h = foldText(s.substr(static_cast<size_t>(a), static_cast<size_t>(b - a)), h); break;
      case 7:
        if (s.size() > 2000) {
          s.clear();
        }
        break;
      default: break;
      }
      h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(s.size()));
    }
    h = foldText(s, h);
    out += std::to_string(h) + "\n";
  }
  return out;
}

// Sizes around the heap's size classes (31 usable bytes in the smallest block, 64 KiB for the large-block switch).
const std::vector<int> Boundaries = {0, 1, 30, 31, 32, 33, 62, 63, 64, 65, 1000, 65535, 65536, 65537, 70000};

std::string boundaryProgram() {
  std::string body;
  int index = 0;
  for (const int size : Boundaries) {
    const std::string copyName = "copy" + std::to_string(index++);
    body += "  s.clear()\n";
    body += "  for([i32 mut] i{0i32}, i < " + std::to_string(size) + "i32, i++) {\n    s.pushByte(convert<u8>(65i32 + i - i / 26i32 * 26i32))\n  }\n";
    body += "  h = 17i32\n" + foldCode("s", "  ") + "  h = h * 31i32 + s.count()\n  print_line(h)\n";
    // copy it, grow it past the next class, cut it back, then compare lengths and content with the copy
    body += "  [String] " + copyName + "{s}\n  s.insert(s.count() / 2i32, \"MIDDLE\")\n  s.erase(0i32, s.count() / 3i32)\n";
    body += "  s.append(\"tail\")\n  h = 17i32\n" + foldCode("s", "  ") + "  h = h * 31i32 + s.count()\n  print_line(h)\n";
    body += "  h = 17i32\n" + foldCode(copyName, "  ") + "  h = h * 31i32 + " + copyName + ".count()\n  print_line(h)\n";
    body += "  s.shrinkToFit()\n  h = 17i32\n" + foldCode("s", "  ") + "  print_line(h)\n";
  }
  return "import /std/text/*\nimport /std/text/view/*\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  [String mut] s{/std/text/stringFrom(\"\")}\n  [i32 mut] h{17i32}\n" +
         body + "  return(0i32)\n}\n";
}

std::string boundaryOracle() {
  std::string out;
  for (const int size : Boundaries) {
    std::string s;
    for (int i = 0; i < size; ++i) {
      s.push_back(static_cast<char>(65 + i - i / 26 * 26));
    }
    out += std::to_string(wrapAdd(wrapMul(foldText(s, 17), 31), static_cast<int32_t>(s.size()))) + "\n";
    const std::string copy = s;
    s.insert(s.size() / 2, "MIDDLE");
    s.erase(0, s.size() / 3);
    s += "tail";
    out += std::to_string(wrapAdd(wrapMul(foldText(s, 17), 31), static_cast<int32_t>(s.size()))) + "\n";
    out += std::to_string(wrapAdd(wrapMul(foldText(copy, 17), 31), static_cast<int32_t>(copy.size()))) + "\n";
    out += std::to_string(foldText(s, 17)) + "\n";
  }
  return out;
}
} // namespace

TEST_CASE("edit sequences give the same text as std::string on the VM") {
  CHECK(vmOutput(editProgram()) == editOracle());
}

// The optexe cases spend their time in the host C++ compiler (about 4 s each), not in the suite.
TEST_CASE("edit sequences give the same text as std::string through optexe") {
  CHECK(optexeOutput("edit", editProgram()) == editOracle());
}

TEST_CASE("sizes around the heap block classes give the same text as std::string on the VM") {
  CHECK(vmOutput(boundaryProgram()) == boundaryOracle());
}

TEST_CASE("sizes around the heap block classes give the same text as std::string through optexe") {
  CHECK(optexeOutput("boundary", boundaryProgram()) == boundaryOracle());
}

// ---- the bulk byte operations (__pstext_move, _fill, _compare, _find) -------------------------------------------

namespace {
const char *BulkText = "abcdefghijklmnopqrstuvwxyz0123456789";

// Operations on two Strings (a, b) through the stdlib's moveBytes / fillBytes / compareBytes / findByte, each result
// printed; mirrored below on std::string with memmove, memset, memcmp and memchr.
std::string bulkProgram() {
  std::string body =
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] a{/std/text/stringFrom(\"abcdefghijklmnopqrstuvwxyz0123456789\")}\n"
      "  [String mut] b{/std/text/stringFrom(\"abcdefghijklmnopqrstuvwxyz0123456789\")}\n";
  const auto fold = [](const char *var) {
    return std::string("  [i32 mut] h") + var + "{17i32}\n  for([i32 mut] i{0i32}, i < " + var + ".count(), i++) {\n    h" + var + " = h" +
           var + " * 31i32 + convert<i32>(" + var + ".byteAt(i))\n  }\n  print_line(h" + var + ")\n";
  };
  // overlapping moves in both directions, a move to a lower address, a move of nothing
  body += "  /std/text/moveBytes(/std/text/textAddress(a.data, 2i32), /std/text/textAddress(a.data, 0i32), 10i32)\n";
  body += "  /std/text/moveBytes(/std/text/textAddress(a.data, 3i32), /std/text/textAddress(a.data, 9i32), 20i32)\n";
  body += "  /std/text/moveBytes(/std/text/textAddress(a.data, 0i32), /std/text/textAddress(a.data, 30i32), 0i32)\n";
  body += "  /std/text/moveBytes(/std/text/textAddress(a.data, 5i32), /std/text/textAddress(a.data, 5i32), 7i32)\n";
  body += fold("a");
  body += "  /std/text/fillBytes(/std/text/textAddress(b.data, 4i32), 90u8, 9i32)\n";
  body += "  /std/text/fillBytes(/std/text/textAddress(b.data, 20i32), 1u8, 0i32)\n";
  body += fold("b");
  // compare: equal prefix, then less / greater / zero length / full equal
  body += "  print_line(/std/text/compareBytes(a.data, b.data, 4i32))\n";
  body += "  print_line(/std/text/compareBytes(a.data, b.data, 6i32))\n";
  body += "  print_line(/std/text/compareBytes(b.data, a.data, 6i32))\n";
  body += "  print_line(/std/text/compareBytes(a.data, b.data, 0i32))\n";
  body += "  print_line(/std/text/compareBytes(a.data, a.data, 36i32))\n";
  // find: first match, a match late in the range, no match, a range that stops short of the match
  body += "  print_line(/std/text/findByte(b.data, 36i32, 90u8))\n";
  body += "  print_line(/std/text/findByte(b.data, 36i32, 57u8))\n";
  body += "  print_line(/std/text/findByte(b.data, 36i32, 33u8))\n";
  body += "  print_line(/std/text/findByte(b.data, 6i32, 90u8))\n";
  body += "  print_line(/std/text/findByte(b.data, 0i32, 90u8))\n";
  body += "  return(0i32)\n}\n";
  return body;
}

std::string bulkOracle() {
  std::string a = BulkText;
  std::string b = BulkText;
  std::string out;
  const auto fold = [](const std::string &text) {
    int32_t h = 17;
    for (const unsigned char ch : text) {
      h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(ch));
    }
    return h;
  };
  std::memmove(&a[2], &a[0], 10);
  std::memmove(&a[3], &a[9], 20);
  std::memmove(&a[5], &a[5], 7);
  out += std::to_string(fold(a)) + "\n";
  std::memset(&b[4], 90, 9);
  out += std::to_string(fold(b)) + "\n";
  const auto sign = [](int value) { return value < 0 ? -1 : (value > 0 ? 1 : 0); };
  out += std::to_string(sign(std::memcmp(a.data(), b.data(), 4))) + "\n";
  out += std::to_string(sign(std::memcmp(a.data(), b.data(), 6))) + "\n";
  out += std::to_string(sign(std::memcmp(b.data(), a.data(), 6))) + "\n";
  out += "0\n";
  out += std::to_string(sign(std::memcmp(a.data(), a.data(), 36))) + "\n";
  const auto find = [&b](size_t count, int value) {
    const void *hit = count == 0 ? nullptr : std::memchr(b.data(), value, count);
    return hit == nullptr ? -1 : static_cast<int>(static_cast<const char *>(hit) - b.data());
  };
  out += std::to_string(find(36, 90)) + "\n";
  out += std::to_string(find(36, 57)) + "\n";
  out += std::to_string(find(36, 33)) + "\n";
  out += std::to_string(find(6, 90)) + "\n";
  out += std::to_string(find(0, 90)) + "\n";
  return out;
}
} // namespace

TEST_CASE("bulk byte operations agree with memmove, memset, memcmp and memchr on the VM") {
  CHECK(vmOutput(bulkProgram()) == bulkOracle());
}

TEST_CASE("bulk byte operations agree with the C library through optexe") {
  CHECK(optexeOutput("bulk", bulkProgram()) == bulkOracle());
}

TEST_CASE("bulk byte operations agree with the C library on the arm64 native backend") {
  if (!NativeRunsBytes) {
    return;
  }
  CHECK(compiledOutput("native", "bulk", bulkProgram()) == bulkOracle());
}

// A text block lives inside a 256 KiB chunk and the chunk is the live region, as for per-byte access; a range that
// leaves the chunk faults.
TEST_CASE("a bulk operation past the end of its region stops the program on the VM") {
  const std::string source =
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
      "  [String mut] a{/std/text/stringFrom(\"abc\")}\n"
      "  /std/text/fillBytes(a.data, 1u8, 1000000i32)\n  return(0i32)\n}\n";
  ScriptEngine engine;
  const auto script = engine.compileSource("/bulk_fault.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  CHECK_FALSE(result.ok);
  CHECK(result.diagnostics.find("invalid text range") != std::string::npos);
}

// ---- StringView: a borrowed view of literals and Strings ---------------------------------------------

namespace {
std::string viewProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] s{/std/text/stringFrom(\"hello world\")}\n"
      "  [StringView] v{s.view()}\n"
      "  [StringView] lit{textOf(\"hello world\")}\n"
      "  [StringView] other{textOf(\"hello there\")}\n"
      "  [StringView] none{textOf(\"\")}\n"
      "  [String] blank{/std/text/stringFrom(\"\")}\n"
      "  [StringView] blankView{blank.view()}\n"
      "  print_line(v.count())\n"
      "  print_line(lit.count())\n"
      "  print_line(none.count())\n"
      "  print_line(blankView.count())\n"
      "  print_line(convert<i32>(v.byteAt(0i32)))\n"
      "  print_line(convert<i32>(lit.byteAt(10i32)))\n"
      "  print_line(if(v.equals(lit), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(v.equals(other), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(none.equals(blankView), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(none.empty(), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(lit.equalsText(\"hello world\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(lit.equalsText(\"hello worle\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(lit.equalsText(\"hello\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  s.pushByte(33u8)\n"
      "  [StringView] grown{s.view()}\n"
      "  print_line(grown.count())\n"
      "  print_line(convert<i32>(grown.byteAt(11i32)))\n"
      "  print_line(if(textOf(\"x\").equals(textOf(\"x\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  return(0i32)\n}\n";
}

const char *ViewExpected =
    "11\n11\n0\n0\n104\n100\n1\n0\n1\n1\n1\n0\n0\n12\n33\n1\n";
} // namespace

TEST_CASE("StringView over Strings and literals gives the same answers on the VM") {
  CHECK(vmOutput(viewProgram()) == ViewExpected);
}

TEST_CASE("StringView over Strings and literals gives the same answers through optexe") {
  CHECK(optexeOutput("view", viewProgram()) == ViewExpected);
}

TEST_CASE("StringView byteAt past the end stops the program") {
  const std::string source =
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
      "  [StringView] v{textOf(\"abc\")}\n  return(convert<i32>(v.byteAt(3i32)))\n}\n";
  ScriptEngine engine;
  const auto script = engine.compileSource("/view_fault.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  CHECK((!result.ok || (result.exitCode & 0xFF) != 0));
}

// ---- slicing, bytes() and UTF-8 validation ---------------------------------------------------------

namespace {
// Reference validator (RFC 3629): the offset of the first byte that starts invalid UTF-8, or -1.
int referenceUtf8ErrorOffset(const std::vector<int> &bytes) {
  const size_t n = bytes.size();
  size_t i = 0;
  while (i < n) {
    const int lead = bytes[i];
    size_t need = 0;
    int low = 0x80;
    int high = 0xBF;
    if (lead < 0x80) {
      need = 0;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
      need = 1;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      need = 2;
      if (lead == 0xE0) {
        low = 0xA0;
      }
      if (lead == 0xED) {
        high = 0x9F;
      }
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      need = 3;
      if (lead == 0xF0) {
        low = 0x90;
      }
      if (lead == 0xF4) {
        high = 0x8F;
      }
    } else {
      return static_cast<int>(i);
    }
    if (i + need >= n && need > 0) {
      return static_cast<int>(i);
    }
    for (size_t k = 1; k <= need; ++k) {
      const int next = bytes[i + k];
      if (next < (k == 1 ? low : 0x80) || next > (k == 1 ? high : 0xBF)) {
        return static_cast<int>(i);
      }
    }
    i += need + 1;
  }
  return -1;
}

const std::vector<std::vector<int>> Utf8Cases = {
    {},
    {0x61, 0x62, 0x63},
    {0xC3, 0xA9},
    {0xE4, 0xB8, 0x96},
    {0xF0, 0x9F, 0x98, 0x80},
    {0x80},
    {0x61, 0x80},
    {0xC0, 0x80},
    {0xC1, 0xBF},
    {0xC3},
    {0xE4, 0xB8},
    {0xED, 0xA0, 0x80},
    {0xED, 0x9F, 0xBF},
    {0xEE, 0x80, 0x80},
    {0xF4, 0x90, 0x80, 0x80},
    {0xF4, 0x8F, 0xBF, 0xBF},
    {0xF0, 0x80, 0x80, 0x80},
    {0xF0, 0x90, 0x80, 0x80},
    {0xE0, 0x80, 0x80},
    {0xE0, 0xA0, 0x80},
    {0x61, 0x62, 0xF8},
    {0x61, 0xE4, 0xB8, 0x96, 0xFF},
    {0xC2, 0x80, 0xDF, 0xBF, 0x7F},
    {0xF5, 0x80, 0x80, 0x80},
};

std::string utf8Program() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  int index = 0;
  for (const std::vector<int> &bytes : Utf8Cases) {
    const std::string name = "t" + std::to_string(index++);
    body += "  [String mut] " + name + "{/std/text/stringFrom(\"\")}\n";
    for (const int value : bytes) {
      body += "  " + name + ".pushByte(" + std::to_string(value) + "u8)\n";
    }
    body += "  print_line(utf8ErrorOffset(" + name + ".data, " + name + ".count()))\n";
  }
  return body + "  return(0i32)\n}\n";
}

std::string utf8Oracle() {
  std::string out;
  for (const std::vector<int> &bytes : Utf8Cases) {
    out += std::to_string(referenceUtf8ErrorOffset(bytes)) + "\n";
  }
  return out;
}

std::string sliceProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] s{/std/text/stringFrom(\"h\xC3\xA9llo, \xE4\xB8\x96\xE7\x95\x8C!\")}\n"
      "  [StringView] v{s.view()}\n"
      "  print_line(v.count())\n"
      "  for([i32 mut] i{0i32}, i <= v.count(), i++) {\n"
      "    print_line(if(v.isBoundary(i), then() { 1i32 }, else() { 0i32 }))\n"
      "  }\n"
      "  print_line(if(v.isBoundary(-1i32), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(v.isBoundary(v.count() + 1i32), then() { 1i32 }, else() { 0i32 }))\n"
      "  [StringView] head{v.slice(0i32, 3i32)}\n"
      "  [StringView] mid{s.slice(3i32, 7i32)}\n"
      "  [StringView] cjk{v.slice(8i32, 14i32)}\n"
      "  [StringView] empty{v.slice(5i32, 5i32)}\n"
      "  [StringView] all{v.slice(0i32, v.count())}\n"
      "  [StringView] nested{cjk.slice(3i32, 6i32)}\n"
      "  print_line(head.count())\n"
      "  print_line(if(head.equalsText(\"h\xC3\xA9\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(mid.equalsText(\"llo,\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(cjk.count())\n"
      "  print_line(nested.count())\n"
      "  print_line(if(nested.equalsText(\"\xE7\x95\x8C\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(empty.count())\n"
      "  print_line(if(all.equals(v), then() { 1i32 }, else() { 0i32 }))\n"
      "  [ReadSlice<u8>] raw{v.bytes()}\n"
      "  print_line(raw.count())\n"
      "  print_line(convert<i32>(raw.at(1i32)))\n"
      "  [StringView] checked{textFromBytes(raw)}\n"
      "  print_line(if(checked.equals(v), then() { 1i32 }, else() { 0i32 }))\n"
      "  return(0i32)\n}\n";
}

std::string sliceOracle() {
  const std::string text = "h\xC3\xA9llo, \xE4\xB8\x96\xE7\x95\x8C!";
  std::string out = std::to_string(text.size()) + "\n";
  for (size_t i = 0; i <= text.size(); ++i) {
    const bool continuation = i != 0 && i != text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80;
    out += continuation ? "0\n" : "1\n";
  }
  out += "0\n0\n";
  out += "3\n1\n1\n6\n3\n1\n0\n1\n";
  out += std::to_string(text.size()) + "\n195\n1\n";
  return out;
}

bool failsToRun(const std::string &source) {
  ScriptEngine engine;
  const auto script = engine.compileSource("/view_cut.prime", source);
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  return !result.ok || (result.exitCode & 0xFF) != 0;
}
} // namespace

TEST_CASE("utf8ErrorOffset agrees with a reference validator on the VM") {
  CHECK(vmOutput(utf8Program()) == utf8Oracle());
}

TEST_CASE("utf8ErrorOffset agrees with a reference validator through optexe") {
  CHECK(optexeOutput("utf8", utf8Program()) == utf8Oracle());
}

TEST_CASE("slices of a StringView cut between code points on the VM") {
  CHECK(vmOutput(sliceProgram()) == sliceOracle());
}

TEST_CASE("slices of a StringView cut between code points through optexe") {
  CHECK(optexeOutput("slices", sliceProgram()) == sliceOracle());
}

TEST_CASE("a slice inside a code point or outside the text stops the program") {
  const std::string head = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                           "  [StringView] v{textOf(\"h\xC3\xA9llo\")}\n";
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(0i32, 2i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(2i32, 4i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(0i32, 9i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(3i32, 1i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(-1i32, 1i32)}\n  return(cut.count())\n}\n"));
  CHECK_FALSE(failsToRun(head + "  [StringView] cut{v.slice(1i32, 3i32)}\n  return(0i32)\n}\n"));
}

TEST_CASE("textFromBytes stops the program on invalid UTF-8") {
  const std::string head = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                           "  [String mut] s{/std/text/stringFrom(\"ab\")}\n";
  CHECK(failsToRun(head + "  s.pushByte(255u8)\n  [StringView] v{textFromBytes(s.view().bytes())}\n  return(v.count())\n}\n"));
  CHECK_FALSE(failsToRun(head + "  [StringView] v{textFromBytes(s.view().bytes())}\n  return(0i32)\n}\n"));
}

// ---- search, comparison, trimming, pieces and lines ------------------------------------------------

namespace {
struct SearchCase {
  std::string hay;
  std::string needle;
};

std::vector<SearchCase> searchCases() {
  std::vector<SearchCase> cases = {{"", ""}, {"", "a"}, {"abc", ""}, {"abc", "abc"}, {"abc", "abcd"}, {"aaaa", "aa"},
                                   {"abababab", "abab"}, {"abababab", "baba"}, {"aaaaab", "aab"}, {"xyz", "z"}};
  uint32_t seed = 987654321u;
  const auto next = [&seed](uint32_t bound) {
    seed = seed * 1664525u + 1013904223u;
    return (seed >> 8) % bound;
  };
  for (int i = 0; i < 160; ++i) {
    std::string hay;
    std::string needle;
    const uint32_t alphabet = i % 3 == 0 ? 2 : (i % 3 == 1 ? 3 : 2);
    for (uint32_t k = 0, n = next(24); k < n; ++k) {
      hay.push_back(static_cast<char>('a' + next(alphabet)));
    }
    for (uint32_t k = 0, n = next(7); k < n; ++k) {
      needle.push_back(static_cast<char>('a' + next(alphabet)));
    }
    cases.push_back({hay, needle});
  }
  return cases;
}

int sign(int value) { return value < 0 ? -1 : (value > 0 ? 1 : 0); }

// The first cases are written out; the rest come from a generator the program runs itself (same LCG as
// searchCases), so the program stays small however many cases there are.
std::string searchProgram() {
  std::string body =
      "import /std/text/*\nimport /std/text/view/*\n\n"
      "[return<void> effects(io_out)]\n"
      "probe([/std/text/view/StringView] h, [/std/text/view/StringView] n) {\n"
      "  print_line(h.indexOf(n))\n"
      "  print_line(h.lastIndexOf(n))\n"
      "  print_line(h.compare(n))\n"
      "  print_line(if(h.startsWith(n), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(h.endsWith(n), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(h.contains(n), then() { 1i32 }, else() { 0i32 }))\n"
      "}\n\n"
      "[return<u64>]\nlcgStep([u64] seed) {\n"
      "  [u64] raw{seed * 1664525u64 + 1013904223u64}\n"
      "  return(raw - raw / 4294967296u64 * 4294967296u64)\n}\n\n"
      "[return<u64>]\nbelow([u64] seed, [u64] bound) {\n"
      "  [u64] value{seed / 256u64}\n"
      "  return(value - value / bound * bound)\n}\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  const std::vector<SearchCase> cases = searchCases();
  for (size_t i = 0; i < 10; ++i) {
    body += "  probe(textOf(\"" + cases[i].hay + "\"), textOf(\"" + cases[i].needle + "\"))\n";
  }
  body +=
      "  [String mut] hay{/std/text/stringFrom(\"\")}\n"
      "  [String mut] needle{/std/text/stringFrom(\"\")}\n"
      "  [u64 mut] seed{987654321u64}\n"
      "  for([i32 mut] round{0i32}, round < 160i32, round++) {\n"
      "    [u64] alphabet{if(round - round / 3i32 * 3i32 == 1i32, then() { 3u64 }, else() { 2u64 })}\n"
      "    hay.clear()\n"
      "    needle.clear()\n"
      "    seed = lcgStep(seed)\n"
      "    [u64] hayLength{below(seed, 24u64)}\n"
      "    for([u64 mut] k{0u64}, k < hayLength, k++) {\n"
      "      seed = lcgStep(seed)\n"
      "      hay.pushByte(convert<u8>(97u64 + below(seed, alphabet)))\n"
      "    }\n"
      "    seed = lcgStep(seed)\n"
      "    [u64] needleLength{below(seed, 7u64)}\n"
      "    for([u64 mut] k{0u64}, k < needleLength, k++) {\n"
      "      seed = lcgStep(seed)\n"
      "      needle.pushByte(convert<u8>(97u64 + below(seed, alphabet)))\n"
      "    }\n"
      "    probe(hay.view(), needle.view())\n"
      "  }\n"
      "  return(0i32)\n}\n";
  return body;
}

std::string searchOracle() {
  std::string out;
  for (const SearchCase &testCase : searchCases()) {
    const std::string &h = testCase.hay;
    const std::string &n = testCase.needle;
    const size_t found = h.find(n);
    const size_t last = h.rfind(n);
    out += std::to_string(found == std::string::npos ? -1 : static_cast<int>(found)) + "\n";
    out += std::to_string(last == std::string::npos ? -1 : static_cast<int>(last)) + "\n";
    out += std::to_string(sign(h.compare(n))) + "\n";
    out += std::string(h.starts_with(n) ? "1" : "0") + "\n";
    out += std::string(h.ends_with(n) ? "1" : "0") + "\n";
    out += std::string(found != std::string::npos ? "1" : "0") + "\n";
  }
  return out;
}

const std::vector<std::string> TrimCases = {"", " ", "a", " a", "a ", " a ", "\t\n a b \r\n", "   ", "x\ty", "\t \n"};
const std::vector<std::pair<std::string, std::string>> SplitCases = {
    {"a,b,c", ","}, {",a,,b,", ","}, {"", ","}, {"abc", ","}, {"a--b--c", "--"}, {"aaa", "aa"}, {"x", "x"}, {",", ","}};
const std::vector<std::string> LineCases = {"", "a", "a\n", "a\nb", "a\r\nb\r\n", "\n", "\n\n", "a\r\n\r\nb", "\r", "ab\ncd\r"};

std::string literal(const std::string &text) {
  std::string out = "\"";
  for (const char ch : text) {
    switch (ch) {
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    case '\v': out += "\\v"; break;
    case '\f': out += "\\f"; break;
    default: out += ch;
    }
  }
  return out + "\"";
}

std::string pieceProgram() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  int index = 0;
  for (const std::string &text : TrimCases) {
    const std::string name = "t" + std::to_string(index++);
    body += "  [StringView] " + name + "{textOf(" + literal(text) + ")}\n";
    body += "  print_line(" + name + ".trim().count())\n  print_line(" + name + ".trimStart().count())\n  print_line(" + name +
            ".trimEnd().count())\n";
  }
  for (const auto &[text, separator] : SplitCases) {
    const std::string name = "s" + std::to_string(index++);
    body += "  [StringView] " + name + "{textOf(" + literal(text) + ")}\n  [StringView] " + name + "sep{textOf(" + literal(separator) + ")}\n";
    body += "  print_line(" + name + ".splitCount(" + name + "sep))\n";
    body += "  for([i32 mut] i{0i32}, i < " + name + ".splitCount(" + name + "sep), i++) {\n";
    body += "    print_line(" + name + ".splitPart(" + name + "sep, i).count())\n  }\n";
  }
  for (const std::string &text : LineCases) {
    const std::string name = "l" + std::to_string(index++);
    body += "  [StringView] " + name + "{textOf(" + literal(text) + ")}\n";
    body += "  print_line(" + name + ".lineCount())\n";
    body += "  for([i32 mut] i{0i32}, i < " + name + ".lineCount(), i++) {\n";
    body += "    print_line(" + name + ".lineAt(i).count())\n  }\n";
  }
  // vertical tab (11) and form feed (12) have no literal escape: build them
  body += "  [String mut] ws{/std/text/stringFrom(\"\")}\n  ws.pushByte(11u8)\n  ws.pushByte(12u8)\n  ws.pushByte(97u8)\n"
          "  ws.pushByte(12u8)\n  ws.pushByte(11u8)\n  print_line(ws.view().trim().count())\n";
  return body + "  return(0i32)\n}\n";
}

std::string pieceOracle() {
  std::string out;
  const auto isSpace = [](char ch) { return ch == ' ' || (ch >= 9 && ch <= 13); };
  for (const std::string &text : TrimCases) {
    size_t start = 0;
    size_t end = text.size();
    while (start < end && isSpace(text[start])) {
      ++start;
    }
    size_t tail = text.size();
    while (tail > 0 && isSpace(text[tail - 1])) {
      --tail;
    }
    while (end > start && isSpace(text[end - 1])) {
      --end;
    }
    out += std::to_string(end - start) + "\n" + std::to_string(text.size() - start) + "\n" + std::to_string(tail) + "\n";
  }
  for (const auto &[text, separator] : SplitCases) {
    std::vector<size_t> sizes;
    size_t from = 0;
    for (;;) {
      const size_t at = text.find(separator, from);
      if (at == std::string::npos) {
        sizes.push_back(text.size() - from);
        break;
      }
      sizes.push_back(at - from);
      from = at + separator.size();
    }
    out += std::to_string(sizes.size()) + "\n";
    for (const size_t size : sizes) {
      out += std::to_string(size) + "\n";
    }
  }
  for (const std::string &text : LineCases) {
    std::vector<size_t> sizes;
    size_t from = 0;
    while (from < text.size()) {
      const size_t at = text.find('\n', from);
      size_t end = at == std::string::npos ? text.size() : at;
      if (end > from && text[end - 1] == '\r') {
        --end;
      }
      sizes.push_back(end - from);
      if (at == std::string::npos) {
        break;
      }
      from = at + 1;
    }
    out += std::to_string(sizes.size()) + "\n";
    for (const size_t size : sizes) {
      out += std::to_string(size) + "\n";
    }
  }
  out += "1\n";
  return out;
}

// A haystack of `hay` 'a' bytes and a needle of `needle` - 1 'a' bytes then 'b': a naive search compares about
// hay * needle bytes, the two-way search about 2 * hay.
std::string worstCaseProgram(int hay, int needle) {
  return "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  [String mut] h{/std/text/stringFrom(\"\")}\n"
         "  for([i32 mut] i{0i32}, i < " + std::to_string(hay) + "i32, i++) {\n    h.pushByte(97u8)\n  }\n"
         "  [String mut] n{/std/text/stringFrom(\"\")}\n"
         "  for([i32 mut] i{1i32}, i < " + std::to_string(needle) + "i32, i++) {\n    n.pushByte(97u8)\n  }\n"
         "  n.pushByte(98u8)\n"
         "  print_line(h.view().indexOf(n.view()))\n"
         "  h.pushByte(98u8)\n"
         "  print_line(h.view().indexOf(n.view()))\n"
         "  return(0i32)\n}\n";
}
} // namespace

TEST_CASE("search and comparison of a StringView agree with std::string on the VM") {
  CHECK(vmOutput(searchProgram()) == searchOracle());
}

TEST_CASE("search and comparison of a StringView agree with std::string through optexe") {
  CHECK(optexeOutput("search", searchProgram()) == searchOracle());
}

TEST_CASE("trim, split and lines of a StringView agree with the reference on the VM") {
  CHECK(vmOutput(pieceProgram()) == pieceOracle());
}

TEST_CASE("trim, split and lines of a StringView agree with the reference through optexe") {
  CHECK(optexeOutput("pieces", pieceProgram()) == pieceOracle());
}

TEST_CASE("searching a worst-case haystack takes linear time on the VM") {
  // The needle is 500 bytes; a quadratic search would compare about 100 million bytes here.
  const auto start = std::chrono::steady_clock::now();
  CHECK(vmOutput(worstCaseProgram(200000, 500)) == "-1\n" + std::to_string(200000 - 499) + "\n");
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(60));
}

TEST_CASE("searching a worst-case haystack takes linear time through optexe") {
  const auto start = std::chrono::steady_clock::now();
  CHECK(optexeOutput("worstcase", worstCaseProgram(2000000, 1000)) == "-1\n" + std::to_string(2000000 - 999) + "\n");
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(30));
}

// ---- numbers, code points and ASCII case ----------------------------------------------------------

namespace {
const std::vector<std::string> IntCases = {
    "", "0", "-0", "+0", "+", "-", "12", "-12", "+12", " 12", "12 ", "1_000", "00012", "1e3", "0x10",
    "9223372036854775807", "9223372036854775808", "-9223372036854775808", "-9223372036854775809",
    "2147483647", "2147483648", "-2147483648", "-2147483649", "99999999999999999999", "--1", "+-1", "1-"};

const std::vector<std::string> FloatCases = {
    "0", "-0", "1", "3.14159", ".5", "5.", "1e10", "1E-5", "+2.5e+3", "-7.25", "1e", "e5", "", ".", "-", "+.e1", "1.2.3",
    "1e999", "-1e999", "1e-999", "0.1", "123456789012345678901234567890", "inf", "nan", "0x1p3", " 1", "1 ", "12abc"};

std::string stringLiteral(const std::string &text) { return "\"" + text + "\""; }

std::string numberProgram() {
  std::string body =
      "import /std/text/*\nimport /std/text/view/*\n\n"
      "[return<void> effects(io_out)]\n"
      "probeInt([/std/text/view/StringView] v) {\n"
      "  [i32 mut] small{-5i32}\n"
      "  [i64 mut] wide{-5i64}\n"
      "  print_line(if(v.parseI32Into(small), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(small)\n"
      "  print_line(if(v.parseI64Into(wide), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(wide)\n"
      "}\n\n"
      "[return<void> effects(io_out)]\n"
      "probeFloat([/std/text/view/StringView] v) {\n"
      "  [f64 mut] value{-0.5f64}\n"
      "  print_line(if(v.parseF64Into(value), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(convert<i64>(value * 1000.0f64))\n"
      "}\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  for (const std::string &text : IntCases) {
    body += "  probeInt(textOf(" + stringLiteral(text) + "))\n";
  }
  for (const std::string &text : FloatCases) {
    body += "  probeFloat(textOf(" + stringLiteral(text) + "))\n";
  }
  return body + "  return(0i32)\n}\n";
}

std::string numberOracle() {
  std::string out;
  for (const std::string &text : IntCases) {
    std::string digits = text;
    bool valid = !digits.empty();
    if (valid && (digits[0] == '+')) {
      digits.erase(0, 1);
      valid = !digits.empty() && digits[0] != '-' && digits[0] != '+';
    }
    int64_t wide = 0;
    if (valid) {
      const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), wide);
      valid = parsed.ec == std::errc() && parsed.ptr == digits.data() + digits.size();
    }
    const bool fits32 = valid && wide >= INT32_MIN && wide <= INT32_MAX;
    out += std::string(fits32 ? "1" : "0") + "\n" + std::to_string(fits32 ? wide : -5) + "\n";
    out += std::string(valid ? "1" : "0") + "\n" + std::to_string(valid ? wide : -5) + "\n";
  }
  for (const std::string &text : FloatCases) {
    // The grammar the library accepts: [+-]? (digits+ ('.' digits*)? | '.' digits+) ([eE] [+-]? digits+)?
    size_t i = 0;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      ++i;
    }
    size_t whole = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
      ++i;
      ++whole;
    }
    size_t fraction = 0;
    if (i < text.size() && text[i] == '.') {
      ++i;
      while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        ++i;
        ++fraction;
      }
    }
    bool valid = whole + fraction > 0;
    if (valid && i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
      ++i;
      if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        ++i;
      }
      size_t exponent = 0;
      while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        ++i;
        ++exponent;
      }
      valid = exponent > 0;
    }
    valid = valid && i == text.size();
    double value = -0.5;
    if (valid) {
      char *end = nullptr;
      const double parsed = std::strtod(text.c_str(), &end);
      if (parsed - parsed == 0.0) {
        value = parsed;
      } else {
        valid = false;
      }
    }
    out += std::string(valid ? "1" : "0") + "\n" + std::to_string(static_cast<int64_t>(value * 1000.0)) + "\n";
  }
  return out;
}

const std::vector<std::string> CodePointCases = {"", "a", "h\xC3\xA9llo", "\xE4\xB8\x96\xE7\x95\x8C", "\xF0\x9F\x98\x80!", "\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xEF\xBF\xBF\xF4\x8F\xBF\xBF"};

std::string codePointProgram() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  int index = 0;
  for (const std::string &text : CodePointCases) {
    std::string escaped;
    for (const unsigned char ch : text) {
      escaped += ch < 0x80 ? std::string(1, static_cast<char>(ch)) : std::string(1, static_cast<char>(ch));
    }
    const std::string name = "v" + std::to_string(index++);
    body += "  [StringView] " + name + "{textOf(" + stringLiteral(escaped) + ")}\n";
    body += "  print_line(" + name + ".codePointCount())\n";
    body += "  for([i32 mut] at{0i32}, at < " + name + ".count(), at = at + " + name + ".codePointLength(at)) {\n";
    body += "    print_line(" + name + ".codePointAt(at))\n  }\n";
  }
  return body + "  return(0i32)\n}\n";
}

std::string codePointOracle() {
  std::string out;
  for (const std::string &text : CodePointCases) {
    std::vector<int32_t> points;
    for (size_t at = 0; at < text.size();) {
      const unsigned char lead = static_cast<unsigned char>(text[at]);
      const size_t width = lead < 0x80 ? 1 : (lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4));
      int32_t value = width == 1 ? lead : (lead & (0xFF >> (width + 1)));
      for (size_t k = 1; k < width; ++k) {
        value = value * 64 + (static_cast<unsigned char>(text[at + k]) & 0x3F);
      }
      points.push_back(value);
      at += width;
    }
    out += std::to_string(points.size()) + "\n";
    for (const int32_t point : points) {
      out += std::to_string(point) + "\n";
    }
  }
  return out;
}

std::string caseProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [StringView] mixed{textOf(\"Hello, W\xC3\x96rld! 123 zZ\")}\n"
      "  [String] lower{mixed.toAsciiLower()}\n"
      "  [String] upper{mixed.toAsciiUpper()}\n"
      "  print_line(lower.count())\n"
      "  for([i32 mut] i{0i32}, i < lower.count(), i++) {\n    print_line(convert<i32>(lower.byteAt(i)))\n  }\n"
      "  for([i32 mut] i{0i32}, i < upper.count(), i++) {\n    print_line(convert<i32>(upper.byteAt(i)))\n  }\n"
      "  [StringView] a{textOf(\"HeLLo\")}\n"
      "  print_line(if(a.equalsIgnoreAsciiCase(textOf(\"hELLO\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsIgnoreAsciiCase(textOf(\"hELL\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsIgnoreAsciiCase(textOf(\"hELLp\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(textOf(\"\xC3\x96\").equalsIgnoreAsciiCase(textOf(\"\xC3\xB6\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  return(0i32)\n}\n";
}

std::string caseOracle() {
  const std::string mixed = "Hello, W\xC3\x96rld! 123 zZ";
  std::string out = std::to_string(mixed.size()) + "\n";
  for (const unsigned char ch : mixed) {
    out += std::to_string(ch >= 'A' && ch <= 'Z' ? ch + 32 : ch) + "\n";
  }
  for (const unsigned char ch : mixed) {
    out += std::to_string(ch >= 'a' && ch <= 'z' ? ch - 32 : ch) + "\n";
  }
  out += "1\n0\n0\n0\n";
  return out;
}
} // namespace

TEST_CASE("integer and float parsing of a StringView agrees with C++ on the VM") {
  CHECK(vmOutput(numberProgram()) == numberOracle());
}

TEST_CASE("integer and float parsing of a StringView agrees with C++ through optexe") {
  CHECK(optexeOutput("numbers", numberProgram()) == numberOracle());
}

TEST_CASE("code points of a StringView agree with a reference decoder on the VM") {
  CHECK(vmOutput(codePointProgram()) == codePointOracle());
}

TEST_CASE("code points of a StringView agree with a reference decoder through optexe") {
  CHECK(optexeOutput("codepoints", codePointProgram()) == codePointOracle());
}

TEST_CASE("ASCII case mapping leaves bytes 128 and up alone on the VM") {
  CHECK(vmOutput(caseProgram()) == caseOracle());
}

TEST_CASE("ASCII case mapping leaves bytes 128 and up alone through optexe") {
  CHECK(optexeOutput("ascii_case", caseProgram()) == caseOracle());
}

// ---- the rest of std::string's editing members ------------------------------------------------------------------

namespace {
constexpr int MutationSteps = 400;

// Seeded edits with the members added for std::string parity; ASCII only, so every cut is a code point boundary.
std::string mutationProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\n\n"
      "[return<u64>]\nlcgStep([u64] seed) {\n"
      "  [u64] raw{seed * 1664525u64 + 1013904223u64}\n"
      "  return(raw - raw / 4294967296u64 * 4294967296u64)\n}\n\n"
      "[return<i32>]\nbelow([u64] seed, [i32] bound) {\n"
      "  [i32] value{convert<i32>(seed / 256u64 - seed / 256u64 / 1073741824u64 * 1073741824u64)}\n"
      "  return(value - value / bound * bound)\n}\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] s{stringFilled(3i32, 120u8)}\n"
      "  [String mut] other{/std/text/stringFrom(\"SWAPPED\")}\n"
      "  [u64 mut] seed{424242u64}\n"
      "  [i32 mut] h{17i32}\n"
      "  for([i32 mut] step{0i32}, step < " + std::to_string(MutationSteps) + "i32, step++) {\n"
      "    seed = lcgStep(seed)\n    [i32] op{below(seed, 10i32)}\n"
      "    seed = lcgStep(seed)\n    [i32] a{below(seed, s.count() + 1i32)}\n"
      "    seed = lcgStep(seed)\n    [i32] b{a + below(seed, s.count() - a + 1i32)}\n"
      "    seed = lcgStep(seed)\n    [i32] n{below(seed, 6i32)}\n"
      "    seed = lcgStep(seed)\n    [u8] ch{convert<u8>(97i32 + below(seed, 26i32))}\n"
      "    if(op == 0i32) {\n      s.appendFilled(n, ch)\n    }\n"
      "    if(op == 1i32) {\n      s.insertFilled(a, n, ch)\n    }\n"
      "    if(op == 2i32) {\n      s.insertByte(a, ch)\n    }\n"
      "    if(op == 3i32) {\n      if(s.count() > 0i32) {\n        s.popByte()\n      }\n    }\n"
      "    if(op == 4i32) {\n      s.resize(n * 5i32, ch)\n    }\n"
      "    if(op == 5i32) {\n      s.replaceFilled(a, b, n, ch)\n    }\n"
      "    if(op == 6i32) {\n      if(s.count() > 0i32) {\n"
      "        h = h * 31i32 + convert<i32>(s.front())\n        h = h * 31i32 + convert<i32>(s.back())\n      }\n    }\n"
      "    if(op == 7i32) {\n      s.swap(other)\n      h = h * 31i32 + s.count()\n      h = h * 31i32 + other.count()\n"
      "      s.swap(other)\n    }\n"
      "    if(op == 8i32) {\n      s.assignFilled(n, ch)\n    }\n"
      "    if(op == 9i32) {\n      s.erase(a, b)\n    }\n"
      "    h = h * 31i32 + s.count()\n"
      "  }\n"
      "  for([i32 mut] i{0i32}, i < s.count(), i++) {\n    h = h * 31i32 + convert<i32>(s.byteAt(i))\n  }\n"
      "  print_line(h)\n"
      "  print_line(s.count())\n"
      "  return(0i32)\n}\n";
}

std::string mutationOracle() {
  std::string s(3, 'x');
  std::string other = "SWAPPED";
  uint32_t seed = 424242u;
  int32_t h = 17;
  const auto step = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed; };
  const auto below = [](uint32_t seedValue, int32_t bound) {
    const int32_t value = static_cast<int32_t>((seedValue / 256u) % 1073741824u);
    return value - value / bound * bound;
  };
  for (int i = 0; i < MutationSteps; ++i) {
    const int32_t op = below(step(), 10);
    const int32_t a = below(step(), static_cast<int32_t>(s.size()) + 1);
    const int32_t b = a + below(step(), static_cast<int32_t>(s.size()) - a + 1);
    const int32_t n = below(step(), 6);
    const char ch = static_cast<char>(97 + below(step(), 26));
    switch (op) {
    case 0: s.append(static_cast<size_t>(n), ch); break;
    case 1: s.insert(static_cast<size_t>(a), static_cast<size_t>(n), ch); break;
    case 2: s.insert(static_cast<size_t>(a), 1, ch); break;
    case 3:
      if (!s.empty()) {
        s.pop_back();
      }
      break;
    case 4: s.resize(static_cast<size_t>(n * 5), ch); break;
    case 5: s.replace(static_cast<size_t>(a), static_cast<size_t>(b - a), static_cast<size_t>(n), ch); break;
    case 6:
      if (!s.empty()) {
        h = wrapAdd(wrapMul(h, 31), static_cast<unsigned char>(s.front()));
        h = wrapAdd(wrapMul(h, 31), static_cast<unsigned char>(s.back()));
      }
      break;
    case 7:
      h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(other.size()));
      h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(s.size()));
      break;
    case 8: s.assign(static_cast<size_t>(n), ch); break;
    default: s.erase(static_cast<size_t>(a), static_cast<size_t>(b - a)); break;
    }
    h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(s.size()));
  }
  h = foldText(s, h);
  return std::to_string(h) + "\n" + std::to_string(s.size()) + "\n";
}

std::string popProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] s{/std/text/stringFrom(\"a\xC3\xA9\xE4\xB8\x96\xF0\x9F\x98\x80z\")}\n"
      "  [i32 mut] guard{0i32}\n"
      "  while(s.count() > 0i32 && guard < 10i32) {\n"
      "    print_line(s.pop())\n    print_line(s.count())\n    guard = guard + 1i32\n  }\n"
      "  [String mut] t{/std/text/stringFrom(\"h\xC3\xA9llo\")}\n"
      "  [String mut] copy{/std/text/stringFrom(\"\")}\n"
      "  copy.resize(7i32, 46u8)\n"
      "  t.view().copyTo(copy.data)\n"
      "  print_line(copy.count())\n"
      "  for([i32 mut] i{0i32}, i < copy.count(), i++) {\n    print_line(convert<i32>(copy.byteAt(i)))\n  }\n"
      "  return(0i32)\n}\n";
}

std::string popOracle() {
  // a, e-acute (U+E9), CJK (U+4E16), emoji (U+1F600), z: 11 bytes, popped from the end: 10, 6, 3, 1, 0 bytes left.
  std::string out = "122\n10\n128512\n6\n19990\n3\n233\n1\n97\n0\n";
  const std::string text = "h\xC3\xA9llo";
  out += "7\n";
  for (size_t i = 0; i < 7; ++i) {
    out += std::to_string(i < text.size() ? static_cast<unsigned char>(text[i]) : 46) + "\n";
  }
  return out;
}

// The Unicode standard's own example (Table 3-8, recommended replacement of maximal subparts): 61 F1 80 80 E1 80 C2 62
// 80 63 80 BF 64 becomes a U+FFFD U+FFFD U+FFFD b U+FFFD c U+FFFD U+FFFD d.
std::string lossyProgram() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  const std::vector<std::vector<int>> vectors = {
      {0x61, 0xF1, 0x80, 0x80, 0xE1, 0x80, 0xC2, 0x62, 0x80, 0x63, 0x80, 0xBF, 0x64},
      {0x68, 0xC3, 0xA9, 0x21},
      {0xFF, 0xFE},
      {0xE4, 0xB8},
      {0xF0, 0x9F, 0x98},
      {0xED, 0xA0, 0x80},
      {},
  };
  int index = 0;
  for (const std::vector<int> &bytes : vectors) {
    const std::string name = "v" + std::to_string(index++);
    body += "  [String mut] " + name + "{/std/text/stringFrom(\"\")}\n";
    for (const int value : bytes) {
      body += "  " + name + ".pushByte(" + std::to_string(value) + "u8)\n";
    }
    body += "  [String] fixed" + name + "{stringFromUtf8Lossy(" + name + ".view().bytes())}\n";
    body += "  print_line(fixed" + name + ".count())\n";
    body += "  for([i32 mut] i{0i32}, i < fixed" + name + ".count(), i++) {\n    print_line(convert<i32>(fixed" + name +
            ".byteAt(i)))\n  }\n";
  }
  return body + "  return(0i32)\n}\n";
}

std::string lossyOracle() {
  const auto emit = [](const std::string &text) {
    std::string out = std::to_string(text.size()) + "\n";
    for (const unsigned char ch : text) {
      out += std::to_string(ch) + "\n";
    }
    return out;
  };
  const std::string replacement = "\xEF\xBF\xBD";
  std::string out;
  out += emit("a" + replacement + replacement + replacement + "b" + replacement + "c" + replacement + replacement + "d");
  out += emit("h\xC3\xA9!");
  out += emit(replacement + replacement);
  out += emit(replacement);
  out += emit(replacement);
  out += emit(replacement + replacement + replacement);
  out += emit("");
  return out;
}
} // namespace

TEST_CASE("the std::string editing members agree with std::string on the VM") {
  CHECK(vmOutput(mutationProgram()) == mutationOracle());
}

TEST_CASE("the std::string editing members agree with std::string through optexe") {
  CHECK(optexeOutput("mutations", mutationProgram()) == mutationOracle());
}

TEST_CASE("pop and copyTo return code points and bytes on the VM") {
  CHECK(vmOutput(popProgram()) == popOracle());
}

TEST_CASE("pop and copyTo return code points and bytes through optexe") {
  CHECK(optexeOutput("pops", popProgram()) == popOracle());
}

TEST_CASE("lossy UTF-8 decoding replaces maximal invalid parts on the VM") {
  CHECK(vmOutput(lossyProgram()) == lossyOracle());
}

TEST_CASE("lossy UTF-8 decoding replaces maximal invalid parts through optexe") {
  CHECK(optexeOutput("lossy", lossyProgram()) == lossyOracle());
}

TEST_CASE("resize inside a code point, popByte and pop on empty text stop the program") {
  const std::string head = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                           "  [String mut] s{/std/text/stringFrom(\"h\xC3\xA9\")}\n";
  CHECK(failsToRun(head + "  s.resize(2i32, 46u8)\n  return(s.count())\n}\n"));
  CHECK_FALSE(failsToRun(head + "  s.resize(1i32, 46u8)\n  return(0i32)\n}\n"));
  CHECK(failsToRun("import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                   "  [String mut] s{/std/text/stringFrom(\"\")}\n  s.popByte()\n  return(0i32)\n}\n"));
  CHECK(failsToRun("import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                   "  [String mut] s{/std/text/stringFrom(\"\")}\n  return(s.pop())\n}\n"));
  CHECK(failsToRun("import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                   "  [String mut] s{/std/text/stringFrom(\"\")}\n  return(convert<i32>(s.front()))\n}\n"));
}

// ---- byte-set search and replaceAll ----------------------------------------------------------------------------

namespace {
constexpr int SetRounds = 200;

// Random haystacks, sets and positions generated by the program itself (same LCG as the oracle).
std::string setProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\n\n"
      "[return<u64>]\nlcgStep([u64] seed) {\n"
      "  [u64] raw{seed * 1664525u64 + 1013904223u64}\n"
      "  return(raw - raw / 4294967296u64 * 4294967296u64)\n}\n\n"
      "[return<u64>]\nbelow([u64] seed, [u64] bound) {\n"
      "  [u64] value{seed / 256u64}\n"
      "  return(value - value / bound * bound)\n}\n\n"
      "[return<void> effects(io_out)]\nprobe([/std/text/view/StringView] h, [/std/text/view/StringView] set, [i32] pos) {\n"
      "  print_line(h.findFirstOf(set))\n  print_line(h.findFirstNotOf(set))\n"
      "  print_line(h.findLastOf(set))\n  print_line(h.findLastNotOf(set))\n"
      "  print_line(h.findFirstOfFrom(set, pos))\n  print_line(h.findFirstNotOfFrom(set, pos))\n"
      "  print_line(h.findLastOfFrom(set, pos))\n  print_line(h.findLastNotOfFrom(set, pos))\n}\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] hay{/std/text/stringFrom(\"\")}\n"
      "  [String mut] set{/std/text/stringFrom(\"\")}\n"
      "  [u64 mut] seed{13579u64}\n"
      "  for([i32 mut] round{0i32}, round < " + std::to_string(SetRounds) + "i32, round++) {\n"
      "    hay.clear()\n    set.clear()\n"
      "    seed = lcgStep(seed)\n    [u64] hayLength{below(seed, 14u64)}\n"
      "    for([u64 mut] k{0u64}, k < hayLength, k++) {\n"
      "      seed = lcgStep(seed)\n      hay.pushByte(convert<u8>(97u64 + below(seed, 4u64)))\n    }\n"
      "    seed = lcgStep(seed)\n    [u64] setLength{below(seed, 4u64)}\n"
      "    for([u64 mut] k{0u64}, k < setLength, k++) {\n"
      "      seed = lcgStep(seed)\n      set.pushByte(convert<u8>(97u64 + below(seed, 4u64)))\n    }\n"
      "    seed = lcgStep(seed)\n    [i32] pos{convert<i32>(below(seed, 17u64))}\n"
      "    probe(hay.view(), set.view(), pos)\n"
      "  }\n"
      "  return(0i32)\n}\n";
}

std::string setOracle() {
  uint32_t seed = 13579u;
  const auto step = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed; };
  const auto below = [](uint32_t value, uint32_t bound) {
    const uint32_t v = value / 256u;
    return v - v / bound * bound;
  };
  const auto idx = [](size_t found) { return found == std::string::npos ? -1 : static_cast<int>(found); };
  std::string out;
  for (int round = 0; round < SetRounds; ++round) {
    std::string hay;
    std::string set;
    for (uint32_t k = 0, n = below(step(), 14); k < n; ++k) {
      hay.push_back(static_cast<char>('a' + below(step(), 4)));
    }
    for (uint32_t k = 0, n = below(step(), 4); k < n; ++k) {
      set.push_back(static_cast<char>('a' + below(step(), 4)));
    }
    const size_t pos = below(step(), 17);
    out += std::to_string(idx(hay.find_first_of(set))) + "\n" + std::to_string(idx(hay.find_first_not_of(set))) + "\n";
    out += std::to_string(idx(hay.find_last_of(set))) + "\n" + std::to_string(idx(hay.find_last_not_of(set))) + "\n";
    out += std::to_string(idx(hay.find_first_of(set, pos))) + "\n" + std::to_string(idx(hay.find_first_not_of(set, pos))) + "\n";
    out += std::to_string(idx(hay.find_last_of(set, pos))) + "\n" + std::to_string(idx(hay.find_last_not_of(set, pos))) + "\n";
  }
  return out;
}

struct ReplaceCase {
  std::string hay;
  std::string from;
  std::string to;
};

const std::vector<ReplaceCase> ReplaceCases = {
    {"", "a", "b"}, {"abc", "", "x"}, {"aaaa", "aa", "b"}, {"aaa", "aa", "b"}, {"banana", "an", ""}, {"banana", "a", "aa"},
    {"hello world", "o", "00"}, {"abcabc", "abc", "abc"}, {"xyz", "q", "r"}, {"a,b,,c", ",", ", "}, {"ababab", "bab", "-"}, {"aXbXc", "X", "XX"}};

std::string replaceProgram() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  int index = 0;
  for (const ReplaceCase &testCase : ReplaceCases) {
    const std::string name = "r" + std::to_string(index++);
    body += "  [StringView] " + name + "h{textOf(\"" + testCase.hay + "\")}\n";
    body += "  [String] " + name + "{" + name + "h.replaceAll(textOf(\"" + testCase.from + "\"), textOf(\"" + testCase.to + "\"))}\n";
    body += "  print_line(" + name + ".count())\n";
    body += "  for([i32 mut] i{0i32}, i < " + name + ".count(), i++) {\n    print_line(convert<i32>(" + name + ".byteAt(i)))\n  }\n";
  }
  return body + "  return(0i32)\n}\n";
}

std::string replaceOracle() {
  std::string out;
  for (const ReplaceCase &testCase : ReplaceCases) {
    std::string result;
    if (testCase.from.empty()) {
      result = testCase.hay;
    } else {
      size_t cursor = 0;
      for (;;) {
        const size_t hit = testCase.hay.find(testCase.from, cursor);
        if (hit == std::string::npos) {
          result += testCase.hay.substr(cursor);
          break;
        }
        result += testCase.hay.substr(cursor, hit - cursor) + testCase.to;
        cursor = hit + testCase.from.size();
      }
    }
    out += std::to_string(result.size()) + "\n";
    for (const unsigned char ch : result) {
      out += std::to_string(ch) + "\n";
    }
  }
  return out;
}
} // namespace

TEST_CASE("find_first_of and its relatives agree with std::string on the VM") {
  CHECK(vmOutput(setProgram()) == setOracle());
}

TEST_CASE("find_first_of and its relatives agree with std::string through optexe") {
  CHECK(optexeOutput("sets", setProgram()) == setOracle());
}

TEST_CASE("replaceAll agrees with a reference on the VM") {
  CHECK(vmOutput(replaceProgram()) == replaceOracle());
}

TEST_CASE("replaceAll agrees with a reference through optexe") {
  CHECK(optexeOutput("replace_all", replaceProgram()) == replaceOracle());
}

// ---- whole-file text (readTextInto, writeText, appendText) -----------------------------------------------------

namespace {
struct FileCase {
  std::string name;
  std::string content;
};

std::string largeText() {
  std::string text;
  for (int i = 0; i < 30000; ++i) {
    text += "line " + std::to_string(i) + " \xC3\xA9\xE4\xB8\x96\n";
  }
  return text;
}

std::vector<FileCase> fileCases() {
  return {{"empty", ""},
          {"ascii", "hello"},
          {"multibyte", "h\xC3\xA9llo \xE4\xB8\x96\xE7\x95\x8C \xF0\x9F\x98\x80"},
          {"lines", "one\r\ntwo\nthree\n"},
          {"invalid", "ab\xFF" "cd"},
          {"truncated", "ab\xE4\xB8"},
          {"large", largeText()}};
}

void writeBytes(const std::string &path, const std::string &content) {
  std::ofstream(path, std::ios::binary) << content;
}

std::string fileProgram(const std::string &directory, const std::string &outPath) {
  std::string body =
      "import /std/text/*\nimport /std/text/view/*\n\n"
      "[return<void> effects(io_out, file_read, heap_alloc)]\n"
      "probe([/std/text/view/StringView] path) {\n"
      "  [String mut] text{/std/text/stringFrom(\"untouched\")}\n"
      "  print_line(readTextInto(path, text))\n"
      "  print_line(text.count())\n"
      "  [i32 mut] h{17i32}\n"
      "  for([i32 mut] i{0i32}, i < text.count(), i++) {\n    h = h * 31i32 + convert<i32>(text.byteAt(i))\n  }\n"
      "  print_line(h)\n}\n\n"
      "[return<int> effects(heap_alloc, io_out, file_read, file_write)]\nmain() {\n";
  for (const FileCase &testCase : fileCases()) {
    body += "  probe(textOf(\"" + directory + "/" + testCase.name + ".txt\"))\n";
  }
  body += "  probe(textOf(\"" + directory + "/missing.txt\"))\n";
  body += "  print_line(writeText(textOf(\"" + outPath + "\"), textOf(\"first \xC3\xA9\\n\")))\n";
  body += "  print_line(appendText(textOf(\"" + outPath + "\"), textOf(\"second\")))\n";
  body += "  print_line(writeText(textOf(\"" + directory + "/no_such_dir/out.txt\"), textOf(\"x\")))\n";
  body += "  print_line(writeText(textOf(\"" + directory + "/empty_out.txt\"), textOf(\"\")))\n";
  return body + "  return(0i32)\n}\n";
}

std::string fileOracle() {
  std::string out;
  for (const FileCase &testCase : fileCases()) {
    const bool valid = referenceUtf8ErrorOffset([&testCase] {
      std::vector<int> bytes;
      for (const unsigned char ch : testCase.content) {
        bytes.push_back(ch);
      }
      return bytes;
    }()) < 0;
    const std::string &shown = valid ? testCase.content : std::string("untouched");
    out += std::string(valid ? "0" : "-1") + "\n" + std::to_string(shown.size()) + "\n" + std::to_string(foldText(shown, 17)) + "\n";
  }
  out += "2\n9\n" + std::to_string(foldText("untouched", 17)) + "\n";
  out += "0\n0\n2\n0\n";
  return out;
}

void checkFiles(const std::string &kind, const std::function<std::string(const std::string &)> &run) {
  const std::string directory = scratch("files_" + kind);
  std::filesystem::create_directories(directory);
  for (const FileCase &testCase : fileCases()) {
    writeBytes(directory + "/" + testCase.name + ".txt", testCase.content);
  }
  const std::string outPath = directory + "/out.txt";
  std::filesystem::remove(outPath);
  CHECK(run(fileProgram(directory, outPath)) == fileOracle());
  CHECK(readFile(outPath) == "first \xC3\xA9\nsecond");
  CHECK(readFile(directory + "/empty_out.txt").empty());
  CHECK_FALSE(std::filesystem::exists(directory + "/no_such_dir"));
}
} // namespace

TEST_CASE("whole-file text reads and writes agree with C++ on the VM") {
  checkFiles("vm", [](const std::string &source) { return vmOutput(source); });
}

TEST_CASE("whole-file text reads and writes agree with C++ through optexe") {
  checkFiles("optexe", [](const std::string &source) { return optexeOutput("filetext", source); });
}

// ---- numbers as text and in any radix --------------------------------------------------------------------------

namespace {
const std::vector<std::string> FloatTexts = {"0.1", "100000", "1e20", "1.5e-7", "123456789.125", "0.3333333333333333", "5e-324",
                                             "1.7976931348623157e308", "0", "-0", "1e21", "1e-5", "123", "2.5", "-7.75", "1e15",
                                             "123456789012345678", "0.000001", "16777216", "3.4028234663852886e38"};
const std::vector<int64_t> IntValues = {0, 1, -1, 42, -42, 2147483647, -2147483648LL, 4294967296LL, 9223372036854775807LL,
                                        INT64_MIN, 255, 1000000007};
const std::vector<int> Radixes = {2, 3, 8, 10, 16, 36};

// Folds the text of a String into a number the program prints with its length.
const char *FoldHelper =
    "[return<void> effects(io_out)]\nshow([/std/text/String] s) {\n"
    "  [i32 mut] h{17i32}\n"
    "  for([i32 mut] i{0i32}, i < s.count(), i++) {\n    h = h * 31i32 + convert<i32>(s.byteAt(i))\n  }\n"
    "  print_line(s.count())\n  print_line(h)\n}\n\n";

std::string showText(const std::string &text) {
  return std::to_string(text.size()) + "\n" + std::to_string(foldText(text, 17)) + "\n";
}

// One call site per kind of conversion, inside loops; the values are picked by if chains, so the program stays small.
std::string formatProgram() {
  std::string body = std::string("import /std/text/*\nimport /std/text/view/*\n\n") + FoldHelper + "[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  // integers: every value in decimal and in every radix
  body += "  for([i32 mut] vi{0i32}, vi < " + std::to_string(IntValues.size()) + "i32, vi++) {\n    [i64 mut] value{0i64}\n";
  for (size_t i = 0; i < IntValues.size(); ++i) {
    body += "    if(vi == " + std::to_string(i) + "i32) {\n      value = " + std::to_string(IntValues[i]) + "i64\n    }\n";
  }
  body += "    show(stringFromI64(value))\n";
  body += "    for([i32 mut] ri{0i32}, ri < " + std::to_string(Radixes.size()) + "i32, ri++) {\n      [i32 mut] radix{10i32}\n";
  for (size_t i = 0; i < Radixes.size(); ++i) {
    body += "      if(ri == " + std::to_string(i) + "i32) {\n        radix = " + std::to_string(Radixes[i]) + "i32\n      }\n";
  }
  body += "      show(stringFromI64Radix(value, radix))\n    }\n  }\n";
  body += "  show(stringFromI32(-2147483647i32))\n  show(stringFromU64(18446744073709551615u64))\n";
  body += "  for([i32 mut] ri{0i32}, ri < " + std::to_string(Radixes.size()) + "i32, ri++) {\n    [i32 mut] radix{10i32}\n";
  for (size_t i = 0; i < Radixes.size(); ++i) {
    body += "    if(ri == " + std::to_string(i) + "i32) {\n      radix = " + std::to_string(Radixes[i]) + "i32\n    }\n";
  }
  body += "    show(stringFromU64Radix(18446744073709551615u64, radix))\n  }\n";
  body += "  show(stringFromBool(true))\n  show(stringFromBool(false))\n";
  // floats: every text in every style
  body += "  for([i32 mut] fi{0i32}, fi < " + std::to_string(FloatTexts.size()) + "i32, fi++) {\n    [StringView mut] source{textOf(\"0\")}\n";
  for (size_t i = 0; i < FloatTexts.size(); ++i) {
    body += "    if(fi == " + std::to_string(i) + "i32) {\n      source = textOf(\"" + FloatTexts[i] + "\")\n    }\n";
  }
  body += "    [f64] value{source.parseF64Or(-1.0f64)}\n";
  body += "    for([i32 mut] si{0i32}, si < 8i32, si++) {\n      [i32 mut] style{0i32}\n      [i32 mut] precision{0i32}\n";
  const int styles[8][2] = {{0, 0}, {3, 0}, {1, 0}, {1, 2}, {1, 10}, {2, 0}, {2, 3}, {2, 15}};
  for (int i = 0; i < 8; ++i) {
    body += "      if(si == " + std::to_string(i) + "i32) {\n        style = " + std::to_string(styles[i][0]) + "i32\n        precision = " +
            std::to_string(styles[i][1]) + "i32\n      }\n";
  }
  body += "      [String mut] made{stringWithCapacity(0i32)}\n      made.appendF64Style(value, style, precision)\n      show(made)\n    }\n  }\n";
  return body + "  return(0i32)\n}\n";
}

std::string toChars(int64_t value, int radix) {
  char buffer[80];
  const auto made = std::to_chars(buffer, buffer + sizeof(buffer), value, radix);
  return std::string(buffer, made.ptr);
}

std::string toCharsUnsigned(uint64_t value, int radix) {
  char buffer[80];
  const auto made = std::to_chars(buffer, buffer + sizeof(buffer), value, radix);
  return std::string(buffer, made.ptr);
}

template <class... Args> std::string floatChars(double value, Args... args) {
  char buffer[1200];
  const auto made = std::to_chars(buffer, buffer + sizeof(buffer), value, args...);
  return std::string(buffer, made.ptr);
}

std::string formatOracle() {
  std::string out;
  for (const int64_t value : IntValues) {
    out += showText(toChars(value, 10));
    for (const int radix : Radixes) {
      out += showText(toChars(value, radix));
    }
  }
  out += showText("-2147483647") + showText("18446744073709551615");
  for (const int radix : Radixes) {
    out += showText(toCharsUnsigned(UINT64_MAX, radix));
  }
  out += showText("true") + showText("false");
  for (const std::string &text : FloatTexts) {
    const double value = std::strtod(text.c_str(), nullptr);
    out += showText(floatChars(value));
    char buffer[64];
    const auto narrow = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<float>(value));
    out += showText(std::string(buffer, narrow.ptr));
    out += showText(floatChars(value, std::chars_format::fixed, 0));
    out += showText(floatChars(value, std::chars_format::fixed, 2));
    out += showText(floatChars(value, std::chars_format::fixed, 10));
    out += showText(floatChars(value, std::chars_format::scientific, 0));
    out += showText(floatChars(value, std::chars_format::scientific, 3));
    out += showText(floatChars(value, std::chars_format::scientific, 15));
  }
  return out;
}

struct RadixParse {
  std::string text;
  int radix;
  bool isSigned;
};

const std::vector<RadixParse> RadixParses = {
    {"ff", 16, true},   {"FF", 16, true},    {"-ff", 16, true},   {"+ff", 16, true},    {"zz", 36, true},   {"ZZ", 36, false},
    {"12", 2, true},    {"102", 2, false},   {"777", 8, true},    {"", 10, true},       {"-", 10, true},    {"+", 10, true},
    {"-1", 10, false},  {"+1", 10, false},   {"0", 10, false},    {"18446744073709551615", 10, false},
    {"18446744073709551616", 10, false},     {"-9223372036854775808", 10, true},  {"9223372036854775808", 10, true},
    {"7fffffffffffffff", 16, true},          {"8000000000000000", 16, true},      {"ffffffffffffffff", 16, false},
    {"10000000000000000", 16, false},        {"1 ", 10, true},    {" 1", 10, true}, {"1_0", 10, true}, {"0x10", 16, true}};

std::string radixProgram() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  int index = 0;
  for (const RadixParse &testCase : RadixParses) {
    const std::string name = "p" + std::to_string(index++);
    body += "  [StringView] " + name + "{textOf(\"" + testCase.text + "\")}\n";
    if (testCase.isSigned) {
      body += "  [i64 mut] " + name + "v{-5i64}\n  print_line(if(" + name + ".parseI64RadixInto(" + std::to_string(testCase.radix) + "i32, " + name +
              "v), then() { 1i32 }, else() { 0i32 }))\n  print_line(" + name + "v)\n";
    } else {
      body += "  [u64 mut] " + name + "v{5u64}\n  print_line(if(" + name + ".parseU64RadixInto(" + std::to_string(testCase.radix) + "i32, " + name +
              "v), then() { 1i32 }, else() { 0i32 }))\n  print_line(" + name + "v)\n";
    }
  }
  return body + "  return(0i32)\n}\n";
}

std::string radixOracle() {
  std::string out;
  for (const RadixParse &testCase : RadixParses) {
    std::string digits = testCase.text;
    bool valid = !digits.empty();
    if (valid && testCase.isSigned && digits[0] == '+') {
      digits.erase(0, 1);
      valid = !digits.empty() && digits[0] != '-' && digits[0] != '+';
    }
    if (testCase.isSigned) {
      int64_t value = 0;
      if (valid) {
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value, testCase.radix);
        valid = parsed.ec == std::errc() && parsed.ptr == digits.data() + digits.size();
      }
      out += std::string(valid ? "1" : "0") + "\n" + std::to_string(valid ? value : -5) + "\n";
    } else {
      uint64_t value = 0;
      if (valid) {
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value, testCase.radix);
        valid = parsed.ec == std::errc() && parsed.ptr == digits.data() + digits.size();
      }
      out += std::string(valid ? "1" : "0") + "\n" + std::to_string(valid ? value : 5ULL) + "\n";
    }
  }
  return out;
}
} // namespace

TEST_CASE("number to text in every style agrees with std::to_chars on the VM") {
  CHECK(vmOutput(formatProgram()) == formatOracle());
}

TEST_CASE("number to text in every style agrees with std::to_chars through optexe") {
  CHECK(optexeOutput("formats", formatProgram()) == formatOracle());
}

TEST_CASE("integer parsing in any radix and unsigned parsing agree with std::from_chars on the VM") {
  CHECK(vmOutput(radixProgram()) == radixOracle());
}

TEST_CASE("integer parsing in any radix and unsigned parsing agree with std::from_chars through optexe") {
  CHECK(optexeOutput("radix", radixProgram()) == radixOracle());
}

// ---- format, join and owned split ------------------------------------------------------------------------------

namespace {
struct FormatCase {
  std::string pattern;
  std::vector<std::string> args;
};

const std::vector<FormatCase> FormatCases = {
    {"{} is {}", {"x", "yy"}},
    {"{1} {0} {1}", {"a", "b"}},
    {"{{}} {}", {"z"}},
    {"[{:>6}]", {"ab"}},
    {"[{:<6}]", {"ab"}},
    {"[{:^6}]", {"ab"}},
    {"[{:^7}]", {"ab"}},
    {"[{:*^7}]", {"abc"}},
    {"[{:-<5}]", {"abcdef"}},
    {"[{:3}]", {"\xC3\xA9"}},
    {"[{:>4}]", {"\xC3\xA9\xC3\xA9"}},
    {"{}", {""}},
    {"no placeholders", {}},
    {"{}{}{}", {"a", "b", "c"}},
    {"{:^1}", {"xyz"}},
    {"{0:>4}{0:<4}|", {"q"}},
    {"}}{{", {}},
    {"{:#>12}|{:_<3}", {"right", "x"}},
    {"{2}{1}{0}", {"1", "2", "3"}},
};

// One call site of formatText inside a loop (the cases are picked by an if chain), so the program stays small.
std::string formatProgram2() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\nimport /std/collections/*\n\n";
  body += "[return<void> effects(io_out, heap_alloc)]\nshow([/std/text/String] s) {\n"
          "  print_line(s.count())\n"
          "  for([i32 mut] i{0i32}, i < s.count(), i++) {\n    print_line(convert<i32>(s.byteAt(i)))\n  }\n}\n\n";
  body += "[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  body += "  for([i32 mut] round{0i32}, round < " + std::to_string(FormatCases.size()) + "i32, round++) {\n";
  body += "    [StringView mut] pattern{textOf(\"\")}\n    [StringView mut] a{textOf(\"\")}\n    [StringView mut] b{textOf(\"\")}\n"
          "    [StringView mut] c{textOf(\"\")}\n";
  for (size_t i = 0; i < FormatCases.size(); ++i) {
    const FormatCase &testCase = FormatCases[i];
    body += "    if(round == " + std::to_string(i) + "i32) {\n      pattern = textOf(\"" + testCase.pattern + "\")\n";
    const char *names[] = {"a", "b", "c"};
    for (size_t k = 0; k < testCase.args.size(); ++k) {
      body += std::string("      ") + names[k] + " = textOf(\"" + testCase.args[k] + "\")\n";
    }
    body += "    }\n";
  }
  body += "    show(formatText(pattern, a, b, c))\n  }\n";
  body += "  show(joinWith(textOf(\", \"), textOf(\"a\"), textOf(\"bb\"), textOf(\"\"), textOf(\"c\")))\n";
  body += "  show(joinWith(textOf(\"-\"), textOf(\"only\")))\n";
  body += "  show(joinWith(textOf(\"-\")))\n";
  // owned pieces of a split
  body += "  [StringView] text{textOf(\"a,b,,c,\")}\n  [vector<String>] pieces{text.splitOwned(textOf(\",\"))}\n"
          "  print_line(pieces.count())\n"
          "  for([i32 mut] i{0i32}, i < pieces.count(), i++) {\n    [i32] n{pieces.at(i).count()}\n    print_line(n)\n  }\n";
  return body + "  return(0i32)\n}\n";
}

std::string showString(const std::string &text) {
  std::string out = std::to_string(text.size()) + "\n";
  for (const unsigned char ch : text) {
    out += std::to_string(ch) + "\n";
  }
  return out;
}

std::string formatOracle2() {
  std::string out;
  for (const FormatCase &testCase : FormatCases) {
    std::string padded[3] = {"", "", ""};
    for (size_t k = 0; k < testCase.args.size(); ++k) {
      padded[k] = testCase.args[k];
    }
    out += showString(std::vformat(testCase.pattern, std::make_format_args(padded[0], padded[1], padded[2])));
  }
  out += showString("a, bb, , c") + showString("only") + showString("");
  out += "5\n1\n1\n0\n1\n0\n";
  return out;
}
} // namespace

TEST_CASE("formatText, joinWith and splitOwned agree with std::format on the VM") {
  CHECK(vmOutput(formatProgram2()) == formatOracle2());
}

TEST_CASE("formatText, joinWith and splitOwned agree with std::format through optexe") {
  CHECK(optexeOutput("formatting", formatProgram2()) == formatOracle2());
}

TEST_CASE("a malformed format pattern or a missing argument stops the program") {
  const std::string head = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n";
  CHECK(failsToRun(head + "  [String] s{formatText(textOf(\"{\"), textOf(\"a\"))}\n  return(s.count())\n}\n"));
  CHECK(failsToRun(head + "  [String] s{formatText(textOf(\"}\"), textOf(\"a\"))}\n  return(s.count())\n}\n"));
  CHECK(failsToRun(head + "  [String] s{formatText(textOf(\"{}{}\"), textOf(\"a\"))}\n  return(s.count())\n}\n"));
  CHECK(failsToRun(head + "  [String] s{formatText(textOf(\"{2}\"), textOf(\"a\"))}\n  return(s.count())\n}\n"));
  CHECK(failsToRun(head + "  [String] s{formatText(textOf(\"{:>x}\"), textOf(\"a\"))}\n  return(s.count())\n}\n"));
  CHECK_FALSE(failsToRun(head + "  [String] s{formatText(textOf(\"{}\"), textOf(\"a\"))}\n  return(0i32)\n}\n"));
}

// ---- Secret ----------------------------------------------------------------------------------------------------

namespace {
std::string secretProgram() {
  return
      "import /std/text/*\nimport /std/text/view/*\nimport /std/text/secret/*\n\n"
      "[return<Pointer<u8>> effects(heap_alloc)]\nleakyPointer() {\n"
      "  [Secret] s{secretOf(textOf(\"topsecret\"))}\n  return(s.reveal().data)\n}\n\n"
      "[return<i32> effects(heap_alloc)]\nchurn() {\n"
      "  [Secret] s{secretOf(textOf(\"a secret value long enough to need a heap block of its own\"))}\n  return(s.count())\n}\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [Secret] a{secretOf(textOf(\"hunter2\"))}\n  [Secret] b{secretOf(textOf(\"hunter2\"))}\n"
      "  [Secret] c{secretOf(textOf(\"hunter3\"))}\n  [Secret] d{secretOf(textOf(\"hunter\"))}\n  [Secret] e{secretOf(textOf(\"\"))}\n"
      "  print_line(if(a.equalsConstantTime(b), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsConstantTime(c), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsConstantTime(d), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(e.equalsConstantTime(e), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsViewConstantTime(textOf(\"hunter2\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsViewConstantTime(textOf(\"Hunter2\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(a.count())\n  print_line(if(e.empty(), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.reveal().equalsText(\"hunter2\"), then() { 1i32 }, else() { 0i32 }))\n"
      "  [Pointer<u8>] gone{leakyPointer()}\n  [i32 mut] nonZero{0i32}\n"
      "  for([i32 mut] i{0i32}, i < 9i32, i++) {\n    if(/std/text/textLoad(gone, i) != 0u8) {\n      nonZero = nonZero + 1i32\n    }\n  }\n"
      "  print_line(nonZero)\n"
      "  for([i32 mut] r{0i32}, r < 40i32, r++) {\n    churn()\n  }\n"
      "  print_line(if(stringHeapBytesInUse() > 0u64, then() { 1i32 }, else() { 0i32 }))\n"
      "  return(0i32)\n}\n";
}

const char *SecretExpected = "1\n0\n0\n1\n1\n0\n7\n1\n1\n0\n1\n";
} // namespace

TEST_CASE("Secret compares, reveals and zeroes its text on the VM") {
  // The last line is 1 here because a..e are still alive: the churn loop itself must not add to the heap.
  CHECK(vmOutput(secretProgram()) == SecretExpected);
}

TEST_CASE("Secret compares, reveals and zeroes its text through optexe") {
  CHECK(optexeOutput("secret", secretProgram()) == SecretExpected);
}

TEST_CASE("a Secret cannot be formatted or printed") {
  const std::string head = "import /std/text/*\nimport /std/text/view/*\nimport /std/text/secret/*\n\n"
                           "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
                           "  [Secret] s{secretOf(textOf(\"hunter2\"))}\n";
  const auto compiles = [](const std::string &source) {
    ScriptEngine engine;
    return engine.compileSource("/secret_misuse.prime", source).valid();
  };
  CHECK(compiles(head + "  return(0i32)\n}\n"));
  CHECK_FALSE(compiles(head + "  [String] f{formatText(textOf(\"{}\"), s)}\n  return(0i32)\n}\n"));
  CHECK_FALSE(compiles(head + "  print_line(s)\n  return(0i32)\n}\n"));
}

// ---- StringMap and hashing -------------------------------------------------------------------------------------

namespace {
// Keys "k0".."k<n-1>" built at run time: insert, look up, erase every third, re-insert every sixth, look up again.
std::string mapProgram(int n) {
  const std::string count = std::to_string(n);
  return
      "import /std/text/*\nimport /std/text/view/*\nimport /std/text/string_map/*\n\n"
      "[return<void> effects(heap_alloc)]\nmakeKey([String mut] key, [i32] i) {\n  key.clear()\n  key.append(\"k\")\n  key.appendI64(convert<i64>(i))\n}\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [StringMap<i32> mut] m{StringMap<i32>{}}\n  [String mut] key{stringWithCapacity(0i32)}\n"
      "  for([i32 mut] i{0i32}, i < " + count + "i32, i++) {\n    makeKey(key, i)\n    m.insert(key.view(), i * 3i32)\n  }\n"
      "  print_line(m.count())\n"
      "  [i64 mut] sum{0i64}\n  for([i32 mut] j{0i32}, j < m.count(), j++) {\n    sum = sum + convert<i64>(m.valueAt(j))\n  }\n  print_line(sum)\n"
      "  [i64 mut] looked{0i64}\n  for([i32 mut] i{0i32}, i < " + count + "i32, i++) {\n    makeKey(key, i)\n    looked = looked + convert<i64>(m.getOr(key.view(), -1i32))\n  }\n  print_line(looked)\n"
      "  [i32 mut] erased{0i32}\n  for([i32 mut] i{0i32}, i < " + count + "i32, i = i + 3i32) {\n    makeKey(key, i)\n    if(m.erase(key.view())) {\n      erased = erased + 1i32\n    }\n  }\n"
      "  print_line(erased)\n  print_line(m.count())\n"
      "  [i32 mut] wrong{0i32}\n  for([i32 mut] i{0i32}, i < " + count + "i32, i++) {\n    makeKey(key, i)\n    [bool] here{m.contains(key.view())}\n"
      "    [bool] shouldBe{i - i / 3i32 * 3i32 != 0i32}\n    if(here != shouldBe) {\n      wrong = wrong + 1i32\n    }\n    if(here && m.at(key.view()) != i * 3i32) {\n      wrong = wrong + 1i32\n    }\n  }\n  print_line(wrong)\n"
      "  print_line(if(m.erase(key.view()) || m.contains(key.view()), then() { 1i32 }, else() { 0i32 }))\n"
      "  for([i32 mut] i{0i32}, i < " + count + "i32, i = i + 6i32) {\n    makeKey(key, i)\n    m.insert(key.view(), i)\n  }\n"
      "  print_line(m.count())\n"
      "  [i64 mut] sum2{0i64}\n  [i64 mut] keyBytes{0i64}\n  for([i32 mut] j{0i32}, j < m.count(), j++) {\n    sum2 = sum2 + convert<i64>(m.valueAt(j))\n    keyBytes = keyBytes + convert<i64>(m.keyAt(j).count())\n  }\n"
      "  print_line(sum2)\n  print_line(keyBytes)\n"
      "  m.clear()\n  print_line(m.count())\n  print_line(if(m.contains(textOf(\"k0\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  return(0i32)\n}\n";
}

std::string mapOracle(int n) {
  std::unordered_map<std::string, int32_t> map;
  std::string out;
  for (int i = 0; i < n; ++i) {
    map["k" + std::to_string(i)] = i * 3;
  }
  const auto total = [&map]() {
    int64_t sum = 0;
    for (const auto &entry : map) {
      sum += entry.second;
    }
    return sum;
  };
  out += std::to_string(map.size()) + "\n" + std::to_string(total()) + "\n";
  int64_t looked = 0;
  for (int i = 0; i < n; ++i) {
    looked += map.at("k" + std::to_string(i));
  }
  out += std::to_string(looked) + "\n";
  int erased = 0;
  for (int i = 0; i < n; i += 3) {
    erased += static_cast<int>(map.erase("k" + std::to_string(i)));
  }
  out += std::to_string(erased) + "\n" + std::to_string(map.size()) + "\n0\n";
  // the key left in the program's buffer is k<n-1>; it is erased (and so absent) only when n-1 is a multiple of 3
  const std::string last = "k" + std::to_string(n - 1);
  const bool lastGone = (n - 1) % 3 == 0;
  out += std::string(lastGone ? "0" : "1") + "\n";
  if (!lastGone) {
    map.erase(last);
  }
  // the program erased `last` above when present; mirror that before the re-insert
  for (int i = 0; i < n; i += 6) {
    map["k" + std::to_string(i)] = i;
  }
  out += std::to_string(map.size()) + "\n" + std::to_string(total()) + "\n";
  int64_t keyBytes = 0;
  for (const auto &entry : map) {
    keyBytes += static_cast<int64_t>(entry.first.size());
  }
  out += std::to_string(keyBytes) + "\n0\n0\n";
  return out;
}

// SipHash-1-3 reference, written from the paper (an independent copy of the host function's algorithm).
uint64_t referenceSipHash13(const std::string &text, uint64_t seed) {
  const uint64_t k0 = seed;
  const uint64_t k1 = seed * 0x9E3779B97F4A7C15ULL + 0xD1B54A32D192ED03ULL;
  uint64_t v[4] = {k0 ^ 0x736f6d6570736575ULL, k1 ^ 0x646f72616e646f6dULL, k0 ^ 0x6c7967656e657261ULL, k1 ^ 0x7465646279746573ULL};
  const auto rotl = [](uint64_t x, unsigned b) { return (x << b) | (x >> (64 - b)); };
  const auto round = [&]() {
    v[0] += v[1]; v[2] += v[3]; v[1] = rotl(v[1], 13); v[3] = rotl(v[3], 16);
    v[1] ^= v[0]; v[3] ^= v[2]; v[0] = rotl(v[0], 32);
    v[2] += v[1]; v[0] += v[3]; v[1] = rotl(v[1], 17); v[3] = rotl(v[3], 21);
    v[1] ^= v[2]; v[3] ^= v[0]; v[2] = rotl(v[2], 32);
  };
  const size_t whole = text.size() / 8 * 8;
  for (size_t at = 0; at < whole; at += 8) {
    uint64_t word = 0;
    std::memcpy(&word, text.data() + at, 8);
    v[3] ^= word; round(); v[0] ^= word;
  }
  uint64_t last = static_cast<uint64_t>(text.size()) << 56;
  for (size_t k = 0; whole + k < text.size(); ++k) {
    last |= static_cast<uint64_t>(static_cast<unsigned char>(text[whole + k])) << (8 * k);
  }
  v[3] ^= last; round(); v[0] ^= last;
  v[2] ^= 0xff; round(); round(); round();
  return v[0] ^ v[1] ^ v[2] ^ v[3];
}

const std::vector<std::string> HashInputs = {"", "a", "abcdefg", "abcdefgh", "abcdefghi", "hello world, this is longer than sixteen bytes", "\xC3\xA9\xE4\xB8\x96"};

std::string hashProgram() {
  std::string body = "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
  int index = 0;
  for (const std::string &text : HashInputs) {
    const std::string name = "h" + std::to_string(index++);
    body += "  [StringView] " + name + "{textOf(\"" + text + "\")}\n";
    body += "  print_line(" + name + ".hash())\n  print_line(" + name + ".hashSeeded(1234567890123u64))\n";
  }
  body += "  [String] owned{stringFrom(\"hello\")}\n  [StringView] literal{textOf(\"hello\")}\n  [u64] ownedHash{owned.hash()}\n"
          "  [u64] literalHash{literal.hash()}\n  print_line(if(ownedHash == literalHash, then() { 1i32 }, else() { 0i32 }))\n";
  return body + "  return(0i32)\n}\n";
}

std::string hashOracle() {
  std::string out;
  for (const std::string &text : HashInputs) {
    out += std::to_string(referenceSipHash13(text, 0)) + "\n" + std::to_string(referenceSipHash13(text, 1234567890123ULL)) + "\n";
  }
  return out + "1\n";
}
} // namespace

TEST_CASE("StringMap agrees with std::unordered_map on the VM") {
  CHECK(vmOutput(mapProgram(2500)) == mapOracle(2500));
}

TEST_CASE("StringMap agrees with std::unordered_map through optexe with 100000 run-time keys") {
  CHECK(optexeOutput("string_map", mapProgram(100000)) == mapOracle(100000));
}

TEST_CASE("string hashes agree with a SipHash-1-3 reference on the VM") {
  CHECK(vmOutput(hashProgram()) == hashOracle());
}

TEST_CASE("string hashes agree with a SipHash-1-3 reference through optexe") {
  CHECK(optexeOutput("hashes", hashProgram()) == hashOracle());
}

// ---- Interner ----------------------------------------------------------------------------------------------------

namespace {
// Texts "n<i mod m>" interned in a loop: the symbols must be numbered in first-seen order and equal exactly when the
// texts are.
std::string internProgram(int rounds, int distinct) {
  return
      "import /std/text/*\nimport /std/text/view/*\nimport /std/text/interner/*\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [Interner mut] names{Interner{}}\n  [String mut] key{stringWithCapacity(0i32)}\n"
      "  [i64 mut] checksum{0i64}\n  [i32 mut] mismatches{0i32}\n"
      "  for([i32 mut] i{0i32}, i < " + std::to_string(rounds) + "i32, i++) {\n"
      "    key.clear()\n    key.append(\"n\")\n    key.appendI64(convert<i64>(i * 7i32 - i * 7i32 / " + std::to_string(distinct) + "i32 * " + std::to_string(distinct) + "i32))\n"
      "    [Symbol] s{names.intern(key.view())}\n    checksum = checksum + convert<i64>(s.id) * convert<i64>(i + 1i32)\n"
      "    [Symbol] again{names.find(key.view())}\n    if(!s.equals(again)) {\n      mismatches = mismatches + 1i32\n    }\n"
      "    [StringView] shown{names.text(s)}\n    [StringView] typed{key.view()}\n    [bool] same{shown.equals(typed)}\n"
      "    if(!same) {\n      mismatches = mismatches + 1i32\n    }\n  }\n"
      "  print_line(names.count())\n  print_line(checksum)\n  print_line(mismatches)\n"
      "  [Symbol] first{names.find(textOf(\"n0\"))}\n  print_line(first.id)\n"
      "  return(0i32)\n}\n";
}

std::string internOracle(int rounds, int distinct) {
  std::unordered_map<int, int> ids;
  int64_t checksum = 0;
  for (int i = 0; i < rounds; ++i) {
    const int key = (i * 7) - (i * 7) / distinct * distinct;
    const auto found = ids.find(key);
    const int id = found == ids.end() ? static_cast<int>(ids.size()) : found->second;
    if (found == ids.end()) {
      ids[key] = id;
    }
    checksum += static_cast<int64_t>(id) * (i + 1);
  }
  return std::to_string(ids.size()) + "\n" + std::to_string(checksum) + "\n0\n" + std::to_string(ids.count(0) ? ids[0] : -1) + "\n";
}
} // namespace

TEST_CASE("Interner numbers texts in first-seen order on the VM") {
  CHECK(vmOutput(internProgram(900, 130)) == internOracle(900, 130));
}

TEST_CASE("Interner numbers texts in first-seen order through optexe with 100000 lookups") {
  CHECK(optexeOutput("interner", internProgram(100000, 20011)) == internOracle(100000, 20011));
}

// ---- Maybe-valued searches and parses (find, rfind, parseI32, first, last) --------------------------------------

namespace {
// One call site per probe: the first cases are picked by an if chain, the rest come from the same in-program generator as
// searchProgram, so the program stays small.
std::string maybeProgram() {
  std::string body =
      "import /std/text/*\nimport /std/text/view/*\nimport /std/maybe/*\n\n"
      "[return<u64>]\nlcgStep([u64] seed) {\n"
      "  [u64] raw{seed * 1664525u64 + 1013904223u64}\n"
      "  return(raw - raw / 4294967296u64 * 4294967296u64)\n}\n\n"
      "[return<u64>]\nbelow([u64] seed, [u64] bound) {\n"
      "  [u64] value{seed / 256u64}\n"
      "  return(value - value / bound * bound)\n}\n\n"
      "[return<void> effects(io_out)]\nprobe([/std/text/view/StringView] h, [/std/text/view/StringView] n) {\n"
      "  [Maybe<i32>] forward{h.find(n)}\n  [Maybe<i32>] backward{h.rfind(n)}\n"
      "  print_line(forward.unwrapOr(-1i32))\n  print_line(h.indexOf(n))\n"
      "  print_line(backward.unwrapOr(-1i32))\n  print_line(h.lastIndexOf(n))\n}\n\n"
      "[return<void> effects(io_out)]\nprobeNumber([/std/text/view/StringView] v) {\n"
      "  [Maybe<i32>] parsed{v.parseI32()}\n  print_line(parsed.unwrapOr(-99i32))\n  print_line(v.parseI32Or(-99i32))\n"
      "  [Maybe<u8>] head{v.first()}\n  [Maybe<u8>] tail{v.last()}\n"
      "  print_line(convert<i32>(head.unwrapOr(0u8)))\n  print_line(convert<i32>(tail.unwrapOr(0u8)))\n"
      "  print_line(if(head.isEmpty(), then() { 1i32 }, else() { 0i32 }))\n}\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [String mut] hay{/std/text/stringFrom(\"\")}\n  [String mut] needle{/std/text/stringFrom(\"\")}\n  [u64 mut] seed{987654321u64}\n";
  const std::vector<SearchCase> cases = searchCases();
  body += "  for([i32 mut] round{0i32}, round < 10i32, round++) {\n    hay.clear()\n    needle.clear()\n";
  for (size_t i = 0; i < 10; ++i) {
    body += "    if(round == " + std::to_string(i) + "i32) {\n      hay.append(\"" + cases[i].hay + "\")\n      needle.append(\"" + cases[i].needle + "\")\n    }\n";
  }
  body += "    probe(hay.view(), needle.view())\n  }\n";
  body +=
      "  for([i32 mut] round{0i32}, round < 160i32, round++) {\n"
      "    [u64] alphabet{if(round - round / 3i32 * 3i32 == 1i32, then() { 3u64 }, else() { 2u64 })}\n"
      "    hay.clear()\n    needle.clear()\n"
      "    seed = lcgStep(seed)\n    [u64] hayLength{below(seed, 24u64)}\n"
      "    for([u64 mut] k{0u64}, k < hayLength, k++) {\n"
      "      seed = lcgStep(seed)\n      hay.pushByte(convert<u8>(97u64 + below(seed, alphabet)))\n    }\n"
      "    seed = lcgStep(seed)\n    [u64] needleLength{below(seed, 7u64)}\n"
      "    for([u64 mut] k{0u64}, k < needleLength, k++) {\n"
      "      seed = lcgStep(seed)\n      needle.pushByte(convert<u8>(97u64 + below(seed, alphabet)))\n    }\n"
      "    probe(hay.view(), needle.view())\n  }\n";
  body += "  for([i32 mut] round{0i32}, round < " + std::to_string(IntCases.size()) + "i32, round++) {\n    hay.clear()\n";
  for (size_t i = 0; i < IntCases.size(); ++i) {
    body += "    if(round == " + std::to_string(i) + "i32) {\n      hay.append(\"" + IntCases[i] + "\")\n    }\n";
  }
  body += "    probeNumber(hay.view())\n  }\n";
  return body + "  return(0i32)\n}\n";
}

std::string maybeOracle() {
  std::string out;
  for (const SearchCase &testCase : searchCases()) {
    const size_t found = testCase.hay.find(testCase.needle);
    const size_t last = testCase.hay.rfind(testCase.needle);
    const int forward = found == std::string::npos ? -1 : static_cast<int>(found);
    const int backward = last == std::string::npos ? -1 : static_cast<int>(last);
    out += std::to_string(forward) + "\n" + std::to_string(forward) + "\n" + std::to_string(backward) + "\n" + std::to_string(backward) + "\n";
  }
  for (const std::string &text : IntCases) {
    std::string digits = text;
    bool valid = !digits.empty();
    if (valid && digits[0] == '+') {
      digits.erase(0, 1);
      valid = !digits.empty() && digits[0] != '-' && digits[0] != '+';
    }
    int64_t wide = 0;
    if (valid) {
      const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), wide);
      valid = parsed.ec == std::errc() && parsed.ptr == digits.data() + digits.size();
    }
    const int parsedValue = valid && wide >= INT32_MIN && wide <= INT32_MAX ? static_cast<int>(wide) : -99;
    out += std::to_string(parsedValue) + "\n" + std::to_string(parsedValue) + "\n";
    out += std::to_string(text.empty() ? 0 : static_cast<unsigned char>(text.front())) + "\n";
    out += std::to_string(text.empty() ? 0 : static_cast<unsigned char>(text.back())) + "\n";
    out += std::string(text.empty() ? "1" : "0") + "\n";
  }
  return out;
}
} // namespace

TEST_CASE("Maybe-valued find, rfind, parseI32, first and last agree with the plain forms on the VM") {
  CHECK(vmOutput(maybeProgram()) == maybeOracle());
}

TEST_CASE("Maybe-valued find, rfind, parseI32, first and last agree with the plain forms through optexe") {
  CHECK(optexeOutput("maybes", maybeProgram()) == maybeOracle());
}

// ---- Operators and index sugar (+, ==, !=, <, <=, >, >=, s[i]) --------------------------------------------------

namespace {
const std::vector<std::string> OperatorWords = {"", "a", "ab", "abc", "abd", "b", "B", "zz", "caf\xc3\xa9", "caf", "0123456789"};

// Every ordered pair goes through the same operators in one loop; the words come from a chain in word().
std::string operatorProgram() {
  std::string chain;
  for (size_t i = 0; i < OperatorWords.size(); ++i) {
    chain += "  if(i == " + std::to_string(i) + "i32) {\n    return(stringFrom(\"" + OperatorWords[i] + "\"))\n  }\n";
  }
  std::string checks;
  for (const char *expr : {"joined == b + a", "a == b", "a != b", "a < b", "a <= b", "a > b", "a >= b",
                           "va < vb", "va == vb", "va != vb", "va <= vb", "va > vb", "va >= vb"}) {
    checks += std::string("      print_line(if(") + expr + ", then() { 1i32 }, else() { 0i32 }))\n";
  }
  const std::string count = std::to_string(OperatorWords.size());
  return "import /std/text/*\nimport /std/text/view/*\n\n"
         "[return<String> effects(heap_alloc)]\nword([i32] i) {\n" + chain + "  return(stringFrom(\"\"))\n}\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  for([i32 mut] i{0i32}, i < " + count + "i32, i++) {\n"
         "    for([i32 mut] j{0i32}, j < " + count + "i32, j++) {\n"
         "      [String] a{word(i)}\n      [String] b{word(j)}\n"
         "      [StringView] va{a.view()}\n      [StringView] vb{b.view()}\n"
         "      [String] joined{a + b}\n      print_line(joined.count())\n" + checks +
         "      if(joined.count() > 0i32) {\n        print_line(convert<i32>(joined[0i32]))\n"
         "        print_line(convert<i32>(joined[joined.count() - 1i32]))\n      }\n"
         "    }\n  }\n  return(0i32)\n}\n";
}

std::string operatorOracle() {
  std::string out;
  const auto flag = [&out](bool value) { out += value ? "1\n" : "0\n"; };
  for (const std::string &a : OperatorWords) {
    for (const std::string &b : OperatorWords) {
      const std::string joined = a + b;
      out += std::to_string(joined.size()) + "\n";
      flag(joined == b + a);
      for (const bool value : {a == b, a != b, a < b, a <= b, a > b, a >= b,
                               a < b, a == b, a != b, a <= b, a > b, a >= b}) {
        flag(value);
      }
      if (!joined.empty()) {
        out += std::to_string(static_cast<unsigned char>(joined.front())) + "\n" +
               std::to_string(static_cast<unsigned char>(joined.back())) + "\n";
      }
    }
  }
  return out;
}
} // namespace

TEST_CASE("operators and index sugar on String and StringView agree with std::string on the VM") {
  CHECK(vmOutput(operatorProgram()) == operatorOracle());
}

TEST_CASE("operators and index sugar on String and StringView agree with std::string through optexe") {
  CHECK(optexeOutput("operators", operatorProgram()) == operatorOracle());
}

// ---- pushCodePoint ------------------------------------------------------------------------------------------------

namespace {
// Every 251st code point plus the encoding boundaries goes through pushCodePoint; the bytes are folded into one hash.
std::string pushCodePointProgram() {
  return "import /std/text/*\nimport /std/text/view/*\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  [String mut] s{String{}}\n  [i32 mut] h{17i32}\n"
         "  for([i32 mut] p{0i32}, p < 1114112i32, p = p + 251i32) {\n"
         "    if(p < 55296i32 || p > 57343i32) {\n      s.pushCodePoint(p)\n    }\n"
         "  }\n"
         "  s.pushCodePoint(127i32)\n  s.pushCodePoint(128i32)\n  s.pushCodePoint(2047i32)\n  s.pushCodePoint(2048i32)\n"
         "  s.pushCodePoint(55295i32)\n  s.pushCodePoint(57344i32)\n  s.pushCodePoint(65535i32)\n  s.pushCodePoint(65536i32)\n"
         "  s.pushCodePoint(1114111i32)\n"
         "  for([i32 mut] i{0i32}, i < s.count(), i++) {\n    h = h * 31i32 + convert<i32>(s.byteAt(i))\n  }\n"
         "  print_line(s.count())\n  print_line(h)\n"
         "  print_line(utf8ErrorOffset(s.data, s.count()))\n"
         "  return(0i32)\n}\n";
}

void encodeUtf8Reference(std::string &out, int32_t point) {
  if (point < 0x80) {
    out += static_cast<char>(point);
  } else if (point < 0x800) {
    out += static_cast<char>(0xC0 + (point >> 6));
    out += static_cast<char>(0x80 + (point & 63));
  } else if (point < 0x10000) {
    out += static_cast<char>(0xE0 + (point >> 12));
    out += static_cast<char>(0x80 + ((point >> 6) & 63));
    out += static_cast<char>(0x80 + (point & 63));
  } else {
    out += static_cast<char>(0xF0 + (point >> 18));
    out += static_cast<char>(0x80 + ((point >> 12) & 63));
    out += static_cast<char>(0x80 + ((point >> 6) & 63));
    out += static_cast<char>(0x80 + (point & 63));
  }
}

std::string pushCodePointOracle() {
  std::string s;
  for (int32_t p = 0; p < 1114112; p += 251) {
    if (p < 55296 || p > 57343) {
      encodeUtf8Reference(s, p);
    }
  }
  for (const int32_t p : {127, 128, 2047, 2048, 55295, 57344, 65535, 65536, 1114111}) {
    encodeUtf8Reference(s, p);
  }
  int32_t h = 17;
  for (const char c : s) {
    h = wrapAdd(wrapMul(h, 31), static_cast<int32_t>(static_cast<unsigned char>(c)));
  }
  return std::to_string(s.size()) + "\n" + std::to_string(h) + "\n-1\n";
}
} // namespace

TEST_CASE("pushCodePoint encodes UTF-8 like a reference encoder on the VM") {
  CHECK(vmOutput(pushCodePointProgram()) == pushCodePointOracle());
}

TEST_CASE("pushCodePoint encodes UTF-8 like a reference encoder through optexe") {
  CHECK(optexeOutput("codepoints", pushCodePointProgram()) == pushCodePointOracle());
}

// ---- fused + chains -------------------------------------------------------------------------------------------------

namespace {
// A chain of Strings allocates once, evaluates its operands left to right, and gives the same text as std::string.
std::string fusedProgram() {
  return "import /std/text/*\nimport /std/text/view/*\n\n"
         "[return<String> effects(heap_alloc, io_out)]\nword([i32] i) {\n"
         "  print_line(i)\n  return(stringFromI32(i * 1000i32 + 7i32))\n}\n\n"
         "[return<void> effects(heap_alloc, io_out)]\nreport([String] text, [u64] used) {\n"
         "  print_line(text.count())\n  print_line(convert<i32>(used))\n"
         "  for([i32 mut] i{0i32}, i < text.count(), i++) {\n    print_line(convert<i32>(text[i]))\n  }\n}\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  [String] a{word(1i32)}\n  [String] b{word(2i32)}\n  [String] c{word(3i32)}\n  [String] empty{String{}}\n"
         "  [u64 mut] start{stringHeapAllocations()}\n"
         "  [String] five{a + b + c + a + b}\n  report(five, stringHeapAllocations() - start)\n"
         "  start = stringHeapAllocations()\n"
         "  [String] mixed{a + (b + c + empty) + word(4i32) + empty}\n  report(mixed, stringHeapAllocations() - start)\n"
         "  start = stringHeapAllocations()\n"
         "  [String] pair{a + b}\n  report(pair, stringHeapAllocations() - start)\n"
         "  return(0i32)\n}\n";
}

std::string fusedOracle() {
  const auto word = [](int i) { return std::to_string(i * 1000 + 7); };
  const auto report = [](const std::string &text, int used) {
    std::string out = std::to_string(text.size()) + "\n" + std::to_string(used) + "\n";
    for (const char c : text) {
      out += std::to_string(static_cast<unsigned char>(c)) + "\n";
    }
    return out;
  };
  const std::string a = word(1);
  const std::string b = word(2);
  const std::string c = word(3);
  // word() runs (and prints its argument) while the arguments of main's bindings are made: 1 2 3 first, then 4 inside the second chain.
  std::string out = "1\n2\n3\n";
  out += report(a + b + c + a + b, 1);
  out += "4\n";
  out += report(a + (b + c) + word(4), 2); // one block for the whole chain, one for word(4) itself
  out += report(a + b, 1);
  return out;
}
} // namespace

TEST_CASE("a chain of Strings allocates once and keeps evaluation order on the VM") {
  CHECK(vmOutput(fusedProgram()) == fusedOracle());
}

TEST_CASE("a chain of Strings allocates once and keeps evaluation order through optexe") {
  CHECK(optexeOutput("fused", fusedProgram()) == fusedOracle());
}

// ---- folding of literal text operations ------------------------------------------------------------------------------

namespace {
const char *FoldedBody =
    "  print_line(\"ab\" + \"cd\" == \"abcd\")\n"
    "  print_line(\"ab\" != \"ab\")\n"
    "  print_line(convert<i32>(at(\"hello\", 1i32)))\n"
    "  print_line(textOf(\"hello\").startsWith(textOf(\"he\")))\n"
    "  print_line(textOf(\"hello\").endsWith(textOf(\"lo\")))\n"
    "  print_line(textOf(\"hello\").contains(textOf(\"ell\")))\n"
    "  print_line(textOf(\"hello\").contains(textOf(\"xyz\")))\n"
    "  print_line(textOf(\"hello\").count())\n"
    "  print_line(textOf(\"abc\") < textOf(\"abd\"))\n"
    "  print_line(textOf(\"abc\") >= textOf(\"abd\"))\n"
    "  print_line(textOf(\"abc\") == textOf(\"abc\"))\n"
    "  print_line(convert<i32>(textOf(\"hello\")[4i32]))\n"
    "  print_line(\"a\\nb\" + \"c\" == \"a\\nbc\")\n";

std::string foldedProgram(const std::string &body) {
  return "import /std/text/*\nimport /std/text/view/*\n\n[return<int> effects(io_out)]\nmain() {\n" + body + "  return(0i32)\n}\n";
}

const char *FoldedOutput = "1\n0\n101\n1\n1\n1\n0\n5\n1\n0\n1\n111\n1\n";
} // namespace

TEST_CASE("operations on literal text give the same results on the VM and through optexe") {
  CHECK(vmOutput(foldedProgram(FoldedBody)) == FoldedOutput);
  CHECK(optexeOutput("folded", foldedProgram(FoldedBody)) == FoldedOutput);
}

TEST_CASE("operations on literal text leave no run-time text work in the lowered IR") {
  const std::string file = scratch("folded_ir.prime");
  std::ofstream(file, std::ios::binary) << foldedProgram(FoldedBody);
  const std::string dump = scratch("folded_ir.txt");
  REQUIRE(run(shellQuote(Primec) + " --emit=vm --dump-stage ir-lowered " + shellQuote(file) + " > " + shellQuote(dump) + " 2>&1") == 0);
  const std::string ir = readFile(dump);
  CHECK(ir.find("LoadStringByte") == std::string::npos);
  CHECK(ir.find("LoadStringLength") == std::string::npos);
  CHECK(ir.find("CallHost") == std::string::npos);
}

TEST_CASE("a literal index out of range is a compile error") {
  const std::string file = scratch("folded_range.prime");
  std::ofstream(file, std::ios::binary) << foldedProgram("  print_line(convert<i32>(at(\"hello\", 9i32)))\n");
  const std::string log = scratch("folded_range.log");
  CHECK(run(shellQuote(Primec) + " --emit=vm " + shellQuote(file) + " -o " + shellQuote(scratch("folded_range.out")) + " > " +
            shellQuote(log) + " 2>&1") != 0);
  CHECK(readFile(log).find("out of bounds for a literal of 5 bytes") != std::string::npos);
}

// ---- Unicode case mapping (/std/text/unicode) -------------------------------------------------------------------------

namespace {
// Expected values come from Python's unicodedata (Unicode 15.0), per code point, without the conditional rules.
const std::vector<std::string> CaseSamples = {"Stra\xc3\x9f""e", "\xce\x91\xce\xa3\xce\xa3", "\xce\x95\xce\xbb\xce\xbb\xce\xb7\xce\xbd\xce\xb9\xce\xba\xce\xac", "\xd0\x9f\xd0\xa0\xd0\x98\xd0\x92\xd0\x95\xd0\xa2 \xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", "\xc4\xb0stanbul", "\xc5\x89\xc7\xb0\xce\x90", "\xef\xac\x81\xef\xac\x82", "\xc7\x85", "\xce\xa3", "\xc3\x80\xc3\x89\xc3\x8e\xc3\x95\xc3\x9c", "", "plain ASCII 123"};
// per sample: lower, upper, fold as count,hash
const std::vector<std::pair<int, int>> CaseSampleExpected = {{7,1976057896}, {7,-1601004956}, {7,1975979652}, {6,-319906404}, {6,-349458114}, {6,-319906404}, {16,-197129569}, {16,616753561}, {16,-197129569}, {25,-1985689105}, {25,306638127}, {25,-1985689105}, {10,-1837133736}, {9,-506916242}, {10,-1837133736}, {6,-614704704}, {12,-1465575493}, {12,-1138488357}, {6,618016978}, {4,17857626}, {4,18842714}, {2,22640}, {2,22638}, {2,22640}, {2,22885}, {2,22886}, {2,22885}, {10,85529802}, {10,-570399702}, {10,85529802}, {0,17}, {0,17}, {0,17}, {15,-1521062436}, {15,1898657180}, {15,-1521062436}};

std::string caseSampleProgram() {
  std::string chain;
  for (size_t i = 0; i < CaseSamples.size(); ++i) {
    chain += "  if(i == " + std::to_string(i) + "i32) {\n    return(textOf(\"" + CaseSamples[i] + "\"))\n  }\n";
  }
  return "import /std/text/*\nimport /std/text/view/*\nimport /std/text/unicode/*\n\n"
         "[return</std/text/view/StringView>]\nsample([i32] i) {\n" + chain + "  return(textOf(\"\"))\n}\n\n"
         "[return<void> effects(heap_alloc, io_out)]\nreport([String] text) {\n"
         "  [i32 mut] h{17i32}\n  for([i32 mut] i{0i32}, i < text.count(), i++) {\n    h = h * 31i32 + convert<i32>(text[i])\n  }\n"
         "  print_line(text.count())\n  print_line(h)\n}\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  for([i32 mut] i{0i32}, i < " + std::to_string(CaseSamples.size()) + "i32, i++) {\n"
         "    [StringView] text{sample(i)}\n"
         "    report(text.toLower())\n    report(text.toUpper())\n    report(text.caseFold())\n"
         "  }\n  return(0i32)\n}\n";
}

std::string caseSampleOracle() {
  std::string out;
  for (const auto &expected : CaseSampleExpected) {
    out += std::to_string(expected.first) + "\n" + std::to_string(expected.second) + "\n";
  }
  return out;
}

// Whole blocks of the cased scripts, mapped as one text.
std::string caseRangeProgram() {
  return "import /std/text/*\nimport /std/text/view/*\nimport /std/text/unicode/*\n\n"
         "[return<void> effects(heap_alloc, io_out)]\nreport([String] text) {\n"
         "  [i32 mut] h{17i32}\n  for([i32 mut] i{0i32}, i < text.count(), i++) {\n    h = h * 31i32 + convert<i32>(text[i])\n  }\n"
         "  print_line(text.count())\n  print_line(h)\n}\n\n"
         "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
         "  [String mut] all{String{}}\n"
         "  for([i32 mut] block{0i32}, block < 9i32, block++) {\n"
         "    [i32 mut] first{0i32}\n    [i32 mut] last{0i32}\n"
         "    if(block == 0i32) { first = 0i32 last = 1536i32 }\n"
         "    if(block == 1i32) { first = 7168i32 last = 8704i32 }\n"
         "    if(block == 2i32) { first = 11264i32 last = 11568i32 }\n"
         "    if(block == 3i32) { first = 42560i32 last = 43008i32 }\n"
         "    if(block == 4i32) { first = 64256i32 last = 64336i32 }\n"
         "    if(block == 5i32) { first = 65312i32 last = 65371i32 }\n"
         "    if(block == 6i32) { first = 66560i32 last = 66816i32 }\n"
         "    if(block == 7i32) { first = 71840i32 last = 71904i32 }\n"
         "    if(block == 8i32) { first = 125184i32 last = 125280i32 }\n"
         "    for([i32 mut] p{first}, p < last, p++) {\n"
         "      if(p < 55296i32 || p > 57343i32) {\n        all.pushCodePoint(p)\n      }\n    }\n"
         "  }\n"
         "  [StringView] text{all.view()}\n"
         "  report(all)\n  report(text.toLower())\n  report(text.toUpper())\n  report(text.caseFold())\n"
         "  return(0i32)\n}\n";
}

const char *CaseRangeOracle = "11889\n546300750\n11869\n1661365438\n12060\n1582006169\n12025\n-125898994\n";
} // namespace

TEST_CASE("case mapping of samples matches the Unicode data on the VM") {
  CHECK(vmOutput(caseSampleProgram()) == caseSampleOracle());
}

TEST_CASE("case mapping of samples matches the Unicode data through optexe") {
  CHECK(optexeOutput("case_samples", caseSampleProgram()) == caseSampleOracle());
}

TEST_CASE("case mapping of whole script blocks matches the Unicode data on the VM") {
  CHECK(vmOutput(caseRangeProgram()) == CaseRangeOracle);
}

TEST_CASE("case mapping of whole script blocks matches the Unicode data through optexe") {
  CHECK(optexeOutput("case_ranges", caseRangeProgram()) == CaseRangeOracle);
}

TEST_CASE("equalsIgnoreCase compares after case folding") {
  const std::string program =
      "import /std/text/*\nimport /std/text/view/*\nimport /std/text/unicode/*\n\n"
      "[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
      "  [StringView] a{textOf(\"Stra\xc3\x9f""e\")}\n  [StringView] b{textOf(\"STRASSE\")}\n  [StringView] c{textOf(\"strasse!\")}\n"
      "  print_line(if(a.equalsIgnoreCase(b), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(a.equalsIgnoreCase(c), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(if(textOf(\"\xce\x91\xce\xa3\").equalsIgnoreCase(textOf(\"\xce\xb1\xcf\x83\")), then() { 1i32 }, else() { 0i32 }))\n"
      "  return(0i32)\n}\n";
  CHECK(vmOutput(program) == "1\n0\n1\n");
}

TEST_CASE("a program that does not use the case module carries none of its tables") {
  const auto bytecode = [](const std::string &name, const std::string &imports, const std::string &body) {
    const std::string file = scratch(name + ".prime");
    std::ofstream(file, std::ios::binary) << imports << "\n[return<int> effects(heap_alloc)]\nmain() {\n" << body << "}\n";
    const std::string out = scratch(name + ".psir");
    const std::string log = out + ".log";
    REQUIRE_MESSAGE(run(shellQuote(Primec) + " --emit=ir " + shellQuote(file) + " -o " + shellQuote(out) + " > " + shellQuote(log) +
                        " 2>&1") == 0,
                    readFile(log));
    return readFile(out);
  };
  const std::string plainImports = "import /std/text/*\nimport /std/text/view/*\n";
  const std::string without = bytecode("case_without", plainImports, "  [String] s{stringFromI32(5i32)}\n  return(s.count() - s.count())\n");
  const std::string unusedImport = bytecode("case_unused", plainImports + "import /std/text/unicode/*\n",
                                            "  [String] s{stringFromI32(5i32)}\n  return(s.count() - s.count())\n");
  const std::string used = bytecode("case_used", plainImports + "import /std/text/unicode/*\n",
                                    "  [StringView] t{textOf(\"Ab\")}\n  [String] s{t.toLower()}\n  return(s.count() - s.count())\n");
  const std::string firstLowerRecord = "000041100006100000000000"; // 'A' maps to 'a'
  CHECK(without.find(firstLowerRecord) == std::string::npos);
  CHECK(unusedImport.find(firstLowerRecord) == std::string::npos);
  CHECK(used.find(firstLowerRecord) != std::string::npos);
}

TEST_SUITE_END();
