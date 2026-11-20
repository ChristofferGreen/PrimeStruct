#pragma once

#include "TrackedText.h"
#include "primec/testing/TextFilterHelpers.h"

namespace primec::text_filter {

// The same helpers over tracked text: the result keeps the origins of the bytes it copies from the operand.
Text stripOuterParens(const Text &text);
Text normalizeUnaryOperand(const Text &operand);
Text maybeAppendI32(const Text &token);
Text maybeAppendUtf8(const Text &token);

} // namespace primec::text_filter
