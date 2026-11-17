// /std/glob (docs/spec/stdlib-reference.md): glob over a fixture tree on the VM and through optexe. The fixture is made
// of plain files and directories, so no time or process state reaches the expected text.
#include "primec/embed/Script.h"
#include "primec/embed/ScriptEngine.h"
#include "primec/testing/TestScratch.h"

#include "third_party/doctest.h"

#include <sys/wait.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using primec::embed::ScriptEngine;

TEST_SUITE_BEGIN("primestruct.backend.glob");

namespace {
const std::string Primec = PRIMESTRUCT_PRIMEC_PATH;

std::string scratch(const std::string &name) {
  const std::filesystem::path dir = primec::testing::testScratchDir("glob");
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

void makeFile(const std::filesystem::path &path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path, std::ios::binary) << "x";
}

// Names chosen for the rules: "Zed.prime" sorts before "a.prime" bytewise; "a.prime" sorts before "a/m.prime" because
// "." is before "/"; dot names (".git", ".hidden.prime", "sub/.dot.prime") are skipped by "*" and "**".
std::filesystem::path makeFixture(const std::string &name) {
  const std::filesystem::path root = scratch(name);
  std::filesystem::remove_all(root);
  makeFile(root / ".git" / "x.prime");
  makeFile(root / ".hidden.prime");
  makeFile(root / "a" / "m.prime");
  makeFile(root / "a.prime");
  makeFile(root / "b.txt");
  makeFile(root / "Zed.prime");
  makeFile(root / "sub" / ".dot.prime");
  makeFile(root / "sub" / "c.prime");
  makeFile(root / "sub" / "deep" / "d.prime");
  makeFile(root / "sub" / "deep" / "e.txt");
  std::filesystem::create_directories(root / "empty");
  return root;
}

struct Case {
  std::string base; // relative to the fixture root; empty is the root
  std::string pattern;
  std::vector<std::string> matches; // relative to the base
  std::vector<std::string> listed;  // relative to the base, "" for the base itself
  std::string error;                // the text of Result.why, empty when the glob succeeds
};

std::string joinPath(const std::string &root, const std::string &rel) {
  if (rel.empty()) {
    return root;
  }
  return root + "/" + rel;
}

std::vector<Case> cases() {
  const std::vector<std::string> allDirs = {"", "a", "empty", "sub", "sub/deep"};
  return {
      {"", "*.prime", {"Zed.prime", "a.prime"}, {""}, ""},
      {"", "**/*.prime", {"Zed.prime", "a.prime", "a/m.prime", "sub/c.prime", "sub/deep/d.prime"}, allDirs, ""},
      {"", "?.prime", {"a.prime"}, {""}, ""},
      {"", ".*.prime", {".hidden.prime"}, {""}, ""},
      {"", "sub/.*", {"sub/.dot.prime"}, {"", "sub"}, ""},
      {"", "**/.dot.prime", {"sub/.dot.prime"}, allDirs, ""},
      {"", "*.nomatch", {}, {""}, ""},
      {"", "sub/**", {"sub/c.prime", "sub/deep", "sub/deep/d.prime", "sub/deep/e.txt"}, {"", "sub", "sub/deep"}, ""},
      {"", "**/deep/*.txt", {"sub/deep/e.txt"}, allDirs, ""},
      {"", "a*", {"a", "a.prime"}, {""}, ""},
      {"", "**/**/c.*", {"sub/c.prime"}, allDirs, ""},
      {"", "*/*/*.prime", {"sub/deep/d.prime"}, allDirs, ""},
      {"sub", "*.prime", {"c.prime"}, {""}, ""},
      {"", "", {}, {}, "EINVAL"},
      {"", "/abs", {}, {}, "EINVAL"},
      {"", "a//b", {}, {}, "EINVAL"},
      {"", "a/", {}, {}, "EINVAL"},
      {"", "../x", {}, {}, "EINVAL"},
      {"", "a/./b", {}, {}, "EINVAL"},
      {"", "a**", {}, {}, "EINVAL"},
      {"", "**b/c", {}, {}, "EINVAL"},
      {"no_such_directory", "*", {}, {}, "ENOENT"},
  };
}

// Prints "# pattern", then "m <name>" per match and "l <name>" per listed directory, each name as its byte codes
// (print_line takes no String), or "error <why>".
std::string program(const std::filesystem::path &root, const std::vector<Case> &list) {
  std::string source =
      "import /std/glob/*\n"
      "import /std/file/*\n"
      "import /std/collections/vector/*\n"
      "import /std/text/*\n"
      "import /std/text/view/*\n\n"
      "[return<void> effects(io_out)]\n"
      "show_name([String] name) {\n"
      "  for([i32 mut] at{0i32}, at < name.count(), at++) {\n"
      "    print(name.byteAt(at))\n    print(\",\")\n  }\n"
      "  print_line(\"\")\n"
      "}\n\n"
      "[return<void> effects(io_out, file_read, heap_alloc)]\n"
      "show([String] base, [String] pattern) {\n"
      "  [vector<String> mut] matches{vector<String>{}}\n"
      "  [vector<String> mut] listed{vector<String>{}}\n"
      "  [Result<FileError>] done{glob(base.view(), pattern.view(), matches, listed)}\n"
      "  if(Result.error(done)) {\n"
      "    print_line(Result.why(done))\n"
      "    return()\n"
      "  }\n"
      "  for([i32 mut] i{0i32}, i < matches.count(), i++) {\n"
      "    print(\"m \")\n    show_name(matches[i])\n  }\n"
      "  for([i32 mut] i{0i32}, i < listed.count(), i++) {\n"
      "    print(\"l \")\n    show_name(listed[i])\n  }\n"
      "}\n\n"
      "[return<int> effects(io_out, file_read, heap_alloc)]\n"
      "main() {\n";
  for (const Case &c : list) {
    source += "  print_line(\"# " + c.base + " | " + c.pattern + "\")\n";
    source += "  show(stringFrom(\"" + joinPath(root.string(), c.base) + "\"), stringFrom(\"" + c.pattern + "\"))\n";
  }
  source += "  return(0i32)\n}\n";
  return source;
}

std::string expectedText(const std::filesystem::path &root, const std::vector<Case> &list) {
  std::string out;
  for (const Case &c : list) {
    out += "# " + c.base + " | " + c.pattern + "\n";
    if (!c.error.empty()) {
      out += c.error + "\n";
      continue;
    }
    const std::string base = joinPath(root.string(), c.base);
    for (const std::string &match : c.matches) {
      out += "m " + joinPath(base, match) + "\n";
    }
    for (const std::string &dir : c.listed) {
      out += "l " + joinPath(base, dir) + "\n";
    }
  }
  return out;
}

// Turns the lines "m 97,98," into "m ab".
std::string decode(const std::string &raw) {
  std::istringstream lines(raw);
  std::string line;
  std::string out;
  while (std::getline(lines, line)) {
    if (line.size() > 2 && (line[0] == 'm' || line[0] == 'l') && line[1] == ' ') {
      std::string name;
      std::istringstream codes(line.substr(2));
      std::string code;
      while (std::getline(codes, code, ',')) {
        name += static_cast<char>(std::stoi(code));
      }
      out += line.substr(0, 2) + name + "\n";
    } else {
      out += line + "\n";
    }
  }
  return out;
}

std::string vmOutput(const std::string &source) {
  ScriptEngine engine;
  const auto script = engine.compileSource("/glob.prime", source);
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
  return decode(out);
}

std::string optexeOutput(const std::string &name, const std::string &source) {
  const std::string file = scratch(name + ".prime");
  std::ofstream(file, std::ios::binary) << source;
  const std::string exe = scratch(name + "_optexe");
  const std::string log = exe + ".log";
  const int status = run(shellQuote(Primec) + " --emit=optexe " + shellQuote(file) + " -o " + shellQuote(exe) + " > " +
                         shellQuote(log) + " 2>&1");
  REQUIRE_MESSAGE(status == 0, readFile(log));
  const std::string out = exe + ".out";
  CHECK(run(shellQuote(exe) + " > " + shellQuote(out)) == 0);
  return decode(readFile(out));
}

} // namespace

TEST_CASE("glob matches star, double star, question mark and dot names on the VM") {
  const std::filesystem::path root = makeFixture("vm_fixture");
  const std::vector<Case> list = cases();
  CHECK(vmOutput(program(root, list)) == expectedText(root, list));
}

TEST_CASE("glob gives the same matches and listings through optexe") {
  const std::filesystem::path root = makeFixture("optexe_fixture");
  const std::vector<Case> list = cases();
  CHECK(optexeOutput("glob", program(root, list)) == expectedText(root, list));
}

TEST_CASE("glob lists exactly the directories it read, so a new file in one changes the answer") {
  // The listing is what a build records as input: adding "sub/new.prime" changes the matches of "sub/*.prime" and
  // leaves the unread "a" directory out of the listing.
  const std::filesystem::path root = makeFixture("vm_changed");
  const std::vector<Case> before = {{"", "sub/*.prime", {"sub/c.prime"}, {"", "sub"}, ""}};
  CHECK(vmOutput(program(root, before)) == expectedText(root, before));
  makeFile(root / "sub" / "new.prime");
  const std::vector<Case> after = {{"", "sub/*.prime", {"sub/c.prime", "sub/new.prime"}, {"", "sub"}, ""}};
  CHECK(vmOutput(program(root, after)) == expectedText(root, after));
}

TEST_CASE("glob stops a walk through a link to a parent directory with an error") {
  const std::filesystem::path root = makeFixture("vm_loop");
  std::filesystem::create_directory_symlink(root, root / "sub" / "loop");
  // The error code is the fixed number 40: Linux names it ELOOP, macOS has no name for it.
#if defined(__APPLE__)
  const std::string loopName = "Unknown file error";
#else
  const std::string loopName = "ELOOP";
#endif
  const std::vector<Case> list = {{"", "**/never", {}, {}, loopName}};
  CHECK(vmOutput(program(root, list)) == expectedText(root, list));
}

TEST_SUITE_END();
