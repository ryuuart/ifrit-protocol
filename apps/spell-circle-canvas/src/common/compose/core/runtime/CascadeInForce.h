#pragma once

/** @file
 * Internal to the kernel — the cascade as it resolved at one node: the font,
 * ink, paragraph setting, sampling and custom properties in force, where
 * the node stands among its siblings for a selector, and what the sheets in
 * force say about a text leaf's named runs and paragraph styles.
 */

#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilweave/style/Type.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "description/ComposeInternal.h"
#include "style/SelectorMatch.h"

namespace sigil::compose::detail {

class UnitSurfaces;

/** WHAT THE SHEETS IN FORCE SAY ABOUT ONE PARAGRAPH STYLE'S NAME on a text
 *  leaf: the block partial a paragraph named so is laid out in, and an
 *  indent the rules gave as a percentage, which is one of the measure the
 *  passage is laid out at and so waits for it. */
struct NamedParagraphStyle {
  std::string name;
  sigil::weave::ParagraphBlock block;
  std::optional<float> indentPercent;
  bool operator==(const NamedParagraphStyle&) const = default;
};

/** THE SHEETS IN FORCE AT A NODE THAT APPLIES ONE, joined into one sheet,
 *  and what they were joined from — the sheet in force above the node,
 *  then each one the node applies — so a pass that finds the same sheets
 *  keeps the join it made. */
struct JoinedSheets {
  StyleSheet joined;
  std::vector<StyleSheet> from;
};

/** THE CASCADE IN FORCE AT A NODE: one part of the retained node, written
 *  by the cascade pass before layout and read off the instance by each
 *  field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct CascadeInForce {
  // THE FONT IN FORCE at this node, every field engaged: the parent's,
  // overlaid with what this node's description declares, with the ink
  // transition this node may be running read in as the colour. Its colour
  // IS the ink. Written by the cascade pass before layout; read by text
  // materialisation, by the relative lengths and by the paint context.
  sigil::weave::Type font;
  // THE INK IN FORCE AS A PAINT at this node, inherited exactly as the
  // font's colour is, and empty on the tree every ink is a colour on.
  InkInForce inkPaint;
  // Prepared ink inputs for the surface maps of a lit ink, kept while its
  // material stands.
  std::optional<material::skia::SurfaceMaps> surfaceMapInkInputs;
  // Whether THIS node is the one that stated that paint — the box an
  // ink anchored to a declaring box maps onto.
  bool inkPaintOrigin = false;
  // THE PARAGRAPH SETTING IN FORCE at this node: the parent's, with what this
  // node's description declares folded over. A partial, since an unset field
  // is the layout's own answer and nothing engages it. Written by the pass
  // before layout; read when a text leaf's layout options are built.
  sigil::weave::ParagraphBlock block;
  // Whether the style in force is ITALIC, which the face alone cannot say:
  // a family with no italic stands in with a lean. Inherited beside `font`.
  bool italic = false;
  // THE FAMILY IN FORCE BY NAME, which a weight or an italic stated further
  // down finds its face in again; empty where the face in force was stated
  // as a face, or no family was named. Inherited beside `font`.
  std::string family;
  // THE FIRST-LINE INDENT IN FORCE AS A PERCENTAGE of each passage's
  // measure, which `block` cannot hold in pixels until the measure is
  // known; absent where the indent in force is pixels or none.
  std::optional<float> indentPercent;
  // THE IMAGE SAMPLING IN FORCE at this node, or none stated, which an
  // image leaf reads as linear.
  std::optional<SkSamplingOptions> sampling;
  // Prepared fill inputs for the surface maps of a lit fill, kept while
  // its material stands; the map paint is replaced when the map changes.
  mutable std::optional<material::Paint> surfaceMapFill;
  mutable std::optional<material::Material> surfaceMapMaterial;
  mutable std::optional<material::skia::SurfaceMaps> surfaceMapFillInputs;
  // The maps of a lit fill or ink that reads the unit it is painted into,
  // prepared per unit while the material and the unit stand.
  mutable std::shared_ptr<UnitSurfaces> unitSurfaces;
  // The face's own line height at `font`, px, measured when the shaping in
  // force moves.
  float faceLineHeight = 0.0f;
  // THE LINE HEIGHT IN FORCE, px: the pitch the leading in force sets at
  // `font` — the face's own unless a line height is stated — and what an
  // `lh` length on this node resolves against, as CSS's computed
  // line-height is.
  float lineHeight = 0.0f;
  // The advance of "0" at `font`, px — what a `ch` length on this node
  // resolves against. Beside the line height because both are answers of
  // the same face at the same size and both are measured when the shaping
  // in force moves.
  float zeroAdvance = 0.0f;
  // THE CUSTOM PROPERTIES IN FORCE: the parent's table shared when this
  // node sets none, or a copy of it with this node's own written over.
  std::shared_ptr<const VarTable> vars;
  // THE SHEETS IN FORCE AS ONE SHEET, which a guest painted from this node
  // stands under: the nearest node's that applies one, or the root's; null
  // where none is in force. Written by the cascade pass, which writes every
  // node under one whose join it replaced.
  const StyleSheet* sheetInForce = nullptr;
  // On a node that applies a sheet: the join `sheetInForce` points at.
  std::unique_ptr<JoinedSheets> joinedSheets;
  // WHERE THIS NODE STANDS AMONG ITS SIBLINGS, by position and by role,
  // written by the cascade pass when its parent's children are resolved
  // and read by the structural pseudo-classes of a selector. The root
  // keeps the default: the only child of nothing.
  SiblingPlace place;
  // Whether this is a NAMED RUN of a text leaf being matched as a virtual
  // child of it rather than a mounted node: it is counted among no
  // siblings, and `:empty` never matches it.
  bool virtualRunChild = false;
  // WHAT THIS NODE'S SUBTREE HOLDS, for a `:has()`: one bit per name the
  // `:has()` arguments in force test for, over the node's own classes and
  // role, over its children's and over everything under it, the cascade
  // pass that wrote them and how many of that pass's names they cover —
  // a later sheet can give a new name a bit mid-pass, and a summary taken
  // before then says nothing about it. Written bottom up ahead of the
  // pass and only under a node applying a sheet that uses `:has()`; kept
  // here, on the retained node, so a subtree a memo reused is summarised
  // without being described again.
  uint64_t hasOwnNames = 0;
  uint64_t hasChildNames = 0;
  uint64_t hasSubtreeNames = 0;
  uint32_t hasSummaryPass = 0;
  uint32_t hasSummaryCovers = 0;
  // Whether the pass has resolved this node at least once — before that,
  // `font` and `vars` are whatever the constructor left and nothing may
  // read them as the truth.
  bool cascadeResolved = false;
  // Whether any length this node's layout declares is relative — an em, a
  // rem, an lh or a custom property — so a change in the font in force, or
  // in the properties, rewrites its Yoga style. Written with the style.
  bool relativeLengths = false;
  // Whether any length this node's layout declares is measured against the
  // CANVAS — a vw, vh, vmin or vmax. Those are resolved into pixels before Yoga
  // sees them, so a canvas that changes size has to have this style written
  // again; nothing else notices a resize.
  bool canvasLengths = false;
  // On a text leaf that inherits: the font the paragraph was last
  // materialised in, compared against `font` to tell a change that
  // re-shapes (any field but the colour) from one that repaints alone.
  sigil::weave::Type textFont;
  // On a text leaf: the block the paragraph was last built under,
  // compared against `block` to tell a change that re-materialises (the
  // writing mode, the locale) from one that lays out again.
  sigil::weave::ParagraphBlock textBlock;
  // On a text leaf: what the selector sheets in force say about each NAME
  // its rich runs and its paragraph styles were written with, each name
  // matched as a virtual child of the leaf — the font partial a run named
  // so is set in, and the block partial a paragraph named so is laid out
  // in. A name no rule speaks about is absent. Resolved by the cascade
  // pass and compared there, so a sheet that changes what a name means
  // re-materialises the leaf.
  sigil::weave::TypeSheet runStyles;
  std::vector<NamedParagraphStyle> paragraphBlockStyles;
  // On a text leaf: whether any sheet is in force where it stands, which is
  // what a paragraph style no rule speaks about is warned against.
  bool sheetsInForce = false;
  // On a text leaf: what the matched rules state about its text
  // properties, and the options in force — those, with the leaf's own
  // standing over them. Resolved by the cascade pass, and read wherever a
  // leaf's options are, so a rule that moved a text property reaches a
  // leaf whose own description did not.
  std::unique_ptr<const RuleTextLayer> ruleText;
  TextOptions textOptions;
  // On a text leaf: its text changed at reconcile and the
  // paragraph is owed. The pass materialises it once, in the font it lands
  // in; reconcile shaping it against the root first would be a second
  // paragraph every frame the text changes.
  bool textDirty = false;
  // …and the UTF-16 ranges whose colour is the ink in force, each with
  // the partial the run was written with, so an ink-only change sets the
  // paint on the paragraph in place and no line is broken again.
  struct InkRange {
    sigil::weave::CharRange range;
    std::optional<sigil::weave::Type> over;
  };
  std::vector<InkRange> inheritedInkRanges;
  // On a leaf set in a whole style: the colour ink stated on the leaf
  // itself, by its own verb or a rule matching it, which its glyphs are
  // set in over the style's own colour; none draws the style's colour.
  std::optional<material::Color> wholeStyleInk;
};

}  // namespace sigil::compose::detail
