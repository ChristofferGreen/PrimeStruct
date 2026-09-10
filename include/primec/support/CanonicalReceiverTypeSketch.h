#pragma once

// NOT PRODUCTION CODE. NOT WIRED INTO ANY BUILD TARGET.
//
// This is a Step 1c *design sketch*, written to accompany the "Step 1c
// Scoping" section of docs/ReceiverTargetResolutionConsolidation.md. It
// exists to make the field-by-field reconciliation discussion in that
// section concrete for a future implementer - it is deliberately not
// added to any CMakeLists.txt target, not included by any .cpp file, and
// not a claim that this is the final shape. Do not #include this file
// from production code; do not extend it in place of updating the
// design-doc section it accompanies.
//
// Background: docs/ReceiverTargetResolutionConsolidation.md's Step 0
// sweep found that every remaining unmigrated receiver-target branch in
// monomorphization (Row F's F3) and ir_lowerer (Row G's RT2/RT3/G7) is a
// *receiver-type-inference* question ("what type does this receiver
// have") - the converse of what classifyReceiverElementFamilyJoint
// already answers ("given a known type and method name, what family is
// it"). This sketch is the proposed shared OUTPUT shape that a new,
// per-stage resolveReceiverType(...) function would produce, so that
// classifyReceiverElementFamilyJoint can stay exactly as-is, consuming
// this struct's fields instead of a raw (type-text, templateShape) pair.
//
// See the design doc for the full F3/RT2/RT3/G7 field-by-field mapping,
// including the one case (monomorphization's F3-C3a) found NOT to fit
// this shape without losing information - documented there as an
// intentionally-unresolved reconciliation gap, not papered over here.

#include <string>
#include <vector>

namespace primec {

// Mirrors ReceiverElementFamilyClassifier.h's ReceiverElementFamily -
// intentionally NOT redeclared as a separate enum here. A real
// implementation would reuse that enum directly; this sketch just notes
// the field exists so the whole struct reads coherently on its own.
//
// enum class ReceiverElementFamily { String, FileError, VectorLike, Soa,
//                                     Buffer, KeyValue, File, Primitive,
//                                     StructOrUnknown };

struct CanonicalReceiverType {
  // --- Family verdict (only filled once resolveReceiverType hands its
  // inferred raw type text to classifyReceiverElementFamilyJoint; NOT
  // itself an independent classification - resolveReceiverType infers,
  // classifyReceiverElementFamilyJoint classifies). Left as an int
  // placeholder here rather than pulling in ReceiverElementFamilyClassifier.h,
  // since this sketch is not meant to compile or link against anything.
  int family = 0;  // primec::ReceiverElementFamily, see note above

  // For VectorLike (and other builtin-name-bearing families): the
  // collection base name ("vector"/"array"/"map"/"soa"/"Buffer"/"File"/a
  // primitive name). Empty when the receiver resolved to a struct type
  // instead (see resolvedTypePath below) or when inference produced
  // nothing at all (see the F3-C3a note below).
  std::string collectionBaseName;

  // Fully-qualified definition path, when the receiver's type resolved to
  // a struct/definition rather than a builtin family name. ir_lowerer's
  // RT2/RT3 (resolveMethodReceiverTypeFromLocalInfo /
  // resolveMethodReceiverTarget) already bifurcate their own output this
  // way (typeNameOut XOR resolvedTypePathOut per branch); monomorphization's
  // F3 does NOT - its single `typeName` text field carries both builtin
  // family markers and struct type names interchangeably, deferring the
  // builtin-vs-struct decision to a later resolution step
  // (resolveTypePath/defMap lookup). A monomorphization-side
  // resolveReceiverType therefore has an extra internal step ir_lowerer's
  // does not: after inferring the raw type text, it must itself decide
  // whether that text is a builtin family name or a struct path before
  // filling collectionBaseName vs resolvedTypePath - in practice this is
  // exactly what handing the raw text to classifyReceiverElementFamilyJoint
  // already does, so this is a clean composition point, not a gap.
  std::string resolvedTypePath;

  // --- Template-shape facts (the union of F3's caller-supplied
  // splitTemplateTypeName result and RT3's own call-shaped receiver
  // parsing). A real implementation would keep isTemplateShaped/
  // templateShapedBaseName exactly as classifyReceiverElementFamilyJoint's
  // existing ReceiverElementFamilyJointInput already models them (this
  // struct does not replace that input type, it is downstream of it).
  bool isTemplateShaped = false;
  std::string templateShapedBaseName;
  // Parsed template argument texts, e.g. ["T"] for vector<T>, ["K","V"]
  // for map<K,V>. Neither F3 nor RT2/RT3 currently parse and retain
  // individual template-arg texts as a list anywhere in the characterized
  // cascades (both only ever check "is there at least one/two args" as
  // part of a constructor-shape probe, e.g. resolveMethodReceiverTypeNameFromCallExpr's
  // Buffer/array/vector/map/soa arg-count checks) - this field is
  // included because the task's background section calls out "template
  // args" as part of the union of facts a shared shape must be ABLE to
  // represent, but no currently-characterized branch fills it beyond a
  // bare count. A future implementer should treat this as aspirational
  // until a real caller needs the actual parsed texts, not evidence that
  // today's code already produces them.
  std::vector<std::string> templateArgTexts;

  // --- Wrapped-vs-unwrapped storage facts (the F3-C2/F6 asymmetry, and
  // classifyReceiverElementFamilyJoint's own documented R7
  // unwrapped-vs-raw distinction).
  //
  // F3 tracks a *second* type-name text, wrappedReceiverTypeName, kept in
  // sync with typeName at every assignment site except one (F3-C2's
  // inferExprTypeTextForTemplatedVectorFallback path, which updates
  // typeName/isBorrowedSoaReceiver but leaves wrappedReceiverTypeName
  // stale) - this matters because F6 (the wrapper-method-path branch)
  // reads wrappedReceiverTypeName specifically to detect a
  // Reference<T>/Pointer<T>-wrapped receiver. RT2/RT3 have no equivalent
  // second text at all; ir_lowerer's own wrapped-vs-unwrapped handling
  // (RT2j/RT2l/RT2m's Reference/Pointer LocalInfo::Kind branches) works
  // directly off LocalInfo's own Kind enum instead of a second type-name
  // string, because ir_lowerer's LocalInfo already distinguishes
  // Reference/Pointer at the storage-kind level before any type-name text
  // is produced. So "isWrapped"/"wrappedBaseTypeName" below are filled
  // two structurally different ways per stage: monomorphization derives
  // them from a second parallel type-name text (and must reproduce F3-C2's
  // asymmetry - or fix it - explicitly); ir_lowerer derives them directly
  // from LocalInfo::Kind, no parallel text needed.
  bool isWrapped = false;
  std::string wrappedBaseTypeName;

  // --- Borrowed-vs-owned SOA fact (F3's isBorrowedSoaReceiver).
  // RT2/RT3's characterized branches have NO equivalent field in their
  // output at all - ir_lowerer's borrowed/owned _ref method-name
  // selection for SOA/key-value happens downstream of receiver-type
  // inference entirely, inside IrLowererSetupTypeCollectionHelpers.cpp's
  // registry-backed helper-name resolution (e.g. CH-V6's
  // soaVectorCount/soaVectorCountRef remap, and the working half of the
  // vector/key-value alias-name asymmetry documented in Row G continued
  // (II)) rather than as a fact resolveMethodReceiverTarget itself
  // produces. This is a legitimate "some stages fill this, some don't"
  // case per the task's own allowance, not a reconciliation problem - but
  // it does mean an ir_lowerer resolveReceiverType would simply never
  // populate this field, and the borrowed/owned decision for that stage's
  // dispatch stays exactly where it lives today, outside this struct.
  bool isBorrowed = false;

  // --- Args-pack storage facts (elemSlotCount/isArgsPack from the task
  // background, and TODO-5289's elemSlotCount/isSingleSlotPointerStyleKeyValueStorage
  // predicates).
  //
  // OPEN QUESTION, not resolved by this sketch: ir_lowerer's RT3b-i
  // sub-cascade keys its *entire* family classification off a distinct
  // `argsPackElementKind` (not the receiver's own plain LocalInfo::Kind) -
  // i.e. "is this receiver an args-pack element, and what element kind
  // does it have" is an INPUT ir_lowerer's inference needs before it can
  // decide family at all, not a fact only worth reporting on the way out.
  // Monomorphization's F3, by contrast, does not consult args-pack-ness
  // during type inference at all - args-pack handling there lives entirely
  // in the sibling function F2 (resolveIndexedArgsPackMapMethodTarget),
  // which runs AFTER F3 and unconditionally discards whatever F3 inferred
  // if it fires. So it is genuinely unclear from the currently
  // characterized code whether isArgsPackElement/elemSlotCount belong
  // INSIDE CanonicalReceiverType as an inference OUTPUT fact (ir_lowerer's
  // shape) or as part of the STAGE-SPECIFIC INPUT to resolveReceiverType,
  // sitting entirely outside this struct (monomorphization's shape,
  // where it is a separate, higher-priority cascade rather than a fact
  // folded into type inference). Included here as placeholder fields with
  // this note attached rather than silently picking one stage's shape as
  // canonical.
  bool isArgsPackElement = false;
  int elemSlotCount = -1;  // -1 == not applicable / not determined
};

}  // namespace primec
