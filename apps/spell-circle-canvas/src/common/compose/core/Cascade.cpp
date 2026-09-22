/** @file
 * The cascade: the custom-property names, the pass that resolves the font,
 * the ink and the properties in force at every node from the root's down
 * and applies what changed, the ink-only repaint of an inheriting leaf,
 * the lengths measured against the font, and the reference a fill
 * resolves through at paint.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the once-per-name diagnostics
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

#include "ComposeRuntime.h"
#include "SelectorMatch.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Names

namespace {

/** The process-wide table a custom property's name is interned in. The
 *  names stand in a deque so a view handed out stays valid when a later
 *  name is added; the table is never freed, because a describe on another
 *  thread may still be reading it while the process exits. */
struct VarNames {
  std::mutex mutex;
  std::deque<std::string> names;  // id − 1 → name
  boost::unordered_flat_map<std::string, uint32_t> ids;
};

VarNames& varNames() {
  static VarNames* const table = new VarNames;
  return *table;
}

/** The size in pixels a resolved font carries; the initial size where a
 *  font has none yet. */
float fontSizePx(const sigil::weave::Type& font) {
  return font.size ? font.size->value : 16.0f;
}

/** Whether two fonts agree in every field but the colour. */
bool sameFontButColour(sigil::weave::Type a, const sigil::weave::Type& b) {
  a.color = b.color;
  return a == b;
}

bool sameVars(const std::shared_ptr<const VarTable>& a,
              const std::shared_ptr<const VarTable>& b) {
  if (a == b) return true;
  if (!a || !b) return (a ? a->empty() : true) && (b ? b->empty() : true);
  return *a == *b;
}

bool sameSheet(const std::shared_ptr<const sigil::weave::StyleSheet>& a,
               const std::shared_ptr<const sigil::weave::StyleSheet>& b) {
  if (a == b) return true;
  if (!a || !b) return false;
  return *a == *b;
}

material::Color lerpColour(const material::Color& from,
                           const material::Color& to, float t) {
  return material::mixToward(from, to, t, from.a + (to.a - from.a) * t);
}

}  // namespace

VarRef var(std::string_view name) {
  VarNames& table = varNames();
  const std::lock_guard<std::mutex> lock(table.mutex);
  std::string key(name);
  if (const auto found = table.ids.find(key); found != table.ids.end())
    return {found->second};
  table.names.push_back(key);
  const auto id = (uint32_t)table.names.size();
  table.ids.emplace(std::move(key), id);
  return {id};
}

std::string_view varName(VarRef ref) {
  VarNames& table = varNames();
  const std::lock_guard<std::mutex> lock(table.mutex);
  if (ref.id == 0 || ref.id > table.names.size()) return {};
  return table.names[ref.id - 1];
}

// ---------------------------------------------------------------------------
// Diagnostics

void detail::warnNoSuchClass(std::string_view name, bool anySheetInScope) {
  static thread_local boost::unordered_flat_set<std::string> warned;
  if (!warned.insert(std::string(name)).second) return;
  SkDebugf(
      "[compose] styleClass(\"%.*s\") names a class the weave::StyleSheet in "
      "force where the element lands does not carry%s — nothing was set, and "
      "the text under it "
      "is set in whatever it inherits. State a sheet with styleSheet() on "
      "the element or on any node above it, and register the name on it. "
      "(warned once)\n",
      (int)name.size(), name.data(),
      anySheetInScope ? "" : " (no sheet is stated on the tree above it)");
}

void detail::warnNoSuchVar(VarRef reference, bool wantColour) {
  static thread_local boost::unordered_flat_set<uint64_t> warned;
  if (!warned.insert(((uint64_t)reference.id << 1) | (wantColour ? 1u : 0u))
           .second)
    return;
  const std::string_view name = varName(reference);
  SkDebugf(
      "[compose] var(\"%.*s\") read as a %s where no ancestor set one as "
      "such — it resolves to nothing. Set it with Element::var on an "
      "ancestor of the node that reads it, holding a %s. (warned once)\n",
      (int)name.size(), name.data(), wantColour ? "colour" : "length",
      wantColour ? "colour" : "length");
}

// ---------------------------------------------------------------------------
// A fill written as a reference

Fill resolveRef(const Fill& fill, const PaintContext& ctx) {
  switch (fill.ref) {
    case Fill::Ref::None:
      return fill;
    case Fill::Ref::CurrentInk:
      // The ink is a whole paint wherever one was stated, and a colour
      // everywhere else; both are what a mark naming no colour is
      // painted in.
      if (ctx.inkPaint) return resolveInk(*ctx.inkPaint, ctx);
      return Fill::color(ctx.ink);
    case Fill::Ref::Var: {
      const VarValue* value =
          ctx.vars ? ctx.vars->find(VarRef{fill.varId}) : nullptr;
      const material::Color* colour =
          value ? std::get_if<material::Color>(value) : nullptr;
      if (colour) return Fill::color(*colour);
      warnNoSuchVar(VarRef{fill.varId}, true);
      return Fill::none();
    }
  }
  return fill;
}

// ---------------------------------------------------------------------------
// Lengths against the font

float Composer::Impl::resolveLength(const Instance& inst,
                                    const Dimension& length,
                                    bool& relative) const {
  switch (length.unit) {
    case Dimension::Unit::Px:
    case Dimension::Unit::Pct:
      return length.value;
    case Dimension::Unit::Auto:
      return YGUndefined;
    case Dimension::Unit::Em:
      relative = true;
      return length.value * fontSizePx(inst.font);
    case Dimension::Unit::Rem:
      relative = true;
      return length.value * fontSizePx(rootFont);
    case Dimension::Unit::Lh:
      relative = true;
      return length.value * inst.lineHeight;
    case Dimension::Unit::Ch:
      relative = true;
      return length.value * inst.zeroAdvance;
    case Dimension::Unit::Pt:
      return length.value * sigil::weave::Length::kPointPx;
    case Dimension::Unit::Pw:
    case Dimension::Unit::Ph: {
      // THE CANVAS, never the parent: the box the composer renders into,
      // which is why this is resolved here and not handed to Yoga as a
      // percent. Under an intrinsic root — a snapshot, which has no
      // viewport — the root's own laid-out extent is the honest stand-in.
      relative = true;
      const SkSize canvas = size.isEmpty() ? rootLayoutSize : size;
      const float extent =
          length.unit == Dimension::Unit::Pw ? canvas.width() : canvas.height();
      return length.value * 0.01f * extent;
    }
    case Dimension::Unit::Var:
      // Dereferenced by the caller, which decides between a pixel and a
      // percent setter; a reference that reaches here names nothing.
      relative = true;
      return 0.0f;
  }
  return length.value;
}

float Composer::Impl::lineHeightAt(const sigil::weave::Type& font) {
  return sigil::weave::lineHeightOf(sigil::weave::toTextStyle(font), fonts);
}

float Composer::Impl::zeroAdvanceAt(const sigil::weave::Type& font) {
  return sigil::weave::zeroAdvanceOf(sigil::weave::toTextStyle(font), fonts);
}

// ---------------------------------------------------------------------------
// The pass

void Composer::Impl::runCascade() {
  if (!root) return;
  inkAnimating = false;
  if (rootLineHeight <= 0.0f) rootLineHeight = lineHeightAt(rootFont);
  indexRoot(*root);
  const SheetChain none;
  const InkInForce noInkPaint;
  resolveCascade(*root, rootFont, rootLineHeight, nullptr, rootBlock,
                 rootSampling, rootSheet, none, noInkPaint);
  // A running ink transition moves the colour every frame, so the next
  // frame resolves again; otherwise the answers stand until a reconcile
  // says otherwise.
  cascadeDirty = inkAnimating;
}

void Composer::Impl::resolveCascade(
    Instance& inst, const sigil::weave::Type& parentFont,
    float parentLineHeight, const std::shared_ptr<const VarTable>& parentVars,
    const sigil::weave::Block& parentBlock,
    const std::optional<SkSamplingOptions>& parentSampling,
    const std::shared_ptr<const sigil::weave::StyleSheet>& parentSheet,
    const SheetChain& parentSheets, const InkInForce& parentInkPaint) {
  const ElementNode& node = *inst.description;
  sigil::weave::Type font = parentFont;
  // The ink's paint inherits exactly as the font's colour does, and the
  // node that STATED it is the box a declaring-box anchor maps onto.
  InkInForce inkPaint = parentInkPaint;
  bool inkPaintOrigin = false;
  std::shared_ptr<const VarTable> vars = parentVars;
  sigil::weave::Block block = parentBlock;
  std::optional<SkSamplingOptions> sampling = parentSampling;
  std::shared_ptr<const sigil::weave::StyleSheet> sheet = parentSheet;
  const CascadeData* const cascade =
      node.cascadeData ? &*node.cascadeData : nullptr;
  // THE SELECTOR SHEETS IN FORCE HERE: the ones the ancestors applied,
  // then the ones this node applies, which speak about this node too.
  // Each carries the node that applied it, because that node bounds
  // what its sheet can see. The chain is extended only where a node
  // applies one, so a tree that applies none shares the root's empty
  // chain all the way down.
  SheetChain extended;
  const SheetChain* sheets = &parentSheets;
  if (cascade != nullptr && !cascade->appliedSheets.empty()) {
    extended = parentSheets;
    for (const StyleSheet& applied : cascade->appliedSheets)
      extended.push_back(ScopedSheet{&applied, &inst});
    sheets = &extended;
  }
  // The rules that speak about this node, weakest first. A rule's
  // SUBJECT is this node, which lies in the applying node's subtree by
  // construction; every other compound of its selector is matched
  // inside that subtree too, so nothing above or beside the applying
  // node can satisfy one.
  const std::vector<MatchedRule> matched =
      sheets->empty() ? std::vector<MatchedRule>{} : matchRules(*sheets, inst);
  if (cascade != nullptr || !matched.empty()) {
    // The sheet this node states: its rules over the inherited ones by
    // name, its base standing, the result shared with everything under it.
    if (cascade != nullptr && cascade->sheet) {
      auto own = std::make_shared<sigil::weave::StyleSheet>(
          parentSheet ? *parentSheet : sigil::weave::StyleSheet{});
      own->base(cascade->sheet->base());
      for (const sigil::weave::Rule& r : cascade->sheet->rules()) own->set(r);
      sheet = std::move(own);
    }
    // The role defaults, its matching sheet rule, ordinary classes, the
    // rules that matched a selector, then direct declarations. A role
    // stays below every ordinary class, regardless of sheet order. Merge
    // partials first so relative sizes resolve once against the parent's
    // font rather than compounding.
    sigil::weave::Type ownFont;
    sigil::weave::Block ownBlock;
    // The property the ink reads, from whichever layer last said so.
    std::optional<VarRef> inkVar;
    VarTable ruleVars;
    if (cascade != nullptr && cascade->role) {
      sigil::weave::merge(ownFont, cascade->role->type());
      sigil::weave::merge(ownBlock, cascade->role->block());
      if (const sigil::weave::Rule* rule =
              sheet ? sheet->find(cascade->role->name()) : nullptr) {
        sigil::weave::merge(ownFont, rule->type());
        sigil::weave::merge(ownBlock, rule->block());
      }
    }
    if (cascade != nullptr && !cascade->classes.empty()) {
      const auto named = [&](std::string_view name) {
        return std::find(cascade->classes.begin(), cascade->classes.end(),
                         name) != cascade->classes.end();
      };
      if (sheet)
        for (const sigil::weave::Rule& r : sheet->rules())
          if (named(r.name())) {
            sigil::weave::merge(ownFont, r.type());
            sigil::weave::merge(ownBlock, r.block());
          }
      // A name is unknown only where NEITHER kind of sheet carries it:
      // a class a selector rule names is registered, whether that rule
      // matched this node or not.
      for (const std::string& name : cascade->classes)
        if (!(sheet && sheet->contains(name)) &&
            !namesStyleClass(*sheets, name))
          warnNoSuchClass(name, sheet != nullptr || !sheets->empty());
    }
    // Weakest first, so the strongest rule is the one left standing,
    // and every one of them under the node's own verbs.
    for (const MatchedRule& one : matched) {
      const Rule& rule = *one.rule;
      sigil::weave::merge(ownFont, rule.type());
      sigil::weave::merge(ownBlock, rule.block());
      if (rule.inkVar()) {
        inkVar = rule.inkVar();
        ownFont.color.reset();
      } else if (rule.type().color) {
        inkVar.reset();
      }
      if (rule.statesInk()) {
        inkPaint = {rule.inkPaint(), rule.inkAnchor()};
        inkPaintOrigin = rule.inkPaint().has_value();
      }
      if (!rule.vars().empty()) ruleVars.overlay(rule.vars());
    }
    if (cascade != nullptr) {
      if (cascade->font) {
        sigil::weave::merge(ownFont, *cascade->font);
        if (cascade->font->color) inkVar.reset();
      }
      if (cascade->block) sigil::weave::merge(ownBlock, *cascade->block);
      if (cascade->inkVar) inkVar = cascade->inkVar;
      if (cascade->statesInk) {
        inkPaint = {cascade->inkPaint, cascade->inkAnchor};
        inkPaintOrigin = cascade->inkPaint.has_value();
      }
    }
    sigil::weave::merge(block, ownBlock);
    if (cascade != nullptr && cascade->sampling) sampling = cascade->sampling;
    const bool anyOwnVars =
        cascade != nullptr &&
        (!cascade->varDefaults.empty() || !cascade->vars.empty());
    if (anyOwnVars || !ruleVars.empty()) {
      auto own = std::make_shared<VarTable>(
          cascade != nullptr ? cascade->varDefaults : VarTable{});
      if (parentVars) own->overlay(*parentVars);
      // A rule sets a property over what the node inherits and under
      // what the node itself sets, exactly where its other lanes sit.
      own->overlay(ruleVars);
      if (cascade != nullptr) own->overlay(cascade->vars);
      vars = std::move(own);
    }
    // The node's partial over the parent's font. A relative size in it is
    // measured against the PARENT — the size inherited — which is what
    // `1.5_em` on a heading means.
    if (!ownFont.empty())
      font = sigil::weave::overlay(parentFont, ownFont, fontSizePx(rootFont),
                                   parentLineHeight);
    if (inkVar) {
      const VarValue* value = vars ? vars->find(*inkVar) : nullptr;
      const material::Color* colour =
          value ? std::get_if<material::Color>(value) : nullptr;
      if (colour)
        font.color = material::skia::toSkColor(*colour);
      else
        warnNoSuchVar(*inkVar, true);
    }
  }
  // The kInkLerp row: the ink this node declares, easing from one colour
  // to another under its transition. The ramp is read into the resolved
  // colour here, so every node under this one inherits the colour in
  // flight and repaints with it.
  if (const auto& anim = inst.anims[Instance::kInkLerp];
      anim && anim->started && anim->value.isConnected()) {
    font.color = material::skia::toSkColor(
        lerpColour(inst.inkFrom, inst.inkTo, anim->value.value()));
    inkAnimating = true;
  }

  const bool first = !inst.cascadeResolved;
  const bool shapeChanged = first || !sameFontButColour(font, inst.font);
  const bool inkChanged =
      first || !(font.color == inst.font.color) || !(inkPaint == inst.inkPaint);
  const bool varsChanged = first || !sameVars(vars, inst.vars);
  const bool samplingChanged = first || !(sampling == inst.sampling);
  inst.font = font;
  inst.inkPaint = inkPaint;
  // An ink anchored to a box that is NOT the one being painted samples a
  // field this node's place in the tree decides, so the node invalidates
  // when it moves, exactly as a world-space material does. Set here and
  // never cleared here: the reconcile writes the flag from the node's own
  // description, and a flag left standing costs an invalidation, never a
  // wrong pixel.
  if (inkPaint.paint && inkPaint.anchor != PaintAnchor::OwnBox)
    inst.hasWorldSpaceMaterial = true;
  inst.inkPaintOrigin = inkPaintOrigin;
  inst.vars = vars;
  inst.block = block;
  inst.sampling = sampling;
  inst.sheet = sheet;
  inst.cascadeResolved = true;
  if (shapeChanged) {
    inst.lineHeight = lineHeightAt(font);
    inst.zeroAdvance = zeroAdvanceAt(font);
  }

  // A text leaf: text reconcile left owed is shaped here, once, in the
  // font and the block it lands in. After that, a change of face, size or
  // any other shaping field of an inheriting leaf, or of the writing mode
  // or locale in force, is a new paragraph and a new layout; a change of
  // any other block field is the same paragraph laid out again; a change
  // of colour alone is set on the paragraph it already has.
  if (node.kind == Kind::Text && node.textData) {
    const bool inherits = node.textData->inherits;
    const bool reshapes = inherits && !sameFontButColour(font, inst.textFont);
    // A named run resolves through the sheet in force, so a sheet that
    // changed under the leaf is a new paragraph as a new face is.
    const bool remakes =
        block.writingMode != inst.textBlock.writingMode ||
        block.lineBreakLocale != inst.textBlock.lineBreakLocale ||
        !sameSheet(sheet, inst.textSheet);
    if (inst.textDirty || !inst.paragraph) {
      inst.textDirty = false;
      materializeText(inst);
      if (inst.yoga) YGNodeMarkDirty(inst.yoga);
      needsLayout = true;
    } else if (reshapes || remakes) {
      inst.contentRev++;
      materializeText(inst);
      if (inst.yoga) YGNodeMarkDirty(inst.yoga);
      needsLayout = true;
    } else {
      if (!(block == inst.textBlock)) {
        inst.textBlock = block;
        inst.contentRev++;
        if (inst.yoga) YGNodeMarkDirty(inst.yoga);
        needsLayout = true;
      }
      if (inherits && !(font.color == inst.textFont.color))
        refreshInheritedInk(inst);
    }
  }
  // A length measured in the font, or read from a property, is rewritten
  // into the flex style when what it measures against moved.
  if ((shapeChanged || varsChanged) && inst.relativeLengths) {
    applyLayoutProps(inst);
    needsLayout = true;
  }
  // An origin is a length too, read at paint instead of by the layout: one
  // in the font or on a property moves the node's matrix when either
  // moves, under recordings that hold the matrix it had. One on the canvas
  // is moved by a resize alone, which the layout pass answers for.
  if (!first && (shapeChanged || varsChanged) &&
      originsFollowCascade(inst.styled())) {
    inst.markPaintDirtyUp();
    contentDirty = true;
  }
  if (originsFollowCanvas(inst.styled())) anyCanvasLengths = true;
  // Whatever reads the ink, a property or the sampling at paint — a stroke
  // in the ink, a fill on a property, an image through its filter — baked
  // the old value into its recording.
  if (!first && (inkChanged || varsChanged || samplingChanged)) {
    inst.markPaintDirtyUp();
    contentDirty = true;
  }
  // Where each child stands among its siblings, counted once here so a
  // structural pseudo-class is a lookup — and counted whether or not a
  // sheet is in force at this node, because a rule applied further down
  // may still name an ancestor by its position.
  indexSiblings(inst);
  for (auto& child : inst.children)
    resolveCascade(*child, font, inst.lineHeight, vars, block, sampling, sheet,
                   *sheets, inkPaint);
}

void Composer::Impl::refreshInheritedInk(Instance& inst) {
  if (!inst.paragraph || inst.inheritedInkRanges.empty()) {
    inst.textFont.color = inst.font.color;
    return;
  }
  const TextData& text = *inst.description->textData;
  const sigil::weave::TextStyle base = sigil::weave::toTextStyle(inst.font);
  for (const Instance::InkRange& range : inst.inheritedInkRanges) {
    const sigil::weave::TextStyle style =
        range.over ? sigil::weave::overlay(base, *range.over) : base;
    inst.paragraph->setPaint(range.range.start, range.range.end, style.paint);
  }
  // The restyles back over it, in the order and by the rule materialisation
  // applied them: a paint-only restyle paints its range; a whole style
  // paints its range and then every earlier paint-only restyle stands
  // again where the two overlap; a restyle folded into an axis track
  // painted nothing.
  if (inst.textState) {
    TextState& state = *inst.textState;
    const size_t count =
        std::min({text.spanRestyles.size(), state.restyleRanges.size(),
                  state.restyleFolded.size(), state.restyleStyles.size(),
                  state.restylePaintOnly.size()});
    // A partial restyle was laid over the font then in force; the ink
    // moved, so it is laid over the font now in force.
    for (size_t i = 0; i < count; ++i)
      if (const auto& partial = text.spanRestyles[i].partial)
        state.restyleStyles[i] = sigil::weave::overlay(base, *partial);
    for (size_t i = 0; i < count; ++i) {
      if (state.restyleFolded[i]) continue;
      inst.paragraph->setPaint(state.restyleRanges[i],
                               state.restyleStyles[i].paint);
      if (state.restylePaintOnly[i]) continue;
      for (size_t j = 0; j < i; ++j) {
        if (!state.restylePaintOnly[j] || state.restyleFolded[j]) continue;
        for (const sigil::weave::CharRange& a : state.restyleRanges[i])
          for (const sigil::weave::CharRange& b : state.restyleRanges[j])
            if (a.start < b.end && b.start < a.end)
              inst.paragraph->setPaint(std::max(a.start, b.start),
                                       std::min(a.end, b.end),
                                       state.restyleStyles[j].paint);
      }
    }
  }
  inst.textFont.color = inst.font.color;
  inst.markPaintDirtyUp();
  contentDirty = true;
}

}  // namespace sigil::compose
