/** @file
 * The volatility walk: which scalars a subtree exposes to the memoized
 * lane, what may cache and at which tier, and the released-motion scan that
 * re-declares a node the moment an external output moves it.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — textFill's cap-height metrics

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "ComposeRuntime.h"
#include "PaintInternal.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose {

using namespace detail;

/** Every animated scalar under a `Cache::Group` root, in tree order.
 *
 *  This is the whole invalidation mechanism, and therefore the whole risk.
 *  What it gathers is the set of numbers that can change what the bake looks
 *  like WITHOUT changing any description — which is exactly the set
 *  `computeVolatile` calls volatility and refuses to cache across. Two rules
 *  keep it honest:
 *
 *   - **Only LIVE slots are pushed.** A plain or settled value cannot move
 *     without a patch, and a patch calls markPaintDirtyUp() on the group.
 *     So the vector's LENGTH is part of the comparison: a motion connecting
 *     or disconnecting changes it, and the group re-bakes.
 *   - **The root's own transform and opacity are excluded.** They are
 *     applied by paint()'s matrix and saveLayer, outside the bake, and a
 *     fading group would otherwise drop its bake on every frame of the fade
 *     for a change the bake does not contain. (Its own transform moving is
 *     handled separately and more strictly — a device-pinned bake is refused
 *     outright while the node moves.) The root's CONTENT scalars are inside
 *     paintContent and are gathered like everyone else's.
 *
 *  Cost is one traversal of the subtree per frame, reading a handful of
 *  floats per node — set against the entire paint of that subtree, which is
 *  what it decides whether to skip. */
void collectGroupScalars(const Instance& inst, bool root,
                         std::vector<float>& out) {
  const ElementNode& node = *inst.desc;
  const auto push = [&](Instance::Slot slot, const Animatable<float>& v) {
    if (motion::isLive(inst.anims[slot].get(), v))
      out.push_back(inst.resolveFloat(slot, v));
  };
  // Every slot the table can reach, in enum order (kSlotSpecs,
  // ComposeRuntime.h — the one enumeration of Instance::Slot). The root's
  // own transform and opacity are the exclusion argued above; its CONTENT
  // scalars are inside paintContent and are gathered like everyone else's.
  //
  // THE ORDER OF THIS VECTOR IS ARBITRARY BUT MUST BE STABLE. It is only
  // ever compared against the vector this same function produced on the
  // previous frame (Impl::paint, `groupScratch == inst.groupPrev`), so any
  // fixed permutation of the gathered values computes the identical
  // verdict; what would break the memo is an order that varies between
  // frames for the same tree.
  for (const SlotSpec& spec : kSlotSpecs) {
    if (root && spec.role != SlotRole::Content) continue;
    if (const Animatable<float>* v = slotValueOf(spec, node))
      push(spec.slot, *v);
  }
  // Mask gates: the same argument, over the per-mask vector. Only LIVE
  // values are pushed, so the vector's LENGTH still carries a motion
  // connecting or disconnecting.
  if (node.hasMasks()) {
    size_t slot = 0;
    for (const Mask& m : node.fxData->masks) {
      const auto pushGate = [&](const Animatable<float>& v) {
        const AnimatedFloat* a =
            slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
        if (motion::isLive(a, v)) out.push_back(inst.resolveFloatAt(a, v));
        ++slot;
      };
      if (m.with.kind == Gate::Kind::Spans)
        for (const Spans::Term& t : m.with.where.terms) {
          pushGate(t.begin);
          pushGate(t.end);
          pushGate(t.offset);
        }
      else if (m.with.kind == Gate::Kind::Edge)
        pushGate(m.with.fraction);
    }
  }
  // fx() track progresses: the same argument again, over the per-track
  // vector. Only LIVE values are pushed, so the vector's LENGTH still
  // carries a track's motion connecting or disconnecting.
  if (node.textData)
    for (size_t i = 0; i < node.textData->tracks.size(); ++i) {
      const Animatable<float>& v = node.textData->tracks[i].progress;
      const AnimatedFloat* a =
          i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
      if (motion::isLive(a, v)) out.push_back(inst.resolveFloatAt(a, v));
    }
  // The kFillLerp row (SlotRole::Bespoke): a synthesized progress with no
  // Animatable in the description, so it is read straight off the motion.
  if (inst.anims[Instance::kFillLerp] &&
      inst.anims[Instance::kFillLerp]->value.isConnected())
    out.push_back(inst.anims[Instance::kFillLerp]->value.value());
  for (const auto& child : inst.children)
    collectGroupScalars(*child, false, out);
}

// ---------------------------------------------------------------------------
// Volatility & caching

/** The movement scan for released instances: re-checked once per draw, so
 *  an EXTERNALLY-driven output that moves re-declares volatility (and
 *  stales every ancestor recording) in the same frame — nothing stale ever
 *  replays. Cheap by construction: released nodes are few and each check is
 *  a handful of float resolves. */
// The node→root matrix, recomputed outside paint. The op sequence per level
// — preTranslate(rect), preConcat(matrix()) — is EXACTLY the pair paint()
// applies to curToRoot as it recurses, on the same resolved floats, so the
// result is bit-identical to the paint-side accumulation. The settle
// compare depends on that: an ulp of drift reads as motion, and the node
// never releases.
SkMatrix Composer::Impl::worldMatrixOf(Instance& inst) {
  std::vector<Instance*> chain;
  for (Instance* i = &inst; i; i = i->parent) chain.push_back(i);
  SkMatrix m = SkMatrix::I();
  for (Instance* link : std::views::reverse(chain)) {
    Instance& node = *link;
    const SkRect rect = instanceRect(node);
    m.preTranslate(rect.left(), rect.top());
    m.preConcat(transformOf(node).matrix({0, 0}, node.desc->paint, rect.width(),
                                         rect.height()));
  }
  return m;
}

namespace {
std::array<float, 6> worldSix(const SkMatrix& w) {
  return {w.getScaleX(), w.getSkewX(),  w.getTranslateX(),
          w.getSkewY(),  w.getScaleY(), w.getTranslateY()};
}
}  // namespace

std::array<float, 6> Composer::Impl::worldScalarsOf(Instance& inst) {
  if (!inst.hasWorldSpaceMaterial)
    return {};  // all-zero, matching the paint-side guard
  return worldSix(worldMatrixOf(inst));
}

void Composer::Impl::scanReleasedScalars() {
  if (volatileDirty || releasedScalars.empty())
    return;  // a pending recompute rebuilds the list (pointers may be stale)
  for (Instance* inst : releasedScalars) {
    Instance::ContentScalars now;
    now.gates = inst->resolveGateValues();
    now.tracks = inst->resolveTrackValues();
    // The node→root matrix: a released world-space node whose externally
    // driven transform resumes must re-declare THE FRAME it resumes, before
    // any recording carrying the old anchoring replays.
    now.world = worldScalarsOf(*inst);
    // The bound fill: a released node whose output is assigned while the
    // volatility walk is idle must re-declare before its settled colour
    // replays from an ancestor's recording or its own promoted bake.
    now.fill = inst->resolveBoundFill();
    // The bound tile pan: same argument, so a parked scroll re-declares
    // before its parked phase replays.
    now.pattern = inst->resolvePatternOffset();
    // …and onPath()'s phase: a released marquee whose output is driven
    // again must re-declare before its parked frame replays.
    now.pathAt = inst->resolvePathAt();
    // The hold's rescan side: it restarts the warmup from the new reading
    // and answers whether the node must re-declare.
    if (inst->settle.moved(std::move(now))) {
      inst->markPaintDirtyUp();
      volatileDirty = true;  // re-walk this frame, before anything paints
    }
  }
}

core::SubtreeVerdict Composer::Impl::computeVolatile(Instance& inst,
                                                     bool movingAbove) {
  const ElementNode& node = *inst.desc;

  auto boundOrRunning = [&](Instance::Slot slot, const Animatable<float>& v) {
    return motion::isLive(inst.anims[slot].get(), v);
  };
  // Span passes: an animated reveal rebuilds the pass's geometry, and an
  // animated brush repaints it. Both are CONTENT volatility, and both are
  // deliberately kept out of the scalar and live-material memos, which
  // compare a bounded per-node list of values and have nowhere to put an
  // open-ended pass list's endpoints.
  const bool spanVolatile = [&] {
    if (!node.hasStrokePasses()) return false;
    size_t slot = 0;
    bool live = false;
    for (const StrokePass& pass : node.strokeData->passes) {
      live |= pass.what.isAnimated();
      for (const Spans::Term& term : pass.where.terms)
        for (const Animatable<float>* v :
             {&term.begin, &term.end, &term.offset}) {
          if (motion::isLive(slot < inst.spanAnims.size()
                                 ? inst.spanAnims[slot].get()
                                 : nullptr,
                             *v))
            live = true;
          ++slot;
        }
    }
    return live;
  }();
  // Mask gates, split by what a memo can compare. A gate whose animation is
  // a BOUNDED LIST OF FLOATS (spans endpoints, an edge fraction) is
  // memo-visible and joins scalarContent below; a gate driven by a LIVE
  // MATERIAL is not a float and refuses both memos, exactly as a live
  // material fill does. A shape gate is a static Region and moves nothing.
  bool maskScalarLive = false, maskOpaque = false;
  if (node.hasMasks()) {
    size_t slot = 0;
    const auto live = [&](const Animatable<float>& v) {
      const AnimatedFloat* a =
          slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
      if (motion::isLive(a, v)) maskScalarLive = true;
      ++slot;
    };
    for (const Mask& m : node.fxData->masks) {
      switch (m.with.kind) {
        case Gate::Kind::Spans:
          for (const Spans::Term& t : m.with.where.terms) {
            live(t.begin);
            live(t.end);
            live(t.offset);
          }
          break;
        case Gate::Kind::Edge:
          live(m.with.fraction);
          break;
        case Gate::Kind::Shape:
          break;
        case Gate::Kind::Coverage:
          if (m.with.coverage && m.with.coverage->isAnimated())
            maskOpaque = true;
          break;
      }
    }
  }
  // THE SLOT ROLES, in one walk of kSlotSpecs (ComposeRuntime.h). This split
  // is where the table's three roles come FROM; the other three consumers
  // read it or ignore it, and none of them wanted a fourth thing.
  //
  //  - Opacity applies OUTSIDE the node's content (in paint()'s layer
  //    stack), so a node animated only there still replays its content
  //    picture — a fading ornament re-records nothing. Ancestors still
  //    can't cache across it (their recording would freeze the motion),
  //    hence the return value.
  //  - Geometric is kept separately because a texture bake taken in device
  //    space is pinned to one device rect and may only be taken while the
  //    node is not moving. Opacity is deliberately not part of it — it does
  //    not move the rect.
  //  - Content rebuilds the recording, and joins `scalarContent` below,
  //    which is the memoizable half of content volatility: the half whose
  //    inputs are floats this frame can read back and compare.
  bool ownPaint = false;
  bool moving = false;
  bool scalarContent = false;
  for (const SlotSpec& spec : kSlotSpecs) {
    const Animatable<float>* v = slotValueOf(spec, node);
    if (!v) continue;  // this node does not carry the block that holds the slot
    switch (spec.role) {
      case SlotRole::Opacity:
        ownPaint |= boundOrRunning(spec.slot, *v);
        break;
      case SlotRole::Geometric:
        moving |= boundOrRunning(spec.slot, *v);
        break;
      case SlotRole::Content:
        scalarContent |= boundOrRunning(spec.slot, *v);
        break;
      case SlotRole::Bespoke:
        break;  // unreachable: slotValueOf answers nullptr for a Bespoke row
    }
  }
  inst.transformLive = moving;
  inst.placementUnderMotion = moving || movingAbove;
  ownPaint |= moving;

  // Content volatility: what actually invalidates the node's own recording
  // (bound/lerping fills, per-frame programs, animated decorations and image
  // frames).
  //
  // THE TERMS ARE NAMED ONCE, AND EVERY CONSUMER IS A SUBTRACTION FROM
  // THEM. Four questions are asked of this one list — "is anything
  // volatile" (ownContent), "can a group's float memo SEE it"
  // (opaqueToTheMemo), "is the live material the ONLY one" (liveMatOnly),
  // "are the animated scalars the only ones" (scalarMemo). Each of them
  // could be written as its own enumeration of the terms, and the copies
  // would drift: a carve-out that forgets, say, a bound fill lets a node
  // carrying one AND an animated gate take a memo, keep the recording that
  // baked the old colour, and replay it for as long as the gate holds
  // still. Deriving each consumer by subtraction is what makes
  // `ownContent == liveMat | otherThanLiveMat == scalarContent |
  // otherThanScalar` true BY CONSTRUCTION rather than by review.
  const bool fillLerp = inst.anims[Instance::kFillLerp] &&
                        inst.anims[Instance::kFillLerp]->value.isConnected();
  const bool boundFill = node.paint.fill && node.paint.fill->binding();
  const Material* nodeLiveMat = liveMaterialOf(node);
  // A fill material whose ONLY animation is its own bound tile pan is NOT
  // the live-material lane — it is two floats, resolvable outside paint by
  // a pointer dereference, so it rides the memoized scalar lane exactly as
  // a gate fraction, the world matrix's six and a bound Fill do. A material
  // with a bound pan AND anything else (a live uniform, an elapsed-time
  // input, a nested pan in a blend layer) stays on the opaque live path.
  // Conservative, and the split is a partition: patternPan and liveMat can
  // never both be true.
  const bool liveMatAnimated = nodeLiveMat && nodeLiveMat->isAnimated();
  const bool patternPan = liveMatAnimated && nodeLiveMat->hasBoundOffset() &&
                          !nodeLiveMat->animatedBeyondBoundOffset();
  // truly live (bound/uTime) — geometry-dependent materials resolve at
  // record time and stay cacheable
  const bool liveMat = liveMatAnimated && !patternPan;
  const Material* mfLive = metricFillOf(node);
  const bool metricLive = mfLive && mfLive->isAnimated();  // chrome type
  const bool cacheNone = node.cacheMode == Cache::None;
  const bool decorLive = [&] {
    bool live = false;
    for (const Decoration& d : node.backgrounds) live |= d.isAnimated();
    for (const Decoration& d : node.foregrounds) live |= d.isAnimated();
    if (node.fxData)
      for (const Decoration& d : node.fxData->overlays) live |= d.isAnimated();
    return live;
  }();
  const bool imageLive = node.kind == Kind::Image && imageAssetOf(node) &&
                         imageAssetOf(node)->animated();
  // A LIVE effect: the filter is captured by the recording, so bound
  // uniforms on it are content volatility, exactly as they are on a fill
  // material.
  const bool liveEffect =
      (layerEffectOf(node) && layerEffectOf(node)->isAnimated()) ||
      (backdropEffectOf(node) && backdropEffectOf(node)->isAnimated());
  // A LIVE pass material on an fx() track — uTime, a bound uniform, a
  // bound block — repaints the pass's output every frame with no float the
  // scalar lane could compare, so it is opaque volatility, exactly as a
  // live textFill material is. A pass whose only motion is its track's
  // PROGRESS is not this: progress already rides the memoized scalar lane
  // above, and the recording replays once it settles.
  const bool passLive = [&] {
    for (const Track& t : tracksOf(node))
      if (t.effect)
        if (const Material* pm = t.effect.passMaterial())
          if (pm->isAnimated()) return true;
    return false;
  }();
  // The MEMOIZABLE scalars, tracked apart from the rest of ownContent: each
  // rebuilds the painted geometry when it moves, and each is a number that
  // can sit still for a long time inside a running motion. Declared and
  // filled with every SlotRole::Content slot up in the table walk; the MASK
  // GATES join here, because their count is a property of the description
  // and no fixed slot can hold them.
  scalarContent |= maskScalarLive;  // a moving gate re-cuts or re-clips
  // fx() TRACKS: a moving master progress rebuilds glyph geometry, so it is
  // content volatility — the memoizable half, because a progress is a float
  // this frame can read back. Every track counts, so a settled entrance
  // sitting under a live loop still declares.
  if (node.textData)
    for (size_t i = 0; i < node.textData->tracks.size(); ++i) {
      const Track& track = node.textData->tracks[i];
      const Animatable<float>& v = track.progress;
      const AnimatedFloat* a =
          i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
      if (!motion::isLive(a, v)) continue;
      scalarContent = true;
      // …and the THIRD way a run's placement creeps: a live track whose
      // effect moves glyphs off their pen positions carries every addressed
      // letter across the device by a fraction of a pixel per frame, exactly
      // as a driven baseline phase or a turning ancestor does. Whole-pixel
      // origins cannot express that, so such a run joins them on the finer
      // grid.
      //
      // BOTH HALVES ARE DECLARATIONS, never a frame diff: what the effect
      // does to a glyph, and whether the progress driving it is live. A
      // SETTLED displacing track is therefore back on whole pixels — its
      // glyphs are standing somewhere else and standing still, which is what
      // whole-pixel origins are for and what lets the frame cache. A track
      // that only fades, tints or substitutes never joins however hard its
      // progress runs.
      if (track.effect.displaces()) inst.placementUnderMotion = true;
    }
  // A world-space material under a CONNECTED transform — this node's own or
  // any ancestor's, threaded down this recursion as movingAbove — has its
  // node→root matrix changing off the describe clock, and that matrix is
  // baked into the recording. That is content volatility, and it joins the
  // MEMOIZED lane rather than the opaque one, because the matrix is six
  // floats ContentScalars carries: the recording survives between ticks,
  // the flag releases once the motion provably settles, and the per-draw
  // scan re-declares the frame it resumes. Note that for THIS node an
  // ancestor's transform is a content input, not a geometric one — it does
  // not move this node's own device rect.
  const bool worldUnderMotion =
      inst.hasWorldSpaceMaterial && (moving || movingAbove);
  scalarContent |= worldUnderMotion;
  // A BOUND fill joins the memoized lane too, even though a Fill is not a
  // float: its equality is structurally exact (kind, colour bitwise, shader
  // pointer) and resolving it is one pointer dereference, so "the value the
  // recording was baked with" is well defined. ContentScalars carries it,
  // so the recording survives between changes, the flag releases after
  // kScalarSettleFrames of identity — which is what lets a never-moving
  // bound accent be promoted like a plain colour — and the per-draw
  // released scan re-declares THE FRAME the output moves, before anything
  // stale replays.
  scalarContent |= boundFill;
  // …and the bound tile PAN, the third member of the lane. ContentScalars
  // carries the resolved pair, so each moved frame re-records with the new
  // phase and nothing re-describes, a parked scroll releases and promotes
  // like a static pattern, and the released scan re-declares THE FRAME the
  // pan resumes.
  scalarContent |= patternPan;
  /** The pre-release reading. The release below may set `scalarContent`
   *  false for a node that is provably holding still; the LIVE-MATERIAL
   *  memo asks a question about what the node DECLARES, not about whether
   *  it is currently moving, so it subtracts this and not the released
   *  value. */
  const bool scalarDeclared = scalarContent;
  // The stability RELEASE, read side. The settle counter accumulates at
  // PAINT time, because this walk re-runs only on reconcile or while the
  // ticker is active and so cannot count frames by itself. Here the walk
  // merely honours a warmed-up release and registers the instance for the
  // per-draw scan (scanReleasedScalars) that re-declares volatility THE
  // FRAME an externally-driven binding moves again. The node's own
  // recording was already kept by the scalar memo; what this frees is the
  // FLAG, so ancestors can cache across a settled reveal as well.
  if (scalarContent && inst.settle.release(Instance::kScalarSettleFrames, [&] {
        Instance::ContentScalars now;
        now.gates = inst.resolveGateValues();
        now.tracks = inst.resolveTrackValues();
        now.world = worldScalarsOf(inst);    // a held world matrix releases too
        now.fill = inst.resolveBoundFill();  // …and a held bound fill
        now.pattern = inst.resolvePatternOffset();  // …and a held bound pan
        now.pathAt = inst.resolvePathAt();          // …and a held marquee phase
        return now;
      })) {
    scalarContent = false;  // released — provably holding still
    releasedScalars.push_back(&inst);
  }
  // What a SUBTREE VALUE MEMO can and cannot see. A group bake is held by
  // comparing floats, so every source of volatility inside it must either
  // BE a float this frame can read back (the transform slots, opacity, the
  // mask gates, glyph progress, the fill lerp) or arrive as a description
  // change, which stales the group root through markPaintDirtyUp().
  // Everything named in `sharedOpaque` is neither: it moves pixels off the
  // clock with no number to compare, and a group holding a bake across one
  // of them would blit an old frame's picture indefinitely. Refused
  // outright rather than approximated — this is where the whole feature's
  // risk sits, and it is the one place to be conservative.
  //
  // The list is split in two: `sharedOpaque` is every term opaque to EVERY
  // memo, named once, while `boundFill` and `liveMat` are handled per
  // consumer below (the fill rides the memoized scalar lane, the live
  // material has its own memo). No consumer re-enumerates.
  const bool sharedOpaque = metricLive || cacheNone || decorLive || imageLive ||
                            spanVolatile || maskOpaque || liveEffect ||
                            passLive;
  // A bound fill still refuses Cache::Group, even though it rides the
  // node-level scalar lane. The group memo's currency is one flat float
  // vector gathered across the subtree (collectGroupScalars), and a Fill's
  // shader-kind value compares by POINTER identity, which is only sound
  // while an owning sk_sp keeps the allocation alive — a vector of floats
  // cannot hold that reference. Flattening the pointer into floats would
  // make a freed shader reallocated at the same address compare stable,
  // which is exactly the silent stale bake this refusal exists to prevent.
  //
  // A bound tile pan refuses the group as well, for a different reason: a
  // pan IS two floats, but collectGroupScalars does not gather it, so a
  // group bake held across a moving one would blit the parked phase. The
  // node-level release above still frees a settled pan's flag; only the
  // group BAKE stays refused.
  const bool opaqueToTheMemo =
      sharedOpaque || boundFill || liveMat || patternPan;
  // Everything volatile about this node EXCEPT its animated scalars, and
  // everything EXCEPT its live material. The two memo carve-outs below are
  // exactly these two subtractions, which is why neither can forget a term.
  // (`boundFill` is inside `scalarDeclared`, so it reaches
  // `otherThanLiveMat` through the scalar term, exactly as a gate does.)
  const bool otherThanScalar = sharedOpaque || liveMat || fillLerp;
  const bool otherThanLiveMat = sharedOpaque || fillLerp || scalarDeclared;
  const bool ownContent = otherThanScalar || scalarContent;

  core::ChildVolatility kids;
  for (auto& child : inst.children)
    // A connected transform HERE moves every descendant's world matrix.
    kids.add(computeVolatile(*child, movingAbove || moving));
  const bool childrenVolatile = kids.anyVolatile;
  // Does anything here composite against what is ALREADY on the canvas? If
  // so the subtree can never be baked into a transparent layer and blitted
  // back — a kMultiply child would resolve against transparent black. This
  // is the trap automatic promotion has to avoid, and it is invisible in a
  // still frame of the common case, so it is computed rather than assumed.
  // Split into halves, because the two cache strategies ask different
  // questions of it. Whole-subtree promotion bakes the children too, so it
  // must ask about the whole subtree. A split bake replaces only the node's
  // OWN layer and draws children over the blit, so it must ask only about
  // the node's own paint — the children composite against the blit exactly
  // as they would against freshly rasterized pixels.
  inst.ownReadsBackdrop = backdropEffectOf(node) != nullptr ||
                          node.paint.blendMode != SkBlendMode::kSrcOver;

  // THE PROOF ITSELF is SigilCoreCache's: everything above resolves this
  // library's own lanes, materials, gates and text into the six
  // declarations the kernel folds, and the kernel decides what the subtree
  // promises. The terms are compose's because they are about Skia paint and
  // shaped glyphs; the fold is not, because it is about trees.
  //
  // A MOVING world-space field joins `memoOpaque` rather than getting a
  // term of its own. The node→root matrix is not among the floats
  // collectGroupScalars gathers (only transforms INSIDE the group are), so
  // a bake held across an ancestor's motion would blit stale anchoring — it
  // is opaque to the group's value memo in exactly the sense the kernel
  // means, even though it is a number the NODE-level lane compares. A fully
  // static chain — no connected transform anywhere above — keeps its group:
  // description changes reach it through markPaintDirtyUp and layout moves
  // through syncLayoutRects.
  const core::SubtreeVerdict verdict = core::foldSubtree(
      {
          .policy = cachePolicy(node.cacheMode),
          .holdSubtree = node.cacheMode == Cache::Group,
          .ownPaint = ownPaint,
          .ownContent = ownContent,
          .memoOpaque = opaqueToTheMemo || worldUnderMotion,
          .readsBackdrop = inst.ownReadsBackdrop,
          // The node's own blend rides `readsBackdrop` alone: a group
          // root's blend and opacity are applied by paint()'s saveLayer,
          // OUTSIDE the bake, exactly as they would be applied outside the
          // live paint. A backdrop FILTER is applied inside it, so it is
          // the one that refuses the root as well as the inside.
          .samplesDestination = backdropEffectOf(node) != nullptr,
      },
      kids);
  inst.subtreeReadsBackdrop = verdict.subtreeReadsBackdrop;
  inst.groupSafe = verdict.memoSafe;
  inst.groupRootOK = verdict.holdRootOK;
  if (node.cacheMode == Cache::Group && !inst.groupRootOK &&
      !inst.groupWarned) {
    inst.groupWarned = true;
    // Loud, because the alternative is an author seeing a node they
    // explicitly asked to bake reported as live paint, with no way to learn
    // that one descendant several levels down declined it for them.
    SkDebugf(
        "sigilcompose Cache::Group: \"%s\" cannot bake — %s. A group is "
        "held by comparing FLOATS, so live materials (uTime or a bound "
        "uniform), animated decorations, animated images, bound fill(), "
        "variable-font drives, Cache::None leaves and non-srcOver "
        "blends below the root all refuse it.\n",
        node.key.empty() ? "(anon)" : node.key.c_str(),
        opaqueToTheMemo     ? "the group node itself carries volatility "
                              "the memo cannot see"
        : !kids.allMemoSafe ? "something in its subtree carries "
                              "volatility the memo cannot see"
                            : "it carries a backdrop filter");
  }

  // subtreeVolatile gates the node's own caches: blocked by content volatility
  // here or ANY volatility below (children paint inside the recording,
  // transforms included) — but not by own paint volatility.
  const bool blocked = verdict.subtreeVolatile;
  // …and WHICH of the two it was. `subtreeVolatile && !ownContentVolatile`
  // is the split bake's whole population: the node's own paint is provably
  // static and only its children move.
  inst.ownContentVolatile = verdict.ownContentVolatile;
  if (ownContent)
    inst.ownImage.reset();  // a volatile own paint can never hold a bake
  // The resolve-memo carve-out: volatility caused SOLELY by a live
  // material keeps its picture — paint() replays it while resolve() stays
  // stable and re-records only when the shader actually changes. Stated as
  // the subtraction it is, so it cannot fall behind the list again.
  inst.liveMatOnly = liveMat && !otherThanLiveMat && !childrenVolatile;
  // The same carve-out for animated SCALARS. A node whose content
  // volatility is entirely memoizable numbers keeps its recording and
  // re-records only when one of them actually ticks, so a keyframe's hold
  // segment repaints nothing. Deliberately disjoint from liveMatOnly: a
  // node with BOTH a live material and an animated gate takes neither memo,
  // which is the conservative answer and costs no more than having no memo
  // at all.
  inst.scalarMemo = scalarContent && !otherThanScalar && !childrenVolatile;
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  if (blocked != inst.subtreeVolatile) {
    inst.subtreeVolatile = blocked;
    if (!memoized)
      inst.paintDirty = true;  // cacheability changed → re-record/drop
  }
  if (inst.subtreeVolatile && !memoized) {
    inst.picture.reset();
    // A group root's bake is dropped by its OWN value memo, in paint(),
    // one frame at a time. Dropping it here instead would drop it every
    // frame — the subtree IS volatile, permanently, and that verdict is
    // precisely the one the group exists to look past. `picture` is still
    // reset: a group root never replays one, and leaving a stale recording
    // reachable is how the fall-through path would blit last frame's pixels
    // on the frame the memo just said not to.
    if (!inst.groupRootOK) inst.textureImage.reset();
  }
  return verdict;
}

}  // namespace sigil::compose
