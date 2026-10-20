#pragma once

/** @file
 * Internal to the kernel — what decides whether a node's held paint is
 * still the paint it would draw: its volatility as the last walk found it,
 * what the last draw left on the canvas for the repaint region, the
 * scalars, shader and matrix a recording was baked with and the release
 * once they settle, the subtree value memo, and the layout rect it was last
 * painted at.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilcore/cache/Settle.h>

#include <array>
#include <cstdint>
#include <vector>

#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** THE MEMOS A NODE'S PAINT IS HELD AGAINST: one part of the retained node,
 *  read off the instance by each field's own name, so
 *  `Instance::ContentScalars` and `Instance::kScalarSettleFrames` are
 *  spelled through the instance. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct PaintMemos {
  // Is the COMPOSER still working on this passage? A text told its
  // input is moving (Text::textWillChange) has its break decisions kept and
  // reused, and a frame that answered every block from that store did no
  // work at all: the passage is set exactly as the frame before it. So the
  // leaf reports what its last layout cost — how many blocks came from the
  // store, how many the budget forced to the greedy breaker — and this is
  // the one bit the caching proof reads off that report: a live passage
  // whose layout is not yet stable can change without any number this node
  // carries changing, which is what "opaque to a value memo" means.
  // Written in layoutText, beside the layout it describes.
  bool textComposing = false;
  /// What the last layout of this leaf cost, for `Composer::settling`.
  int textReusedBlocks = 0, textDegradedBlocks = 0;
  bool subtreeVolatile = false;
  /** WHAT THE LAST DRAW LEFT ON THE CANVAS FOR THIS NODE, for a host that
   *  keeps its pixels and repaints only what changed
   *  (`Composer::changedRegion`). `changedOwn` says this node's own paint
   *  changed since that draw — set by every patch and invalidation that
   *  reaches `markPaintDirtyUp`, and true on a node no draw has seen.
   *  `ownVaries` says its own paint moves off the describe clock (a live
   *  material, a running transition, a connected transform, its opacity),
   *  read off the volatility walk. The extent is the subtree's ink, its
   *  own layer effect's reach included, in the parent's space; the matrix
   *  is what placed it there; the signature names the children it held, in
   *  their order. */
  bool changedOwn = true;
  /** The subtree's allocation rect (`bakeBounds`) as the last paint that
   *  measured it found it, and whether one has. */
  SkRect heldBakeBounds = SkRect::MakeEmpty();
  bool bakeBoundsHeld = false;
  bool ownVaries = false;
  /** Whether the node's own paint lands differently from frame to frame —
   *  its transform, opacity or blend in motion — whatever it draws. */
  bool ownPaintMoves = false;
  /** Whether what this subtree COVERS can change from frame to frame: a
   *  content term of the node's own, or any paint below it that moves.
   *  A world-space material's anchoring moving with its node is not one:
   *  it moves where the material's field is sampled, which the node's
   *  coverage is not read from. Written by the volatility walk. */
  bool silhouetteVolatile = false;
  bool extentSeen = false;
  SkRect lastExtent = SkRect::MakeEmpty();
  SkMatrix lastPlacement = SkMatrix::I();
  uint64_t lastChildren = 0;
  // Live-material stability (the resolve memo's paint half): set when the
  // node's ONLY volatility is its live material; the painter then replays
  // the cached picture whenever resolve() returns the shader the picture
  // baked (quantized/held materials repaint at their own rate, not the
  // frame rate).
  bool liveMatOnly = false;
  // Set when the node's only content volatility is animated scalars, which
  // lets the painter re-record just when one of those values actually
  // changes. Volatility alone answers whether a motion is connected, not
  // whether it is currently moving: a keyframe's hold segment, a settled
  // easing, and any waypoint pair with equal endpoints are constant while
  // they hold, and a recording baked from them stays exact that whole time.
  //
  // Deliberately separate from liveMatOnly. The two compare different things
  // — a shader pointer versus a list of floats — so a node can qualify for
  // one and not the other.
  bool scalarMemo = false;
  // Set when the node's ONLY volatility is its own LAYER effect's bound
  // parameters: the content under the effect is static (no live child, no
  // live material, no animated scalar, no live decoration), so it can be
  // rasterized once and the effect run over that one image at every blit.
  // The image's identity holds from frame to frame, which is what lets an
  // effect built over held passes find them already filtered instead of
  // filtering a fresh layer again. A BACKDROP effect is never this: it
  // reads what is already on the canvas, which a bake cannot hold.
  bool effectOnly = false;
  /** The content scalars a recording was baked with. A node that has none
   *  compares equal to itself forever.
   *
   *  To ride here a value must be resolvable to floats outside of painting,
   *  bounded per node, and cheap to compare by value — that is what makes it
   *  usable as a cache key. Mask gate scalars qualify, so a held keyframe on
   *  a masked node repaints nothing.
   *
   *  Per-pass span endpoints do not qualify and are excluded: they belong to
   *  an open-ended pass list rather than to the node, so there is no bounded
   *  set of floats to compare. A node animated only by a per-pass span falls
   *  back to per-frame content volatility and does not cache. */
  struct ContentScalars {
    /** Every textFx() track's master progress, resolved, in the order
     *  trackAnims indexes them. Empty on text carrying no tracks. */
    std::vector<float> tracks;
    /** Every mask gate's animated floats, resolved, in the order
     *  maskAnims indexes them. */
    std::vector<float> gates;
    /** The node→root matrix's affine six, for a node whose material paints
     *  in world space under a connected transform (its own or an
     *  ancestor's). That matrix is then a content input of the recording
     *  exactly as a gate fraction is — floats, resolvable outside paint —
     *  so a turning node re-anchors and a held one re-caches with no
     *  machinery beyond this compare. All-zero, not identity, when the node
     *  carries no world-space material: both sites that fill this member
     *  apply the same guard, so the compare stays meaningful. */
    std::array<float, 6> world{};
    /** The resolved value of a fill bound to a live output. `Fill`'s
     *  equality is structurally exact (kind, plus the colour bitwise, plus
     *  shader POINTER identity — the same rule `bakedLiveShader` holds the
     *  live-material memo with) and resolving one is a pointer dereference,
     *  so "the value the recording was baked with" is well defined for this
     *  lane even though it is not a float. Default (None) when the node's
     *  fill is not bound — every site reads it through
     *  Instance::resolveBoundFill(), so the guard cannot drift between the
     *  volatility walk, the released scan and the paint probe. The sk_sp
     *  keeps a shader-kind value alive, so pointer identity can never be a
     *  freed allocation reused at the same address and comparing equal by
     *  accident. */
    Fill fill;
    /** The pan a recording's fill shader was baked with, when the fill
     *  material's tile offset is bound to live outputs: two floats resolved
     *  by pointer dereference, the world lane's and the fill lane's third
     *  sibling. All-zero when the node's fill material carries no top-level
     *  bound offset — every site reads it through
     *  Instance::resolvePatternOffset(), so the guard cannot drift between
     *  the volatility walk, the released scan and the paint probe. */
    std::array<float, 2> pattern{};
    /** textOnPath()'s resolved phase — WHERE ALONG the baseline the run sits
     *  this frame. The recording bakes the glyph positions that phase
     *  produced, so it is a content input exactly as a gate fraction is.
     *  Zero when the node carries no path baseline. */
    float pathAt = 0;
    bool operator==(const ContentScalars&) const = default;
  };
  ContentScalars bakedScalars;
  // The observed-stability RELEASE, and what scalarMemo alone cannot do.
  // scalarMemo keeps THIS node's own recording; the node still DECLARES
  // content volatility, so no ancestor may cache across it. Once the same
  // scalars resolve identically for kScalarSettleFrames consecutive paints,
  // the node stops declaring that volatility and ancestors can cache too.
  // The frame any value moves, the compare below fails, volatility
  // re-declares, and the ancestor's recording is refused before anything
  // stale replays.
  // The three sides of that release — the count kept where paint() runs,
  // the release the volatility walk performs, and the per-draw scan over
  // what it released — are SigilCoreCache's protocol, held here over this
  // library's own scalars.
  core::Settle<ContentScalars> settle;
  /** Consecutive stable paints before the release. Long enough that a value
   *  pausing between two keyframes does not count as settled. */
  static constexpr int kScalarSettleFrames = 8;
  // ---- Cache::Group, the SUBTREE value memo -------------------------------
  // scalarMemo asks "did this node's own animated scalars move". A group
  // asks the same question of a whole subtree, and for a different reason:
  // many small pieces forming one static assembly, each carrying a bound
  // opacity and scale for an entrance that runs and then holds. Their
  // pictures never change, so nothing about them is cacheable by the
  // volatility rule (the bindings stay connected forever) and everything
  // about them is cacheable for every frame the entrance is not running.
  //
  // `groupSafe` is the subtree-wide AND: does this node, and everything
  // under it, carry only volatility a float comparison can SEE? A live
  // material, an animated decoration, a GIF frame, a bound Fill or a
  // Cache::None paint program all move pixels with no float to compare, so
  // one of them anywhere below a group root refuses the whole thing rather
  // than baking something that goes stale silently.
  bool groupSafe = false;
  // …and may THIS node be a group root — cacheMode is Group, its own paint
  // is memo-visible, every child is groupSafe, no backdrop filter. The
  // root's own blend and opacity are deliberately allowed: they are applied
  // by paint()'s saveLayer OUTSIDE the bake, exactly as they would be
  // applied outside the live paint.
  bool groupRootOK = false;
  bool groupWarned = false;  // the refusal is printed once per instance
  // The subtree's animated scalars as of the PREVIOUS frame, in tree order.
  // Compared by value rather than hashed. A hash collision here would not
  // show up as a glitch — it would silently replay a stale picture for as
  // long as the subtree stayed collided — and the float vector is small
  // enough, on the few nodes that opt in, that the trade is not worth taking.
  std::vector<float> groupPrev;
  bool groupPrevSeen = false;
  // True when this node OR anything below it blends against what is already
  // on the canvas (a non-srcOver blend, or a backdrop filter). Such a
  // subtree cannot be baked into a transparent layer and blitted back: the
  // blend would resolve against transparent black instead of the real
  // backdrop, and the pixels would differ. Computed by computeVolatile.
  bool subtreeReadsBackdrop = false;
  // The same question asked of the node's OWN paint alone, without its
  // children: what the subtree's answer is folded from.
  bool ownReadsBackdrop = false;
  // What the node's own paint COVERS may change: its own content apart
  // from marks cast from its silhouette, which follow its children and
  // which a trace of the node never holds.
  bool ownCoverageVolatile = false;
  bool hasPendingLiveFill = false;
  Fill pendingLiveFill;
  sk_sp<SkShader> bakedLiveShader;

  // Does this node's description carry a world-space material ANYWHERE a
  // paint consumes one — the fill slot, the ink's paint, an effect's child
  // materials, a mask's coverage? Computed once at reconcile patch
  // (Reconcile.cpp), so the per-relayout syncLayoutRects walk and the
  // volatility walk read a bool instead of re-walking material trees.
  // Such a node's recording bakes its node→root matrix, so any movement of
  // the node OR of an ancestor stales its OWN paint. Note that the ancestor
  // case cannot be detected from this node's rect: instanceRect is
  // parent-relative, so an ancestor's move changes this node's world matrix
  // while its own rect compares equal.
  bool hasWorldSpaceMaterial = false;

  // The layout rect this node was last painted/recorded at. ensureLayout's
  // post-pass compares and invalidates: a SIZE change stales this node's own
  // recording (its content baked the old bounds — text lines, geometry
  // materials' uResolution, rrect geometry); a POSITION change stales the
  // parent's recording (which baked the old offset). Without this, cached
  // ancestors replay stale geometry after any relayout that wasn't caused by
  // a prop patch.
  SkRect lastLayoutRect = SkRect::MakeLTRB(-1, -1, -1, -1);
};

}  // namespace sigil::compose::detail
