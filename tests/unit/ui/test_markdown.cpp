#include "primec/ui/Markdown.h"

#include "third_party/doctest.h"

#include <string>

using primec::ui::markdownPreviewPage;
using primec::ui::markdownToHtml;

TEST_SUITE_BEGIN("primestruct.ui.markdown");

TEST_CASE("headings, setext headings, paragraphs and breaks") {
  CHECK(markdownToHtml("# One\n## Two ##\n###### Six\n####### seven") ==
        "<h1>One</h1>\n<h2>Two</h2>\n<h6>Six</h6>\n<p>####### seven</p>\n");
  CHECK(markdownToHtml("Title\n=====\n\nSub\n---\n") == "<h1>Title</h1>\n<h2>Sub</h2>\n");
  CHECK(markdownToHtml("one\ntwo\n\nthree") == "<p>one\ntwo</p>\n<p>three</p>\n");
  CHECK(markdownToHtml("a  \nb") == "<p>a<br>\nb</p>\n");
  CHECK(markdownToHtml("a\\\nb") == "<p>a<br>\nb</p>\n");
  CHECK(markdownToHtml("a\r\nb") == "<p>a\nb</p>\n");
  CHECK(markdownToHtml("a\n\n---\n\n***\n\n___") == "<p>a</p>\n<hr>\n<hr>\n<hr>\n");
}

TEST_CASE("emphasis, strong, strikethrough and escapes") {
  CHECK(markdownToHtml("*a* _b_ **c** __d__") == "<p><em>a</em> <em>b</em> <strong>c</strong> <strong>d</strong></p>\n");
  CHECK(markdownToHtml("***e***") == "<p><em><strong>e</strong></em></p>\n");
  CHECK(markdownToHtml("~~gone~~ ~kept~") == "<p><del>gone</del> ~kept~</p>\n");
  CHECK(markdownToHtml("snake_case_name and 2*3*4") == "<p>snake_case_name and 2<em>3</em>4</p>\n");
  CHECK(markdownToHtml("*not a list*") == "<p><em>not a list</em></p>\n");
  CHECK(markdownToHtml("\\*not\\* a & b < c") == "<p>*not* a &amp; b &lt; c</p>\n");
  CHECK(markdownToHtml("&copy; &amp; &#35;") == "<p>&copy; &amp; &#35;</p>\n");
  CHECK(markdownToHtml("héllo *wörld* 日本語") == "<p>héllo <em>wörld</em> 日本語</p>\n");
  CHECK(markdownToHtml("**unclosed and *mixed") == "<p>**unclosed and *mixed</p>\n");
}

TEST_CASE("code spans and fenced and indented code") {
  CHECK(markdownToHtml("`a``b` and `` x`y ``") == "<p><code>a``b</code> and <code>x`y</code></p>\n");
  CHECK(markdownToHtml("`<b>&`") == "<p><code>&lt;b&gt;&amp;</code></p>\n");
  CHECK(markdownToHtml("```cpp\nint a = 1 < 2;\n```") ==
        "<pre><code class=\"language-cpp\">int a = 1 &lt; 2;\n</code></pre>\n");
  CHECK(markdownToHtml("~~~ python extra\nprint(1)\n~~~") ==
        "<pre><code class=\"language-python\">print(1)\n</code></pre>\n");
  CHECK(markdownToHtml("```\nx") == "<pre><code>x\n</code></pre>\n");
  CHECK(markdownToHtml("````\n```\nnested\n```\n````") == "<pre><code>```\nnested\n```\n</code></pre>\n");
  CHECK(markdownToHtml("    code <x>\n\n    more") == "<pre><code>code &lt;x&gt;\n\nmore\n</code></pre>\n");
}

TEST_CASE("links, images and autolinks") {
  CHECK(markdownToHtml("[link](https://example.com \"t\")") ==
        "<p><a href=\"https://example.com\" title=\"t\">link</a></p>\n");
  CHECK(markdownToHtml("![alt text](pic.png)") == "<p><img src=\"pic.png\" alt=\"alt text\"></p>\n");
  CHECK(markdownToHtml("[*styled* text](docs/a.md#top)") ==
        "<p><a href=\"docs/a.md#top\"><em>styled</em> text</a></p>\n");
  CHECK(markdownToHtml("<https://x.org> and <a@b.co>") ==
        "<p><a href=\"https://x.org\">https://x.org</a> and <a href=\"mailto:a@b.co\">a@b.co</a></p>\n");
  CHECK(markdownToHtml("see https://example.com/a_b, now.") ==
        "<p>see <a href=\"https://example.com/a_b\">https://example.com/a_b</a>, now.</p>\n");
  CHECK(markdownToHtml("(https://e.com/x_(y))") == "<p>(<a href=\"https://e.com/x_(y)\">https://e.com/x_(y)</a>)</p>\n");
  CHECK(markdownToHtml("[open](https://e.com/a(b)c)") == "<p><a href=\"https://e.com/a(b)c\">open</a></p>\n");
  CHECK(markdownToHtml("[not a link] (x)") == "<p>[not a link] (x)</p>\n");
}

TEST_CASE("a preview cannot run scripts") {
  CHECK(markdownToHtml("[x](javascript:alert(1))") == "<p><a href=\"#\">x</a></p>\n");
  CHECK(markdownToHtml("[x](JaVaScRiPt:alert(1))") == "<p><a href=\"#\">x</a></p>\n");
  CHECK(markdownToHtml("[x](data:text/html;base64,AAA)") == "<p><a href=\"#\">x</a></p>\n");
  CHECK(markdownToHtml("![i](javascript:alert(1))") == "<p><img src=\"#\" alt=\"i\"></p>\n");
  CHECK(markdownToHtml("![i](data:image/png;base64,AAA)") == "<p><img src=\"data:image/png;base64,AAA\" alt=\"i\"></p>\n");
  CHECK(markdownToHtml("[m](mailto:a@b.c)") == "<p><a href=\"mailto:a@b.c\">m</a></p>\n");
  CHECK(markdownToHtml("<script>alert(1)</script>") == "<p>&lt;script&gt;alert(1)&lt;/script&gt;</p>\n");
  // A target is data: its quotes are escaped, so it cannot close the attribute.
  CHECK(markdownToHtml("[q](\"onclick=\"x)") == "<p><a href=\"&quot;onclick=&quot;x\">q</a></p>\n");
  CHECK(markdownToHtml("[t](a \"x\\\" onmouseover=\\\"y\")") ==
        "<p><a href=\"a\" title=\"x&quot; onmouseover=&quot;y\">t</a></p>\n");
}

TEST_CASE("lists: tight, loose, ordered, nested, tasks") {
  CHECK(markdownToHtml("- a\n- b") == "<ul>\n<li>a</li>\n<li>b</li>\n</ul>\n");
  CHECK(markdownToHtml("- a\n- b\n\n- c") == "<ul>\n<li><p>a</p></li>\n<li><p>b</p></li>\n<li><p>c</p></li>\n</ul>\n");
  CHECK(markdownToHtml("1. x\n2. y") == "<ol>\n<li>x</li>\n<li>y</li>\n</ol>\n");
  CHECK(markdownToHtml("3. x\n4. y") == "<ol start=\"3\">\n<li>x</li>\n<li>y</li>\n</ol>\n");
  CHECK(markdownToHtml("- a\n  - b\n    - c\n- d") ==
        "<ul>\n<li>a\n<ul>\n<li>b\n<ul>\n<li>c</li>\n</ul></li>\n</ul></li>\n<li>d</li>\n</ul>\n");
  CHECK(markdownToHtml("- [ ] todo\n- [x] done") ==
        "<ul>\n<li><input type=\"checkbox\" disabled> todo</li>\n<li><input type=\"checkbox\" disabled checked> done</li>\n</ul>\n");
  CHECK(markdownToHtml("- a\n* b") == "<ul>\n<li>a</li>\n</ul>\n<ul>\n<li>b</li>\n</ul>\n");
  CHECK(markdownToHtml("- a\ncontinued\n- b") == "<ul>\n<li>a\ncontinued</li>\n<li>b</li>\n</ul>\n");
  CHECK(markdownToHtml("para\n- item") == "<p>para</p>\n<ul>\n<li>item</li>\n</ul>\n");
  CHECK(markdownToHtml("- item\n\n  second paragraph\n- next") ==
        "<ul>\n<li><p>item</p>\n<p>second paragraph</p></li>\n<li><p>next</p></li>\n</ul>\n");
  CHECK(markdownToHtml("-\n- b") == "<ul>\n<li></li>\n<li>b</li>\n</ul>\n");
}

TEST_CASE("block quotes and tables") {
  CHECK(markdownToHtml("> quote\n> more") == "<blockquote>\n<p>quote\nmore</p>\n</blockquote>\n");
  CHECK(markdownToHtml("> # Head\n> - item") == "<blockquote>\n<h1>Head</h1>\n<ul>\n<li>item</li>\n</ul>\n</blockquote>\n");
  CHECK(markdownToHtml("> a\nlazy") == "<blockquote>\n<p>a\nlazy</p>\n</blockquote>\n");
  CHECK(markdownToHtml("> outer\n> > inner") ==
        "<blockquote>\n<p>outer</p>\n<blockquote>\n<p>inner</p>\n</blockquote>\n</blockquote>\n");
  CHECK(markdownToHtml("| a | b |\n|:--|--:|\n| 1 | 2 |") ==
        "<table>\n<thead>\n<tr><th style=\"text-align:left\">a</th><th style=\"text-align:right\">b</th></tr>\n</thead>\n"
        "<tbody>\n<tr><td style=\"text-align:left\">1</td><td style=\"text-align:right\">2</td></tr>\n</tbody>\n</table>\n");
  CHECK(markdownToHtml("| a | b |\n|---|---|\n| 1 |\n| 2 | 3 | 4 |") ==
        "<table>\n<thead>\n<tr><th>a</th><th>b</th></tr>\n</thead>\n<tbody>\n<tr><td>1</td><td></td></tr>\n"
        "<tr><td>2</td><td>3</td></tr>\n</tbody>\n</table>\n");
  CHECK(markdownToHtml("a | b\nc | d") == "<p>a | b\nc | d</p>\n");
  // As in GitHub's tables, a pipe splits a cell even inside a code span unless it is escaped.
  CHECK(markdownToHtml("x | y\n--|--\n`a|b` | *c*") ==
        "<table>\n<thead>\n<tr><th>x</th><th>y</th></tr>\n</thead>\n<tbody>\n<tr><td>`a</td><td>b`</td></tr>\n</tbody>\n</table>\n");
  CHECK(markdownToHtml("x | y\n--|--\n`a\\|b` | *c*") ==
        "<table>\n<thead>\n<tr><th>x</th><th>y</th></tr>\n</thead>\n<tbody>\n<tr><td><code>a|b</code></td><td><em>c</em></td></tr>\n</tbody>\n</table>\n");
}

TEST_CASE("the preview page wraps the body and follows the system appearance") {
  const std::string page = markdownPreviewPage("# Hi");
  CHECK(page.find("<!DOCTYPE html>") == 0);
  CHECK(page.find("<h1>Hi</h1>") != std::string::npos);
  CHECK(page.find("color-scheme: light dark") != std::string::npos);
  CHECK(page.find("prefers-color-scheme: dark") != std::string::npos);
  CHECK(page.find("</html>") != std::string::npos);
}

TEST_CASE("odd and hostile input does not crash") {
  CHECK(markdownToHtml("").empty());
  CHECK(markdownToHtml("\n\n   \n").empty());
  CHECK_FALSE(markdownToHtml("[[[[[").empty());
  CHECK_FALSE(markdownToHtml("![[](").empty());
  CHECK_FALSE(markdownToHtml(std::string(3000, '*')).empty());
  std::string nested;
  for (int i = 0; i < 400; ++i) {
    nested += "> ";
  }
  CHECK_FALSE(markdownToHtml(nested + "deep").empty());
  std::string lists;
  for (int i = 0; i < 200; ++i) {
    lists += std::string(static_cast<size_t>(i % 4) * 2, ' ') + "- item\n";
  }
  CHECK_FALSE(markdownToHtml(lists).empty());
  CHECK_FALSE(markdownToHtml(std::string(2000, '[') + std::string(2000, ']') + "(x)").empty());
  CHECK_FALSE(markdownToHtml(std::string("a\0b", 3)).empty());
}
