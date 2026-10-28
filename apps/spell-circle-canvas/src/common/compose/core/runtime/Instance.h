#pragma once

/** @file
 * Internal to the kernel — the RETAINED runtime node: one Instance per
 * mounted element. Its fields are grouped by subject into the parts it
 * derives from — the text it retains, the cascade in force, its
 * transition state, the bakes it holds, the memos they are held against
 * and the outlines it keeps — and the instance itself carries its place in
 * the tree, its computed style, what the adding operators and the matched
 * rules gave it, and what the derive phase resolved for it.
 */

#include <include/core/SkRect.h>
#include <sigilcore/reconcile/Node.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/advanced/Held.h>
#include <sigilweave/style/Type.h>
#include <yoga/Yoga.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "CascadeInForce.h"
#include "Exclusion.h"
#include "FollowedAnswers.h"
#include "HeldBakes.h"
#include "OutlineCaches.h"
#include "PaintMemos.h"
#include "RetainedText.h"
#include "TextState.h"
#include "TransitionState.h"
#include "description/ComposeInternal.h"
#include "style/ComputedStyle.h"

namespace sigil::compose::detail {

struct Instance;

/** WHETHER THE CHILDREN OF THIS NODE CARRY YOGA NODES. Stated once because
 *  two places ask — the mount that creates the node and the patch that
 *  decides a reused instance is in the wrong mode — and a drift between
 *  them is a hard Yoga abort rather than a wrong pixel.
 *
 *  Two families answer no. A `positioned()` container's descendants carry
 *  their rects in their own descriptions instead of in the flex engine. And
 *  a TEXT node's children never can: Yoga forbids children under a node
 *  that has a measure function, and the measure function is how text sizes
 *  to its container. So text keeps measuring, and a `weave::rich().slot()` pill
 *  takes its box from the paragraph — which is the only thing that knows
 *  where the reserved run landed. */
inline bool childrenCarryYoga(const Instance& inst);

/** The retained node. The tree skeleton — `parent`, `description` (the
 * resolved, post-memo description), `memoShell` (the memo element, if any) and
 *  `children` — is SigilCore's Node, which is what the reconciler walks;
 *  everything after it is what this kernel retains per node, the parts by
 *  subject and the rest below. The skeleton stays the first base. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Instance : core::Node<Instance, std::shared_ptr<ElementNode>>,
                  RetainedText,
                  CascadeInForce,
                  TransitionState,
                  HeldBakes,
                  PaintMemos,
                  OutlineCaches {
  Composer::Impl* owner = nullptr;
  YGNodeRef yoga = nullptr;
  std::vector<size_t> paintOrder;  // child indices sorted by zIndex

  // WHAT THIS NODE'S PROPERTIES CAME TO. Every phase after the patch —
  // layout, paint, hit testing, the transition lanes — reads a property
  // HERE and not off `description`, so what fills it can change without
  // any of them changing. Written where the description is patched (see
  // ComputedStyle).
  ComputedStyle computed;
  /** This node and the style computed for it — what a lane reads an
   *  endpoint from. */
  StyledNode styled() const { return {computed, *description}; }

  // Which axes a layout SCHEME sized on this container, so that it keeps
  // sizing an axis it once sized. Remembered rather than read back off
  // Yoga: a scheme writes a point size on every child it places, so the
  // style alone cannot tell a container's own answer from its parent's
  // placement of it.
  bool schemeSizedWidth = false;
  bool schemeSizedHeight = false;
  // The turn an arranging operator on the parent gave this node, degrees
  // clockwise about its own transform origin: a paint-only lane laid
  // over the rotation the description states, written by the layout
  // pass and read wherever the node's transform is produced.
  float arrangedTurn = 0.0f;
  // Whether the layout pass has reported this node: a flex container whose
  // children run past its box, a text leaf a column shrank below its
  // lines. Raised when the report is said and lowered when the node fits
  // again, so one standing overflow is said once.
  bool overflowReported = false;
  bool squashReported = false;
  // Where this node landed, as a memo's describe that asked a placement
  // query of it follows it: made by the first such query and advanced by
  // the settle when a layout moves the node or sets its text again. Null
  // on every node no followed query has asked about.
  std::shared_ptr<FollowedPlacement> followedPlacement;

  /** The fill in force as the surface map the composer paints the page
   *  as, or null where the fill is not lit or no map is selected — where
   *  the fill beneath paints the material's colours flat. */
  const material::Paint* surfaceMapFillOf(const MaterialData& slot) const;
  /** Whether the ink in force here, or a span's, samples root
   *  coordinates. */
  bool inkUsesWorldSpace() const;

  // WHAT THE ADDING OPERATORS BUILT UNDER THIS NODE, kept from the last
  // time they ran, one slice per SCOPE that attached here: a node can be
  // attached to by its own operators and by every scope above it, and
  // each scope's pass replaces only its own slice. `addedChildren` is
  // the slices flattened in slice order — what the reconciler reads
  // through the host's children() beside the description's own list, so
  // one pass mounts, patches and retires both.
  struct AdditionSlice {
    Instance* source = nullptr;
    std::vector<Element> elements;
  };
  std::vector<AdditionSlice> additions;
  std::vector<Element> addedChildren;

  /** Whether this node or an ancestor is leaving. */
  [[nodiscard]] bool exiting() const {
    for (const Instance* node = this; node != nullptr; node = node->parent)
      if (node->leaving) return true;
    return false;
  }

  // WHAT THE MATCHED RULES STATE about the properties the computed style
  // carries, as the last cascade pass found them: the layer the patch and
  // the pass both fold between the parent's answers and the node's own
  // verbs. Null where no matched rule states one.
  std::unique_ptr<const RuleLayer> ruleLayer;
  // THE RULES BOTH LAYERS WERE BUILT FROM, weakest first: the nodes of
  // the matched rules at the pass that last built them. A layer is a
  // function of these alone and a sheet never changes once applied, so
  // a pass that matches the same ones keeps both layers as they stand.
  // Held, not merely pointed at, so a later sheet cannot reuse an address.
  std::vector<std::shared_ptr<ElementNode>> ruleLayerSource;
  // THE DESCRIPTION AS IT IS PAINTED, where a matched rule states marks —
  // a stroke, a background, an overlay, a foreground, or the effects of the
  // fill or ink it states: `description` with the rules' marks laid under
  // the node's own in each slot. Null where no matched rule states one,
  // which is nearly every node, and `painted()` is then the description.
  // Only the paint phase reads it; the reconcile compares `description`.
  std::shared_ptr<const ElementNode> dressed;
  // The description `dressed` was built from, held so a later description
  // cannot reuse its address; a patch that swaps the description dresses
  // it again.
  std::shared_ptr<ElementNode> dressedFrom;
  /** What the paint phase reads this node's marks, effects and fields off:
   *  the dressed description where a rule states marks, else the
   *  description itself. */
  [[nodiscard]] const ElementNode& painted() const {
    return dressed ? *dressed : *description;
  }
  /** How this node's values change when a describe moves them: its own
   *  `transition()`, else the one a matched rule states, else none. */
  [[nodiscard]] const std::optional<motion::Tween<float>>& transitionInForce()
      const {
    return description->transition() ? description->transition()
                                     : ruleTransition;
  }

  // Derive-phase state
  std::vector<Exclusion>
      exclusionsLocal;  // contentFlowAround targets, text-local
  // spans::fit(key): the keyed boxes, in this node's local space, in the
  // order the element declared them.
  std::vector<std::pair<std::string, SkRect>> spanFitRects;
  // strand::from(key): the keyed paths, in this node's local space —
  // handed to decorations as PaintContext::borrowed.
  std::vector<std::pair<std::string, geometry::path::Outline>> borrowedPaths;

  ~Instance();
  float resolveFloat(Instance::Slot slot,
                     const motion::Animatable<float>& v) const;
  /** The same resolution over an explicitly-held motion — the span
   *  endpoints, whose count the description decides. One body: a live
   *  value wins, then a running ramp, then the constant. */
  float resolveFloatAt(const HeldMotion* anim,
                       const motion::Animatable<float>& v) const;
  /** Resolve every mask gate's animatable floats for this frame, in the
   *  order maskAnims indexes them (and ContentScalars::gates stores
   *  them) — every value, live or settled, because the memo compares what
   *  the recording was BAKED with. Empty when the node carries no mask, or
   *  only shape/alpha gates, which have no numbers. */
  std::vector<float> resolveGateValues() const;
  /** The same resolution over the textFx() TRACKS: every track's master
   *  progress this frame, in the order trackAnims indexes them (and
   *  ContentScalars::tracks stores them). Empty when the node carries no
   *  tracks. */
  std::vector<float> resolveTrackValues() const;
  /** The same resolution over textOnPath()'s `at` phase — one float, or zero
   *  when the node carries no path baseline. Every compare site reads this
   *  one body, so the volatility walk, the released scan and the paint
   *  probe cannot drift apart. */
  float resolvePathAt() const;
  /** `resolveGateValues`'s sibling for the fill lane: the value a bound
   *  `fill(&output)` resolves to this frame — `binding()->value()`, exactly
   *  the read paint() bakes into the recording — or a default (None) Fill
   *  when the node's fill is not bound.
   *
   *  ALL THREE compare sites call this one body: the volatility walk's
   *  release test, the per-draw released scan, and the paint-side probe.
   *  Re-spelling the resolution at any of them makes the compares disagree,
   *  and disagreement here is silent — a recording keeps replaying with a
   *  colour the binding has since moved off. */
  Fill resolveBoundFill() const;
  /** The third sibling: the tile pan a bound `Pattern::offset(&x, &y)`
   *  resolves to this frame — one pointer dereference per axis, through
   *  `Material::boundOffsetValue()` — or all-zero when the node's fill
   *  material carries no top-level bound offset. Same one-body rule as
   *  resolveBoundFill: the walk release, the per-draw scan and the paint
   *  probe all call THIS, so the three compares cannot drift apart. */
  std::array<float, 2> resolvePatternOffset() const;

  /** A change here stales every ancestor's recording too.
   *
   *  `ownPaint` says whether THIS node's own paint changed. It is false for
   *  exactly one caller — a child whose layout POSITION moved, which stales
   *  the parent's recording (it baked the old offset) and leaves the
   *  parent's own paint untouched. Only the repaint region reads that
   *  distinction; passing true is always the safe answer. */
  void markPaintDirtyUp(bool ownPaint = true) {
    if (ownPaint) changedOwn = true;
    for (Instance* i = this; i; i = i->parent) {
      if (i->paintDirty && i != this) break;  // ancestors already invalidated
      i->paintDirty = true;
      i->picture.reset();
      i->textureImage.reset();
      i->movingImage.reset();
    }
    // Every ancestor's held bounds, with no early stop: one already dirty
    // may be in the middle of its paint, holding bounds it measured before
    // this change, and would keep them once that paint marks it clean. The
    // same walk lowers the density-only mark, since what each of them
    // covers may now have changed.
    for (Instance* i = this; i; i = i->parent) {
      i->bakeBoundsHeld = false;
      i->paintDirtyOnlyForDensity = false;
    }
  }
};

/** The node's text-engine state, created on first use. */
inline TextState& textStateOf(Instance& inst) {
  if (!inst.textState) inst.textState = std::make_unique<TextState>();
  return *inst.textState;
}

/** THE STYLE A TEXT LEAF IS SET IN, as one total style: the font in force
 *  for a leaf that inherits — the resolved one once the cascade pass has
 *  run, the initial values before — and the description's own style
 *  otherwise. What the strut, the metric band and the reach of a path run
 *  are read from. */
[[nodiscard]] inline sigil::weave::TextStyle baseStyleOf(const Instance& inst) {
  const ElementNode& node = *inst.description;
  if (node.textData && node.textData->inherits)
    return sigil::weave::toTextStyle(
        inst.cascadeResolved ? inst.font : sigil::weave::initialType());
  return node.textData ? node.textData->style : sigil::weave::TextStyle{};
}

inline bool childrenCarryYoga(const Instance& inst) {
  return inst.yoga != nullptr && inst.description &&
         !inst.computed.layout.positioned &&
         inst.description->kind != Kind::Text;
}

/** WHERE A LEAF STANDS IN ITS STORY, read off the retained instance: the
 *  line offset the frame chain gave it, and its own key for the frame-local
 *  address. A leaf that is not a frame of a chain answers a zero offset and
 *  its own key, so the ordinary text is the general case with nothing
 *  subtracted. */
[[nodiscard]] inline TextScope scopeOf(const Instance& inst) {
  const bool threads = inst.description && inst.description->textData &&
                       !inst.description->textData->threadTo.empty();
  return {inst.threadLineOffset, inst.threadStoryLines,
          threads || inst.threadLineOffset > 0,
          inst.description ? inst.description->key : std::string{}};
}

}  // namespace sigil::compose::detail
