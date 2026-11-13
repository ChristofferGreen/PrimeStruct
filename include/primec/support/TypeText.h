#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace primec {

// Types travel through the compiler as text (`map<K, vector<V>>`). These are the only parsers of that text: the
// semantics, the lowerer and the IR printer all use them, so every layer reads a spelling the same way.

// `text` without leading and trailing whitespace.
std::string trimTemplateTypeText(const std::string &text);

// Splits `Base<args>` into `Base` and the text between the outer angle brackets. The brackets are matched by depth, so
// `A<B>C<D>`, `A<B>>`, `A<B`, `<B>` (no base), text with anything after the closing bracket and empty text are not
// one template type. Returns false for those and for text without brackets; then `base` is the whole `text` and `arg`
// is empty, so a caller that ignores the result reads the plain name. Results are memoized in the compile context.
bool splitTemplateTypeName(const std::string &text, std::string &base, std::string &arg);

// Splits template argument text at the commas outside nested brackets and trims each argument. Returns false (with
// `out` as far as it was filled) for empty text and for an empty argument (`A,,B`, `A,`). Memoized like
// splitTemplateTypeName.
bool splitTopLevelTemplateArgs(std::string_view text, std::vector<std::string> &out);

} // namespace primec
