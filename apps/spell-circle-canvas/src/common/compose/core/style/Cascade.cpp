/** @file
 * The cascade pass: the font, the ink, the paragraph setting, the custom
 * properties and the sampling in force at every node, resolved from the
 * root's down — the role's defaults, the matched rules and the node's own
 * declarations folded over what it inherits, the wide keywords over that —
 * and what changed applied to the instance.
 */

#include <include/core/SkFontStyle.h>
#include <include/core/SkTypeface.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilweave/advanced/Skia.h>
#include <sigilweave/style/Type.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "CascadePass.h"
#include "FaceChoice.h"
#include "LonghandFold.h"
#include "SelectorMatch.h"
#include "SheetBody.h"
#include "paint/FillLowering.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

bool sameVars(const std::shared_ptr<const VarTable>& a,
              const std::shared_ptr<const VarTable>& b) {
  if (a == b) return true;
  if (!a || !b) return (a ? a->empty() : true) && (b ? b->empty() : true);
  return *a == *b;
}

material::Color lerpColour(const material::Color& from,
                           const material::Color& to, float t) {
  return material::mixToward(from, to, t, from.a + (to.a - from.a) * t);
}

/** THE GLYPH SHADOWS NAMING A CUSTOM PROPERTY in @p font, read against
 *  @p vars: each takes the property's colour here, where it is stated,
 *  and is inherited as that colour, as CSS computes a `var()`. A property
 *  nobody set, or one holding no colour, leaves its shadow out. */
void resolveShadowProperties(sigil::weave::Type& font, const VarTable* vars) {
  if (!font.underlays) return;
  std::vector<sigil::weave::PaintLayer>& passes = *font.underlays;
  for (size_t at = 0; at < passes.size();) {
    const std::optional<VarRef> property =
        detail::shadowPropertyOf(passes[at].material);
    if (!property) {
      ++at;
      continue;
    }
    const VarValue* value = vars ? vars->find(*property) : nullptr;
    const material::Color* colour =
        value ? std::get_if<material::Color>(value) : nullptr;
    if (!colour || colour->a <= 0) {
      if (!colour) warnNoSuchVar(*property, true);
      passes.erase(passes.begin() + (std::ptrdiff_t)at);
      continue;
    }
    passes[at].paint.setColor4f(material::skia::toSkColor(*colour), nullptr);
    passes[at].material.reset();
    ++at;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// The pass

void Composer::Impl::runCascade() {
  if (!root) return;
  inkAnimating = false;
  if (rootLineHeight <= 0.0f) rootLineHeight = lineHeightAt(rootFont);
  remMoved = false;
  hasNames.begin(++cascadePass);
  indexRoot(*root);
  // The composer's own sheet stands first in the chain, scoped at the
  // root, so the root is its `:root` and every sheet the tree applies
  // stands after it.
  SheetChain chain;
  if (rootSheet) {
    chain.push_back(ScopedSheet{&*rootSheet, root.get()});
    if (const SheetBody* body = SheetAccess::body(*rootSheet);
        body != nullptr && body->usesHas) {
      hasNames.add(*body);
      summariseForHas(*root, hasNames);
    }
  }
  const InkInForce noInkPaint;
  resolveCascade(*root, rootFont, rootLineHeight, nullptr, rootBlock,
                 rootSampling, chain, noInkPaint, rootFont.color);
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
  const bool isRoot = &inst == root.get();
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
  // THE RULES IN FORCE HERE, and what they leave on the instance.
  SheetChain extended;
  const RulesInForce rules =
      resolveRulesInForce(*this, inst, parentSheets, extended, first);
  const SheetChain* const sheets = rules.sheets;
  const std::vector<MatchedRule>& matched = rules.matched;
  const bool layerMoved = rules.layerMoved;
  const bool fillMaterialMoved = rules.fillMaterialMoved;
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
      if (cascade->role->ink) inkVar = cascade->role->ink;
    }
    // A class is unknown only where no rule of the sheets in force names
    // it, whether that rule matched this node or not, and no operator in
    // the tree picks its nodes by it.
    if (cascade != nullptr)
      for (const std::string& name : cascade->classes)
        if (!namesStyleClass(*sheets, name) && !operatorClasses.contains(name))
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
        const detail::ElementNode& stated = *rule.node();
        const std::optional<material::Paint> rulePaint =
            stated.cascadeData ? stated.cascadeData->inkPaint : std::nullopt;
        inkPaint = {rulePaint, box,
                    stated.cascadeData ? stated.cascadeData->inkSurfaced
                                       : std::nullopt};
        inkPaintOrigin = rulePaint.has_value();
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
        inkPaint = {cascade->inkPaint, cascade->inkBox, cascade->inkSurfaced};
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
    // `1.5_em` on a heading means. A rem in it is the root element's size,
    // except on the root itself, whose rem is the size it inherits.
    const float rem = isRoot ? fontSizePx(rootFont) : remPx;
    if (fontFromInitial) {
      sigil::weave::Type initial = sigil::weave::initialType();
      initial.color = parentFont.color;
      font = sigil::weave::overlay(initial, ownFont, rem, parentLineHeight);
    } else if (!ownFont.empty()) {
      font = sigil::weave::overlay(parentFont, ownFont, rem, parentLineHeight);
    }
    // A FAMILY NAMED, AN ITALIC TURNED ON OR OFF, OR A WEIGHT MOVED UNDER A
    // FAMILY NAMED ABOVE, CHOOSES THE FACE — through the composer's font
    // context, which is why it is done here and not by the verb — and so
    // does a face stated under an italic. The face is the family's at the
    // weight and style in force, as CSS matches one per element.
    // A STYLE NAMED BESIDE THE FAMILY is the face the font names so,
    // written into the font as a held face is, its own weight and style
    // standing. A weight or a style stated after it, or in a stronger
    // layer, overrides that trait alone, and the family chooses its face
    // at the traits so overridden and the named face's others.
    bool chosenByName = false;
    if (const std::string* styleName = longhands.styleNamed())
      if (sk_sp<SkTypeface> named =
              namedStyleFace(fonts, *longhands.familyNamed(), *styleName)) {
        if (!longhands.styleOverridesNamedStyle()) {
          longhands.faceChosenByStyleName(named->fontStyle().slant() !=
                                          SkFontStyle::kUpright_Slant);
          if (font.slant && *font.slant != 0.0f) font.slant = 0.0f;
        }
        if (!longhands.weightOverridesNamedStyle() && font.weight)
          font.weight = 0.0f;
        font.face = sigil::weave::fromSk(std::move(named));
        chosenByName = !longhands.styleOverridesNamedStyle() &&
                       !longhands.weightOverridesNamedStyle();
      }
    const bool italic = longhands.italic();
    const std::string* family = longhands.familyInForce();
    if (!chosenByName &&
        (longhands.familyNamed() != nullptr || italic != parentItalic ||
         (longhands.faceStated() && italic) ||
         (family != nullptr && font.weight != parentFont.weight)))
      chooseFace(fonts, font, family, italic);
    if (ownFont.color) statesOwnInk = true;
    if (inkVar) {
      const VarValue* value = vars ? vars->find(*inkVar) : nullptr;
      const material::Color* colour =
          value ? std::get_if<material::Color>(value) : nullptr;
      const Fill* paint = value ? std::get_if<Fill>(value) : nullptr;
      if (colour) {
        font.color = *colour;
        statesOwnInk = true;
      } else if (paint && paint->kind == Fill::Kind::Paint) {
        // A PAINT HELD BY A PROPERTY is the ink as a whole paint, exactly
        // as `ink(paint)` states one: it inherits from here, stretched
        // over each thing it is laid on, and the colour stands beneath.
        inkPaint = {detail::paintOf(*paint), PaintBox::Element, std::nullopt};
        inkPaintOrigin = true;
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
  const std::optional<motion::Tween<float>>& inkTransition =
      inst.transitionInForce();
  retargetInk(inst, inkTarget, inkTransition, first);
  // …and the colour this node shows is read here, so every node under it
  // inherits it and repaints with it: the lane's colour while it runs,
  // else its target. A node with NO transition of its own keeps the
  // colour that arrived from above, so it follows an ancestor's ramp as
  // that ramp happens.
  if (const auto& anim = inst.anims[Instance::kInkLerp];
      anim && anim->started && anim->isRunning() && inst.inkTarget) {
    font.color = lerpColour(inst.inkFrom, *inst.inkTarget, anim->value());
    inkAnimating = true;
  } else if (inkTransition && inst.inkTarget) {
    font.color = *inst.inkTarget;
  }

  // THE ROOT ELEMENT'S SIZE IS WHAT A REM MEASURES from here down, the
  // root's own box included, so it is taken the moment the root's font is
  // final and before any length of the root is resolved.
  if (isRoot) {
    const float rootSize = fontSizePx(font);
    remMoved = rootSize != remPx;
    remPx = rootSize;
  }
  // AN INK THAT STATES A SURFACE, lit or unlit, on a page painted as a
  // surface map paints as that map of
  // it; the paint held for one map is dropped with every other when the
  // map changes. Every other draw paints the ink's colours flat. The paint
  // made last time stands while the ink's material has not moved.
  if (inkPaint.surfaced) {
    const bool sameMaterial = inst.inkPaint.surfaced == inkPaint.surfaced;
    if (!sameMaterial) inst.surfaceMapInkInputs.reset();
    if (!first && sameMaterial && inst.inkPaint.paint) {
      inkPaint.paint = inst.inkPaint.paint;
    } else if (surfaceMap && inkPaint.surfaced->surface()) {
      if (!inst.surfaceMapInkInputs)
        inst.surfaceMapInkInputs.emplace(*inkPaint.surfaced);
      inkPaint.paint = inst.surfaceMapInkInputs->asMap(*surfaceMap);
    } else {
      inkPaint.paint = material::skia::paint(*inkPaint.surfaced);
    }
  } else {
    inst.surfaceMapInkInputs.reset();
  }
  resolveShadowProperties(font, vars.get());
  const bool shapeChanged = first || !sameFontButColour(font, inst.font);
  const bool inkChanged =
      first || !(font.color == inst.font.color) || !(inkPaint == inst.inkPaint);
  const bool varsChanged = first || !sameVars(vars, inst.vars);
  const bool samplingChanged = first || !(sampling == inst.sampling);
  inst.font = font;
  inst.inkPaint = inkPaint;
  // Inherited ink spanning another box, or sampling root coordinates,
  // depends on this node's root placement. Retained pixels must be
  // invalidated when the node or an ancestor moves. Reconcile resets the
  // dependency from the description; the cascade adds the effective ink
  // dependency after inheritance and overrides resolve.
  if ((inkPaint.paint && spreadsAcrossTree(inkPaint.box)) ||
      inst.inkUsesWorldSpace())
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
    inst.faceLineHeight = lineHeightAt(font);
    inst.zeroAdvance = zeroAdvanceAt(font);
  }
  // A LINE HEIGHT IN A RELATIVE UNIT becomes an absolute pitch here,
  // against this node's own font, and is inherited as the pitch; an `lh`
  // in it is the parent's line height, as CSS reads one there.
  const float lineHeightBefore = inst.lineHeight;
  inst.lineHeight = parentLineHeight;
  if (longhands.lineHeight())
    resolveLineHeight(inst, *longhands.lineHeight(), block);
  // THE LINE HEIGHT IN FORCE, which every other `lh` on this node reads.
  inst.lineHeight = block.leading.value_or(sigil::weave::Leading{})
                        .pitchFor(inst.faceLineHeight, fontSizePx(font));
  const bool lineHeightMoved = !first && inst.lineHeight != lineHeightBefore;
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

  if (node.kind == Kind::Text && node.textData)
    resolveTextLeafCascade(*this, inst, font, block, statesOwnInk, rules, first,
                           indentPercentMoved);
  // A length measured in the font, or read from a property, is rewritten
  // into the flex style when what it measures against moved.
  if ((shapeChanged || varsChanged || remMoved || lineHeightMoved) &&
      inst.relativeLengths) {
    applyLayoutProps(inst);
    needsLayout = true;
  }
  // An origin is a length too, read at paint instead of by the layout: one
  // in the font or on a property moves the node's matrix when either
  // moves, under recordings that hold the matrix it had. One on the canvas
  // is moved by a resize alone, which the layout pass answers for.
  if (!first && (shapeChanged || varsChanged || remMoved || lineHeightMoved) &&
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

}  // namespace sigil::compose
