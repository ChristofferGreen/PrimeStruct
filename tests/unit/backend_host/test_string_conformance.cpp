#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
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
std::string optexeOutput(const std::string &name, const std::string &source) {
  const std::string file = scratch(name + ".prime");
  std::ofstream(file, std::ios::binary) << source;
  const std::string exe = scratch(name + "_optexe");
  const std::string log = exe + ".log";
  const int status =
      run(shellQuote(Primec) + " --emit=optexe " + shellQuote(file) + " -o " + shellQuote(exe) + " > " + shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const std::string out = exe + ".out";
  CHECK(run(shellQuote(exe) + " > " + shellQuote(out)) == 0);
  return readFile(out);
}

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
    body += "  [String] " + copyName + "{duplicate(s)}\n  s.insert(s.count() / 2i32, \"MIDDLE\")\n  s.erase(0i32, s.count() / 3i32)\n";
    body += "  s.append(\"tail\")\n  h = 17i32\n" + foldCode("s", "  ") + "  h = h * 31i32 + s.count()\n  print_line(h)\n";
    body += "  h = 17i32\n" + foldCode(copyName, "  ") + "  h = h * 31i32 + " + copyName + ".count()\n  print_line(h)\n";
    body += "  s.shrinkToFit()\n  h = 17i32\n" + foldCode("s", "  ") + "  print_line(h)\n";
  }
  return "import /std/text/*\n\n"
         // A `[String copy]` parameter is the documented way to duplicate text (a binding from a binding aliases, TODO-5639).
         "[return</std/text/String> effects(heap_alloc)]\nduplicate([/std/text/String copy] value) {\n  return(value)\n}\n\n"
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

TEST_SUITE_END();
