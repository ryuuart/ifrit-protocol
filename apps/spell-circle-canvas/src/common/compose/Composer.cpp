// The Composer: keyed reconciliation into a retained instance tree,
// Yoga layout with SigilWeave-measured text leaves, stacking-order paint,
// automatic SkPicture caching of provably-static subtrees, and
// Choreograph-backed transitions/bindings stepped by the host's Ticker.

#include "ComposeInternal.h"

#include <sigilimage/ImageAsset.h>

#include <sigilweave/Flow.h>
#include <sigilweave/FontContext.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>

#include <yoga/Yoga.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace sigil::compose {

using namespace detail;

namespace {

YGAlign toYogaAlign(Align a) {
  switch (a) {
  case Align::Auto: return YGAlignAuto;
  case Align::Start: return YGAlignFlexStart;
  case Align::Center: return YGAlignCenter;
  case Align::End: return YGAlignFlexEnd;
  case Align::Stretch: return YGAlignStretch;
  case Align::Baseline: return YGAlignBaseline;
  }
  return YGAlignAuto;
}

YGJustify toYogaJustify(Justify j) {
  switch (j) {
  case Justify::Start: return YGJustifyFlexStart;
  case Justify::Center: return YGJustifyCenter;
  case Justify::End: return YGJustifyFlexEnd;
  case Justify::SpaceBetween: return YGJustifySpaceBetween;
  case Justify::SpaceAround: return YGJustifySpaceAround;
  case Justify::SpaceEvenly: return YGJustifySpaceEvenly;
  }
  return YGJustifyFlexStart;
}

void applyDim(YGNodeRef node, const Dim &d,
              void (*setPx)(YGNodeRef, float),
              void (*setPct)(YGNodeRef, float)) {
  switch (d.unit) {
  case Dim::Unit::Px: setPx(node, d.value); break;
  case Dim::Unit::Pct: setPct(node, d.value); break;
  case Dim::Unit::Auto: break;
  }
}

} // namespace

// ---------------------------------------------------------------------------

namespace detail {

/** One float property that can transition: the Choreograph output is
 *  the source of truth while a motion is connected. */
struct AnimatedFloat {
  choreograph::Output<float> value{0.0f};
  bool started = false;
};

struct Instance {
  Composer::Impl *owner = nullptr;
  Instance *parent = nullptr;
  std::shared_ptr<ElementNode> desc; // resolved (post-memo) description
  std::shared_ptr<ElementNode> memoShell; // the memo element, if any
  YGNodeRef yoga = nullptr;
  std::vector<std::unique_ptr<Instance>> children;
  std::vector<size_t> paintOrder; // child indices sorted by zIndex

  // Text state
  std::optional<sigil::weave::Paragraph> paragraph;
  sigil::weave::ParagraphLayout textLayout;
  std::vector<sigil::weave::LineMetrics> lines;
  float measuredForWidth = -1.0f;
  YGSize measuredSize{0, 0};
  uint32_t contentRev = 0;     // bumped on text/exclusion change
  uint32_t measuredRev = ~0u;  // rev the cached measurement belongs to

  // Transition state, keyed by property slot
  enum Slot : int { kOpacity, kTx, kTy, kRotate, kScale, kFillLerp, kSlots };
  std::unique_ptr<AnimatedFloat> anims[kSlots];
  Fill fillFrom, fillTo; // endpoints for kFillLerp

  // Derive-phase state
  std::vector<SkRect> exclusionsLocal;    // flowAround rects, text-local
  SkPath connectorPath;                    // routed path, connector-local
  SkRect connectorFrom = SkRect::MakeEmpty(), connectorTo = SkRect::MakeEmpty();

  // Caching
  sk_sp<SkPicture> picture;
  sk_sp<SkImage> textureImage;
  float textureScale = 1.0f;
  bool paintDirty = true;
  bool subtreeVolatile = false;

  ~Instance();
  float resolveFloat(Instance::Slot slot, const PropValue<float> &v) const;

  /** A change here stales every ancestor's recording too. */
  void markPaintDirtyUp() {
    for (Instance *i = this; i; i = i->parent) {
      if (i->paintDirty && i != this)
        break; // ancestors already invalidated
      i->paintDirty = true;
      i->picture.reset();
      i->textureImage.reset();
    }
  }
};

} // namespace detail

// ---------------------------------------------------------------------------

struct Composer::Impl {
  tick::Ticker &ticker;
  sigil::weave::FontContext &fonts;
  const tick::FrameClock *clock = nullptr;

  SkSize size = SkSize::MakeEmpty();
  std::unique_ptr<Instance> root;
  YGConfigRef yogaConfig = nullptr;
  bool needsLayout = true;
  bool contentDirty = true;
  std::unordered_map<std::string, Instance *> byKey;
  bool volatileDirty = true;   // recompute needed (render or animation)
  bool tickerWasActive = false;
  bool hasDerived = false;     // any flowAround/connector in the tree
  bool hasCustomLayout = false;

  mutable Stats stats;

  Impl(tick::Ticker &t, sigil::weave::FontContext &f) : ticker(t), fonts(f) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return clock ? clock->elapsed() : 0.0; }

  // ---- reconcile ----
  std::unique_ptr<Instance> mount(const std::shared_ptr<ElementNode> &node,
                                  Instance *parent);
  void patch(Instance &inst, std::shared_ptr<ElementNode> node);
  void patchChildren(Instance &inst,
                     const std::vector<Element> &newChildren);
  void applyLayoutProps(Instance &inst);
  void applyTransitions(Instance &inst, const ElementNode &prev,
                        const ElementNode &next);
  std::shared_ptr<ElementNode>
  resolveMemo(Instance *existing, const std::shared_ptr<ElementNode> &node,
              bool &described);

  // ---- volatility & caching ----
  bool computeVolatile(Instance &inst);
  bool applyCustomLayouts(Instance &inst);

  // ---- text ----
  void layoutText(Instance &inst, float constraint);

  // ---- paint ----
  void paint(Instance &inst, SkCanvas &canvas);
  void paintContent(Instance &inst, SkCanvas &canvas);
  void rebuildKeyIndex();
  void indexKeys(Instance &inst);
  SkRect instanceRect(const Instance &inst) const;
  SkRect absoluteRect(const Instance &inst) const;
  bool resolveDerived(Instance &inst);
};

// ---------------------------------------------------------------------------
// Instance helpers

detail::Instance::~Instance() {
  if (yoga)
    YGNodeRemoveAllChildren(yoga); // detach before children free theirs
  children.clear();
  if (yoga)
    YGNodeFree(yoga);
  // AnimatedFloat outputs die here; Choreograph disconnects their
  // motions automatically — unmount cancels transitions by construction.
}

float detail::Instance::resolveFloat(Slot slot,
                                     const PropValue<float> &v) const {
  if (const auto *binding =
          std::get_if<const choreograph::Output<float> *>(&v))
    return (*binding)->value();
  if (anims[slot] && anims[slot]->started)
    return anims[slot]->value.value();
  if (const float *plain = std::get_if<float>(&v))
    return *plain;
  return std::get<Transitioned<float>>(v).value;
}

// ---------------------------------------------------------------------------
// Text measurement (the spike, productized)

namespace {

YGSize measureTextNode(YGNodeConstRef node, float width, YGMeasureMode widthMode,
                       float, YGMeasureMode) {
  auto *inst = static_cast<Instance *>(YGNodeGetContext(
      const_cast<YGNodeRef>(node)));
  const float constraint =
      widthMode == YGMeasureModeUndefined ? 1.0e6f : width;
  inst->owner->layoutText(*inst, constraint);
  return inst->measuredSize;
}

float baselineOfTextNode(YGNodeConstRef node, float, float) {
  auto *inst = static_cast<Instance *>(YGNodeGetContext(
      const_cast<YGNodeRef>(node)));
  if (inst->lines.empty())
    return 0.0f;
  const sigil::weave::LineMetrics &first = inst->lines.front();
  return first.baseline - first.rect().top();
}

} // namespace

void Composer::Impl::layoutText(Instance &inst, float constraint) {
  if (constraint == inst.measuredForWidth &&
      inst.measuredRev == inst.contentRev)
    return; // layout is already valid for this content and width
  if (!inst.exclusionsLocal.empty()) {
    sigil::weave::ExclusionFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
    for (const SkRect &exclusion : inst.exclusionsLocal)
      flow.shapes().push_back(sigil::weave::ExclusionFlow::Shape::fromRectangle(
          exclusion, inst.desc->flowAroundMargin));
    inst.textLayout = sigil::weave::layoutParagraph(fonts, *inst.paragraph,
                                                flow, {});
  } else {
    sigil::weave::BlockFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
    inst.textLayout = sigil::weave::layoutParagraph(fonts, *inst.paragraph,
                                                flow, {});
  }
  inst.lines = inst.textLayout.lineMetrics(*inst.paragraph);
  inst.measuredForWidth = constraint;
  SkRect bounds = SkRect::MakeEmpty();
  for (const sigil::weave::LineMetrics &line : inst.lines)
    bounds.join(line.rect());
  inst.measuredSize = {std::ceil(bounds.width()), std::ceil(bounds.height())};
  inst.measuredRev = inst.contentRev;
}

// ---------------------------------------------------------------------------
// Reconcile

std::shared_ptr<ElementNode>
Composer::Impl::resolveMemo(Instance *existing,
                            const std::shared_ptr<ElementNode> &node,
                            bool &described) {
  if (!node->isMemo) {
    described = true;
    return node;
  }
  if (existing && existing->memoShell &&
      existing->memoShell->memoEqual(existing->memoShell->memoProps,
                                     node->memoProps)) {
    stats.memoHits++;
    described = false;
    return existing->desc; // reuse the previously described payload
  }
  described = true;
  Element produced = node->memoInvoke(node->memoProps);
  return produced.node();
}

std::unique_ptr<Instance>
Composer::Impl::mount(const std::shared_ptr<ElementNode> &node,
                      Instance *parent) {
  auto inst = std::make_unique<Instance>();
  inst->owner = this;
  inst->parent = parent;
  inst->yoga = YGNodeNewWithConfig(yogaConfig);
  YGNodeSetContext(inst->yoga, inst.get());
  patch(*inst, node);
  return inst;
}

void Composer::Impl::patch(Instance &inst, std::shared_ptr<ElementNode> node) {
  stats.describedNodes++;
  bool described = true;
  std::shared_ptr<ElementNode> resolved =
      resolveMemo(inst.desc ? &inst : nullptr, node, described);
  if (node->isMemo)
    inst.memoShell = node;
  else
    inst.memoShell = nullptr;

  if (resolved == inst.desc)
    return; // payload identity: untouched subtree (memo hit)

  std::shared_ptr<ElementNode> prev = std::move(inst.desc);
  inst.desc = resolved;
  stats.patchedNodes++;
  inst.markPaintDirtyUp();
  contentDirty = true;

  // Kind change → full remount of content state.
  const bool kindChanged = prev && prev->kind != resolved->kind;
  if (kindChanged) {
    inst.paragraph.reset();
    inst.lines.clear();
    YGNodeSetMeasureFunc(inst.yoga, nullptr);
  }

  applyLayoutProps(inst);

  if (!resolved->flowAroundKeys.empty() || !resolved->connectFrom.empty())
    hasDerived = true;
  if (resolved->placeFn)
    hasCustomLayout = true;

  if (resolved->kind == Kind::Text) {
    const bool textChanged = !prev || kindChanged ||
                             prev->textUtf8 != resolved->textUtf8 ||
                             !(prev->textStyle == resolved->textStyle);
    if (textChanged) {
      inst.contentRev++;
      inst.paragraph.emplace();
      inst.paragraph->appendText(resolved->textUtf8, resolved->textStyle);
      YGNodeSetMeasureFunc(inst.yoga, measureTextNode);
      YGNodeSetBaselineFunc(inst.yoga, baselineOfTextNode);
      YGNodeSetNodeType(inst.yoga, YGNodeTypeText);
      YGNodeMarkDirty(inst.yoga);
      needsLayout = true;
    }
  }

  if (prev)
    applyTransitions(inst, *prev, *resolved);

  // Slot content is owned by renderSlot(), not the description.
  if (resolved->kind != Kind::Slot)
    patchChildren(inst, resolved->children);

  // Paint order: stable sort by zIndex.
  inst.paintOrder.resize(inst.children.size());
  std::iota(inst.paintOrder.begin(), inst.paintOrder.end(), size_t{0});
  std::stable_sort(inst.paintOrder.begin(), inst.paintOrder.end(),
                   [&](size_t a, size_t b) {
                     return inst.children[a]->desc->paint.zIndex <
                            inst.children[b]->desc->paint.zIndex;
                   });
}

void Composer::Impl::patchChildren(Instance &inst,
                                   const std::vector<Element> &newChildren) {
  // Match by key when present, else by position among unkeyed children.
  std::unordered_map<std::string, std::unique_ptr<Instance>> keyed;
  std::vector<std::unique_ptr<Instance>> unkeyed;
  for (auto &child : inst.children) {
    if (child) {
      YGNodeRemoveChild(inst.yoga, child->yoga);
      const std::shared_ptr<ElementNode> &shell =
          child->memoShell ? child->memoShell : child->desc;
      if (!shell->key.empty())
        keyed.emplace(shell->key, std::move(child));
      else
        unkeyed.push_back(std::move(child));
    }
  }
  inst.children.clear();

  size_t unkeyedCursor = 0;
  for (const Element &childElement : newChildren) {
    const std::shared_ptr<ElementNode> &node = childElement.node();
    std::unique_ptr<Instance> match;
    if (!node->key.empty()) {
      if (auto it = keyed.find(node->key); it != keyed.end()) {
        match = std::move(it->second);
        keyed.erase(it);
      }
    } else if (unkeyedCursor < unkeyed.size()) {
      match = std::move(unkeyed[unkeyedCursor++]);
    }

    if (match) {
      match->parent = &inst;
      patch(*match, node);
      inst.children.push_back(std::move(match));
    } else {
      inst.children.push_back(mount(node, &inst));
      needsLayout = true;
    }
    Instance &placed = *inst.children.back();
    // Stack children overlap: absolute unless they positioned themselves.
    if (inst.desc->kind == Kind::Stack)
      YGNodeStyleSetPositionType(placed.yoga, YGPositionTypeAbsolute);
    YGNodeInsertChild(inst.yoga, placed.yoga,
                      YGNodeGetChildCount(inst.yoga));
  }
  // Unmatched old children unmount here (destructors cancel motions).
}

void Composer::Impl::applyLayoutProps(Instance &inst) {
  const LayoutProps &l = inst.desc->layout;
  YGNodeRef n = inst.yoga;
  const bool isStack = inst.desc->kind == Kind::Stack;

  YGNodeStyleSetFlexDirection(n, l.row ? YGFlexDirectionRow
                                       : YGFlexDirectionColumn);
  YGNodeStyleSetGap(n, YGGutterAll, l.gap);
  YGNodeStyleSetPadding(n, YGEdgeLeft, l.padding.left);
  YGNodeStyleSetPadding(n, YGEdgeTop, l.padding.top);
  YGNodeStyleSetPadding(n, YGEdgeRight, l.padding.right);
  YGNodeStyleSetPadding(n, YGEdgeBottom, l.padding.bottom);
  YGNodeStyleSetMargin(n, YGEdgeLeft, l.margin.left);
  YGNodeStyleSetMargin(n, YGEdgeTop, l.margin.top);
  YGNodeStyleSetMargin(n, YGEdgeRight, l.margin.right);
  YGNodeStyleSetMargin(n, YGEdgeBottom, l.margin.bottom);

  applyDim(n, l.width, &YGNodeStyleSetWidth, &YGNodeStyleSetWidthPercent);
  applyDim(n, l.height, &YGNodeStyleSetHeight, &YGNodeStyleSetHeightPercent);
  applyDim(n, l.minWidth, &YGNodeStyleSetMinWidth,
           &YGNodeStyleSetMinWidthPercent);
  applyDim(n, l.maxWidth, &YGNodeStyleSetMaxWidth,
           &YGNodeStyleSetMaxWidthPercent);
  applyDim(n, l.minHeight, &YGNodeStyleSetMinHeight,
           &YGNodeStyleSetMinHeightPercent);
  applyDim(n, l.maxHeight, &YGNodeStyleSetMaxHeight,
           &YGNodeStyleSetMaxHeightPercent);
  if (l.aspect > 0)
    YGNodeStyleSetAspectRatio(n, l.aspect);
  YGNodeStyleSetFlexGrow(n, l.grow);
  YGNodeStyleSetFlexShrink(n, l.shrink);
  applyDim(n, l.basis, &YGNodeStyleSetFlexBasis,
           &YGNodeStyleSetFlexBasisPercent);
  YGNodeStyleSetAlignItems(n, toYogaAlign(l.alignItems));
  // Measured text must not stretch on the cross axis (the spike's API
  // lesson): demote a resolved Stretch to Start for text leaves, but let
  // explicit alignment — own or inherited — through untouched.
  Align self = l.alignSelf;
  if (inst.desc->kind == Kind::Text) {
    const Align resolved =
        self != Align::Auto
            ? self
            : (inst.parent && inst.parent->desc
                   ? inst.parent->desc->layout.alignItems
                   : Align::Stretch);
    if (resolved == Align::Stretch)
      self = Align::Start;
  }
  YGNodeStyleSetAlignSelf(n, toYogaAlign(self));
  YGNodeStyleSetJustifyContent(n, toYogaJustify(l.justify));

  (void)isStack;
  YGNodeStyleSetPositionType(n, l.absolute ? YGPositionTypeAbsolute
                                           : YGPositionTypeRelative);
  if (l.hasInsets) {
    YGNodeStyleSetPosition(n, YGEdgeLeft, l.insets.left);
    YGNodeStyleSetPosition(n, YGEdgeTop, l.insets.top);
    YGNodeStyleSetPosition(n, YGEdgeRight, l.insets.right);
    YGNodeStyleSetPosition(n, YGEdgeBottom, l.insets.bottom);
  }
}

// ---------------------------------------------------------------------------
// Transitions

namespace {

/** Starts (or retargets) a ramp on `slot` when the plain target changed.
 *  Returns true if a motion is running. */
bool transitionFloat(Composer::Impl &impl, Instance &inst,
                     Instance::Slot slot, const PropValue<float> &prevValue,
                     const PropValue<float> &nextValue,
                     const std::optional<Transition> &nodeDefault) {
  ResolvedProp<float> prev = resolveProp(prevValue, nodeDefault);
  ResolvedProp<float> next = resolveProp(nextValue, nodeDefault);
  if (next.binding || !next.transition)
    return false; // bound, or plain snap
  if (prev.binding)
    return false; // binding → constant: snap (no meaningful "from")

  auto &anim = inst.anims[slot];
  const float current = anim && anim->started ? anim->value.value()
                                              : prev.target;
  if (current == next.target)
    return anim && anim->value.isConnected();

  if (!anim)
    anim = std::make_unique<AnimatedFloat>();
  anim->value = current; // seed the retarget start point
  anim->started = true;
  impl.ticker.timeline()
      .apply(&anim->value)
      .then<choreograph::RampTo>(
          next.target,
          std::chrono::duration<float>(next.transition->duration).count(),
          next.transition->ease);
  return true;
}

} // namespace

void Composer::Impl::applyTransitions(Instance &inst, const ElementNode &prev,
                                      const ElementNode &next) {
  const auto &nd = next.nodeTransition;
  transitionFloat(*this, inst, Instance::kOpacity, prev.paint.opacity,
                  next.paint.opacity, nd);
  transitionFloat(*this, inst, Instance::kTx, prev.paint.translateX,
                  next.paint.translateX, nd);
  transitionFloat(*this, inst, Instance::kTy, prev.paint.translateY,
                  next.paint.translateY, nd);
  transitionFloat(*this, inst, Instance::kRotate, prev.paint.rotate,
                  next.paint.rotate, nd);
  transitionFloat(*this, inst, Instance::kScale, prev.paint.scale,
                  next.paint.scale, nd);

  // Fill: color→color lerp via a progress output.
  if (prev.paint.fill && next.paint.fill) {
    ResolvedProp<Fill> prevFill = resolveProp(*prev.paint.fill, nd);
    ResolvedProp<Fill> nextFill = resolveProp(*next.paint.fill, nd);
    if (!prevFill.binding && !nextFill.binding && nextFill.transition &&
        prevFill.target.kind == Fill::Kind::Color &&
        nextFill.target.kind == Fill::Kind::Color &&
        !(prevFill.target == nextFill.target)) {
      // Current visual color as the new "from" (retarget-from-current).
      Fill from = prevFill.target;
      auto &anim = inst.anims[Instance::kFillLerp];
      if (anim && anim->started && anim->value.isConnected()) {
        const float t = anim->value.value();
        for (int i = 0; i < 4; ++i)
          from.colorValue.vec()[i] =
              inst.fillFrom.colorValue.vec()[i] +
              (inst.fillTo.colorValue.vec()[i] -
               inst.fillFrom.colorValue.vec()[i]) * t;
      }
      inst.fillFrom = from;
      inst.fillTo = nextFill.target;
      if (!anim)
        anim = std::make_unique<AnimatedFloat>();
      anim->value = 0.0f;
      anim->started = true;
      ticker.timeline()
          .apply(&anim->value)
          .then<choreograph::RampTo>(
              1.0f,
              std::chrono::duration<float>(nextFill.transition->duration)
                  .count(),
              nextFill.transition->ease);
    }
  }
}

bool Composer::Impl::applyCustomLayouts(Instance &inst) {
  bool applied = false;
  if (inst.desc->placeFn && !inst.children.empty()) {
    LayoutInput input;
    input.container = {YGNodeLayoutGetWidth(inst.yoga),
                       YGNodeLayoutGetHeight(inst.yoga)};
    for (const auto &child : inst.children)
      input.childSizes.push_back({YGNodeLayoutGetWidth(child->yoga),
                                  YGNodeLayoutGetHeight(child->yoga)});
    std::vector<SkRect> rects = inst.desc->placeFn(input);
    const size_t count = std::min(rects.size(), inst.children.size());
    for (size_t i = 0; i < count; ++i) {
      YGNodeRef child = inst.children[i]->yoga;
      YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
      YGNodeStyleSetPosition(child, YGEdgeLeft, rects[i].left());
      YGNodeStyleSetPosition(child, YGEdgeTop, rects[i].top());
      YGNodeStyleSetWidth(child, rects[i].width());
      YGNodeStyleSetHeight(child, rects[i].height());
    }
    applied = true;
  }
  for (const auto &child : inst.children)
    applied |= applyCustomLayouts(*child);
  return applied;
}

SkRect Composer::Impl::absoluteRect(const Instance &inst) const {
  SkRect rect = instanceRect(inst);
  for (Instance *p = inst.parent; p; p = p->parent)
    rect.offset(YGNodeLayoutGetLeft(p->yoga), YGNodeLayoutGetTop(p->yoga));
  return rect;
}

/** The derive pass: content whose input is resolved geometry. Returns
 *  true when a text exclusion changed (second layout pass needed). */
bool Composer::Impl::resolveDerived(Instance &inst) {
  bool relayout = false;
  const ElementNode &node = *inst.desc;

  if (!node.flowAroundKeys.empty() && inst.paragraph) {
    std::vector<SkRect> exclusions;
    const SkRect own = absoluteRect(inst);
    for (const std::string &key : node.flowAroundKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end())
        continue;
      // Cycle guard: the target must not be this node or a descendant.
      bool cyclic = false;
      for (Instance *p = it->second; p; p = p->parent)
        if (p == &inst) { cyclic = true; break; }
      if (cyclic)
        continue;
      SkRect target = absoluteRect(*it->second);
      target.offset(-own.left(), -own.top());
      exclusions.push_back(target);
    }
    if (exclusions != inst.exclusionsLocal) {
      inst.exclusionsLocal = std::move(exclusions);
      inst.contentRev++;
      YGNodeMarkDirty(inst.yoga);
      inst.markPaintDirtyUp();
      relayout = true;
    }
  }

  if (!node.connectFrom.empty() && !node.connectTo.empty()) {
    auto fromIt = byKey.find(node.connectFrom);
    auto toIt = byKey.find(node.connectTo);
    if (fromIt != byKey.end() && toIt != byKey.end()) {
      SkRect own = absoluteRect(inst);
      SkRect from = absoluteRect(*fromIt->second);
      SkRect to = absoluteRect(*toIt->second);
      from.offset(-own.left(), -own.top());
      to.offset(-own.left(), -own.top());
      if (from != inst.connectorFrom || to != inst.connectorTo) {
        inst.connectorFrom = from;
        inst.connectorTo = to;
        if (node.router) {
          inst.connectorPath = node.router(from, to);
        } else {
          SkPathBuilder b;
          b.moveTo(from.centerX(), from.centerY());
          b.lineTo(to.centerX(), to.centerY());
          inst.connectorPath = b.detach();
        }
        inst.markPaintDirtyUp();
      }
    }
  }

  for (auto &child : inst.children)
    relayout |= resolveDerived(*child);
  return relayout;
}

// ---------------------------------------------------------------------------
// Volatility & caching

bool Composer::Impl::computeVolatile(Instance &inst) {
  const ElementNode &node = *inst.desc;
  bool own = false;

  auto boundOrRunning = [&](Instance::Slot slot,
                            const PropValue<float> &v) {
    if (std::holds_alternative<const choreograph::Output<float> *>(v))
      return true;
    return inst.anims[slot] && inst.anims[slot]->value.isConnected();
  };
  own |= boundOrRunning(Instance::kOpacity, node.paint.opacity);
  own |= boundOrRunning(Instance::kTx, node.paint.translateX);
  own |= boundOrRunning(Instance::kTy, node.paint.translateY);
  own |= boundOrRunning(Instance::kRotate, node.paint.rotate);
  own |= boundOrRunning(Instance::kScale, node.paint.scale);
  own |= inst.anims[Instance::kFillLerp] &&
         inst.anims[Instance::kFillLerp]->value.isConnected();
  if (node.paint.fill &&
      std::holds_alternative<const choreograph::Output<Fill> *>(
          *node.paint.fill))
    own = true;
  if (node.cacheMode == Cache::None)
    own = true;
  for (const Decoration &d : node.backgrounds)
    own |= d.animated();
  for (const Decoration &d : node.foregrounds)
    own |= d.animated();
  if (node.kind == Kind::Image && node.imageAsset &&
      node.imageAsset->animated())
    own = true;

  bool subtree = own;
  for (auto &child : inst.children)
    subtree |= computeVolatile(*child);
  if (subtree != inst.subtreeVolatile) {
    inst.subtreeVolatile = subtree;
    inst.paintDirty = true; // cacheability changed → re-record/drop
  }
  if (inst.subtreeVolatile) {
    inst.picture.reset();
    inst.textureImage.reset();
  }
  return subtree;
}

// ---------------------------------------------------------------------------
// Paint

SkRect Composer::Impl::instanceRect(const Instance &inst) const {
  return SkRect::MakeXYWH(YGNodeLayoutGetLeft(inst.yoga),
                          YGNodeLayoutGetTop(inst.yoga),
                          YGNodeLayoutGetWidth(inst.yoga),
                          YGNodeLayoutGetHeight(inst.yoga));
}

void Composer::Impl::paintContent(Instance &inst, SkCanvas &canvas) {
  const ElementNode &node = *inst.desc;
  const SkRect bounds = SkRect::MakeWH(YGNodeLayoutGetWidth(inst.yoga),
                                       YGNodeLayoutGetHeight(inst.yoga));
  const SkRRect rrect =
      SkRRect::MakeRectXY(bounds, node.corners.radius, node.corners.radius);

  // The node's shape: routed connector path, custom outline(), or the
  // corner-rounded box.
  const bool customShape = node.shapeFn && node.connectFrom.empty();
  SkPath outlinePath;
  if (!node.connectFrom.empty()) {
    outlinePath = inst.connectorPath; // derive phase routed it
  } else if (customShape) {
    outlinePath = node.shapeFn({bounds.width(), bounds.height()});
  } else {
    SkPathBuilder outlineBuilder;
    outlineBuilder.addRRect(rrect);
    outlinePath = outlineBuilder.detach();
  }

  if (node.clipContent) {
    if (customShape)
      canvas.clipPath(outlinePath, true);
    else
      canvas.clipRRect(rrect, true);
  }

  // The node's own layer effect wraps everything painted here, so it is
  // captured by picture recordings and BAKED by texture snapshots.
  const bool hasEffect =
      node.layerEffect && node.layerEffect->imageFilter();
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(node.layerEffect->imageFilter());
    canvas.saveLayer(nullptr, &effectPaint);
  }
  const PaintContext paintCtx{{bounds.width(), bounds.height()},
                              std::move(outlinePath), elapsed(), 1.0f,
                              ticker.active()};

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow/pattern layers first, then the surface.
  for (const Decoration &decoration : node.backgrounds)
    decoration.paint(canvas, paintCtx);

  // Fill (background)
  if (node.paint.fill) {
    Fill fill;
    if (const auto *binding =
            std::get_if<const choreograph::Output<Fill> *>(&*node.paint.fill))
      fill = (*binding)->value();
    else if (inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->value.isConnected()) {
      const float t = inst.anims[Instance::kFillLerp]->value.value();
      fill = inst.fillTo;
      for (int i = 0; i < 4; ++i)
        fill.colorValue.vec()[i] =
            inst.fillFrom.colorValue.vec()[i] +
            (inst.fillTo.colorValue.vec()[i] -
             inst.fillFrom.colorValue.vec()[i]) * t;
      fill.kind = Fill::Kind::Color;
    } else {
      ResolvedProp<Fill> resolved =
          resolveProp(*node.paint.fill, node.nodeTransition);
      fill = resolved.target;
    }

    if (fill.kind != Fill::Kind::None) {
      SkPaint paint;
      paint.setAntiAlias(true);
      if (fill.kind == Fill::Kind::Color)
        paint.setColor4f(fill.colorValue, nullptr);
      else
        paint.setShader(fill.shaderValue);
      if (customShape)
        canvas.drawPath(paintCtx.outline, paint);
      else
        canvas.drawRRect(rrect, paint);
    }
  }

  // Content
  switch (node.kind) {
  case Kind::Text:
    if (inst.paragraph) {
      // Yoga skips the measure callback when both dimensions are fully
      // determined (absolute + all four insets); lay out on demand at
      // the resolved width so such text still paints.
      if (inst.measuredRev != inst.contentRev)
        layoutText(inst, bounds.width());
      inst.textLayout.drawBatched(&canvas, *inst.paragraph);
    }
    break;
  case Kind::Image:
    if (node.imageAsset && !node.imageAsset->frames().empty()) {
      const auto &frame = node.imageAsset->frameAt(elapsed() * 1000.0);
      if (frame.image)
        canvas.drawImageRect(frame.image, bounds,
                             SkSamplingOptions(SkFilterMode::kLinear));
    }
    break;
  case Kind::Custom:
    if (node.program)
      node.program(canvas, paintCtx);
    break;
  case Kind::Box:
  case Kind::Stack:
  case Kind::Slot:
    break;
  }

  // Children in stacking order (each clean static child replays its own
  // nested picture — ancestor re-records don't repaint clean subtrees).
  for (size_t index : inst.paintOrder)
    paint(*inst.children[index], canvas);

  for (const Decoration &decoration : node.foregrounds)
    decoration.paint(canvas, paintCtx);

  if (hasEffect)
    canvas.restore();
}

void Composer::Impl::paint(Instance &inst, SkCanvas &canvas) {
  const ElementNode &node = *inst.desc;
  const SkRect rect = instanceRect(inst);

  const float opacity =
      std::clamp(inst.resolveFloat(Instance::kOpacity, node.paint.opacity),
                 0.0f, 1.0f);
  if (opacity <= 0.0f)
    return;

  canvas.save();
  canvas.translate(rect.left(), rect.top());

  const float tx = inst.resolveFloat(Instance::kTx, node.paint.translateX);
  const float ty = inst.resolveFloat(Instance::kTy, node.paint.translateY);
  const float rot = inst.resolveFloat(Instance::kRotate, node.paint.rotate);
  const float scl = inst.resolveFloat(Instance::kScale, node.paint.scale);
  if (tx != 0 || ty != 0)
    canvas.translate(tx, ty);
  if (rot != 0 || scl != 1) {
    const SkPoint origin = {rect.width() * node.paint.originX,
                            rect.height() * node.paint.originY};
    canvas.translate(origin.x(), origin.y());
    if (rot != 0)
      canvas.rotate(rot);
    if (scl != 1)
      canvas.scale(scl, scl);
    canvas.translate(-origin.x(), -origin.y());
  }

  const bool hasBackdrop =
      node.backdropEffect && node.backdropEffect->imageFilter();
  if (hasBackdrop) {
    canvas.save();
    if (node.shapeFn)
      canvas.clipPath(node.shapeFn({rect.width(), rect.height()}), true);
    else
      canvas.clipRRect(SkRRect::MakeRectXY(
                           SkRect::MakeWH(rect.width(), rect.height()),
                           node.corners.radius, node.corners.radius),
                       true);
    SkCanvas::SaveLayerRec rec(nullptr, nullptr,
                               node.backdropEffect->imageFilter().get(), 0);
    canvas.saveLayer(rec);
  }

  const bool needsLayer =
      opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    canvas.saveLayer(nullptr, &layerPaint);
  }

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target
  // pixel win — replaying a picture re-rasterizes, blitting doesn't).
  if (!inst.subtreeVolatile && node.cacheMode == Cache::Texture &&
      !node.backdropEffect) {
    // Rasterize at the canvas's current scale so zoomed hosts stay
    // crisp — but quantized UP to a coarse step, so a continuously
    // changing scale (window resize, pinch zoom) reuses one bake per
    // step instead of re-rasterizing every frame. Between steps the
    // draw minifies slightly, which stays sharp.
    SkMatrix total = canvas.getTotalMatrix();
    const float raw = std::clamp(
        std::max(std::abs(total.getScaleX()), std::abs(total.getScaleY())),
        0.25f, 4.0f);
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f, 2.0f, 3.0f, 4.0f};
    float scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) { scale = step; break; }
    if (!inst.textureImage || inst.paintDirty ||
        inst.textureScale != scale) {
      const int pw = std::max(1, (int)std::ceil(rect.width() * scale));
      const int ph = std::max(1, (int)std::ceil(rect.height() * scale));
      sk_sp<SkSurface> layer =
          canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
      if (!layer)
        layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
      layer->getCanvas()->scale(scale, scale);
      paintContent(inst, *layer->getCanvas());
      inst.textureImage = layer->makeImageSnapshot();
      inst.textureScale = scale;
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    canvas.drawImageRect(inst.textureImage,
                         SkRect::MakeWH(rect.width(), rect.height()),
                         SkSamplingOptions(SkFilterMode::kLinear));
  } else if (!inst.subtreeVolatile && node.cacheMode != Cache::None &&
             (node.cacheMode == Cache::Picture || !inst.children.empty() ||
              node.kind == Kind::Text || node.kind == Kind::Custom ||
              node.kind == Kind::Image || !node.backgrounds.empty() ||
              !node.foregrounds.empty() || node.layerEffect)) {
    if (!inst.picture || inst.paintDirty) {
      SkPictureRecorder recorder;
      SkCanvas *rec = recorder.beginRecording(
          SkRect::MakeWH(rect.width(), rect.height()));
      paintContent(inst, *rec);
      inst.picture = recorder.finishRecordingAsPicture();
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    canvas.drawPicture(inst.picture);
  } else {
    stats.nodesPainted++;
    paintContent(inst, canvas);
    inst.paintDirty = false;
  }

  if (needsLayer)
    canvas.restore();
  if (hasBackdrop) {
    canvas.restore(); // backdrop layer
    canvas.restore(); // clip
  }
  canvas.restore();
}

void Composer::Impl::rebuildKeyIndex() {
  byKey.clear();
  if (root)
    indexKeys(*root);
}

void Composer::Impl::indexKeys(Instance &inst) {
  const std::shared_ptr<ElementNode> &shell =
      inst.memoShell ? inst.memoShell : inst.desc;
  if (!shell->key.empty())
    byKey[shell->key] = &inst;
  else if (!inst.desc->key.empty())
    byKey[inst.desc->key] = &inst;
  for (auto &child : inst.children)
    indexKeys(*child);
}

// ---------------------------------------------------------------------------
// Composer public surface

Composer::Composer(tick::Ticker &ticker, sigil::weave::FontContext &fonts)
    : m_impl(std::make_unique<Impl>(ticker, fonts)) {}
Composer::~Composer() = default;

void Composer::setSize(SkSize size) {
  if (m_impl->size == size)
    return;
  m_impl->size = size;
  m_impl->needsLayout = true;
  m_impl->contentDirty = true;
}

void Composer::setClock(const tick::FrameClock *clock) {
  m_impl->clock = clock;
}

void Composer::render(Element root) {
  Impl &impl = *m_impl;
  impl.stats.describedNodes = 0;
  impl.stats.memoHits = 0;
  impl.stats.patchedNodes = 0;

  if (!impl.root)
    impl.root = impl.mount(root.node(), nullptr);
  else
    impl.patch(*impl.root, root.node());

  impl.computeVolatile(*impl.root);
  impl.volatileDirty = true; // transitions may have started
  impl.rebuildKeyIndex();
}

void Composer::renderSlot(std::string_view name, Element content) {
  Impl &impl = *m_impl;
  auto it = impl.byKey.find(std::string(name));
  if (it == impl.byKey.end() || it->second->desc->kind != Kind::Slot)
    return;
  Instance &slotInst = *it->second;

  // Patch or mount the slot's single content child.
  if (slotInst.children.size() == 1) {
    impl.patch(*slotInst.children.front(), content.node());
  } else {
    slotInst.children.clear();
    slotInst.children.push_back(impl.mount(content.node(), &slotInst));
    YGNodeRemoveAllChildren(slotInst.yoga);
    YGNodeInsertChild(slotInst.yoga, slotInst.children.front()->yoga, 0);
    slotInst.paintOrder = {0};
    impl.needsLayout = true;
  }
  slotInst.markPaintDirtyUp();
  impl.contentDirty = true;
  impl.volatileDirty = true;
  if (impl.root)
    impl.computeVolatile(*impl.root);
  impl.rebuildKeyIndex();
}

bool Composer::dirty() const {
  return m_impl->contentDirty || m_impl->needsLayout;
}

void Composer::draw(SkCanvas &canvas) {
  Impl &impl = *m_impl;
  if (!impl.root)
    return;

  impl.stats.picturesRecorded = 0;
  impl.stats.nodesPainted = 0;

  if (impl.needsLayout || YGNodeIsDirty(impl.root->yoga)) {
    YGNodeStyleSetWidth(impl.root->yoga, impl.size.width());
    YGNodeStyleSetHeight(impl.root->yoga, impl.size.height());
    YGNodeCalculateLayout(impl.root->yoga, YGUndefined, YGUndefined,
                          YGDirectionLTR);
    // Custom layout() containers: a bounded second pass — children were
    // measured by pass one; scheme.place() pins them, pass two resolves.
    bool repass =
        impl.hasCustomLayout && impl.applyCustomLayouts(*impl.root);
    // Derive phase: geometry-consuming content (exclusions, connectors).
    if (impl.hasDerived)
      repass |= impl.resolveDerived(*impl.root);
    if (repass) {
      YGNodeCalculateLayout(impl.root->yoga, YGUndefined, YGUndefined,
                            YGDirectionLTR);
      if (impl.hasDerived)
        impl.resolveDerived(*impl.root); // refresh connectors post-move
    }
    impl.needsLayout = false;
  }

  // Volatility changes only on reconcile or while animations run (and
  // once more on the settling frame) — skip the walk otherwise.
  const bool active = impl.ticker.active();
  if (impl.volatileDirty || active || impl.tickerWasActive) {
    impl.computeVolatile(*impl.root);
    impl.volatileDirty = false;
  }
  impl.tickerWasActive = active;

  impl.paint(*impl.root, canvas);
  impl.contentDirty = false;
}

std::optional<SkRect> Composer::bounds(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end())
    return std::nullopt;
  // Accumulate offsets up the yoga tree.
  SkRect rect = m_impl->instanceRect(*it->second);
  YGNodeRef node = YGNodeGetParent(it->second->yoga);
  while (node) {
    rect.offset(YGNodeLayoutGetLeft(node), YGNodeLayoutGetTop(node));
    node = YGNodeGetParent(node);
  }
  return rect;
}

const sigil::weave::ParagraphLayout *
Composer::paragraphLayout(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end() || !it->second->paragraph)
    return nullptr;
  return &it->second->textLayout;
}

const Composer::Stats &Composer::stats() const {
  // Tree tallies are computed on demand, never in the frame loop.
  size_t instances = 0, pictures = 0, textures = 0;
  std::function<void(const Instance &)> tally = [&](const Instance &i) {
    ++instances;
    if (i.picture)
      ++pictures;
    if (i.textureImage)
      ++textures;
    for (const auto &child : i.children)
      tally(*child);
  };
  if (m_impl->root)
    tally(*m_impl->root);
  m_impl->stats.instances = instances;
  m_impl->stats.picturesLive = pictures;
  m_impl->stats.texturesLive = textures;
  return m_impl->stats;
}

} // namespace sigil::compose
