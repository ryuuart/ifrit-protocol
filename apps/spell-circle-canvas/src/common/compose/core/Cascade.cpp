/** @file
 * The cascade: the custom-property names, the pass that resolves the font,
 * the ink and the properties in force at every node from the root's down
 * and applies what changed, the ink-only repaint of an inheriting leaf,
 * the lengths measured against the font, and the reference a fill
 * resolves through at paint.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "Calc.h"
#include "ComposeRuntime.h"
#include "FaceChoice.h"
#include "LonghandFold.h"
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

/** What the rules of @p matched state about a text leaf's text
 *  properties, folded weakest first, or null where none states one. */
std::unique_ptr<const RuleTextLayer> ruleTextLayerOf(
    const std::vector<MatchedRule>& matched) {
  std::unique_ptr<RuleTextLayer> layer;
  for (const MatchedRule& one : matched) {
    const ElementNode& stated = *one.rule->node();
    if (!stated.textData) continue;
    const TextData& said = *stated.textData;
    if (said.options.set == 0) continue;
    if (!layer) layer = std::make_unique<RuleTextLayer>();
    layer->options.overlay(said.options);
    if (said.options.set & TextOptions::kTextStroke) {
      layer->statesStroke = true;
      layer->hasTextStroke = said.hasTextStroke;
      layer->textStrokeWidth = said.textStrokeWidth;
      layer->textStrokeFill = said.textStrokeFill;
    }
  }
  return layer;
}

bool sameRuleText(const RuleTextLayer* a, const RuleTextLayer* b) {
  if (a == nullptr || b == nullptr) return a == b;
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

void Composer::Impl::resolveTextIndent(const Instance& inst,
                                       const Dimension& indent,
                                       sigil::weave::ParagraphBlock& block,
                                       std::optional<float>& percent) const {
  // A property is read once, into the length it holds, and that length is
  // resolved as if it had been written here.
  const Dimension* length = &indent;
  if (indent.unit == Dimension::Unit::Var) {
    const VarValue* value =
        inst.vars ? inst.vars->find(indent.reference()) : nullptr;
    length = value ? std::get_if<Dimension>(value) : nullptr;
    if (length == nullptr || length->unit == Dimension::Unit::Var ||
        length->unit == Dimension::Unit::Auto) {
      warnNoSuchVar(indent.reference(), false);
      return;
    }
  }
  if (length->unit == Dimension::Unit::Pct) {
    percent = length->value;
    block.firstLineIndent.reset();
    return;
  }
  bool relative = false;
  block.firstLineIndent = resolveLength(inst, *length, relative);
  percent.reset();
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
                 rootSampling, none, noInkPaint, rootFont.color);
  // A running ink transition moves the colour every frame, so the next
  // frame resolves again; otherwise the answers stand until a reconcile
  // says otherwise.
  cascadeDirty = inkAnimating;
}

void Composer::Impl::resolveCascade(
    Instance& inst, const sigil::weave::Type& parentFont,
    float parentLineHeight, const std::shared_ptr<const VarTable>& parentVars,
    const sigil::weave::ParagraphBlock& parentBlock,
    const std::optional<SkSamplingOptions>& parentSampling,
    const SheetChain& parentSheets, const InkInForce& parentInkPaint,
    const std::optional<material::Color>& parentInkTarget) {
  const ElementNode& node = *inst.description;
  const bool first = !inst.cascadeResolved;
  sigil::weave::Type font = parentFont;
  // THE FAMILY BY NAME, WHETHER THE STYLE IS ITALIC, AND AN INDENT AS A
  // PERCENTAGE OF THE MEASURE, each inherited beside the font and the
  // block, which cannot hold them: a face does not say which name found it
  // or whether it stands in for an italic, and a percentage waits for the
  // measure.
  const bool parentItalic = inst.parent != nullptr && inst.parent->italic;
  const std::string* parentFamily =
      inst.parent != nullptr && !inst.parent->family.empty()
          ? &inst.parent->family
          : nullptr;
  LonghandFold longhands(
      parentFamily, parentItalic,
      inst.parent != nullptr ? inst.parent->indentPercent : std::nullopt);
  // The ink's paint inherits exactly as the font's colour does, and the
  // node that STATED it is the box `PaintBox::Subtree` stretches it over.
  InkInForce inkPaint = parentInkPaint;
  bool inkPaintOrigin = false;
  // Whether the colour resolved below is one this node produced — a
  // layer of its own fold said so — rather than one that simply arrived
  // from its parent. Only the second kind can be a colour an ancestor is
  // in the middle of easing, so only the second takes its lane's target
  // from the parent's target rather than from the colour it arrived as.
  bool statesOwnInk = false;
  std::shared_ptr<const VarTable> vars = parentVars;
  sigil::weave::ParagraphBlock block = parentBlock;
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
  //
  // Only where the rules themselves changed: a pass that matches the rules
  // it matched last time — the same sheet, the same classes — keeps the
  // layers it built then, so a tree re-described every frame under one
  // sheet does not rebuild a layer per matched node per frame.
  const bool sameRules = std::equal(
      matched.begin(), matched.end(), inst.ruleLayerSource.begin(),
      inst.ruleLayerSource.end(),
      [](const MatchedRule& one, const std::shared_ptr<ElementNode>& held) {
        return one.rule->node().get() == held.get();
      });
  bool layerMoved = false;
  bool fillMaterialMoved = false;
  if (!sameRules) {
    inst.ruleLayerSource.clear();
    for (const MatchedRule& one : matched)
      inst.ruleLayerSource.push_back(one.rule->node());
    std::unique_ptr<const RuleLayer> layer;
    if (!matched.empty()) {
      std::vector<const Rule*> rules;
      rules.reserve(matched.size());
      for (const MatchedRule& one : matched) rules.push_back(one.rule);
      layer = ruleLayerOf(rules);
    }
    layerMoved = !ruleLayerEqual(inst.ruleLayer.get(), layer.get());
    if (layerMoved) {
      fillMaterialMoved =
          !ruleFillMaterialEqual(inst.ruleLayer.get(), layer.get());
      inst.ruleLayer = std::move(layer);
    }
    // Set, never cleared here, for the reason the ink's box below is.
    if (ruleFillUsesWorldSpace(inst.ruleLayer.get()))
      inst.hasWorldSpaceMaterial = true;
  }
  // A NODE THAT WRITES A KEYWORD takes a value from its parent, and the
  // parent's answer can move while this node's own declarations stand
  // still — the one case the patch cannot see, because a node whose
  // declarations did not move never reaches it. So the fold runs again
  // here, for those nodes and for the ones whose rules moved; a node with
  // neither is untouched and pays nothing. What it costs is one
  // invalidation per such node per pass, and the pass runs only on a
  // frame where something already changed.
  if (node.fields.keywords() || layerMoved) {
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
    // A rule's paint behind the fill is the one half of the layer the
    // computed style does not hold, so it is asked about apart.
    if (!first &&
        (fillMaterialMoved || !computedStyleEqual(before, inst.computed))) {
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
    sigil::weave::ParagraphBlock ownBlock;
    // The property the ink reads, from whichever layer last said so.
    std::optional<VarRef> inkVar;
    VarTable ruleVars;
    VarTable ruleVarDefaults;
    // Where a rule wrote the font, the paragraph setting or the custom
    // properties as `initial`, the layers above it are laid over the initial
    // value rather than over the one inherited.
    bool fontFromInitial = false;
    bool blockFromInitial = false;
    bool varsFromInitial = false;
    if (cascade != nullptr && cascade->role) {
      sigil::weave::merge(ownFont, cascade->role->font);
      sigil::weave::merge(ownBlock, cascade->role->block);
      longhands.state(&cascade->role->font, &cascade->role->block, nullptr);
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
      sigil::weave::merge(ownBlock, rule.paragraph());
      if (rule.inkVar()) {
        inkVar = rule.inkVar();
        ownFont.color.reset();
      } else if (rule.type().color) {
        inkVar.reset();
      }
      if (rule.statesInk()) {
        // A text unit needs a passage to cut; on any other node it is
        // dropped exactly as the node's own ink() drops it.
        PaintBox box = rule.inkBox();
        if (textUnitOf(box) && node.kind != Kind::Text) {
          warnInkTextUnitNeedsAPassage();
          box = PaintBox::Element;
        }
        inkPaint = {rule.inkPaint(), box};
        inkPaintOrigin = rule.inkPaint().has_value();
      }
      if (!rule.vars().empty()) ruleVars.overlay(rule.vars());
      const ElementNode& stated = *rule.node();
      if (const CascadeData* said =
              stated.cascadeData ? &*stated.cascadeData : nullptr) {
        longhands.state(said->font ? &*said->font : nullptr,
                        said->block ? &*said->block : nullptr, said);
        if (said->sampling) sampling = said->sampling;
        if (!said->varDefaults.empty())
          ruleVarDefaults.overlay(said->varDefaults);
      }
      // A RULE'S KEYWORD over an inherited property throws away what the
      // weaker layers said and stands in the value arriving from above,
      // or the initial one; the stronger layers are then laid over that.
      if (!stated.fields.keywords()) continue;
      for (const sigil::weave::KeywordTable<Property>::Entry& entry :
           stated.fields.keywords()->entries()) {
        const bool fromParent = resolveKeyword(entry.keyword, entry.field) ==
                                sigil::weave::Keyword::Inherit;
        switch (entry.field) {
          case Property::Font: {
            const std::optional<material::Color> colour = ownFont.color;
            ownFont = {};
            ownFont.color = colour;
            fontFromInitial = !fromParent;
            longhands.fontKeyword(fromParent);
            break;
          }
          case Property::Paragraph:
            ownBlock = {};
            blockFromInitial = !fromParent;
            longhands.paragraphKeyword(fromParent);
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
      longhands.state(cascade->font ? &*cascade->font : nullptr,
                      cascade->block ? &*cascade->block : nullptr, cascade);
      if (cascade->inkVar) inkVar = cascade->inkVar;
      if (cascade->statesInk) {
        inkPaint = {cascade->inkPaint, cascade->inkBox};
        inkPaintOrigin = cascade->inkPaint.has_value();
      }
    }
    // The node's partial RESOLVED against the block it inherits, so a
    // field written as a keyword takes the inherited value or none at
    // all. A merge could not: it has no base to inherit from.
    block = sigil::weave::overlay(
        blockFromInitial ? sigil::weave::ParagraphBlock{} : block, ownBlock);
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
    // A FAMILY NAMED, AN ITALIC TURNED ON OR OFF, OR A WEIGHT MOVED UNDER A
    // FAMILY NAMED ABOVE, CHOOSES THE FACE — through the composer's font
    // context, which is why it is done here and not by the verb — and so
    // does a face stated under an italic. The face is the family's at the
    // weight and style in force, as CSS matches one per element.
    const bool italic = longhands.italic();
    const std::string* family = longhands.familyInForce();
    if (longhands.familyNamed() != nullptr || italic != parentItalic ||
        (longhands.faceStated() && italic) ||
        (family != nullptr && font.weight != parentFont.weight))
      chooseFace(fonts, font, family, italic);
    if (ownFont.color) statesOwnInk = true;
    if (inkVar) {
      const VarValue* value = vars ? vars->find(*inkVar) : nullptr;
      const material::Color* colour =
          value ? std::get_if<material::Color>(value) : nullptr;
      if (colour) {
        font.color = *colour;
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
  if (node.fields.keywords())
    for (const sigil::weave::KeywordTable<Property>::Entry& entry :
         node.fields.keywords()->entries()) {
      const bool fromParent = resolveKeyword(entry.keyword, entry.field) ==
                              sigil::weave::Keyword::Inherit;
      switch (entry.field) {
        case Property::Font: {
          // The colour is the INK, a property of its own, so a type
          // written as a keyword leaves it exactly as it stood.
          const std::optional<material::Color> ink = font.color;
          font = fromParent ? parentFont : sigil::weave::initialType();
          font.color = ink;
          longhands.fontKeyword(fromParent);
          break;
        }
        case Property::Ink:
          font.color =
              fromParent ? parentFont.color : sigil::weave::initialType().color;
          // `inherit` names the colour arriving from above and nothing
          // else; `initial` is a colour of this node's own choosing.
          statesOwnInk = !fromParent;
          // Whichever way it went, the paint in force came from somewhere
          // above this node or from nowhere, so this node is not the box
          // `PaintBox::Subtree` stretches it over.
          inkPaint = fromParent ? parentInkPaint : InkInForce{};
          inkPaintOrigin = false;
          break;
        case Property::Paragraph:
          block = fromParent ? parentBlock : sigil::weave::ParagraphBlock{};
          longhands.paragraphKeyword(fromParent);
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
  // records the target and starts nothing.
  //
  // THE TARGET IS NEVER A COLOUR IN FLIGHT. A node that states its ink
  // heads for the colour it states; one that inherits heads for the
  // TARGET its parent heads for, not the colour the parent's ramp stands
  // at this frame, which moves every frame and would restart the lane on
  // each. So a node under its own `transition()` eases what it inherits
  // with a lane of its own, over its own duration, as it eases a fill.
  const std::optional<material::Color> inkTarget =
      statesOwnInk ? font.color : parentInkTarget;
  const std::optional<motion::Transition>& inkTransition =
      inst.transitionInForce();
  retargetInk(inst, inkTarget, inkTransition, first);
  // …and the colour this node shows is read here, so every node under it
  // inherits it and repaints with it: the lane's colour while it runs,
  // else its target. A node with NO transition of its own keeps the
  // colour that arrived from above, so it follows an ancestor's ramp as
  // that ramp happens.
  if (const auto& anim = inst.anims[Instance::kInkLerp];
      anim && anim->started && anim->value.isConnected() && inst.inkTarget) {
    font.color = lerpColour(inst.inkFrom, *inst.inkTarget, anim->value.value());
    inkAnimating = true;
  } else if (inkTransition && inst.inkTarget) {
    font.color = *inst.inkTarget;
  }

  const bool shapeChanged = first || !sameFontButColour(font, inst.font);
  const bool inkChanged =
      first || !(font.color == inst.font.color) || !(inkPaint == inst.inkPaint);
  const bool varsChanged = first || !sameVars(vars, inst.vars);
  const bool samplingChanged = first || !(sampling == inst.sampling);
  inst.font = font;
  inst.inkPaint = inkPaint;
  // An ink stretched over a box that is NOT the one painted samples a
  // field this node's place in the tree decides, so the node invalidates
  // when it moves, exactly as a world-space material does. Set here and
  // never cleared here: the reconcile writes the flag from the node's own
  // description, and a flag left standing costs an invalidation, never a
  // wrong pixel.
  if (inkPaint.paint && spreadsAcrossTree(inkPaint.box))
    inst.hasWorldSpaceMaterial = true;
  inst.inkPaintOrigin = inkPaintOrigin;
  inst.vars = vars;
  inst.italic = longhands.italic();
  if (const std::string* family = longhands.familyInForce()) {
    if (inst.family != *family) inst.family = *family;
  } else if (!inst.family.empty()) {
    inst.family.clear();
  }
  if (shapeChanged) {
    inst.lineHeight = lineHeightAt(font);
    inst.zeroAdvance = zeroAdvanceAt(font);
  }
  // AN INDENT IN A RELATIVE UNIT BECOMES PIXELS HERE, against this node's
  // own font, line and properties, and is inherited as the pixels, as
  // CSS computes it; a percentage of the measure waits for the measure.
  std::optional<float>& indentPercent = longhands.indentPercent();
  if (longhands.indent())
    resolveTextIndent(inst, *longhands.indent(), block, indentPercent);
  const bool indentPercentMoved = indentPercent != inst.indentPercent;
  inst.indentPercent = indentPercent;
  inst.block = block;
  inst.sampling = sampling;
  inst.cascadeResolved = true;

  // A text leaf: text reconcile left owed is shaped here, once, in the font
  // and the paragraph setting it lands in. After that, a change of face, size
  // or any other shaping field of an inheriting leaf, or of the writing mode
  // or locale in force, is a new paragraph and a new layout; a change of any
  // other block field is the same paragraph laid out again; a change of colour
  // alone is set on the paragraph it already has.
  if (node.kind == Kind::Text && node.textData) {
    const bool inherits = node.textData->inherits;
    const bool reshapes = inherits && !sameFontButColour(font, inst.textFont);
    // THE TEXT PROPERTIES IN FORCE: what the matched rules state, with the
    // leaf's own statements standing over it. A rule that moved one is a
    // new layout of the same words; one that moved the outline alone is a
    // repaint.
    bool ruleTextMoved = false;
    if (!sameRules) {
      std::unique_ptr<const RuleTextLayer> ruleText = ruleTextLayerOf(matched);
      ruleTextMoved = !sameRuleText(inst.ruleText.get(), ruleText.get());
      if (ruleTextMoved) inst.ruleText = std::move(ruleText);
    }
    TextOptions options =
        inst.ruleText ? inst.ruleText->options : TextOptions{};
    options.overlay(node.textData->options);
    const bool optionsMoved = !(options == inst.textOptions);
    if (optionsMoved) inst.textOptions = std::move(options);
    if (ruleTextMoved && !first) {
      inst.markPaintDirtyUp();
      contentDirty = true;
    }
    // A named run resolves through the sheets in force, so a sheet that
    // changed what a name means under the leaf is a new paragraph as a new
    // face is.
    sigil::weave::TypeSheet runStyles;
    std::vector<NamedParagraphStyle> paragraphBlockStyles;
    namedStylesOf(inst, *sheets, runStyles, paragraphBlockStyles);
    const bool namesMoved =
        !(runStyles == inst.runStyles) ||
        !(paragraphBlockStyles == inst.paragraphBlockStyles);
    inst.runStyles = std::move(runStyles);
    inst.paragraphBlockStyles = std::move(paragraphBlockStyles);
    inst.sheetsInForce = !sheets->empty();
    const bool remakes =
        block.writingMode != inst.textBlock.writingMode ||
        block.lineBreakLocale != inst.textBlock.lineBreakLocale || namesMoved ||
        optionsMoved;
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
      if (!(block == inst.textBlock) || indentPercentMoved) {
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
                   *sheets, inkPaint, inst.inkTarget);
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
    // A span was laid over the font then in force; the ink moved, so it
    // is laid over the font now in force.
    for (size_t i = 0; i < count; ++i)
      state.restyleStyles[i] = styleOfSpan(base, text.spanRestyles[i], inst);
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
