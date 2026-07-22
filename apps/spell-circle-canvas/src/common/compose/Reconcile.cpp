// Reconcile phase: keyed reconciliation of element descriptions into the
// retained Instance tree, the structural-equality prune (the no-memo skip),
// Yoga-style application of layout props, and the key index. See DESIGN.md
// "Animation — two write paths" and "Caching".

#include "ComposeRuntime.h"

#include <algorithm>
#include <cstring>
#include <numeric>

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

void applyDim(YGNodeRef node, const Dim &d, void (*setPx)(YGNodeRef, float),
              void (*setPct)(YGNodeRef, float)) {
  switch (d.unit) {
  case Dim::Unit::Px: setPx(node, d.value); break;
  case Dim::Unit::Pct: setPct(node, d.value); break;
  case Dim::Unit::Auto:
    // Patch reuses the yoga node: a dim REMOVED from the description must
    // actually release (YGUndefined = unset), or last describe's value
    // sticks forever — the review-workflow staleness finding.
    setPx(node, YGUndefined);
    break;
  }
}

// ---- structural equality (the no-memo prune) ------------------------------
// Conservative: equal only when provably identical. Anything carrying an
// incomparable callable (custom programs, decorations, outlines, routers,
// custom layouts) compares unequal — memo is the tool for those; the common
// plain cases (boxes, fills, text runs, images) prune for free.

bool easeEqual(const choreograph::EaseFn &a, const choreograph::EaseFn &b) {
  const bool aSet = (bool)a, bSet = (bool)b;
  if (aSet != bSet)
    return false;
  if (!aSet)
    return true;
  using Ptr = float (*)(float);
  const Ptr *pa = a.target<Ptr>();
  const Ptr *pb = b.target<Ptr>();
  return pa && pb && *pa == *pb; // lambdas: unequal (conservative)
}

bool transitionEqual(const Transition &a, const Transition &b) {
  return a.duration == b.duration && a.delay == b.delay &&
         easeEqual(a.easing(), b.easing());
}

/** Shaped bindings prune like anything else: same Output, same affine,
 *  same curve under easeEqual's conservative rule. A re-describe that
 *  only changes the CURVE must NOT prune — the map is read live, so a
 *  pruned node would keep shaping through the old one forever. */
bool boundMapEqual(const BoundFloat &a, const BoundFloat &b) {
  return a.source == b.source && a.inScale == b.inScale &&
         a.inOffset == b.inOffset && a.steps == b.steps && a.scale == b.scale &&
         a.offset == b.offset && a.clamped == b.clamped && a.lo == b.lo &&
         a.hi == b.hi && easeEqual(a.curve, b.curve);
}

template <typename T>
bool propEqual(const PropValue<T> &a, const PropValue<T> &b) {
  if (a.index() != b.index())
    return false;
  if (const T *plainA = a.plain())
    return *plainA == *b.plain();
  if (const Transitioned<T> *trA = a.transitioned()) {
    const Transitioned<T> *trB = b.transitioned();
    return trA->value == trB->value && trA->from == trB->from &&
           trA->waypoints == trB->waypoints &&
           transitionEqual(trA->spec, trB->spec);
  }
  if (const BoundFloat *mapA = a.boundMap())
    return boundMapEqual(*mapA, *b.boundMap());
  return a.binding() == b.binding();
}

bool effectEqual(const std::optional<Effect> &a,
                 const std::optional<Effect> &b) {
  if (a.has_value() != b.has_value())
    return false;
  if (!a)
    return true;
  return a->imageFilter().get() == b->imageFilter().get();
}

// ---- block equality (presence must match; then contents, preserving the
// monolith's exact semantics — callables stay conservatively unequal) ----

bool textEqual(const ElementNode &a, const ElementNode &b) {
  if ((bool)a.textData != (bool)b.textData)
    return false;
  if (!a.textData)
    return true;
  const TextData &ta = *a.textData, &tb = *b.textData;
  if (ta.glyphFx.has_value() != tb.glyphFx.has_value())
    return false;
  if (ta.glyphFx)
    return false; // effect is a callable — memo covers settled kinetic text
  // VariationDrive: BINDING identity, like PropValue bindings — same tag
  // and same Output pointer prune (the value lives outside the tree).
  if (std::memcmp(ta.driveTag, tb.driveTag, 4) != 0 ||
      ta.driveValue != tb.driveValue)
    return false;
  if (ta.utf8 != tb.utf8 || !(ta.style == tb.style))
    return false;
  // layoutOptions aren't comparable in full, but alignment is the one knob
  // the simple text() path exposes (textAlign) — compare it so an alignment
  // change actually patches.
  if (a.kind == Kind::Text &&
      ta.layoutOptions.alignment != tb.layoutOptions.alignment)
    return false;
  if (ta.paragraphOverride != tb.paragraphOverride)
    return false;
  if (ta.paragraphOverride)
    return false; // layoutOptions aren't comparable — memo these
  // onPath(): the baseline is an incomparable std::function, so a run
  // that has one never prunes — the same conservative rule deriveEqual
  // applies to placeFn/router. (It was omitted here entirely when onPath
  // landed, which meant a moving `at` or a new baseline silently kept the
  // OLD path forever. TextPath's defaulted operator== was implicitly
  // deleted for the same reason and never caught it.)
  if (ta.onPath.has_value() != tb.onPath.has_value())
    return false;
  if (ta.onPath)
    return false;
  // textFill(): live never prunes, static compares by recipe.
  if (ta.metricFill.has_value() != tb.metricFill.has_value())
    return false;
  if (ta.metricFill) {
    if (ta.metricFill->isLive() || tb.metricFill->isLive())
      return false;
    if (!(*ta.metricFill == *tb.metricFill))
      return false;
  }
  return true;
}

bool deriveEqual(const Box<DeriveData> &a, const Box<DeriveData> &b) {
  if ((bool)a != (bool)b)
    return false;
  if (!a)
    return true;
  // Incomparable callables → conservative inequality.
  if (a->placeFn || b->placeFn || a->router || b->router || a->railRouter ||
      b->railRouter)
    return false;
  return a->railAnchors == b->railAnchors &&
         a->flowAroundKeys == b->flowAroundKeys &&
         a->flowAroundMargin == b->flowAroundMargin &&
         a->connectFrom == b->connectFrom && a->connectTo == b->connectTo;
}

bool fxEqual(const Box<FxData> &a, const Box<FxData> &b) {
  if ((bool)a != (bool)b)
    return false;
  if (!a)
    return true;
  if (a->echoes != b->echoes)
    return false;
  if (a->hasTrim != b->hasTrim)
    return false;
  if (a->hasTrim && (!propEqual(a->trimStart, b->trimStart) ||
                     !propEqual(a->trimEnd, b->trimEnd) ||
                     !propEqual(a->trimOffset, b->trimOffset) ||
                     a->trimMode != b->trimMode))
    return false;
  if (a->staggerChildrenMs != b->staggerChildrenMs ||
      a->staggerFrom != b->staggerFrom)
    return false;
  if (a->overlays.size() != b->overlays.size())
    return false;
  for (size_t i = 0; i < a->overlays.size(); ++i)
    if (!(a->overlays[i] == b->overlays[i]))
      return false;
  return effectEqual(a->layerEffect, b->layerEffect) &&
         effectEqual(a->backdropEffect, b->backdropEffect);
}

bool materialEqual(const Box<MaterialData> &a, const Box<MaterialData> &b) {
  if ((bool)a != (bool)b)
    return false;
  if (!a)
    return true;
  // Material-slot fills: truly live ones (bound/uTime) never prune —
  // conservative, like an incomparable callable. Geometry-dependent-but-
  // static ones (SDF chrome and friends) compare by recipe, so identical
  // re-describes prune like any other static material.
  if (a->live.has_value() != b->live.has_value())
    return false;
  if (a->live) {
    if (a->live->isLive() || b->live->isLive())
      return false;
    if (!(*a->live == *b->live))
      return false;
  }
  return true; // ->recipe is handled with the fill compare in propsEqual
}

bool propsEqual(const ElementNode &a, const ElementNode &b) {
  if (a.kind != b.kind || a.key != b.key)
    return false;
  // Incomparable callables → conservative inequality.
  if (a.shapeFn || b.shapeFn || (bool)a.customData || (bool)b.customData)
    return false;
  if (!deriveEqual(a.deriveData, b.deriveData))
    return false;
  // Decorations compare when they wrap value-comparable schemes (PathFormat,
  // Slice, Shadow…); an incomparable one (bare program, ContourWalk with a
  // draw lambda) makes Decoration::operator== false, so the node stays
  // conservative — static chrome prunes, live/opaque decorations don't.
  if (a.backgrounds.size() != b.backgrounds.size() ||
      a.foregrounds.size() != b.foregrounds.size())
    return false;
  for (size_t i = 0; i < a.backgrounds.size(); ++i)
    if (!(a.backgrounds[i] == b.backgrounds[i]))
      return false;
  for (size_t i = 0; i < a.foregrounds.size(); ++i)
    if (!(a.foregrounds[i] == b.foregrounds[i]))
      return false;
  if (!(a.layout == b.layout) || !(a.corners == b.corners) ||
      a.clipContent != b.clipContent || a.cacheMode != b.cacheMode ||
      a.bakeScale != b.bakeScale)
    return false;
  if (!fxEqual(a.fxData, b.fxData))
    return false;
  if (a.nodeTransition.has_value() != b.nodeTransition.has_value())
    return false;
  if (a.nodeTransition && !transitionEqual(*a.nodeTransition, *b.nodeTransition))
    return false;
  // Paint.
  const PaintProps &pa = a.paint, &pb = b.paint;
  if (pa.fill.has_value() != pb.fill.has_value())
    return false;
  if (!materialEqual(a.materialData, b.materialData))
    return false;
  // Material-set fills compare by RECIPE (the structural signature): equal
  // recipes mean interchangeable shaders, even though each describe minted a
  // fresh one — the §8.1 "materials CAN be compared" payoff. Everything else
  // falls through to the plain fill compare (color values, shader pointers).
  const Material *recipeA = a.materialData ? (a.materialData->recipe
                                                  ? &*a.materialData->recipe
                                                  : nullptr)
                                           : nullptr;
  const Material *recipeB = b.materialData ? (b.materialData->recipe
                                                  ? &*b.materialData->recipe
                                                  : nullptr)
                                           : nullptr;
  if ((recipeA != nullptr) != (recipeB != nullptr))
    return false;
  if (recipeA) {
    if (!(*recipeA == *recipeB))
      return false;
  } else if (pa.fill && !propEqual(*pa.fill, *pb.fill)) {
    return false;
  }
  if (!propEqual(pa.opacity, pb.opacity) || pa.blendMode != pb.blendMode ||
      !propEqual(pa.translateX, pb.translateX) ||
      !propEqual(pa.translateY, pb.translateY) ||
      !propEqual(pa.rotate, pb.rotate) || !propEqual(pa.scale, pb.scale) ||
      !propEqual(pa.skewX, pb.skewX) || !propEqual(pa.skewY, pb.skewY) ||
      pa.originX != pb.originX || pa.originY != pb.originY ||
      pa.originPx != pb.originPx || pa.zIndex != pb.zIndex)
    return false;
  // Content.
  if (!textEqual(a, b))
    return false;
  if ((bool)a.imageData != (bool)b.imageData)
    return false;
  if (a.imageData && (a.imageData->asset != b.imageData->asset ||
                      a.imageData->region != b.imageData->region ||
                      a.imageData->sampling != b.imageData->sampling))
    return false;
  return true;
}

} // namespace

// ---------------------------------------------------------------------------

std::shared_ptr<ElementNode>
Composer::Impl::resolveMemo(Instance *existing,
                            const std::shared_ptr<ElementNode> &node,
                            bool &described) {
  if (!node->isMemo()) {
    described = true;
    return node;
  }
  if (existing && existing->memoShell &&
      existing->memoShell->memoData->equal(existing->memoShell->memoData->props,
                                           node->memoData->props)) {
    stats.memoHits++;
    described = false;
    return existing->desc; // reuse the previously described payload
  }
  described = true;
  Element produced = node->memoData->invoke(node->memoData->props);
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
  if (node->isMemo())
    inst.memoShell = node;
  else
    inst.memoShell = nullptr;

  if (resolved == inst.desc)
    return; // payload identity: untouched subtree (memo hit)

  std::shared_ptr<ElementNode> prev = std::move(inst.desc);
  inst.desc = resolved;

  // Structural prune (the no-memo path): a fresh description that is provably
  // identical to the retained one patches nothing and — key property —
  // dirties nothing; only its children keep reconciling.
  const bool own = !prev || !propsEqual(*prev, *resolved);
  if (own) {
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
    // centerAt lives outside Yoga's style set (resolved in ensureLayout's
    // second pass), so a moved pin must force the layout pass itself.
    if (!prev || prev->layout.centerAt != resolved->layout.centerAt)
      needsLayout = true;

    if (resolved->kind == Kind::Text && resolved->textData) {
      const TextData &text = *resolved->textData;
      const TextData *prevText =
          prev && prev->textData ? &*prev->textData : nullptr;
      const bool textChanged =
          !prevText || kindChanged || prevText->utf8 != text.utf8 ||
          !(prevText->style == text.style) ||
          prevText->paragraphOverride != text.paragraphOverride ||
          prevText->layoutOptions.alignment != text.layoutOptions.alignment;
      if (textChanged) {
        inst.contentRev++;
        inst.driveProbe = -1; // new content → re-probe the drive gate
        if (text.paragraphOverride)
          inst.paragraph.emplace(*text.paragraphOverride);
        else {
          inst.paragraph.emplace();
          inst.paragraph->appendText(text.utf8, text.style);
        }
        YGNodeSetMeasureFunc(inst.yoga, measureTextNode);
        YGNodeSetBaselineFunc(inst.yoga, baselineOfTextNode);
        YGNodeSetNodeType(inst.yoga, YGNodeTypeText);
        YGNodeMarkDirty(inst.yoga);
        needsLayout = true;
      }
    }

    if (prev)
      applyTransitions(inst, *prev, *resolved);
    else
      applyMountTransitions(inst, *resolved); // withFrom() entrances

    // flowAround changes (margin or key set) re-derive too: exclusions are
    // cached per instance and the derive guards compare geometry, not the
    // description.
    const auto flowKeys = [](const std::shared_ptr<ElementNode> &n)
        -> const std::vector<std::string> * {
      static const std::vector<std::string> kNone;
      return n->deriveData ? &n->deriveData->flowAroundKeys : &kNone;
    };
    const auto flowMargin = [](const std::shared_ptr<ElementNode> &n) {
      return n->deriveData ? n->deriveData->flowAroundMargin : 0.0f;
    };
    if (prev && (*flowKeys(prev) != *flowKeys(resolved) ||
                 flowMargin(prev) != flowMargin(resolved))) {
      inst.exclusionsLocal.clear();
      inst.contentRev++; // relayout the text without exclusions, then derive
      needsLayout = true;
    }

    // Full-control text: ParagraphLayoutOptions are not comparable, so a
    // patched override node re-lays its text unconditionally — stale
    // justification/hyphenation is worse than a relayout on describe
    // (these nodes never prune anyway; hosts re-render on data change).
    if (prev && resolved->textData && resolved->textData->paragraphOverride &&
        prev->textData &&
        prev->textData->paragraphOverride ==
            resolved->textData->paragraphOverride) {
      inst.contentRev++;
      YGNodeMarkDirty(inst.yoga);
      needsLayout = true;
    }

    // A container LOSING its custom layout must release the out-of-band
    // yoga writes place() left on the children (absolute + pinned rects
    // survive the structural prune otherwise — frozen children).
    if (prev && prev->deriveData && prev->deriveData->placeFn &&
        !(resolved->deriveData && resolved->deriveData->placeFn)) {
      for (auto &child : inst.children)
        if (child)
          applyLayoutProps(*child);
      needsLayout = true;
    }

    // A re-described ROUTE must re-derive even when no geometry moved: the
    // derive guards key cached geometry (resolved points/rects), not the
    // description — a router swap or an anchor-norm change would otherwise
    // keep replaying the stale path. Clearing the cached inputs defeats the
    // guards, and needsLayout makes ensureLayout run the derive pass.
    if (resolved->deriveData &&
        (!resolved->deriveData->railAnchors.empty() ||
         (!resolved->deriveData->connectFrom.empty() &&
          !resolved->deriveData->connectTo.empty()))) {
      inst.railPoints.clear();
      inst.connectorFrom = SkRect::MakeLTRB(-1, -1, -1, -1);
      inst.connectorTo = SkRect::MakeLTRB(-1, -1, -1, -1);
      needsLayout = true;
    }
  }

  // (hasDerived/hasCustomLayout/hasCenterPins are recomputed with the key
  // index after the patch walk — see rebuildKeyIndex.)

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
  std::vector<const Instance *> oldOrder;
  oldOrder.reserve(inst.children.size());
  std::unordered_map<std::string, std::unique_ptr<Instance>> keyed;
  std::vector<std::unique_ptr<Instance>> unkeyed;
  for (auto &child : inst.children) {
    if (child) {
      oldOrder.push_back(child.get());
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
  size_t childOrdinal = 0;
  size_t mountOrdinal = 0; // order among children mounted THIS patch
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
      // staggerChildren(): the child's whole subtree mounts with
      // order·each extra entrance delay (saved/restored so siblings don't
      // leak; nested staggered containers compound). `from` remaps the
      // order — End counts from the last child (the bottom-up cascade),
      // Center ripples outward.
      const float saved = mountDelayCarryMs;
      const float staggerMs =
          inst.desc->fxData ? inst.desc->fxData->staggerChildrenMs : 0.0f;
      if (staggerMs > 0) {
        // Order among NEWLY MOUNTED children: the initial cascade staggers
        // the whole list, but one item appended to a LIVE list enters with
        // no extra delay (it is the only new mount) instead of inheriting
        // its full-list ordinal.
        const float n = (float)newChildren.size();
        float order = (float)mountOrdinal;
        switch (inst.desc->fxData->staggerFrom) {
        case Stagger::From::End:
          order = n - 1.0f - order;
          break;
        case Stagger::From::Center:
          order = std::abs(order - (n - 1.0f) * 0.5f) * 2.0f;
          break;
        case Stagger::From::Start:
          break;
        }
        mountDelayCarryMs += staggerMs * order;
      }
      inst.children.push_back(mount(node, &inst));
      mountDelayCarryMs = saved;
      ++mountOrdinal;
      needsLayout = true;
    }
    ++childOrdinal;
    Instance &placed = *inst.children.back();
    // Stack children overlap: absolute unless they positioned themselves.
    if (inst.desc->kind == Kind::Stack)
      YGNodeStyleSetPositionType(placed.yoga, YGPositionTypeAbsolute);
    YGNodeInsertChild(inst.yoga, placed.yoga, YGNodeGetChildCount(inst.yoga));
  }

  // Mounts, unmounts, and reorders change what this node's recording paints
  // even when every surviving child is prop-identical — the structural prune
  // must not swallow them.
  bool structureChanged = oldOrder.size() != inst.children.size();
  if (!structureChanged)
    for (size_t i = 0; i < oldOrder.size(); ++i)
      if (oldOrder[i] != inst.children[i].get()) {
        structureChanged = true;
        break;
      }
  if (structureChanged) {
    inst.markPaintDirtyUp();
    contentDirty = true;
    needsLayout = true;
  }
  // Unmatched old children unmount here (destructors cancel motions).
}

void Composer::Impl::applyLayoutProps(Instance &inst) {
  const LayoutProps &l = inst.desc->layout;
  YGNodeRef n = inst.yoga;
  const bool isStack = inst.desc->kind == Kind::Stack;

  YGNodeStyleSetFlexDirection(n, l.row ? YGFlexDirectionRow
                                       : YGFlexDirectionColumn);
  YGNodeStyleSetFlexWrap(n, l.wrap ? YGWrapWrap : YGWrapNoWrap);
  YGNodeStyleSetGap(n, YGGutterAll, l.gap);
  YGNodeStyleSetPadding(n, YGEdgeLeft, l.padding.left);
  YGNodeStyleSetPadding(n, YGEdgeTop, l.padding.top);
  YGNodeStyleSetPadding(n, YGEdgeRight, l.padding.right);
  YGNodeStyleSetPadding(n, YGEdgeBottom, l.padding.bottom);
  YGNodeStyleSetMargin(n, YGEdgeLeft, l.margin.left);
  YGNodeStyleSetMargin(n, YGEdgeTop, l.margin.top);
  YGNodeStyleSetMargin(n, YGEdgeRight, l.margin.right);
  YGNodeStyleSetMargin(n, YGEdgeBottom, l.margin.bottom);

  // Auto-sized layout() containers: applyCustomLayouts writes the placed
  // extent as explicit W/H onto auto-dim absolute containers — releasing
  // those here would zero the container every re-describe and feed
  // place() a degenerate input for a pass.
  const bool autoSized =
      inst.desc->deriveData && inst.desc->deriveData->placeFn && l.absolute;
  if (!(autoSized && l.width.unit == Dim::Unit::Auto))
    applyDim(n, l.width, &YGNodeStyleSetWidth, &YGNodeStyleSetWidthPercent);
  if (!(autoSized && l.height.unit == Dim::Unit::Auto))
    applyDim(n, l.height, &YGNodeStyleSetHeight, &YGNodeStyleSetHeightPercent);
  applyDim(n, l.minWidth, &YGNodeStyleSetMinWidth,
           &YGNodeStyleSetMinWidthPercent);
  applyDim(n, l.maxWidth, &YGNodeStyleSetMaxWidth,
           &YGNodeStyleSetMaxWidthPercent);
  applyDim(n, l.minHeight, &YGNodeStyleSetMinHeight,
           &YGNodeStyleSetMinHeightPercent);
  applyDim(n, l.maxHeight, &YGNodeStyleSetMaxHeight,
           &YGNodeStyleSetMaxHeightPercent);
  YGNodeStyleSetAspectRatio(n, l.aspect > 0 ? l.aspect : YGUndefined);
  YGNodeStyleSetFlexGrow(n, l.grow);
  YGNodeStyleSetFlexShrink(n, l.shrink);
  applyDim(n, l.basis, &YGNodeStyleSetFlexBasis, &YGNodeStyleSetFlexBasisPercent);
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
    // Per-side Dims: Auto leaves the side UNPINNED (YGUndefined), so
    // `.top(12).right(12)` pins a corner badge without stretching it.
    // Always write all four — patch() reuses the yoga node, and a side
    // that was pinned last describe must actually release.
    auto applyInset = [n](YGEdge edge, const Dim &d) {
      switch (d.unit) {
      case Dim::Unit::Px: YGNodeStyleSetPosition(n, edge, d.value); break;
      case Dim::Unit::Pct:
        YGNodeStyleSetPositionPercent(n, edge, d.value);
        break;
      case Dim::Unit::Auto:
        YGNodeStyleSetPosition(n, edge, YGUndefined);
        break;
      }
    };
    applyInset(YGEdgeLeft, l.insets.left);
    applyInset(YGEdgeTop, l.insets.top);
    applyInset(YGEdgeRight, l.insets.right);
    applyInset(YGEdgeBottom, l.insets.bottom);
  } else {
    // Insets REMOVED since the last describe must release too.
    YGNodeStyleSetPosition(n, YGEdgeLeft, YGUndefined);
    YGNodeStyleSetPosition(n, YGEdgeTop, YGUndefined);
    YGNodeStyleSetPosition(n, YGEdgeRight, YGUndefined);
    YGNodeStyleSetPosition(n, YGEdgeBottom, YGUndefined);
  }
}

void Composer::Impl::rebuildKeyIndex() {
  byKey.clear();
  routedInstances.clear();
  flowInstances.clear();
  routesByAnchor.clear();
  hasDerived = false;
  hasCustomLayout = false;
  hasCenterPins = false;
  if (root)
    indexKeys(*root);
  hasDerived = !routedInstances.empty() || !flowInstances.empty();
}

void Composer::Impl::indexKeys(Instance &inst) {
  const std::shared_ptr<ElementNode> &shell =
      inst.memoShell ? inst.memoShell : inst.desc;
  if (!shell->key.empty())
    byKey[shell->key] = &inst;
  else if (!inst.desc->key.empty())
    byKey[inst.desc->key] = &inst;
  // The edge store (flat derive lists + anchor back-index) and the pass
  // gates ride the same walk. Tree order here IS the derive order.
  const ElementNode &node = *inst.desc;
  if (node.deriveData) {
    const DeriveData &derive = *node.deriveData;
    if (!derive.flowAroundKeys.empty())
      flowInstances.push_back(&inst);
    const bool isConnector =
        !derive.connectFrom.empty() && !derive.connectTo.empty();
    const bool isRail = derive.railAnchors.size() >= 2;
    if (isConnector || isRail) {
      routedInstances.push_back(&inst);
      if (isConnector) {
        routesByAnchor[derive.connectFrom].push_back(&inst);
        if (derive.connectTo != derive.connectFrom)
          routesByAnchor[derive.connectTo].push_back(&inst);
      }
      for (const Anchor &anchor : derive.railAnchors) {
        auto &at = routesByAnchor[anchor.nodeKey];
        if (at.empty() || at.back() != &inst) // rails revisit anchors
          at.push_back(&inst);
      }
    }
    if (derive.placeFn)
      hasCustomLayout = true;
  }
  if (node.layout.centerAt)
    hasCenterPins = true;
  for (auto &child : inst.children)
    indexKeys(*child);
}

} // namespace sigil::compose
