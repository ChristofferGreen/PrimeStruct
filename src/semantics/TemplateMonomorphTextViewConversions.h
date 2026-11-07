#pragma once

#include <string>
#include <vector>

#include "TemplateMonomorphExpectedTypeSites.h"

namespace primec {

// A `string` (a literal or a `string` value) passed where a `/std/text/view/StringView` is expected converts as if
// written `textOf(value)` (docs/spec/strings-and-views.md, Current surface of the text view), at every site
// `rewriteExpectedTypeSites` finds. At a call with same-arity overloads the conversion applies only when every
// candidate takes a StringView there, so an overload that takes the `string` as it is still wins.
class TextViewConversion final : public ExpectedTypeConversion {
public:
  bool enabled(const Context &ctx) const override;
  bool mayBeTarget(const std::string &typeName) const override;
  bool mayConvert(const Expr &arg, const ExpectedTypeScope &scope) const override;
  bool convert(Expr &arg,
               const std::vector<const ExpectedType *> &expected,
               const ExpectedTypeScope &scope,
               std::string &error) const override;
};

} // namespace primec
