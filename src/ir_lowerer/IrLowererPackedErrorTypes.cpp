#include "IrLowererPackedErrorTypes.h"

#include "primec/support/CollectionHelperNames.h"

namespace primec::ir_lowerer {

// FileError/ImageError/ContainerError/GfxError are single-i32-field structs
// that `valueKindFromTypeName` (and therefore `isSupportedScalarTypeName`)
// treats as Int32 so that Result<T,E> packing and binding representations
// can use a plain scalar - but a definition whose *declared return type* is
// bare one of these names still constructs and returns a real struct via
// the generic struct-constructor path (IrLowererInlineStructArgHelpers.cpp's
// emitInlineStructDefinitionArguments), which pushes an AddressOfLocal
// pointing into the callee's own per-call locals array. That address is
// only valid for as long as the callee's call frame lives, which is fine
// for inlined calls (the "struct" lives in the caller's own frame) but not
// for a real (non-inlined) Call/Return - the callee's frame is torn down
// before the caller ever reads the value, so the returned "scalar" is a
// stale, frame-relative small integer instead of a real payload. Excluding
// these four names here keeps such definitions on the safe inlining path.
bool isPackedErrorStructTypeName(const std::string &typeName) {
  return typeName == "FileError" || typeName == "/std/file/FileError" ||
         typeName == "ImageError" || typeName == "/std/image/ImageError" ||
         typeName == "ContainerError" || typeName == collection_helpers::kCanonicalContainerErrorType ||
         typeName == "GfxError" || typeName == "/std/gfx/GfxError" ||
         typeName == "/std/gfx/experimental/GfxError";
}

} // namespace primec::ir_lowerer
