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
#include <fstream>
#include <iterator>
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
  return "import /std/text/*\n\n"
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
  return "import /std/text/*\n\n"
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
      "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
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
      "import /std/text/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
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
      "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
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
      "import /std/text/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
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
  std::string body = "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
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
      "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
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
  const std::string head = "import /std/text/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
                           "  [StringView] v{textOf(\"h\xC3\xA9llo\")}\n";
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(0i32, 2i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(2i32, 4i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(0i32, 9i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(3i32, 1i32)}\n  return(cut.count())\n}\n"));
  CHECK(failsToRun(head + "  [StringView] cut{v.slice(-1i32, 1i32)}\n  return(cut.count())\n}\n"));
  CHECK_FALSE(failsToRun(head + "  [StringView] cut{v.slice(1i32, 3i32)}\n  return(0i32)\n}\n"));
}

TEST_CASE("textFromBytes stops the program on invalid UTF-8") {
  const std::string head = "import /std/text/*\n\n[return<int> effects(heap_alloc)]\nmain() {\n"
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
      "import /std/text/*\n\n"
      "[return<void> effects(io_out)]\n"
      "probe([/std/text/StringView] h, [/std/text/StringView] n) {\n"
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
  std::string body = "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
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
  return "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
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
      "import /std/text/*\n\n"
      "[return<void> effects(io_out)]\n"
      "probeInt([/std/text/StringView] v) {\n"
      "  [i32 mut] small{-5i32}\n"
      "  [i64 mut] wide{-5i64}\n"
      "  print_line(if(v.parseI32Into(small), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(small)\n"
      "  print_line(if(v.parseI64Into(wide), then() { 1i32 }, else() { 0i32 }))\n"
      "  print_line(wide)\n"
      "}\n\n"
      "[return<void> effects(io_out)]\n"
      "probeFloat([/std/text/StringView] v) {\n"
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
  std::string body = "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n";
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
      "import /std/text/*\n\n[return<int> effects(heap_alloc, io_out)]\nmain() {\n"
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

TEST_SUITE_END();
