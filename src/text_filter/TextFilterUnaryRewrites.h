#pragma once

#include "TextFilterPipelineInternal.h"

#include "primec/support/TextFilterPipeline.h"

namespace primec::text_filter {

// The prefix-operator rewrites of the operators filter over tracked text (the `std::string` forms are the testing API
// in primec/testing/TextFilterHelpers.h): each is tried at `input[index]`; on a match it appends the call form to
// `output`, moves `index` to the last byte it consumed and returns true. The operand text keeps its origins.
bool rewriteUnaryNot(const Text &input,
                     Text &output,
                     size_t &index,
                     const TextFilterOptions &options);
bool rewriteUnaryAddressOf(const Text &input,
                           Text &output,
                           size_t &index,
                           const TextFilterOptions &options);
bool rewriteUnaryDeref(const Text &input,
                       Text &output,
                       size_t &index,
                       const TextFilterOptions &options);
bool rewriteUnaryIncrement(const Text &input,
                           Text &output,
                           size_t &index,
                           const TextFilterOptions &options);
bool rewriteUnaryDecrement(const Text &input,
                           Text &output,
                           size_t &index,
                           const TextFilterOptions &options);
bool rewriteUnaryMinus(const Text &input,
                       Text &output,
                       size_t &index,
                       const TextFilterOptions &options);

} // namespace primec::text_filter
