#include "primec/ui/MarkdownEdit.h"

#include "third_party/doctest.h"

#include <string>

using primec::ui::MarkdownEditOp;

TEST_SUITE_BEGIN("primestruct.ui.markdown_edit");

namespace {

// Texts are written with the selection marked: «...» for a selection and ¦ for a caret.
struct Marked {
  std::string text;
  size_t start = 0;
  size_t end = 0;
};

Marked parse(const std::string &marked) {
  Marked out;
  bool haveStart = false;
  for (size_t i = 0; i < marked.size();) {
    if (marked.compare(i, 2, "\xC2\xAB") == 0) { // «
      out.start = out.text.size();
      haveStart = true;
      i += 2;
    } else if (marked.compare(i, 2, "\xC2\xBB") == 0) { // »
      out.end = out.text.size();
      i += 2;
    } else if (marked.compare(i, 2, "\xC2\xA6") == 0) { // ¦
      out.start = out.end = out.text.size();
      haveStart = true;
      i += 2;
    } else {
      out.text += marked[i++];
    }
  }
  (void)haveStart;
  return out;
}

std::string render(const std::string &text, size_t start, size_t end) {
  std::string out = text.substr(0, start);
  out += start == end ? "\xC2\xA6" : "\xC2\xAB";
  out += text.substr(start, end - start);
  if (start != end) {
    out += "\xC2\xBB";
  }
  out += text.substr(end);
  return out;
}

std::string run(const std::string &marked, MarkdownEditOp op, int a = 0, int b = 0) {
  const Marked in = parse(marked);
  const auto result = primec::ui::markdownEdit(in.text, in.start, in.end, op, a, b);
  if (!result.applied) {
    return "(not applied)";
  }
  return render(primec::ui::applyMarkdownEdit(in.text, result), result.selectionStart, result.selectionEnd);
}

} // namespace

TEST_CASE("markdown edit wraps a selection and the word at the caret") {
  CHECK(run("a «bold» b", MarkdownEditOp::Bold) == "a **«bold»** b");
  CHECK(run("a bo¦ld b", MarkdownEditOp::Bold) == "a **«bold»** b");
  CHECK(run("a «two words» b", MarkdownEditOp::Italic) == "a *«two words»* b");
  CHECK(run("a «x» b", MarkdownEditOp::Strike) == "a ~~«x»~~ b");
  CHECK(run("a «x» b", MarkdownEditOp::Code) == "a `«x»` b");
}

TEST_CASE("markdown edit takes marks off again") {
  CHECK(run("a **«bold»** b", MarkdownEditOp::Bold) == "a «bold» b");
  CHECK(run("a **«bold»** b", MarkdownEditOp::Bold) == "a «bold» b");
  CHECK(run("a *«it»* b", MarkdownEditOp::Italic) == "a «it» b");
  CHECK(run("a ~~«x»~~ b", MarkdownEditOp::Strike) == "a «x» b");
  CHECK(run("a `«x»` b", MarkdownEditOp::Code) == "a «x» b");
}

TEST_CASE("markdown edit combines bold and italic") {
  CHECK(run("a **«b»** c", MarkdownEditOp::Italic) == "a ***«b»*** c");
  CHECK(run("a ***«b»*** c", MarkdownEditOp::Italic) == "a **«b»** c");
  CHECK(run("a ***«b»*** c", MarkdownEditOp::Bold) == "a *«b»* c");
}

TEST_CASE("markdown edit keeps spaces outside the marks and handles empty text") {
  CHECK(run("a« word »b", MarkdownEditOp::Bold) == "a **«word»** b");
  CHECK(run("a ¦ b", MarkdownEditOp::Bold) == "a **¦** b");
  CHECK(run("\xC3\xA5\xC3\xA4¦\xC3\xB6 x", MarkdownEditOp::Bold) == "**«\xC3\xA5\xC3\xA4\xC3\xB6»** x");
}

TEST_CASE("markdown edit builds links and images") {
  CHECK(run("see «docs» now", MarkdownEditOp::Link) == "see [docs](«url») now");
  CHECK(run("see «https://x.org» now", MarkdownEditOp::Link) == "see [«text»](https://x.org) now");
  CHECK(run("see ¦ now", MarkdownEditOp::Link) == "see [«text»](url) now");
  CHECK(run("see «cat» now", MarkdownEditOp::Image) == "see ![cat](«url») now");
  CHECK(run("see ¦ now", MarkdownEditOp::Image) == "see ![«alt»](url) now");
}

TEST_CASE("markdown edit sets and clears headings") {
  CHECK(run("¦title", MarkdownEditOp::Heading, 2) == "## ¦title");
  CHECK(run("## ¦title", MarkdownEditOp::Heading, 2) == "¦title");
  CHECK(run("## ¦title", MarkdownEditOp::Heading, 1) == "# ¦title");
  CHECK(run("## ¦title", MarkdownEditOp::Heading, 0) == "¦title");
  CHECK(run("«a\nb»", MarkdownEditOp::Heading, 3) == "«### a\n### b»");
}

TEST_CASE("markdown edit toggles and converts lists") {
  CHECK(run("¦item", MarkdownEditOp::Bullets) == "- ¦item");
  CHECK(run("- ¦item", MarkdownEditOp::Bullets) == "¦item");
  CHECK(run("«a\nb\nc»", MarkdownEditOp::Numbers) == "«1. a\n2. b\n3. c»");
  CHECK(run("«1. a\n2. b»", MarkdownEditOp::Numbers) == "«a\nb»");
  CHECK(run("«1. a\n2. b»", MarkdownEditOp::Bullets) == "«- a\n- b»");
  CHECK(run("«a\nb»", MarkdownEditOp::Tasks) == "«- [ ] a\n- [ ] b»");
  CHECK(run("«a\n\nb»", MarkdownEditOp::Bullets) == "«- a\n\n- b»");
}

TEST_CASE("markdown edit toggles quotes") {
  CHECK(run("¦text", MarkdownEditOp::Quote) == "> ¦text");
  CHECK(run("> ¦text", MarkdownEditOp::Quote) == "¦text");
  CHECK(run("«a\nb»", MarkdownEditOp::Quote) == "«> a\n> b»");
}

TEST_CASE("markdown edit wraps lines in code fences") {
  CHECK(run("«a\nb»", MarkdownEditOp::CodeBlock) == "```\n«a\nb»\n```");
  CHECK(run("```\n«a»\n```", MarkdownEditOp::CodeBlock) == "«a»");
  CHECK(run("x\n¦", MarkdownEditOp::CodeBlock) == "x\n```\n¦\n```");
}

TEST_CASE("markdown edit puts a rule on its own line") {
  CHECK(run("a¦", MarkdownEditOp::Rule) == "a\n\n---\n¦");
  CHECK(run("a\n\n¦\n\nb", MarkdownEditOp::Rule) == "a\n\n---\n¦\nb");
}

TEST_CASE("markdown edit inserts tables") {
  CHECK(run("¦", MarkdownEditOp::TableInsert, 2, 1) ==
        "| «Column 1» | Column 2 |\n| -------- | -------- |\n|          |          |");
  CHECK(run("para¦", MarkdownEditOp::TableInsert, 1, 1) ==
        "para\n\n| «Column 1» |\n| -------- |\n|          |");
}

TEST_CASE("markdown edit formats tables") {
  CHECK(run("|a|bb|\n|-|:-:|\n|ccc|d|¦", MarkdownEditOp::TableFormat) ==
        "| a   | bb  |\n| --- | :-: |\n| ccc |  ¦d  |");
}

TEST_CASE("markdown edit changes table rows and columns") {
  const std::string table = "| a | b |\n| --- | --- |\n| 1¦ | 2 |";
  CHECK(run(table, MarkdownEditOp::TableAddRow) ==
        "| a   | b   |\n| --- | --- |\n| 1   | 2   |\n| ¦    |     |");
  CHECK(run(table, MarkdownEditOp::TableDeleteRow) == "| ¦a   | b   |\n| --- | --- |");
  CHECK(run(table, MarkdownEditOp::TableDeleteColumn) == "| b   |\n| --- |\n| ¦2   |");
  CHECK(run("| a¦ | b |\n| --- | --- |", MarkdownEditOp::TableAddColumn) ==
        "| a   | «Column 3» | b   |\n| --- | -------- | --- |");
}

TEST_CASE("markdown edit aligns table columns") {
  CHECK(run("| a¦ | b |\n| --- | --- |", MarkdownEditOp::TableAlign, 2) ==
        "|  ¦a  | b   |\n| :-: | --- |");
  CHECK(run("| a¦ | b |\n| --- | --- |", MarkdownEditOp::TableAlign, 3) ==
        "|   ¦a | b   |\n| --: | --- |");
}

TEST_CASE("markdown edit moves between table cells") {
  const std::string table = "| a¦ | b |\n| --- | --- |\n| 1 | 2 |";
  CHECK(run(table, MarkdownEditOp::TableNextCell) ==
        "| a   | «b»   |\n| --- | --- |\n| 1   | 2   |");
  CHECK(run("| a | b¦ |\n| --- | --- |\n| 1 | 2 |", MarkdownEditOp::TableNextCell) ==
        "| a   | b   |\n| --- | --- |\n| «1»   | 2   |");
  CHECK(run("| a | b |\n| --- | --- |\n| 1 | 2¦ |", MarkdownEditOp::TableNextCell) ==
        "| a   | b   |\n| --- | --- |\n| 1   | 2   |\n| ¦    |     |");
  CHECK(run(table, MarkdownEditOp::TablePreviousCell) == "(not applied)");
}

TEST_CASE("markdown edit leaves non-tables alone") {
  CHECK(run("plain¦ text", MarkdownEditOp::TableFormat) == "(not applied)");
  CHECK(run("a | b¦\nc | d", MarkdownEditOp::TableNextCell) == "(not applied)");
  CHECK(run("```\n| a¦ | b |\n| - | - |\n```", MarkdownEditOp::TableFormat) == "(not applied)");
}

TEST_CASE("markdown edit keeps escaped pipes and pads ragged rows") {
  CHECK(run("| a¦ | b |\n| - | - |\n| x \\| y |", MarkdownEditOp::TableFormat) ==
        "| ¦a      | b   |\n| ------ | --- |\n| x \\| y |     |");
}

TEST_SUITE_END();
