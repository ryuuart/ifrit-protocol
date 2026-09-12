/** @file
 * The cascade: the custom-property names, the pass that resolves the font,
 * the ink and the properties in force at every node from the root's down
 * and applies what changed, the ink-only repaint of an inheriting leaf,
 * the lengths measured against the font, and the reference a fill
 * resolves through at paint.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the once-per-name diagnostics
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

SkColor4f lerpColour(const SkColor4f& from, const SkColor4f& to, float t) {
  SkColor4f out = to;
  for (int i = 0; i < 4; ++i)
    out.vec()[i] = from.vec()[i] + (to.vec()[i] - from.vec()[i]) * t;
  return out;
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
      "[compose] styleClass(\"%.*s\") names a class no weave::StyleSheet in "
      "scope carries%s — nothing was set, and the text under it is set in "
      "whatever it inherits. Bind a sheet with "
      "environment::Provide<weave::StyleSheet> around the code that builds "
      "the element, and register the name on it. (warned once)\n",
      (int)name.size(), name.data(),
      anySheetInScope ? "" : " (no sheet is bound at all)");
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
      return Fill::color(ctx.ink);
    case Fill::Ref::Var: {
      const VarValue* value =
          ctx.vars ? ctx.vars->find(VarRef{fill.varId}) : nullptr;
      const SkColor4f* colour = value ? std::get_if<SkColor4f>(value) : nullptr;
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

// ---------------------------------------------------------------------------
// The pass

void Composer::Impl::runCascade() {
  if (!root) return;
  inkAnimating = false;
  if (rootLineHeight <= 0.0f) rootLineHeight = lineHeightAt(rootFont);
  resolveCascade(*root, rootFont, rootLineHeight, nullptr);
  // A running ink transition moves the colour every frame, so the next
  // frame resolves again; otherwise the answers stand until a reconcile
  // says otherwise.
  cascadeDirty = inkAnimating;
}

void Composer::Impl::resolveCascade(
    Instance& inst, const sigil::weave::Type& parentFont,
    float parentLineHeight, const std::shared_ptr<const VarTable>& parentVars) {
  const ElementNode& node = *inst.description;
  sigil::weave::Type font = parentFont;
  std::shared_ptr<const VarTable> vars = parentVars;
  if (node.cascadeData) {
    const CascadeData& cascade = *node.cascadeData;
    if (!cascade.vars.empty()) {
      auto own = std::make_shared<VarTable>(parentVars ? *parentVars
                                                       : VarTable{});
      own->overlay(cascade.vars);
      vars = std::move(own);
    }
    // The node's partial over the parent's font. A relative size in it is
    // measured against the PARENT — the size inherited — which is what
    // `1.5_em` on a heading means.
    if (cascade.font)
      font = sigil::weave::overlay(parentFont, *cascade.font,
                                   fontSizePx(rootFont), parentLineHeight);
    if (cascade.inkVar) {
      const VarValue* value = vars ? vars->find(*cascade.inkVar) : nullptr;
      const SkColor4f* colour = value ? std::get_if<SkColor4f>(value) : nullptr;
      if (colour)
        font.color = *colour;
      else
        warnNoSuchVar(*cascade.inkVar, true);
    }
  }
  // The kInkLerp row: the ink this node declares, easing from one colour
  // to another under its transition. The ramp is read into the resolved
  // colour here, so every node under this one inherits the colour in
  // flight and repaints with it.
  if (const auto& anim = inst.anims[Instance::kInkLerp];
      anim && anim->started && anim->value.isConnected()) {
    font.color = lerpColour(inst.inkFrom, inst.inkTo, anim->value.value());
    inkAnimating = true;
  }

  const bool first = !inst.cascadeResolved;
  const bool shapeChanged = first || !sameFontButColour(font, inst.font);
  const bool inkChanged = first || !(font.color == inst.font.color);
  const bool varsChanged = first || !sameVars(vars, inst.vars);
  inst.font = font;
  inst.vars = vars;
  inst.cascadeResolved = true;
  if (shapeChanged) inst.lineHeight = lineHeightAt(font);

  // An inheriting text leaf: a change of face, size or any other shaping
  // field is a new paragraph and a new layout; a change of colour alone
  // is set on the paragraph it already has.
  if (node.kind == Kind::Text && node.textData && node.textData->inherits &&
      inst.paragraph) {
    if (!sameFontButColour(font, inst.textFont)) {
      inst.contentRev++;
      materializeText(inst);
      if (inst.yoga) YGNodeMarkDirty(inst.yoga);
      needsLayout = true;
    } else if (!(font.color == inst.textFont.color)) {
      refreshInheritedInk(inst);
    }
  }
  // A length measured in the font, or read from a property, is rewritten
  // into the flex style when what it measures against moved.
  if ((shapeChanged || varsChanged) && inst.relativeLengths) {
    applyLayoutProps(inst);
    needsLayout = true;
  }
  // Whatever reads the ink or a property at paint — a stroke in the ink, a
  // fill on a property — baked the old value into its recording.
  if (!first && (inkChanged || varsChanged)) {
    inst.markPaintDirtyUp();
    contentDirty = true;
  }
  for (auto& child : inst.children)
    resolveCascade(*child, font, inst.lineHeight, vars);
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
    const TextState& state = *inst.textState;
    const size_t count =
        std::min({text.spanRestyles.size(), state.restyleRanges.size(),
                  state.restyleFolded.size()});
    for (size_t i = 0; i < count; ++i) {
      if (state.restyleFolded[i]) continue;
      const SpanRestyle& restyle = text.spanRestyles[i];
      inst.paragraph->setPaint(state.restyleRanges[i], restyle.style.paint);
      if (restyle.paintOnly) continue;
      for (size_t j = 0; j < i; ++j) {
        if (!text.spanRestyles[j].paintOnly || state.restyleFolded[j])
          continue;
        for (const sigil::weave::CharRange& a : state.restyleRanges[i])
          for (const sigil::weave::CharRange& b : state.restyleRanges[j])
            if (a.start < b.end && b.start < a.end)
              inst.paragraph->setPaint(std::max(a.start, b.start),
                                       std::min(a.end, b.end),
                                       text.spanRestyles[j].style.paint);
      }
    }
  }
  inst.textFont.color = inst.font.color;
  inst.markPaintDirtyUp();
  contentDirty = true;
}

}  // namespace sigil::compose
