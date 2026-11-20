#include "primec/support/TextFilterOffsetMap.h"
#include "primec/support/TextFilterPipeline.h"

#include "third_party/doctest.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("primestruct.text_filters.offset_map");

// The offset map from the filtered text back to the source (TODO-6080): every filtered byte knows the source byte it
// was copied from, so a diagnostic that the parser or the validator reports in the filtered text is shown at the
// column and with the snippet of the text the user wrote.

namespace {

struct FilteredSource {
  std::string filtered;
  std::vector<std::uint32_t> origins;
};

FilteredSource filterWithOrigins(const std::string &source) {
  FilteredSource result;
  std::string error;
  CHECK(primec::TextFilterPipeline().applyWithOrigins(
      source, result.filtered, result.origins, error));
  CHECK(error.empty());
  return result;
}

// Where `needle` first starts in `text`, as a 1-based line and column.
bool lineAndColumnOf(const std::string &text, const std::string &needle, int &line, int &column) {
  const size_t found = text.find(needle);
  if (found == std::string::npos) {
    return false;
  }
  line = 1 + static_cast<int>(
                 std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(found), '\n'));
  size_t lineStart = 0;
  if (found > 0) {
    const size_t newline = text.rfind('\n', found - 1);
    lineStart = newline == std::string::npos ? 0 : newline + 1;
  }
  column = static_cast<int>(found - lineStart) + 1;
  return true;
}

// The bytes a filter copies keep their order and their value; every other byte is marked as written by the filter.
void checkOriginsAreCopies(const std::string &source, const FilteredSource &filtered) {
  REQUIRE(filtered.origins.size() == filtered.filtered.size());
  std::uint32_t previous = 0;
  for (size_t i = 0; i < filtered.filtered.size(); ++i) {
    const std::uint32_t origin = filtered.origins[i];
    if (origin == primec::TextFilterSynthesizedOrigin) {
      continue;
    }
    REQUIRE(origin < source.size());
    REQUIRE_MESSAGE(filtered.filtered[i] == source[origin],
                    "filtered offset " << i << " origin " << origin);
    REQUIRE_MESSAGE(origin >= previous,
                    "filtered offset " << i << " origin " << origin << " after " << previous);
    previous = origin;
  }
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::filesystem::path repoRoot() {
  const auto cwd = std::filesystem::current_path();
  const auto marker = std::filesystem::path("stdlib") / "std" / "collections" / "map.prime";
  if (std::filesystem::exists(cwd / marker)) {
    return cwd;
  }
  return cwd.parent_path();
}

} // namespace

TEST_CASE("text without rewrites keeps every offset") {
  const std::string source = "main() {\n  [i32] a{1i32}\n  return(a)\n}\n";
  const FilteredSource filtered = filterWithOrigins(source);
  CHECK(filtered.filtered == source);
  REQUIRE(filtered.origins.size() == source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    CHECK(filtered.origins[i] == i);
  }
}

TEST_CASE("an operand of a rewritten operator maps to its place in the source") {
  const std::string source =
      "[return<int>]\nmain() {\n  [i32] b{a + 2i32 * nope}\n  return(b)\n}\n";
  primec::TextFilterOffsetMap map;
  REQUIRE(map.build(source, primec::TextFilterOptions{}));
  CHECK(map.filteredText().find("plus(a, multiply(2i32, nope))") != std::string::npos);
  int filteredLine = 0;
  int filteredColumn = 0;
  REQUIRE(lineAndColumnOf(map.filteredText(), "nope", filteredLine, filteredColumn));
  int sourceLine = 0;
  int sourceColumn = 0;
  REQUIRE(map.mapToSource(filteredLine, filteredColumn, sourceLine, sourceColumn));
  int expectedLine = 0;
  int expectedColumn = 0;
  REQUIRE(lineAndColumnOf(source, "nope", expectedLine, expectedColumn));
  CHECK(filteredColumn != expectedColumn);
  CHECK(sourceLine == expectedLine);
  CHECK(sourceColumn == expectedColumn);
}

TEST_CASE("text after a rewritten span on the same line maps to its place in the source") {
  const std::string source = "main() {\n  call(a * 2i32, tail)\n}\n";
  primec::TextFilterOffsetMap map;
  REQUIRE(map.build(source, primec::TextFilterOptions{}));
  int filteredLine = 0;
  int filteredColumn = 0;
  REQUIRE(lineAndColumnOf(map.filteredText(), "tail", filteredLine, filteredColumn));
  int sourceLine = 0;
  int sourceColumn = 0;
  REQUIRE(map.mapToSource(filteredLine, filteredColumn, sourceLine, sourceColumn));
  int expectedLine = 0;
  int expectedColumn = 0;
  REQUIRE(lineAndColumnOf(source, "tail", expectedLine, expectedColumn));
  CHECK(sourceLine == expectedLine);
  CHECK(sourceColumn == expectedColumn);
}

TEST_CASE("a byte the filter wrote takes the position of the next copied byte") {
  const std::string source = "main() {\n  x{a + b}\n}\n";
  primec::TextFilterOffsetMap map;
  REQUIRE(map.build(source, primec::TextFilterOptions{}));
  int callLine = 0;
  int callColumn = 0;
  REQUIRE(lineAndColumnOf(map.filteredText(), "plus(", callLine, callColumn));
  int sourceLine = 0;
  int sourceColumn = 0;
  REQUIRE(map.mapToSource(callLine, callColumn, sourceLine, sourceColumn));
  int operandLine = 0;
  int operandColumn = 0;
  REQUIRE(lineAndColumnOf(source, "a + b", operandLine, operandColumn));
  CHECK(sourceLine == operandLine);
  CHECK(sourceColumn == operandColumn);
}

TEST_CASE("a position outside the filtered text maps nothing") {
  primec::TextFilterOffsetMap map;
  int line = 0;
  int column = 0;
  CHECK_FALSE(map.mapToSource(1, 1, line, column));
  REQUIRE(map.build("main() {}\n", primec::TextFilterOptions{}));
  CHECK_FALSE(map.mapToSource(0, 1, line, column));
  CHECK_FALSE(map.mapToSource(9, 1, line, column));
}

TEST_CASE("every rewrite keeps the origin of the bytes it copies") {
  const std::vector<std::string> sources = {
      "main() {\n  [i32] v{-x + !flag}\n  y = &z\n  w = *p\n  i++\n  j--\n}\n",
      "main() {\n  [i32] v{values[i + 1] * 3}\n  total = total + rows[0][k]\n}\n",
      "main() {\n  [vector<i32>] v{vector<i32>{1, 2, 3}}\n  [array<i32>] a{array<i32>{4, 5}}\n}\n",
      "main() {\n  print_line(\"text\")\n  [string] s{\"abc\"}\n  [i32] n{1,234}\n}\n",
      "main() {\n  [f32] f{1.5f * 2.0f}\n  [i32] h{0x2A + 7}\n  r = a < b && c >= d || !e\n}\n",
      "[struct]\nBox() {\n  [i32] base{1 + 2}\n}\n\nmain() {\n  [Box] b{Box{[base] 3 * 4}}\n  "
      "return(b.base - -1)\n}\n",
      "main() {\n  [i32] v{pick(x) { a = 1 + 2\n b = 3 * 4 }}\n}\n",
      "// a + b in a comment\nmain() {\n  /* c * d */\n  x = a / b\n  y = f(a + g(b * c), -d)\n}\n",
      "import /std/math/*\n\n[return<int>]\nmain() {\n  [i32] k{max(1 + 2, 3)}\n  return(k)\n}\n",
  };
  for (const std::string &source : sources) {
    INFO(source);
    const FilteredSource filtered = filterWithOrigins(source);
    checkOriginsAreCopies(source, filtered);
    std::string plain;
    std::string error;
    CHECK(primec::TextFilterPipeline().apply(source, plain, error));
    CHECK(plain == filtered.filtered);
  }
}

TEST_CASE("tracking the origins does not change the filtered text of the repository sources") {
  const std::filesystem::path root = repoRoot();
  REQUIRE(std::filesystem::exists(root / "stdlib"));
  std::vector<std::filesystem::path> files;
  for (const char *directory : {"stdlib", "examples"}) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root / directory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".prime") {
        files.push_back(entry.path());
      }
    }
  }
  std::sort(files.begin(), files.end());
  REQUIRE(files.size() > 50);
  for (const std::filesystem::path &file : files) {
    const std::string source = readFile(file);
    std::string plain;
    std::string error;
    const bool plainOk = primec::TextFilterPipeline().apply(source, plain, error);
    FilteredSource tracked;
    std::string trackedError;
    const bool trackedOk = primec::TextFilterPipeline().applyWithOrigins(
        source, tracked.filtered, tracked.origins, trackedError);
    INFO(file.string());
    REQUIRE(plainOk == trackedOk);
    if (!plainOk) {
      continue;
    }
    REQUIRE(plain == tracked.filtered);
    checkOriginsAreCopies(source, tracked);
  }
}

TEST_SUITE_END();
