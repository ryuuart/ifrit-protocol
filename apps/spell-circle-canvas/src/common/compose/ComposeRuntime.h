#pragma once
// Internal to SigilCompose — the RETAINED runtime shared by the reconciler's
// phase translation units (Reconcile/Layout/Derive/Paint/Transitions/Query.cpp)
// and the Composer facade (Composer.cpp). Holds the retained Instance node and
// the Composer::Impl state; every phase file includes this and defines its
// slice of the Impl/Instance method set. Element DESCRIPTIONS live in
// ComposeInternal.h; this is the resolved, mutable, per-frame side.

#include "ComposeInternal.h"

#include <yoga/Yoga.h>

// markPaintDirtyUp() calls sk_sp::reset() inline, so the ref-counted payload
// types must be complete here (not merely forward-declared).
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sigil::compose::detail {

/** One float property that can transition: the Choreograph output is the
 *  source of truth while a motion is connected. */
struct AnimatedFloat {
  choreograph::Output<float> value{0.0f};
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this slot's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;
};

struct Instance {
  Composer::Impl *owner = nullptr;
  Instance *parent = nullptr;
  std::shared_ptr<ElementNode> desc;       // resolved (post-memo) description
  std::shared_ptr<ElementNode> memoShell;  // the memo element, if any
  YGNodeRef yoga = nullptr;
  std::vector<std::unique_ptr<Instance>> children;
  std::vector<size_t> paintOrder; // child indices sorted by zIndex

  // Text state
  std::optional<sigil::weave::Paragraph> paragraph;
  sigil::weave::ParagraphLayout textLayout;
  std::vector<sigil::weave::LineMetrics> lines;
  float measuredForWidth = -1.0f;
  YGSize measuredSize{0, 0};
  uint32_t contentRev = 0;    // bumped on text/exclusion change
  uint32_t measuredRev = ~0u; // rev the cached measurement belongs to
  // VariationDrive probe result for the CURRENT text content:
  // -1 unprobed, 0 refused (axis absent or advance-variant), 1 live.
  int8_t driveProbe = -1;

  // Transition state, keyed by property slot
  enum Slot : int {
    kOpacity, kTx, kTy, kRotate, kScale, kFillLerp,
    kTrimStart, kTrimEnd, kTrimOffset, kGlyphProgress, kSkewX, kSkewY,
    kScaleX, kScaleY, kWipe,
    kSlots
  };
  std::unique_ptr<AnimatedFloat> anims[kSlots];
  Fill fillFrom, fillTo; // endpoints for kFillLerp

  // Derive-phase state
  std::vector<SkRect> exclusionsLocal; // flowAround rects, text-local
  SkPath connectorPath;                // routed path (connector OR rail), local
  SkRect connectorFrom = SkRect::MakeEmpty(),
         connectorTo = SkRect::MakeEmpty();
  std::vector<SkPoint> railPoints;     // last resolved rail waypoints
  SkPath routedHitPath;                // stroke-expanded route (hit testing)

  // Caching
  sk_sp<SkPicture> picture;
  sk_sp<SkImage> textureImage;
  float textureScale = 1.0f;
  SkRect textureBakeRect = SkRect::MakeEmpty(); // bake covers paint bounds
  // Which SPACE the held bake lives in, and therefore how it must be
  // blitted: a device-space bake is snapped to whole device pixels and
  // drawn with the matrix reset (a literal copy, at any angle); a local
  // one is drawn through the node's transform and resampled by it. The
  // flag exists so a bake taken in one mode is never blitted in the other
  // — the two store different rectangles in textureBakeRect.
  bool textureDeviceSpace = false;
  // Is this node's OWN transform animating? (Geometric slots only — opacity
  // does not move the device rect.) A device-space bake is exact but is
  // pinned to one device rect, so it re-bakes whenever the node moves;
  // the quantized local bake exists precisely so a spinning ornament bakes
  // ONCE and rides its transform. Computed in computeVolatile, which runs
  // every frame whether or not the node is repainted — unlike anything
  // derived from paint history, which a node under a cached parent never
  // accumulates.
  bool transformLive = false;
  // …and is the device rect it actually LANDS on holding still? These are
  // NOT the same predicate, which is the seam a quantized-scale test caught:
  // a node with no animated property of its own still moves every frame
  // under a resizing window or a pinch zoom, and a device-pinned bake would
  // re-bake each time — losing exactly the one-bake-per-quantized-step reuse
  // the local bake exists to provide.
  //
  // Per-node history is sound HERE, and only here, because the device path
  // is refused inside a picture recording — so every node that can reach it
  // is painted every frame and does accumulate history. The first sighting
  // counts as stable: a node's first frame is otherwise forced down the
  // local path and then re-bakes on its second, which is a re-bake the
  // common case (a host that never changes scale) should never pay.
  SkIRect lastDeviceRect = SkIRect::MakeEmpty();
  bool deviceRectSeen = false;
  float bakedLeafOpacity = 1.0f;               // frozen into the recording
  SkBlendMode bakedLeafBlend = SkBlendMode::kSrcOver;
  bool paintDirty = true;
  bool subtreeVolatile = false;
  // Live-material stability (the resolve memo's paint half): set when the
  // node's ONLY volatility is its live material; the painter then replays
  // the cached picture whenever resolve() returns the shader the picture
  // baked (quantized/held materials repaint at their own rate, not the
  // frame rate).
  bool liveMatOnly = false;
  // Auto texture promotion: a rolling estimate of what this node's PAINT
  // costs each frame — a picture replay for a cached subtree, the live
  // draw for a leaf that never records one — and how many consecutive
  // frames it has been expensive. Promotion is sticky (autoTexture) until
  // the node goes dirty.
  float replayMs = 0;
  uint8_t hotFrames = 0;
  bool autoTexture = false;
  // Temporal stability, for the liveMatOnly case: the fraction of recent
  // frames on which the live material resolved to the SHADER ALREADY
  // BAKED. quantizeTime(10) at 60 FPS settles near 0.83; a material bound
  // to a continuous Output sits at 0 and never promotes. This is why the
  // rule is measured instead of read off quantizeTime(): a bound Output
  // that happens to be holding still is just as cacheable, and a quantized
  // one being driven past its step rate is not.
  float liveStableRate = 0;
  // True when this node OR anything below it blends against what is already
  // on the canvas (a non-srcOver blend, or a backdrop filter). Such a
  // subtree cannot be baked into a transparent layer and blitted back: the
  // blend would resolve against transparent black instead of the real
  // backdrop, and the pixels would differ. Computed by computeVolatile.
  bool subtreeReadsBackdrop = false;
  bool hasPendingLiveFill = false;
  Fill pendingLiveFill;
  sk_sp<SkShader> bakedLiveShader;

  // The layout rect this node was last painted/recorded at. ensureLayout's
  // post-pass compares and invalidates: a SIZE change stales this node's own
  // recording (its content baked the old bounds — text lines, geometry
  // materials' uResolution, rrect geometry); a POSITION change stales the
  // parent's recording (which baked the old offset). Without this, cached
  // ancestors replay stale geometry after any relayout that wasn't caused by
  // a prop patch — the latent resize-staleness gap.
  SkRect lastLayoutRect = SkRect::MakeLTRB(-1, -1, -1, -1);

  // Resolved custom-outline cache: generators (blobs, rounded stars) can be
  // arbitrarily expensive — resolve once per (description, size). Desc pointer
  // identity keys invalidation: every patch swaps the description.
  SkPath outlineCache;
  SkSize outlineCacheSize = {-1.0f, -1.0f};
  const ElementNode *outlineCacheDesc = nullptr;

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

// ---- cross-TU paint/shape helpers -----------------------------------------

/** The matrix's true maximum geometric scale — how many device pixels one
 *  local unit can span, whatever the rotation.
 *
 *  NOT max(|getScaleX()|, |getScaleY()|). Those are the matrix DIAGONAL, and
 *  a quarter turn moves the whole scale into the SKEW terms: at ±90° the
 *  diagonal is exactly (0, 0), because Skia's setRotate snaps cos(90°) to
 *  zero. Every raster-target decision that read the diagonal therefore saw
 *  "scale 0", clamped to the 0.25 floor, baked a rotated node at QUARTER
 *  resolution and linear-upscaled it 4×. Measured on a 196×33 pill against
 *  the identical uncached render: mean |Δ| over its ink was 30–32/255 at
 *  ±90°, 14.5 at 45° (0.707 → the 0.75 step), 2.4 at 180° (|−1| → correct).
 *  Singular values instead; a pure rotation reports 1. */
inline float maxScaleOf(const SkMatrix &m) {
  SkScalar s[2];
  if (m.getMinMaxScales(s) && s[1] > 0)
    return s[1];
  // Perspective (getMinMaxScales refuses): the diagonal is all there is.
  return std::max(std::abs(m.getScaleX()), std::abs(m.getScaleY()));
}

/** The paint-transform pivot: fractional by default, node-local px under
 *  transformOriginPx(). One definition for paint(), recordBounds(), and
 *  the hit-test inverse. */
inline SkPoint resolveOrigin(const PaintProps &p, float w, float h) {
  return p.originPx ? SkPoint{p.originX, p.originY}
                    : SkPoint{w * p.originX, h * p.originY};
}

inline SkRRect cornersRRect(const SkRect &bounds, const Corners &c) {
  const SkVector radii[4] = {{c.topLeft, c.topLeft},
                             {c.topRight, c.topRight},
                             {c.bottomRight, c.bottomRight},
                             {c.bottomLeft, c.bottomLeft}};
  SkRRect rrect;
  rrect.setRectRadii(bounds, radii);
  return rrect;
}

// Yoga measure/baseline callbacks (defined in Layout.cpp; referenced by
// Reconcile.cpp when it installs them on a text leaf's YGNode).
YGSize measureTextNode(YGNodeConstRef node, float width,
                       YGMeasureMode widthMode, float heightHint,
                       YGMeasureMode heightMode);
float baselineOfTextNode(YGNodeConstRef node, float width, float height);

} // namespace sigil::compose::detail

namespace sigil::compose {

struct Composer::Impl {
  motion::Ticker &ticker;
  sigil::weave::FontContext &fonts;
  const motion::FrameClock *clock = nullptr;

  SkSize size = SkSize::MakeEmpty();
  std::unique_ptr<detail::Instance> root;
  YGConfigRef yogaConfig = nullptr;
  bool needsLayout = true;
  bool contentDirty = true;
  std::unordered_map<std::string, detail::Instance *> byKey;
  // Slots get their OWN index. They live in byKey too (so bounds() and
  // hitTest() still answer for a slot's name), but a slot's CONTENT may
  // legitimately carry a root .key() with the same name — and since a
  // child is indexed after its parent, last-writer-wins let it shadow the
  // slot, so every later renderSlot() returned silently and the slot
  // froze on its first value. Two namespaces, no collision.
  std::unordered_map<std::string, detail::Instance *> bySlot;
  // The EDGE STORE, rebuilt with the key index each render: routed nodes
  // (connector()/rail()) as a flat list in tree order, plus the back-index
  // anchor-key → routes-anchored-there. The derive pass iterates these flat
  // lists instead of recursing the whole tree, and routesAt() answers graph
  // queries ("which edges touch this node") in O(routes-at-node).
  std::vector<detail::Instance *> routedInstances;
  std::vector<detail::Instance *> flowInstances; // flowAround() text nodes
  std::unordered_map<std::string, std::vector<detail::Instance *>>
      routesByAnchor;
  bool volatileDirty = true; // recompute needed (render or animation)
  bool tickerWasActive = false;
  // Recomputed with the key index (so unmounting the last derived/pinned
  // node actually clears them — they were latch-only before).
  bool hasDerived = false; // any flowAround/connector/rail in the tree
  bool hasCustomLayout = false;
  bool hasCenterPins = false; // any centerAt() in the tree
  bool liveOnly = false; // snapshot(): skip per-node caches
  Effect view;           // output view transform (null filter = pass-through)
  // staggerChildren(): the accumulated extra mount delay for the subtree
  // being mounted right now (depth-first, saved/restored per child — a
  // nested staggered container compounds on its parent's carry).
  float mountDelayCarryMs = 0;

  mutable Stats stats;
  // ---- per-node paint profiler (opt-in; Composer::setProfiling) ----------
  // profChildMs is the running total the CURRENT node's children have cost;
  // each node saves its parent's value, zeroes it, paints, then reports its
  // own total upward. That gives selfMs = totalMs - children without a
  // second traversal.
  bool profileEnabled = false;
  bool autoPromote = true; // Composer::setAutoTexturePromotion
  // Promoted bakes are pixels, and a dense study has a lot of full-canvas
  // nodes: chaucer_astrolabe paints 174 at 2400x1600, which is 15 MB each.
  // A budget, carried from the previous frame (paint order is stable, so
  // the previous frame's total is the right question to ask before adding
  // one more), keeps an automatic win from becoming an automatic OOM.
  size_t promotedBytes = 0;     // accumulated during the current paint
  size_t promotedBytesLast = 0; // what the previous frame ended up holding
  // >0 while painting INTO an SkPicture. Device-space bakes are pinned to
  // a device rect and must not be recorded into a picture that can replay
  // under a different matrix.
  int recordingDepth = 0;
  static constexpr size_t kPromotedBudget = 192u * 1024 * 1024;
  std::vector<Composer::NodeCost> profileRows;
  double profChildMs = 0;
  int profDepth = 0;
  // render()/renderSlot() phase time accumulated since the previous draw();
  // draw() publishes it as stats.reconcileMs and zeroes the accumulator.
  double reconcileAccumMs = 0;

  Impl(motion::Ticker &t, sigil::weave::FontContext &f) : ticker(t), fonts(f) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return clock ? clock->elapsed() : 0.0; }

  // ---- reconcile (Reconcile.cpp) ----
  std::unique_ptr<detail::Instance>
  mount(const std::shared_ptr<detail::ElementNode> &node,
        detail::Instance *parent);
  void patch(detail::Instance &inst,
             std::shared_ptr<detail::ElementNode> node);
  void patchChildren(detail::Instance &inst,
                     const std::vector<Element> &newChildren);
  void applyLayoutProps(detail::Instance &inst);
  void applyTransitions(detail::Instance &inst, const detail::ElementNode &prev,
                        const detail::ElementNode &next);
  void applyMountTransitions(detail::Instance &inst,
                             const detail::ElementNode &node);
  std::shared_ptr<detail::ElementNode>
  resolveMemo(detail::Instance *existing,
              const std::shared_ptr<detail::ElementNode> &node, bool &described);
  void rebuildKeyIndex();
  void indexKeys(detail::Instance &inst);

  // ---- volatility & caching (Paint.cpp) ----
  bool computeVolatile(detail::Instance &inst);

  // ---- layout (Layout.cpp) ----
  bool applyCustomLayouts(detail::Instance &inst);
  bool applyCenterPins(detail::Instance &inst);
  void ensureLayout();
  void syncLayoutRects(detail::Instance &inst);
  void layoutText(detail::Instance &inst, float constraint);
  SkRect instanceRect(const detail::Instance &inst) const;
  SkRect absoluteRect(const detail::Instance &inst) const;

  // ---- derive (Derive.cpp) ----
  /** One pass over the flat flow/route lists (the edge store) — no tree
   *  recursion. Returns true when a text exclusion changed (second layout
   *  pass needed). */
  bool resolveDerived();
  bool deriveFlow(detail::Instance &inst);
  void deriveRoute(detail::Instance &inst);

  // ---- paint (Paint.cpp) ----
  float hostScale = 1.0f; // device px per layout px at draw() entry
  void paint(detail::Instance &inst, SkCanvas &canvas);
  void paintTextOnPath(detail::Instance &inst, SkCanvas &canvas,
                       const TextPath &spec, SkSize size);
  void paintKineticText(detail::Instance &inst, SkCanvas &canvas,
                        const GlyphFx &fx);
  void paintContent(detail::Instance &inst, SkCanvas &canvas, float contentScale,
                    SkBlendMode leafBlend = SkBlendMode::kSrcOver,
                    float leafOpacity = 1.0f);
  const SkPath &resolveOutline(detail::Instance &inst, SkSize size) const;
  SkRect recordBounds(detail::Instance &inst);

  // ---- hit testing / queries (Query.cpp) ----
  bool shapeContains(detail::Instance &inst, SkPoint local, SkSize size) const;
  std::optional<std::string> hitInstance(detail::Instance &inst,
                                         SkPoint parentPt,
                                         const std::string *inheritedKey);
};

} // namespace sigil::compose
