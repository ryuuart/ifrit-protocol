#pragma once

/** @file
 * Internal to the kernel — what the root of the cascade inherits from and
 * the state of the pass that resolves it: the root's font, line height,
 * rem, properties, block and sampling, whether the resolved answers may be
 * stale, and which pass is running with the `:has()` names it met.
 */

#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilweave/layout/ParagraphBlock.h>
#include <sigilweave/style/Type.h>

#include <cstdint>
#include <memory>
#include <optional>

#include "style/SelectorMatch.h"

namespace sigil::compose::detail {

/** THE ROOT OF THE CASCADE: one part of the composer's retained state, read
 *  off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct CascadeRoot {
  // What the root inherits from — Composer::setInherited — every field
  // engaged, its colour the root ink; and the face's line height at it,
  // computed when it is first needed.
  sigil::weave::Type rootFont = sigil::weave::initialType();
  float rootLineHeight = 0.0f;
  // What `1_rem` measures: the ROOT ELEMENT's computed font size, as CSS's
  // rem is, so a size stated on the tree's root node — directly or by a
  // `:root` rule — sets every rem below it, and what `setInherited` states
  // decides it only where the root states none. The root's own font size
  // is measured against the inherited one, since a root cannot be measured
  // against itself. Set by the cascade pass once the root's font resolves;
  // `remMoved` says this pass moved it, so every length written in rems is
  // rewritten even where nothing about its own node moved.
  float remPx = sigil::weave::kInitialTypeSizePx;
  bool remMoved = false;
  // The custom properties the root inherits — Composer::setInherited —
  // in force under every name the tree or the root's sheet sets; null
  // where none is inherited.
  std::shared_ptr<const VarTable> rootVars;
  // What the root inherits as its block and its sampling —
  // Composer::setInherited — and by default nothing stated, so every block
  // under nothing is set in the layout's own answer and every image
  // samples linearly.
  sigil::weave::ParagraphBlock rootBlock;
  std::optional<SkSamplingOptions> rootSampling;
  // The sheets in force ABOVE the root — Composer::setInherited — which
  // stand ahead of the root's own and see this tree alone, with no node in
  // it their `:root`.
  std::optional<StyleSheet> inheritedSheet;
  // The sheet in force at the root of whatever is rendered —
  // Composer::setStyleSheet — applied as though the root node applied it
  // before its own.
  std::optional<StyleSheet> rootSheet;
  // The two above as one sheet, the inherited first, which is the sheet in
  // force at a node under no applied sheet; none where neither is set.
  // Joined where either is set, so the cascade pass only reads it.
  std::optional<StyleSheet> rootSheetsJoined;
  // Whether the resolved fonts, inks and properties on the instances may
  // be stale: set by every reconcile that changed anything and by
  // setInherited, and left set by a pass that found an ink transition
  // running, so the next frame resolves again.
  bool cascadeDirty = true;
  bool inkAnimating = false;
  // Which cascade pass is running, and the names the `:has()` arguments
  // of the sheets it met test for — the key a node's `:has()` summary is
  // read against.
  uint32_t cascadePass = 0;
  HasNames hasNames;
};

}  // namespace sigil::compose::detail
