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
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

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

  // Transition state, keyed by property slot
  enum Slot : int {
    kOpacity, kTx, kTy, kRotate, kScale, kFillLerp,
    kTrimStart, kTrimEnd, kTrimOffset, kGlyphProgress, kSkewX, kSkewY,
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
  bool paintDirty = true;
  bool subtreeVolatile = false;

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
  bool volatileDirty = true; // recompute needed (render or animation)
  bool tickerWasActive = false;
  bool hasDerived = false; // any flowAround/connector in the tree
  bool hasCustomLayout = false;
  bool liveOnly = false; // snapshot(): skip per-node caches
  Effect view;           // output view transform (null filter = pass-through)

  mutable Stats stats;

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
  void ensureLayout();
  void syncLayoutRects(detail::Instance &inst);
  void layoutText(detail::Instance &inst, float constraint);
  SkRect instanceRect(const detail::Instance &inst) const;
  SkRect absoluteRect(const detail::Instance &inst) const;

  // ---- derive (Derive.cpp) ----
  bool resolveDerived(detail::Instance &inst);

  // ---- paint (Paint.cpp) ----
  float hostScale = 1.0f; // device px per layout px at draw() entry
  void paint(detail::Instance &inst, SkCanvas &canvas);
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
