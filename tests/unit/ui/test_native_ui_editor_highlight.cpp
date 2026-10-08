#include "primec/embed/ScriptEngine.h"
#include "primec/ui/NativeUi.h"
#include "primec/ui/NativeUiBindings.h"
#include "primec/ui/NativeUiHeadless.h"
#include "primec/ui/SyntaxHighlight.h"

#include "third_party/doctest.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// Highlighting part of the editor acceptance gate: the editor opens
// one sample per language and the headless backend shows the style runs.

namespace headless = primec::ui::headless;
namespace colors = primec::ui::highlight_colors;
using primec::embed::Script;
using primec::embed::ScriptEngine;
using primec::ui::Language;

TEST_SUITE_BEGIN("primestruct.ui.editor_acceptance");

namespace {
constexpr int32_t CommandOpen = 2;
constexpr uint64_t EditorWindow = 1;
constexpr uint64_t EditorView = 2;

std::string fixturePath(const std::string &name) {
  return std::string(PRIMESTRUCT_SOURCE_DIR) + "/tests/fixtures/ui/highlight/" + name;
}

std::string readFile(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Opens `path` in the editor, optionally types `typed`, closes without a prompt
// (typing is followed by a Don't Save answer), and returns the style runs.
std::vector<headless::StyleRun> openInEditor(const std::string &path, const std::string &typed = "") {
  headless::reset();
  headless::pushCommand(CommandOpen);
  headless::pushOpenPanelAnswer(path);
  if (!typed.empty()) {
    headless::pushTypeText(0, typed);
    headless::pushAlertAnswer(1);
  }
  headless::pushCloseWindow(EditorWindow);
  ScriptEngine engine;
  primec::ui::bindNativeUi(engine);
  const Script script = engine.compileFile(std::string(PRIMESTRUCT_SOURCE_DIR) + "/examples/apps/text_editor/main.prime");
  REQUIRE_MESSAGE(script.valid(), script.diagnostics());
  const auto result = script.run();
  REQUIRE_MESSAGE(result.ok, result.diagnostics);
  return headless::styleRuns(EditorView);
}

struct Expect {
  const char *slice;
  int32_t rgb;
  int32_t flags;
};

// True when some run covers exactly `slice` of `text` with the given style.
bool hasRun(const std::vector<headless::StyleRun> &runs, const std::string &text, const Expect &expect) {
  for (const auto &run : runs) {
    if (text.substr(static_cast<size_t>(run.start), static_cast<size_t>(run.end - run.start)) == expect.slice &&
        run.rgb == expect.rgb && run.flags == expect.flags) {
      return true;
    }
  }
  return false;
}

void checkSample(const std::string &name, Language language, const std::vector<Expect> &expected) {
  const std::string path = fixturePath(name);
  const std::string text = readFile(path);
  REQUIRE(!text.empty());
  const auto runs = openInEditor(path);
  std::string dump;
  for (const auto &run : runs) {
    dump += "[" + text.substr(static_cast<size_t>(run.start), static_cast<size_t>(run.end - run.start)) + "|" +
            std::to_string(run.rgb) + "," + std::to_string(run.flags) + "] ";
  }
  for (const Expect &expect : expected) {
    CHECK_MESSAGE(hasRun(runs, text, expect), name << ": " << expect.slice << " " << expect.rgb << "," << expect.flags
                                                   << " in " << dump);
  }
  // The editor shows exactly what the highlighter computes, in order, without
  // overlaps, on code point boundaries (the backend rejects anything else).
  const auto direct = primec::ui::highlight(language, text);
  REQUIRE(runs.size() == direct.size());
  int32_t previousEnd = 0;
  for (size_t i = 0; i < runs.size(); ++i) {
    CHECK(runs[i].start == direct[i].start);
    CHECK(runs[i].end == direct[i].end);
    CHECK(runs[i].start >= previousEnd);
    previousEnd = runs[i].end;
  }
}
} // namespace

TEST_CASE("highlight: language follows the file extension") {
  using primec::ui::languageForPath;
  CHECK(languageForPath("/a/b/main.prime") == Language::PrimeStruct);
  CHECK(languageForPath("x.CPP") == Language::Cpp);
  CHECK(languageForPath("x.hpp") == Language::Cpp);
  CHECK(languageForPath("x.mm") == Language::Cpp);
  CHECK(languageForPath("tool.py") == Language::Python);
  CHECK(languageForPath("data.json") == Language::Json);
  CHECK(languageForPath("README.md") == Language::Markdown);
  CHECK(languageForPath("notes.txt") == Language::None);
  CHECK(languageForPath("Makefile") == Language::None);
  CHECK(languageForPath("dir.d/noext") == Language::None);
  CHECK(languageForPath("") == Language::None);
}

TEST_CASE("highlight: PrimeStruct sample") {
  checkSample("sample.prime", Language::PrimeStruct,
              {{"import", colors::Keyword, 0},
               {"// Adds one.", colors::Comment, PS_UI_STYLE_ITALIC},
               {"/* block */", colors::Comment, PS_UI_STYLE_ITALIC},
               {"return", colors::Keyword, 0},
               {"i32", colors::Type, 0},
               {"string", colors::Type, 0},
               {"1i32", colors::Number, 0},
               {"\"h\xC3\xA9llo, \xE6\x97\xA5\xE6\x9C\xAC\"", colors::String, 0}});
}

TEST_CASE("highlight: C++ sample") {
  checkSample("sample.cpp", Language::Cpp,
              {{"#include <vector>", colors::Keyword, 0},
               {"// counts items", colors::Comment, PS_UI_STYLE_ITALIC},
               {"/* total */", colors::Comment, PS_UI_STYLE_ITALIC},
               {"int", colors::Type, 0},
               {"char", colors::Type, 0},
               {"const", colors::Keyword, 0},
               {"for", colors::Keyword, 0},
               {"return", colors::Keyword, 0},
               {"\"items: 42\"", colors::String, 0},
               {"42", colors::Number, 0}});
}

TEST_CASE("highlight: Python sample") {
  checkSample("sample.py", Language::Python,
              {{"# say hello", colors::Comment, PS_UI_STYLE_ITALIC},
               {"def", colors::Keyword, 0},
               {"str", colors::Type, 0},
               {"\"\"\"Docstring with 'quotes'.\"\"\"", colors::String, 0},
               {"3.5", colors::Number, 0},
               {"if", colors::Keyword, 0},
               {"None", colors::Keyword, 0},
               {"'nobody'", colors::String, 0},
               {"\"hello {name}\"", colors::String, 0}});
}

TEST_CASE("highlight: JSON sample") {
  checkSample("sample.json", Language::Json,
              {{"\"name\"", colors::Type, 0},
               {"\"tags\"", colors::Type, 0},
               {"\"text editor\"", colors::String, 0},
               {"2", colors::Number, 0},
               {"-1.5e3", colors::Number, 0},
               {"\"a\"", colors::String, 0},
               {"\"\xE6\x97\xA5\xE6\x9C\xAC\"", colors::String, 0},
               {"true", colors::Keyword, 0},
               {"null", colors::Keyword, 0}});
}

TEST_CASE("highlight: Markdown sample") {
  checkSample("sample.md", Language::Markdown,
              {{"# Title", colors::Heading, PS_UI_STYLE_BOLD},
               {"*emphasis*", colors::Marker, PS_UI_STYLE_ITALIC},
               {"**strong**", colors::Strong, PS_UI_STYLE_BOLD},
               {"`code`", colors::Code, 0},
               {"[link](http://x.y)", colors::Link, 0},
               {"-", colors::Marker, PS_UI_STYLE_BOLD},
               {"1.", colors::Marker, PS_UI_STYLE_BOLD},
               {"> quoted", colors::Comment, PS_UI_STYLE_ITALIC},
               {"```", colors::Code, 0},
               {"fenced code", colors::Code, 0}});
}

TEST_CASE("highlight: strings and comments hide their contents from the lexer") {
  const auto runs = primec::ui::highlight(Language::Cpp, "// int return 42\nconst char *s = \"if 7 //\";\n");
  REQUIRE(runs.size() == 4);
  CHECK(runs[0] == primec::ui::HighlightRun{0, 16, colors::Comment, PS_UI_STYLE_ITALIC});
  CHECK(runs[1].rgb == colors::Keyword); // const
  CHECK(runs[2].rgb == colors::Type);    // char
  CHECK(runs[3].rgb == colors::String);  // the whole literal, one run
  CHECK(runs[3].end - runs[3].start == 9);
}

TEST_CASE("highlight: unknown extensions and untitled documents get no styles") {
  CHECK(openInEditor(fixturePath("notes.txt")).empty());
}

TEST_CASE("highlight: the editor restyles after typing") {
  const std::string path = fixturePath("sample.json");
  const auto before = openInEditor(path);
  const auto after = openInEditor(path, " true");
  REQUIRE(!before.empty());
  // The typed text is appended to the document; the whole text is restyled.
  CHECK(after.size() >= before.size());
}
