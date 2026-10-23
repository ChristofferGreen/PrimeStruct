#pragma once

// Markdown to HTML for the native UI's preview pane (docs/NativeUiPlan.md). A CommonMark and
// GitHub-flavoured subset, written for previewing notes and READMEs; it is plain text in and text out, so
// every backend and every compile mode shares it.
//
// Block syntax: ATX and setext headings, paragraphs, fenced and indented code, block quotes, bullet and
// ordered lists (nested, tight and loose, task items), thematic breaks and pipe tables with alignment.
// Inline syntax: code spans, emphasis, strong, strikethrough, links, images, autolinks, bare http(s)
// URLs, hard line breaks and backslash escapes. Raw HTML in the source is shown as text, never passed
// through, and link and image targets are limited to http, https, mailto, relative and #fragment
// references (images may also be data:image/ URIs), so a preview cannot run scripts.

#include <string>
#include <string_view>

namespace primec::ui {

// The HTML for the body of a document.
std::string markdownToHtml(std::string_view markdown);

// A complete HTML page around that body, with a stylesheet that follows the system's light or dark mode.
std::string markdownPreviewPage(std::string_view markdown);

} // namespace primec::ui
