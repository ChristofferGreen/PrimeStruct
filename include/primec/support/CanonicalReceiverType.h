#pragma once

#include <string>
#include <vector>

#include "primec/support/ReceiverElementFamilyClassifier.h"

namespace primec {

// Step 1c of docs/ReceiverTargetResolutionConsolidation.md.
//
// This is the promoted, real, compiling successor to the Step 1c design
// sketch (include/primec/support/CanonicalReceiverTypeSketch.h) - see that
// file's history and the "Step 1c Scoping" / "Ready to implement" sections
// of the design doc for the full field-by-field reconciliation this shape
// is built from. Summary: every remaining unmigrated receiver-target branch
// in monomorphization (Row F's F3) and ir_lowerer (Row G's RT2/RT3/G7) is a
// receiver-type-*inference* question ("what type does this receiver have"),
// the converse of what classifyReceiverElementFamilyJoint already answers
// ("given a known type and method name, what family is it"). This struct is
// the shared OUTPUT shape a per-stage resolveReceiverType(...) function
// converges on, so classifyReceiverElementFamilyJoint can stay exactly
// as-is, consuming this struct's fields instead of a raw
// (type-text, templateShape) pair.
//
// This round (2026-09-10) implements exactly one producer of this struct:
// ir_lowerer's resolveReceiverType(const LocalInfo &), covering RT2's logic
// (resolveMethodReceiverTypeFromLocalInfo,
// IrLowererSetupTypeReceiverTargetHelpers.cpp) - see that file. It is a NEW
// function living alongside the existing
// resolveMethodReceiverTypeFromLocalInfo, wired only behind an
// env-gated observational diff-audit harness
// (PRIMESTRUCT_RECEIVER_TARGET_DIFF_AUDIT=1) - the old function remains the
// sole production code path this round. No call site has been migrated to
// consume this struct's output yet; monomorphization's F3 producer has not
// been implemented yet either (see the design doc's "Ready to implement"
// checklist for the intended order - RT2 first, F3 later, one stage per
// round).
//
// `family` is deliberately NOT filled by ir_lowerer's resolveReceiverType
// this round: producing a real family verdict requires handing the
// inferred raw type text to classifyReceiverElementFamilyJoint together
// with the method name and template-shape facts, both of which live outside
// RT2's own (LocalInfo-only) input shape - RT2 itself never takes a method
// name. The design doc's composition note describes that hand-off as a
// job for a future call-site-level resolveReceiverType wrapper (e.g. RT3/G7's
// eventual implementation, which does have a method name and template-shape
// facts in scope), not for this LocalInfo-only slice. `family` stays at its
// default (StructOrUnknown) whenever this function fills the struct.
struct CanonicalReceiverType {
  // --- Family verdict. See the note above: never filled by ir_lowerer's
  // resolveReceiverType(const LocalInfo &) this round - stays at its
  // default. A future round's call-site-level composition (handing the
  // resolved raw type text + method name + template shape to
  // classifyReceiverElementFamilyJoint) is what would fill this.
  ReceiverElementFamily family = ReceiverElementFamily::StructOrUnknown;

  // For VectorLike (and other builtin-name-bearing families): the
  // collection base name ("vector"/"array"/"map"/"soa"/"Buffer"/"File"/a
  // primitive name). Empty when the receiver resolved to a struct type
  // instead (see resolvedTypePath below) or when inference produced
  // nothing at all. Maps directly onto RT2's own typeNameOut.
  std::string collectionBaseName;

  // Fully-qualified definition path, when the receiver's type resolved to
  // a struct/definition rather than a builtin family name. Maps directly
  // onto RT2's own resolvedTypePathOut. collectionBaseName and
  // resolvedTypePath are mutually exclusive in every branch RT2 (and this
  // round's resolveReceiverType) characterizes - at most one is non-empty
  // on a successful resolution.
  std::string resolvedTypePath;

  // --- Template-shape facts. Never filled by ir_lowerer's
  // resolveReceiverType(const LocalInfo &) this round - LocalInfo alone
  // carries no template-argument-text information; that lives on the
  // caller's own Expr/splitTemplateTypeName side (RT3/G7's job, not RT2's).
  bool isTemplateShaped = false;
  std::string templateShapedBaseName;
  std::vector<std::string> templateArgTexts;

  // --- Wrapped-vs-unwrapped storage facts. ir_lowerer derives these
  // directly from LocalInfo::Kind's Reference/Pointer variants (RT2's own
  // kind-gated branches), not from a second parallel type-name text the
  // way monomorphization's F3 does (see the design doc's field-list note).
  // isWrapped is true exactly when the successful resolution came from a
  // LocalInfo::Kind::Reference or LocalInfo::Kind::Pointer branch (i.e. the
  // receiver is itself a reference/pointer to the collection or struct
  // being reported, not the collection/struct directly). wrappedBaseTypeName
  // is filled with whatever collectionBaseName/resolvedTypePath already
  // holds for that branch - ir_lowerer has no second, independent "wrapped"
  // type-name text the way F3's wrappedReceiverTypeName is, so this field
  // is redundant with collectionBaseName/resolvedTypePath by construction
  // for this stage (documented explicitly here rather than left implicit,
  // since a future F3 producer will NOT have this redundancy - filling it
  // from its own separate wrappedReceiverTypeName text instead).
  bool isWrapped = false;
  std::string wrappedBaseTypeName;

  // --- Borrowed-vs-owned SOA fact. RT2 has no equivalent field anywhere in
  // its output - ir_lowerer's borrowed/owned _ref method-name selection
  // happens downstream, inside IrLowererSetupTypeCollectionHelpers.cpp's
  // registry-backed helper-name resolution, not as a receiver-type-inference
  // output. Never filled by ir_lowerer's resolveReceiverType.
  bool isBorrowed = false;

  // Args-pack storage facts (isArgsPackElement/elemSlotCount) were
  // considered and deliberately removed from this struct during Step 1c
  // scoping - see the design doc's "Open question, resolved" subsection.
  // Neither belongs here on either the input or output side for either
  // stage.
};

}  // namespace primec
