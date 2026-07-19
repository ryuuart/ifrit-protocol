// The Composer: keyed reconciliation into a retained instance tree,
// Yoga layout with TextFlow-measured text leaves, stacking-order paint,
// automatic SkPicture caching of provably-static subtrees, and
// Choreograph-backed transitions/bindings stepped by the host's Ticker.

#include "ComposeInternal.h"

#include <ifritimage/ImageAsset.h>

#include <textflow/Flow.h>
#include <textflow/FontContext.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>

#include <yoga/Yoga.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace ifrit::compose {

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
  std::shared_ptr<ElementNode> desc; // resolved (post-memo) description
  std::shared_ptr<ElementNode> memoShell; // the memo element, if any
  YGNodeRef yoga = nullptr;
  std::vector<std::unique_ptr<Instance>> children;
  std::vector<size_t> paintOrder; // child indices sorted by zIndex

  // Text state
  std::optional<textflow::Paragraph> paragraph;
  textflow::ParagraphLayout textLayout;
  std::vector<textflow::LineMetrics> lines;
  float measuredForWidth = -1.0f;

  // Transition state, keyed by property slot
  enum Slot : int { kOpacity, kTx, kTy, kRotate, kScale, kFillLerp, kSlots };
  std::unique_ptr<AnimatedFloat> anims[kSlots];
  Fill fillFrom, fillTo; // endpoints for kFillLerp

  // Caching
  sk_sp<SkPicture> picture;
  bool paintDirty = true;
  bool subtreeVolatile = false;

  ~Instance();
  float resolveFloat(Instance::Slot slot, const PropValue<float> &v) const;
};

} // namespace detail

// ---------------------------------------------------------------------------

struct Composer::Impl {
  tick::Ticker &ticker;
  textflow::FontContext &fonts;
  const tick::FrameClock *clock = nullptr;

  SkSize size = SkSize::MakeEmpty();
  std::unique_ptr<Instance> root;
  YGConfigRef yogaConfig = nullptr;
  bool needsLayout = true;
  bool contentDirty = true;
  std::unordered_map<std::string, Instance *> byKey;

  Stats stats;

  Impl(tick::Ticker &t, textflow::FontContext &f) : ticker(t), fonts(f) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return clock ? clock->elapsed() : 0.0; }

  // ---- reconcile ----
  std::unique_ptr<Instance> mount(const std::shared_ptr<ElementNode> &node);
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

  // ---- paint ----
  void paint(Instance &inst, SkCanvas &canvas, bool insideCache);
  void paintContent(Instance &inst, SkCanvas &canvas);
  void rebuildKeyIndex();
  void indexKeys(Instance &inst);
  SkRect instanceRect(const Instance &inst) const;
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
  auto &impl = *inst->owner;
  textflow::BlockFlow flow(SkRect::MakeWH(constraint, 1.0e6f));
  inst->textLayout = textflow::layoutParagraph(impl.fonts, *inst->paragraph,
                                               flow, {});
  inst->lines = inst->textLayout.lineMetrics(*inst->paragraph);
  inst->measuredForWidth = constraint;
  SkRect bounds = SkRect::MakeEmpty();
  for (const textflow::LineMetrics &line : inst->lines)
    bounds.join(line.rect());
  return {std::ceil(bounds.width()), std::ceil(bounds.height())};
}

float baselineOfTextNode(YGNodeConstRef node, float, float) {
  auto *inst = static_cast<Instance *>(YGNodeGetContext(
      const_cast<YGNodeRef>(node)));
  if (inst->lines.empty())
    return 0.0f;
  const textflow::LineMetrics &first = inst->lines.front();
  return first.baseline - first.rect().top();
}

} // namespace

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
Composer::Impl::mount(const std::shared_ptr<ElementNode> &node) {
  auto inst = std::make_unique<Instance>();
  inst->owner = this;
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
  inst.paintDirty = true;
  contentDirty = true;

  // Kind change → full remount of content state.
  const bool kindChanged = prev && prev->kind != resolved->kind;
  if (kindChanged) {
    inst.paragraph.reset();
    inst.lines.clear();
    YGNodeSetMeasureFunc(inst.yoga, nullptr);
  }

  applyLayoutProps(inst);

  if (resolved->kind == Kind::Text) {
    const bool textChanged = !prev || kindChanged ||
                             prev->textUtf8 != resolved->textUtf8 ||
                             !(prev->textStyle == resolved->textStyle);
    if (textChanged) {
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
      patch(*match, node);
      inst.children.push_back(std::move(match));
    } else {
      inst.children.push_back(mount(node));
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
  YGNodeStyleSetAlignSelf(n, toYogaAlign(l.alignSelf));
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
  if (inst.subtreeVolatile)
    inst.picture.reset();
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

  if (node.clipContent)
    canvas.clipRRect(rrect, true);

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
      canvas.drawRRect(rrect, paint);
    }
  }

  // Content
  switch (node.kind) {
  case Kind::Text:
    if (inst.paragraph)
      inst.textLayout.drawBatched(&canvas, *inst.paragraph);
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
    if (node.program) {
      SkPathBuilder outlineBuilder;
      outlineBuilder.addRRect(rrect);
      node.program(canvas,
                   PaintContext{{bounds.width(), bounds.height()},
                                outlineBuilder.detach(), elapsed(), 1.0f,
                                ticker.active()});
    }
    break;
  case Kind::Box:
  case Kind::Stack:
    break;
  }

  // Children in stacking order.
  for (size_t index : inst.paintOrder)
    paint(*inst.children[index], canvas, /*insideCache handled upstream*/
          !inst.subtreeVolatile);
}

void Composer::Impl::paint(Instance &inst, SkCanvas &canvas,
                           bool insideCache) {
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

  const bool needsLayer =
      opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    canvas.saveLayer(nullptr, &layerPaint);
  }

  // Automatic picture caching at topmost provably-static subtrees.
  if (!inst.subtreeVolatile && !insideCache) {
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

Composer::Composer(tick::Ticker &ticker, textflow::FontContext &fonts)
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
    impl.root = impl.mount(root.node());
  else
    impl.patch(*impl.root, root.node());

  impl.computeVolatile(*impl.root);
  impl.rebuildKeyIndex();

  size_t count = 0;
  std::function<void(const Instance &)> tally = [&](const Instance &i) {
    ++count;
    for (const auto &c : i.children)
      tally(*c);
  };
  tally(*impl.root);
  impl.stats.instances = count;
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
    impl.needsLayout = false;
  }

  // Volatility can settle frame to frame (transitions finish).
  impl.computeVolatile(*impl.root);

  impl.paint(*impl.root, canvas, false);
  impl.contentDirty = false;

  size_t live = 0;
  std::function<void(const Instance &)> tally = [&](const Instance &i) {
    if (i.picture)
      ++live;
    for (const auto &c : i.children)
      tally(*c);
  };
  tally(*impl.root);
  impl.stats.picturesLive = live;
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

const textflow::ParagraphLayout *
Composer::paragraphLayout(std::string_view key) const {
  auto it = m_impl->byKey.find(std::string(key));
  if (it == m_impl->byKey.end() || !it->second->paragraph)
    return nullptr;
  return &it->second->textLayout;
}

const Composer::Stats &Composer::stats() const { return m_impl->stats; }

} // namespace ifrit::compose
