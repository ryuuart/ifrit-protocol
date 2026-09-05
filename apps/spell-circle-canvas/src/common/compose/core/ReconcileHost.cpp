/** @file
 * The host half of the reconcile phase: what the composer does to an
 * instance once the generic reconciler has found its description changed
 * (dirtying, Yoga style writes, text content revisions, transition
 * application, derive resets), how a child instance is created and how a
 * parent's children are reattached to Yoga and ordered for paint, and the
 * one rule under which a surviving instance is remounted rather than
 * patched. The reconciler itself — memo resolution, the structural prune,
 * keyed and positional matching — is SigilCore's and knows nothing of what
 * is done here; these are the ReconcileHost operations it drives.
 */

#include <include/core/SkTypes.h>  // SkDebugf — the ignored-shell warning

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <numeric>
#include <string>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** Does anything this description paints anchor its field to the composer
 *  root (Material::worldSpace)? Every place a Material is resolved against
 *  the node's PaintContext is checked — the fill slot, textFill's metric
 *  material, both effects' child materials, the mask coverage materials.
 *  Miss one and a world-space material reached through it will not be
 *  invalidated when the node moves.
 *
 *  The answer is cached on the INSTANCE as a single bool so that the
 *  per-relayout rect walk and the per-frame volatility walk never have to
 *  descend a material tree again. */
bool nodeUsesWorldSpace(const ElementNode& n) {
  if (n.materialData) {
    if (n.materialData->live && n.materialData->live->usesWorldSpace())
      return true;
    if (n.materialData->recipe && n.materialData->recipe->usesWorldSpace())
      return true;
  }
  if (n.textData && n.textData->metricFill &&
      n.textData->metricFill->usesWorldSpace())
    return true;
  if (n.fxData) {
    if (n.fxData->layerEffect && n.fxData->layerEffect->usesWorldSpace())
      return true;
    if (n.fxData->backdropEffect && n.fxData->backdropEffect->usesWorldSpace())
      return true;
    for (const Mask& m : n.fxData->masks)
      if (m.with.coverage && m.with.coverage->usesWorldSpace()) return true;
  }
  return false;
}

/** Mark every world-space-carrying descendant's OWN paint dirty — their
 *  recordings baked a node-to-root matrix the caller just changed. */
void staleWorldSpaceBelow(Instance& inst) {
  for (auto& child : inst.children) {
    if (child->hasWorldSpaceMaterial) child->markPaintDirtyUp();
    staleWorldSpaceBelow(*child);
  }
}

/** A property set on a memo's SHELL — the element memo() returned, after
 *  `.key()`, `.cache()` and `.bakeScale()` — that describes nothing: the
 *  produced element is the node's whole look, and the reconciler retains
 *  the produce as the description. Said once per property, because a
 *  description is rebuilt every frame and a call that silently takes no
 *  effect must not be silent. Each group is judged by the structural
 *  compare itself, over a node carrying only that group, so a lane the
 *  compare rules on is a lane this warning sees. */
void warnIgnoredMemoShellProps(const ElementNode& shell) {
  const ElementNode blank;
  const auto only = [&](auto&& copy) {
    ElementNode probe;
    copy(probe);
    return !propsEqual(probe, blank);
  };
  struct Probe {
    const char* what;
    bool set;
  };
  const Probe probes[] = {
      {"a layout property",
       only([&](ElementNode& n) { n.layout = shell.layout; })},
      {"a fill, opacity, blend, transform or zIndex",
       only([&](ElementNode& n) {
         n.paint = shell.paint;
         n.materialData = shell.materialData;
       })},
      {"corners or a shape",
       only([&](ElementNode& n) {
         n.corners = shell.corners;
         n.shapeFn = shell.shapeFn;
       })},
      {"a decoration", !shell.backgrounds.empty() || !shell.foregrounds.empty()},
      {"a transition", shell.nodeTransition.has_value()},
      {"a child", !shell.children.empty()},
      {"a mask, overlay, effect or stagger", (bool)shell.fxData},
      {"a stroke pass", (bool)shell.strokeData},
      {"travel()", (bool)shell.motionData},
      {"a depth lane", (bool)shell.depthData},
      {"clipContent", shell.clipContent},
      {"hitTestable(false)", !shell.hitTestable},
      {"a boundary", shell.boundary != Boundary::Auto},
  };
  static boost::unordered_flat_set<std::string> warned;
  for (const Probe& probe : probes) {
    if (!probe.set || !warned.insert(probe.what).second) continue;
    SkDebugf(
        "[compose] memo(...) shell carries %s — ignored. A memo shell "
        "takes .key(), .cache() and .bakeScale() only; its look is what "
        "the deferred describe produces, so set this on the element "
        "produced inside it. (warned once)\n",
        probe.what);
  }
}

/** What a memo's shell says about the node it stands for, carried onto
 *  the produced description the reconciler retains: which node it is
 *  (the key, which the reconciler matches on itself) and how its
 *  recording is held. The painter reads cacheMode and bakeScale off the
 *  description alone, so an explicit choice on the shell would otherwise
 *  never reach it. The shell's Cache::Auto and unit bakeScale are its
 *  silence, and the produce's own values stand. */
void mergeMemoShell(ElementNode& produced, const ElementNode& shell) {
  if (shell.cacheMode != Cache::Auto) produced.cacheMode = shell.cacheMode;
  if (shell.bakeScale != 1.0f) produced.bakeScale = shell.bakeScale;
  warnIgnoredMemoShellProps(shell);
}

}  // namespace

void Composer::Impl::invalidate(Instance& inst) {
  inst.markPaintDirtyUp();
  contentDirty = true;
}

void Composer::Impl::destroy(std::unique_ptr<Instance> inst,
                             uint64_t /*frame*/) {
  inst.reset();
}

bool Composer::Impl::remountRequired(const Instance& match,
                                     const Instance& parent) {
  // Whether children of THIS parent carry Yoga nodes; a mismatch on a
  // reused instance (the container toggled positioned()) forces a fresh
  // mount, because a Yoga node's existence is fixed at mount.
  return (match.yoga != nullptr) != childrenCarryYoga(parent);
}

std::unique_ptr<Instance> Composer::Impl::create(const Description& node,
                                                 Instance* parent,
                                                 size_t ordinal, size_t count) {
  // staggerChildren(): the child's whole subtree mounts with
  // order·each extra entrance delay (saved/restored so siblings don't
  // leak; nested staggered containers compound). `from` remaps the
  // order — End counts from the last child (the bottom-up cascade),
  // Center ripples outward.
  const float saved = mountDelayCarryMs;
  const float staggerMs = parent && parent->description->fxData
                              ? parent->description->fxData->staggerChildrenMs
                              : 0.0f;
  if (staggerMs > 0) {
    // Order among NEWLY MOUNTED children: the initial cascade staggers
    // the whole list, but one item appended to a LIVE list enters with
    // no extra delay (it is the only new mount) instead of inheriting
    // its full-list ordinal.
    // The SAME ordering an fx() track's units take, so `From` means one
    // thing wherever it is written.
    static thread_local std::vector<float> order;
    // Child stagger has no seed knob: a Random child order is the
    // count-keyed deal, as it always was.
    // staggerMs > 0 only when parent->description->fxData exists: the ternary
    // above yields 0 for a null parent, so this dereference is guarded.
    // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
    motion::cascadeOrder(parent->description->fxData->staggerFrom, (uint32_t)count, 0u,
                         order);
    if (ordinal < order.size()) mountDelayCarryMs += staggerMs * order[ordinal];
  }
  auto inst = std::make_unique<Instance>();
  inst->owner = this;
  inst->parent = parent;
  // Positioned subtrees skip Yoga entirely: a child of a positioned()
  // container — and everything below it — carries its rect in its own
  // description, resolved by instanceRect() with no flex engine behind
  // it. The container itself keeps its node (it lives in its parent's
  // flow); its descendants never get one.
  if (!parent || childrenCarryYoga(*parent)) {
    inst->yoga = YGNodeNewWithConfig(yogaConfig);
    YGNodeSetContext(inst->yoga, inst.get());
  }
  reconciler.patch(*inst, node);
  mountDelayCarryMs = saved;
  needsLayout = true;
  return inst;
}

void Composer::Impl::onPatched(Instance& inst, const ElementNode* prev,
                               const ElementNode& next) {
  invalidate(inst);
  // `next` IS `*inst.description`: on a memo that is the produce, and the
  // shell's say over it is applied here, before anything reads the node.
  // A memo hit returns before this runs and keeps the payload it merged
  // on the patch that produced it, so a `.cache()` changed while props and
  // environment compare equal does not take — the memo's own contract.
  if (inst.memoShell) mergeMemoShell(*inst.description, *inst.memoShell);

  // Recompute the world-space flag once per patch. A pruned node keeps
  // its existing flag, which is correct: equal props mean equal
  // materials. Then the movement class only this branch can see — a
  // changed described transform moves every descendant's node-to-root
  // matrix while those descendants prune — so the world-space ones are
  // staled by hand. (Layout moves are caught by the rect sync walk;
  // bound transforms by the volatility walk.)
  inst.hasWorldSpaceMaterial = nodeUsesWorldSpace(next);
  if (prev && !describedTransformEqual(*prev, next)) staleWorldSpaceBelow(inst);

  // Kind change → full remount of content state.
  const bool kindChanged = prev && prev->kind != next.kind;
  if (kindChanged) {
    inst.paragraph.reset();
    inst.lines.clear();
    inst.columns.clear();
    if (inst.yoga) YGNodeSetMeasureFunc(inst.yoga, nullptr);
  }

  applyLayoutProps(inst);
  // centerAt lives outside Yoga's style set — it is applied inside
  // ensureLayout's convergence loop — so a moved pin has no dirty bit of
  // its own and must force the layout pass to run.
  if (!prev || prev->layout.centerAt != next.layout.centerAt)
    needsLayout = true;
  // A positioned child's rect IS its layout props: a change must run
  // the layout pass so syncLayoutRects stales the recordings that
  // baked the old rect (the job Yoga's dirty bit does elsewhere).
  if (!inst.yoga && prev && !(prev->layout == next.layout)) needsLayout = true;

  if (next.kind == Kind::Text && next.textData) {
    const TextData& text = *next.textData;
    const TextData* prevText =
        prev && prev->textData ? &*prev->textData : nullptr;
    const bool textChanged =
        !prevText || kindChanged || prevText->utf8 != text.utf8 ||
        !(prevText->style == text.style) || !(prevText->rich == text.rich) ||
        prevText->paragraphOverride != text.paragraphOverride ||
        !(prevText->options == text.options) ||
        prevText->spanRestyles != text.spanRestyles;
    if (textChanged) {
      inst.contentRev++;
      // No layout yet at describe time, so a weave::sel::line restyle resolves
      // against nothing here; layoutText() re-materializes against the
      // fresh line geometry when one is asked for.
      materializeText(inst);
      if (inst.yoga) {
        YGNodeSetMeasureFunc(inst.yoga, measureTextNode);
        YGNodeSetBaselineFunc(inst.yoga, baselineOfTextNode);
        YGNodeSetNodeType(inst.yoga, YGNodeTypeText);
        YGNodeMarkDirty(inst.yoga);
      }
      needsLayout = true;
    }
    // mark(): a mark's rect is an answer of the TEXT LAYOUT, and
    // layoutText() reuses a layout that is valid for the same content
    // revision. So a mark list that changed without the content changing
    // must invalidate that guard, or the marks keep the rects the
    // previous selectors resolved. The paragraph itself is untouched —
    // nothing is re-materialized and nothing re-shapes.
    else if (prevText && prevText->marks != text.marks) {
      inst.contentRev++;
      needsLayout = true;
    }
  }

  if (prev)
    applyTransitions(inst, *prev, next);
  else
    applyMountTransitions(inst, next);  // animate(from().to()) entrances

  // flowAround changes (margin or key set) re-derive too: exclusions are
  // cached per instance and the derive guards compare geometry, not the
  // description.
  const auto flowKeys =
      [](const ElementNode* n) -> const std::vector<std::string>* {
    static const std::vector<std::string> kNone;
    return n->deriveData ? &n->deriveData->flowAroundKeys : &kNone;
  };
  const auto flowMargin = [](const ElementNode* n) {
    return n->deriveData ? n->deriveData->flowAroundMargin : 0.0f;
  };
  if (prev && (*flowKeys(prev) != *flowKeys(&next) ||
               flowMargin(prev) != flowMargin(&next))) {
    inst.exclusionsLocal.clear();
    inst.contentRev++;  // relayout the text without exclusions, then derive
    needsLayout = true;
  }

  // Full-control text: ParagraphLayoutOptions are not comparable, so a
  // patched override node re-lays its text unconditionally — stale
  // justification/hyphenation is worse than a relayout on describe
  // (these nodes never prune anyway; hosts re-render on data change).
  if (prev && next.textData && next.textData->paragraphOverride &&
      prev->textData &&
      prev->textData->paragraphOverride == next.textData->paragraphOverride) {
    inst.contentRev++;
    YGNodeMarkDirty(inst.yoga);
    needsLayout = true;
  }

  // A container LOSING its custom layout must release the out-of-band
  // yoga writes place() left on the children (absolute + pinned rects
  // survive the structural prune otherwise — frozen children).
  if (prev && prev->deriveData && prev->deriveData->placeFn &&
      !(next.deriveData && next.deriveData->placeFn)) {
    for (auto& child : inst.children)
      if (child) applyLayoutProps(*child);
    needsLayout = true;
  }

  // A re-described ROUTE must re-derive even when no geometry moved: the
  // derive guards key cached geometry (resolved points/rects), not the
  // description — a router swap or an anchor-norm change would otherwise
  // keep replaying the stale path. Clearing the cached inputs defeats the
  // guards, and needsLayout makes ensureLayout run the derive pass.
  if (next.deriveData && (!next.deriveData->railAnchors.empty() ||
                          (!next.deriveData->connectFrom.empty() &&
                           !next.deriveData->connectTo.empty()))) {
    inst.railPoints.clear();
    inst.connectorFrom = SkRect::MakeLTRB(-1, -1, -1, -1);
    inst.connectorTo = SkRect::MakeLTRB(-1, -1, -1, -1);
    needsLayout = true;
  }
}

void Composer::Impl::reorder(Instance& parent, bool structureChanged) {
  // Paint order: stable sort by zIndex.
  parent.paintOrder.resize(parent.children.size());
  std::iota(parent.paintOrder.begin(), parent.paintOrder.end(), size_t{0});
  std::stable_sort(parent.paintOrder.begin(), parent.paintOrder.end(),
                   [&](size_t a, size_t b) {
                     return parent.children[a]->description->paint.zIndex <
                            parent.children[b]->description->paint.zIndex;
                   });

  // Yoga sees the children in the order they now stand: every child is
  // detached and reattached in `children` order, so the flex engine's child
  // list and the instance list cannot disagree.
  if (parent.yoga) {
    YGNodeRemoveAllChildren(parent.yoga);
    for (auto& child : parent.children) {
      // Stack children overlap: EVERY child is absolute, unconditionally.
      // This runs after create()/patch() applied the child's own layout props,
      // so it is the last write and a child cannot opt out — which is the
      // container's contract, not an oversight: a stack whose children could
      // individually rejoin the flex flow would lay out as neither. What a
      // child DOES keep is its insets — `.top(12).right(12)` inside a stack
      // pins that corner, because absolute is exactly the mode insets need.
      if (child->yoga) {
        if (parent.description->kind == Kind::Stack)
          YGNodeStyleSetPositionType(child->yoga, YGPositionTypeAbsolute);
        YGNodeInsertChild(parent.yoga, child->yoga,
                          YGNodeGetChildCount(parent.yoga));
      }
    }
  }

  // Mounts, unmounts, and reorders change what this node's recording paints
  // even when every surviving child is prop-identical — the structural prune
  // must not swallow them.
  if (structureChanged) {
    invalidate(parent);
    needsLayout = true;
  }
}

}  // namespace sigil::compose
