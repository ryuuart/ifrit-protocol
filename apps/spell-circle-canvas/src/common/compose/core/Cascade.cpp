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
#include <vector>

#include "Calc.h"
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

/** The size in PIXELS a resolved font carries; the initial size where a
 *  font has none yet. A resolved size may still be absolute in another
 *  unit — an overlay leaves a size in points in points, since points are
 *  a fixed count of pixels and nothing is owed to resolve them — so the
 *  conversion is asked for rather than assumed. An `em` against a size
 *  stated in points is otherwise a multiple of the point COUNT. */
float fontSizePx(const sigil::weave::Type& font) {
  return font.size ? font.size->absolutePx() : 16.0f;
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

/** What the sheets in force say about each name @p leaf's rich runs and
 *  paragraph styles were written with, each matched as a virtual child of
 *  the leaf: the font partial and the ink the matched rules state, folded
 *  weakest first, and the block partial they state. A name no rule speaks
 *  about is left out, so it resolves as an unregistered name always has. */
void namedStylesOf(
    const Instance& leaf, const SheetChain& chain, const HasNames& names,
    sigil::weave::TypeSheet& runs,
    std::vector<std::pair<std::string, sigil::weave::Block>>& blocks) {
  runs = {};
  blocks.clear();
  if (chain.empty()) return;
  const TextData& text = *leaf.description->textData;
  const auto fontFor = [&](std::string_view name) {
    std::optional<sigil::weave::Type> partial;
    for (const MatchedRule& one : matchRulesForName(chain, leaf, name, names)) {
      if (!partial) partial.emplace();
      sigil::weave::merge(*partial, one.rule->type());
      if (const std::optional<VarRef>& reference = one.rule->inkVar()) {
        const VarValue* value =
            leaf.vars ? leaf.vars->find(*reference) : nullptr;
        if (const material::Color* colour =
                value ? std::get_if<material::Color>(value) : nullptr)
          partial->color = material::skia::toSkColor(*colour);
        else
          warnNoSuchVar(*reference, true);
      }
    }
    return partial;
  };
  if (!text.rich.hasStyles())
    for (const sigil::weave::RichText::Run& run : text.rich.runs())
      if (!run.styleName.empty() && !runs.contains(run.styleName))
        if (std::optional<sigil::weave::Type> partial = fontFor(run.styleName))
          runs.set(run.styleName, std::move(*partial));
  if (text.options.set & TextOptions::kBlockClasses)
    for (const std::string& name : text.options.blockClassNames) {
      const bool seen =
          std::any_of(blocks.begin(), blocks.end(),
                      [&](const auto& entry) { return entry.first == name; });
      if (seen) continue;
      const std::vector<MatchedRule> matched =
          matchRulesForName(chain, leaf, name, names);
      if (matched.empty()) continue;
      sigil::weave::Block partial;
      for (const MatchedRule& one : matched)
        sigil::weave::merge(partial, one.rule->block());
      blocks.emplace_back(name, std::move(partial));
    }
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
      "[compose] styleClass(\"%.*s\") names a class no rule of the sheets "
      "in force where the element lands speaks about%s — nothing was set, "
      "and the text under it is set in whatever it inherits. Apply a sheet "
      "with applyStyleSheet() on the element or on a node above it, with a "
      "rule naming .%.*s. (warned once)\n",
      (int)name.size(), name.data(),
      anySheetInScope ? "" : " (no sheet is applied on the tree above it)",
      (int)name.size(), name.data());
}

void detail::warnPropertyAnswersNoKeyword(Property property) {
  static thread_local boost::unordered_flat_set<uint8_t> warned;
  if (!warned.insert((uint8_t)property).second) return;
  const std::string_view name = propertyName(property);
  SkDebugf(
      "[compose] inherit/initial/unset was said about %.*s, and nothing "
      "resolves a keyword for that property — it is kept on the "
      "description, which no fold reads. The keyword was REFUSED, so the "
      "node describes as one that never wrote it rather than as one "
      "carrying a statement nothing answers. State the value itself. "
      "(warned once)\n",
      (int)name.size(), name.data());
}

void detail::warnRuleHoldsOnlyStaticValues(Property property) {
  static thread_local boost::unordered_flat_set<uint8_t> warned;
  if (!warned.insert((uint8_t)property).second) return;
  const std::string_view name = propertyName(property);
  SkDebugf(
      "[compose] a rule states %.*s as a live binding, an animation or a "
      "live paint, and a rule holds static values — the statement was left "
      "out, so the elements it matches keep what they would have without "
      "it. State the value itself in the rule, or the live form on the "
      "element. (warned once)\n",
      (int)name.size(), name.data());
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
    case Dimension::Unit::Calc: {
      // Every term in pixels here, where the font, the properties and the
      // canvas in force are known. A property read inside a sum must hold
      // a length a sum can hold: not a percentage, not auto, and not
      // another property.
      relative = true;
      const CalcLength& sum = calcLength(length);
      const SkSize canvas = size.isEmpty() ? rootLayoutSize : size;
      float total = sum.px + sum.em * fontSizePx(inst.font) +
                    sum.rem * fontSizePx(rootFont) + sum.lh * inst.lineHeight +
                    sum.ch * inst.zeroAdvance +
                    sum.pw * 0.01f * canvas.width() +
                    sum.ph * 0.01f * canvas.height();
      for (const auto& [id, coefficient] : sum.vars) {
        const VarValue* value =
            inst.vars ? inst.vars->find(VarRef{id}) : nullptr;
        const Dimension* found =
            value ? std::get_if<Dimension>(value) : nullptr;
        if (found == nullptr || found->unit == Dimension::Unit::Var) {
          if (inst.cascadeResolved) warnNoSuchVar(VarRef{id}, false);
          continue;
        }
        if (found->unit == Dimension::Unit::Pct ||
            found->unit == Dimension::Unit::Auto) {
          warnRefusedSum(found->unit, Dimension::Unit::Calc);
          continue;
        }
        bool inner = false;
        total += coefficient * resolveLength(inst, *found, inner);
      }
      return total;
    }
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
  hasNames.begin(++cascadePass);
  indexRoot(*root);
  const SheetChain none;
  const InkInForce noInkPaint;
  resolveCascade(*root, rootFont, rootLineHeight, nullptr, rootBlock,
                 rootSampling, none, noInkPaint);
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
    const SheetChain& parentSheets, const InkInForce& parentInkPaint) {
  const ElementNode& node = *inst.description;
  const bool first = !inst.cascadeResolved;
  sigil::weave::Type font = parentFont;
  // The ink's paint inherits exactly as the font's colour does, and the
  // node that STATED it is the box a declaring-box anchor maps onto.
  InkInForce inkPaint = parentInkPaint;
  bool inkPaintOrigin = false;
  // Whether the colour resolved below is one this node produced — a
  // layer of its own fold said so — rather than one that simply arrived
  // from its parent. Only the second kind can be a colour an ancestor is
  // in the middle of easing, which is the one case the node's own lane
  // must stand aside for.
  bool statesOwnInk = false;
  std::shared_ptr<const VarTable> vars = parentVars;
  sigil::weave::Block block = parentBlock;
  std::optional<SkSamplingOptions> sampling = parentSampling;
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
    bool anyHas = false;
    for (const StyleSheet& applied : cascade->appliedSheets) {
      extended.push_back(ScopedSheet{&applied, &inst});
      const SheetBody* body = SheetAccess::body(applied);
      if (body == nullptr || !body->usesHas) continue;
      anyHas = true;
      hasNames.add(*body);
    }
    // A :has() LOOKS DOWN, at a subtree the pass has not reached yet, so
    // what it asks about is summarised here, bottom up — only under a
    // node applying a sheet that uses one, since a :has() can see nothing
    // outside the subtree its sheet was applied at. A summary an ancestor
    // took this pass is taken again where a sheet since gave a name a bit,
    // because that summary says nothing about the new name.
    if (anyHas && !hasNames.summaryCurrent(inst))
      summariseForHas(inst, hasNames);
    sheets = &extended;
  }
  // The rules that speak about this node, weakest first. A rule's
  // SUBJECT is this node, which lies in the applying node's subtree by
  // construction; every other compound of its selector is matched
  // inside that subtree too, so nothing above or beside the applying
  // node can satisfy one.
  const std::vector<MatchedRule> matched =
      sheets->empty() ? std::vector<MatchedRule>{}
                      : matchRules(*sheets, inst, hasNames);
  // The strongest matched rule stating a transition gives the node the
  // one its own verb has not; the matches run weakest first.
  const Rule* transitionRule = nullptr;
  for (const MatchedRule& one : matched)
    if (one.rule->transition()) transitionRule = one.rule;
  if (transitionRule != nullptr)
    inst.ruleTransition = transitionRule->transition();
  else if (inst.ruleTransition)
    inst.ruleTransition.reset();
  // WHAT THE MATCHED RULES STATE about the carried properties, folded
  // into the layer that stands between the parent's answers and this
  // node's own verbs. It is rebuilt here because a rule can move while
  // this node's own declarations stand still — a class toggled, a sheet
  // applied above — and a node whose declarations did not move never
  // reaches the patch.
  std::unique_ptr<const RuleLayer> layer;
  if (!matched.empty()) {
    std::vector<const Rule*> rules;
    rules.reserve(matched.size());
    for (const MatchedRule& one : matched) rules.push_back(one.rule);
    layer = ruleLayerOf(rules);
  }
  const bool layerMoved = !ruleLayerEqual(inst.ruleLayer.get(), layer.get());
  if (layerMoved) inst.ruleLayer = std::move(layer);
  // A NODE THAT WRITES A KEYWORD takes a value from its parent, and the
  // parent's answer can move while this node's own declarations stand
  // still — the one case the patch cannot see, because a node whose
  // declarations did not move never reaches it. So the fold runs again
  // here, for those nodes and for the ones whose rules moved; a node with
  // neither is untouched and pays nothing. What it costs is one
  // invalidation per such node per pass, and the pass runs only on a
  // frame where something already changed.
  if (node.keywords || layerMoved) {
    const ComputedStyle before = inst.computed;
    resolveStyle(inst.parent ? &inst.parent->computed : nullptr, node,
                 inst.computed, inst.ruleLayer.get());
    if (!(before.layout == inst.computed.layout)) {
      applyLayoutProps(inst);
      needsLayout = true;
    }
    // ONLY WHERE THE SECOND FOLD MOVED SOMETHING. The fold's inputs are
    // this node's own declarations, which did not move — a node whose
    // declarations moved came through the patch — and the parent's
    // answers and the rules', which may have. Invalidating whether or not
    // they did re-records every keyword-bearing node and its ancestors on
    // every frame anything anywhere changed.
    if (!first && !computedStyleEqual(before, inst.computed)) {
      // A PROPERTY THAT MOVED HERE MOVED FOR THE SAME REASON one changed
      // by the node's own verbs does, so it eases the same way: the lanes
      // are retargeted from the style that stood before this fold. The
      // patch cannot do it — this node never reaches the patch, its own
      // declarations having compared equal — which is why the pass that
      // found the change is the one that acts on it.
      // The lane lists hold pointers into `before`, a local of this
      // frame, so they live and die with it rather than outliving the
      // description they point at.
      std::vector<Lane> prevLanes, nextLanes;
      retargetProperties(inst, {before, node}, prevLanes, nextLanes);
      inst.markPaintDirtyUp();
      contentDirty = true;
    }
  }
  if (cascade != nullptr || !matched.empty()) {
    // The role's defaults, the rules that matched a selector, then the
    // node's own declarations. Partials are merged first so a relative
    // size resolves once against the parent's font rather than
    // compounding.
    sigil::weave::Type ownFont;
    sigil::weave::Block ownBlock;
    // The property the ink reads, from whichever layer last said so.
    std::optional<VarRef> inkVar;
    VarTable ruleVars;
    VarTable ruleVarDefaults;
    // Where a rule wrote the font, the block or the custom properties as
    // `initial`, the layers above it are laid over the initial value
    // rather than over the one inherited.
    bool fontFromInitial = false;
    bool blockFromInitial = false;
    bool varsFromInitial = false;
    if (cascade != nullptr && cascade->role) {
      sigil::weave::merge(ownFont, cascade->role->font);
      sigil::weave::merge(ownBlock, cascade->role->block);
    }
    // A class is unknown only where no rule of the sheets in force names
    // it, whether that rule matched this node or not.
    if (cascade != nullptr)
      for (const std::string& name : cascade->classes)
        if (!namesStyleClass(*sheets, name))
          warnNoSuchClass(name, !sheets->empty());
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
      const ElementNode& stated = *rule.node();
      if (const CascadeData* said =
              stated.cascadeData ? &*stated.cascadeData : nullptr) {
        if (said->sampling) sampling = said->sampling;
        if (!said->varDefaults.empty())
          ruleVarDefaults.overlay(said->varDefaults);
      }
      // A RULE'S KEYWORD over an inherited property throws away what the
      // weaker layers said and stands in the value arriving from above,
      // or the initial one; the stronger layers are then laid over that.
      if (!stated.keywords) continue;
      for (const sigil::weave::KeywordTable<Property>::Entry& entry :
           stated.keywords->entries()) {
        const bool fromParent = resolveKeyword(entry.keyword, entry.field) ==
                                sigil::weave::Keyword::Inherit;
        switch (entry.field) {
          case Property::Font: {
            const std::optional<SkColor4f> colour = ownFont.color;
            ownFont = {};
            ownFont.color = colour;
            fontFromInitial = !fromParent;
            break;
          }
          case Property::Block:
            ownBlock = {};
            blockFromInitial = !fromParent;
            break;
          case Property::Ink:
            inkVar.reset();
            if (fromParent)
              ownFont.color.reset();
            else
              ownFont.color = sigil::weave::initialType().color;
            inkPaint = fromParent ? parentInkPaint : InkInForce{};
            inkPaintOrigin = false;
            break;
          case Property::CustomProperties:
            ruleVars = {};
            ruleVarDefaults = {};
            varsFromInitial = !fromParent;
            break;
          case Property::ImageRendering:
            if (fromParent)
              sampling = parentSampling;
            else
              sampling.reset();
            break;
          default:
            break;
        }
      }
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
    // The node's partial RESOLVED against the block it inherits, so a
    // field written as a keyword takes the inherited value or none at
    // all. A merge could not: it has no base to inherit from.
    block = sigil::weave::overlay(
        blockFromInitial ? sigil::weave::Block{} : block, ownBlock);
    if (cascade != nullptr && cascade->sampling) sampling = cascade->sampling;
    const bool anyOwnVars =
        cascade != nullptr &&
        (!cascade->varDefaults.empty() || !cascade->vars.empty());
    if (varsFromInitial) vars = nullptr;
    if (anyOwnVars || !ruleVars.empty() || !ruleVarDefaults.empty()) {
      // Defaults stand under everything inherited: a rule's under the
      // node's own, and both under any value set above.
      auto own = std::make_shared<VarTable>(ruleVarDefaults);
      if (cascade != nullptr) own->overlay(cascade->varDefaults);
      if (parentVars && !varsFromInitial) own->overlay(*parentVars);
      // A rule sets a property over what the node inherits and under
      // what the node itself sets, exactly where its other lanes sit.
      own->overlay(ruleVars);
      if (cascade != nullptr) own->overlay(cascade->vars);
      vars = std::move(own);
    }
    // The node's partial over the parent's font. A relative size in it is
    // measured against the PARENT — the size inherited — which is what
    // `1.5_em` on a heading means.
    if (fontFromInitial) {
      sigil::weave::Type initial = sigil::weave::initialType();
      initial.color = parentFont.color;
      font = sigil::weave::overlay(initial, ownFont, fontSizePx(rootFont),
                                   parentLineHeight);
    } else if (!ownFont.empty()) {
      font = sigil::weave::overlay(parentFont, ownFont, fontSizePx(rootFont),
                                   parentLineHeight);
    }
    if (ownFont.color) statesOwnInk = true;
    if (inkVar) {
      const VarValue* value = vars ? vars->find(*inkVar) : nullptr;
      const material::Color* colour =
          value ? std::get_if<material::Color>(value) : nullptr;
      if (colour) {
        font.color = material::skia::toSkColor(*colour);
        statesOwnInk = true;
      } else {
        warnNoSuchVar(*inkVar, true);
      }
    }
  }
  // THE WIDE KEYWORDS, over everything a role default, a rule or this
  // node's own verbs said. These five properties INHERIT, and `inherit`
  // is NOT what already happens to them: it throws away the layers folded
  // above and stands in the value that arrived from the parent, which is
  // the only thing a keyword can mean when the verbs and the rules sit
  // over the inherited value. `initial` stops the inheriting instead and
  // stands in the value the property has under no ancestor at all, and
  // `unset` comes to `inherit` here because these five inherit.
  if (node.keywords)
    for (const sigil::weave::KeywordTable<Property>::Entry& entry :
         node.keywords->entries()) {
      const bool fromParent = resolveKeyword(entry.keyword, entry.field) ==
                              sigil::weave::Keyword::Inherit;
      switch (entry.field) {
        case Property::Font: {
          // The colour is the INK, a property of its own, so a type
          // written as a keyword leaves it exactly as it stood.
          const std::optional<SkColor4f> ink = font.color;
          font = fromParent ? parentFont : sigil::weave::initialType();
          font.color = ink;
          break;
        }
        case Property::Ink:
          font.color =
              fromParent ? parentFont.color : sigil::weave::initialType().color;
          // `inherit` names the colour arriving from above and nothing
          // else; `initial` is a colour of this node's own choosing.
          statesOwnInk = !fromParent;
          // Whichever way it went, the paint in force came from somewhere
          // above this node or from nowhere, so this node is not the box a
          // declaring-box anchor maps onto.
          inkPaint = fromParent ? parentInkPaint : InkInForce{};
          inkPaintOrigin = false;
          break;
        case Property::Block:
          block = fromParent ? parentBlock : sigil::weave::Block{};
          break;
        case Property::ImageRendering:
          if (fromParent)
            sampling = parentSampling;
          else
            sampling.reset();
          break;
        case Property::CustomProperties:
          vars = fromParent ? parentVars : nullptr;
          break;
        default:
          break;
      }
    }

  // THE kInkLerp ROW'S TRIGGER, on the colour the fold above RESOLVED. A
  // colour that moved because a class, a matched rule or a custom property
  // moved eases exactly as the same change written with `ink()` does,
  // which is the whole reason the trigger stands here and not in the
  // patch: a node whose own declarations did not move never reaches the
  // patch, and a node whose ink arrives through a rule has none that did.
  // A node resolving its first colour has nothing to ease from, so it
  // records the target and starts nothing. A node whose colour ARRIVES
  // from an ancestor has no lane of its own at all: the ancestor that
  // states the colour is the one easing it, everything under follows the
  // ramp through the inherited value, and a second lane here would run a
  // whole duration behind the one above it.
  //
  // THIS IS WHERE INK PARTS FROM THE PROPERTY LANES, whose inherited
  // answer a child DOES ease for itself, and the difference is mechanical
  // rather than a matter of taste. The ink ramp is read back into the
  // resolved colour just below, so it propagates down the tree on its own
  // and a child that ran a lane would be easing an input already in
  // flight. A property ramp stays on the node that runs it — the paint
  // layer reads it, the fold does not — so a child inherits the property's
  // TARGET, and its own lane is the only thing that can move it.
  retargetInk(inst, statesOwnInk ? font.color : std::nullopt,
              inst.transitionInForce(), first);
  // …and the ramp is read into the resolved colour here, so every node
  // under this one inherits the colour in flight and repaints with it.
  if (const auto& anim = inst.anims[Instance::kInkLerp];
      anim && anim->started && anim->value.isConnected() && inst.inkTarget) {
    font.color = material::skia::toSkColor(
        lerpColour(inst.inkFrom, *inst.inkTarget, anim->value.value()));
    inkAnimating = true;
  }

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
    // A named run resolves through the sheets in force, so a sheet that
    // changed what a name means under the leaf is a new paragraph as a new
    // face is.
    sigil::weave::TypeSheet runStyles;
    std::vector<std::pair<std::string, sigil::weave::Block>> blockStyles;
    namedStylesOf(inst, *sheets, hasNames, runStyles, blockStyles);
    const bool namesMoved =
        !(runStyles == inst.runStyles) || !(blockStyles == inst.blockStyles);
    inst.runStyles = std::move(runStyles);
    inst.blockStyles = std::move(blockStyles);
    inst.sheetsInForce = !sheets->empty();
    const bool remakes =
        block.writingMode != inst.textBlock.writingMode ||
        block.lineBreakLocale != inst.textBlock.lineBreakLocale || namesMoved;
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
  // may still name an ancestor by its position. A subtree a :has()
  // summary covered this pass was indexed by that summary already.
  if (inst.hasSummaryPass != cascadePass) indexSiblings(inst);
  for (auto& child : inst.children)
    resolveCascade(*child, font, inst.lineHeight, vars, block, sampling,
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
