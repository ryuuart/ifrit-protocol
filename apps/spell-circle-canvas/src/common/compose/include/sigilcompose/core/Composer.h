#pragma once

/** @file
 * SigilCompose Composer — the retained side: the tree that diffs each
 * described Element tree against the last, lays out, caches, animates and
 * paints into a canvas the host owns, and answers queries about what it
 * laid out.
 */

#include <include/core/SkPicture.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Text.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class SkCanvas;

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

namespace detail {
struct Instance;
}  // namespace detail

class Material;

// ---------------------------------------------------------------------------
// Composer — the retained side; a guest in the host's canvas

/** The retained tree standing behind the descriptions. It diffs each new
 *  Element tree against the last one, keeps layout and cached rasters
 *  alive across frames for the parts that did not change, and paints
 *  into a canvas the caller owns — it creates no surface and takes over
 *  no rendering loop. */
class Composer {
 public:
  /** BOTH REFERENCES ARE HELD, not copied, and both must outlive the
   *  composer. @p ticker drives transitions and, through its FrameClock
   *  when one is attached, PaintContext time; @p fontContext measures and
   *  shapes every text leaf. */
  Composer(motion::Ticker& ticker, sigil::weave::FontContext& fontContext);
  ~Composer();

  Composer(const Composer&) = delete;
  Composer& operator=(const Composer&) = delete;

  /** Layout viewport in canvas-space px; percent dims resolve here.
   *  The root element always fills the viewport (its own width/height
   *  are ignored, like the CSS root) — size content via children.
   *  An EMPTY size means INTRINSIC instead: the root sizes to its content
   *  and its own dims ARE respected. That is the rule the
   *  snapshot()/measure() path runs under. */
  void setSize(SkSize size);

  /** Feeds PaintContext::elapsedSeconds (one clock everywhere). Null
   *  freezes paint time at 0 — fine for static content and goldens. */
  void setClock(const motion::FrameClock* clock);

  /** Output view transform (color management): applied to the composer's
   *  whole output as the final stage — one saveLayer while set, zero cost
   *  when cleared (a default Effect{}). The intended source is an OCIO
   *  display/view baked to a 3D LUT (<sigilcompose/Ocio.h>), but any Effect
   *  works. Per-node caches are unaffected (this is post-cache, at
   *  composite). */
  void setView(Effect view);

  /** THE DECLARED INPUT SPACE — a declaration, NOT a conversion.
   *
   *  Compose composites in ENCODED sRGB and has no linear stage: every
   *  surface it paints into is N32Premul with no SkColorSpace, so the
   *  numbers an author writes are the numbers that land in the bytes. That
   *  is not configurable, because every channel weighting in the library —
   *  `by::luma`'s Rec. 601 coefficients first among them — is defined
   *  against it.
   *
   *  What this adds is the ability to SAY what you believe your colour
   *  values are, since "I deliberately author encoded sRGB" and "nobody
   *  thought about colour at all" otherwise produce identical trees.
   *  `EncodedSRGB`, the default, matches reality and is silent. Anything
   *  else is a mismatch the library can see, and it says so once: your
   *  values are still TREATED as encoded sRGB, so under a `LinearSRGB`
   *  declaration every channel computation in the pipeline — blending,
   *  `by::luma`, alpha compositing — runs on numbers it was not defined
   *  for.
   *
   *  NO conversion is performed, ever, and the declaration participates in
   *  nothing else: two renders under different declarations are
   *  byte-identical. */
  enum class InputSpace : uint8_t {
    EncodedSRGB,  ///< display-encoded sRGB — the space compose composites in
    LinearSRGB,   ///< linear-light sRGB — NOT compose's space; declaring warns
    DisplayP3,    ///< display-encoded Display P3 — NOT compose's space; warns
  };
  void declareInputSpace(InputSpace space);
  InputSpace declaredInputSpace() const;

  /** THE DESCRIBE PATH: reconciles @p root against the retained tree,
   *  matching children by key and pruning where a memo hits or the
   *  description compares equal. Call it whenever your data changed —
   *  never per frame just to move a value, which is what a binding is
   *  for. */
  void render(Element root);

  /** Updates only the named slot() mount point. Layout and stacking still
   *  integrate normally and ancestors re-record their caches, but the rest
   *  of the tree is untouched — which is the point: two data domains
   *  changing at different rates do not invalidate each other.
   *
   *  A name that matches no slot does nothing, silently. */
  void renderSlot(std::string_view name, Element content);

  /** Content or layout changed since the last draw(). Redraw when
   *  dirty() || ticker.active(). */
  bool dirty() const;

  /** Lays out if needed and paints at the canvas's current matrix/clip.
   *  Provably-static subtrees replay their auto-recorded pictures. */
  void draw(SkCanvas& canvas);

  /** Drops every per-node cache (auto pictures, Cache::Texture bakes,
   *  held live-material shaders) and marks the tree for a full repaint.
   *  GPU hosts call this on device loss or a backend switch: cached
   *  images minted by a dead context must not replay onto the next
   *  canvas. The retained tree, layout, and animations are untouched. */
  void purgeCaches();

  // ---- queries (resolved side only) ----
  /** Layout rect of a keyed node, in the composer's coordinate space.
   *  Valid after a draw() (or any other call that runs layout). */
  std::optional<SkRect> bounds(std::string_view key) const;
  /** Live SigilWeave layout of a keyed text node (valid until the next
   *  layout; for glyph choreography and queries). */
  const sigil::weave::ParagraphLayout* paragraphLayout(
      std::string_view key) const;
  /** THE SCHEDULE ONE fx() TRACK IS RUNNING: a `Beat` per beat of track
   *  @p trackIndex on the keyed text node, in draw order. Valid after a
   *  draw() (or any other call that runs layout), and computed on demand —
   *  nothing pays for it until it is asked for.
   *
   *  This is the read-back that keeps a non-glyph mark honest. Position a
   *  ball, a playhead or an underline from `rect` and `localT` here and it
   *  agrees with the glyphs by construction, whatever the cascade turns out
   *  to be — flat, nested, cue-driven, numbered over the selection or over
   *  the paragraph.
   *
   *  THIS IS THE ANSWER FOR A MOUNTED, ANIMATED RUN, where `measureRun` and
   *  `runPens` are the answer for a static one that is not in the tree: a
   *  beat's rect is read off the placement the layout produced, so it knows
   *  about wrapping, mixed styles and a path baseline, none of which a
   *  single-style measurement of one run can see.
   *
   *  An unknown key, a node that is not text, a track index past the node's
   *  list, and a track carrying no effect all resolve to an EMPTY vector,
   *  silently, exactly as an unknown key resolves everywhere else in the
   *  query family. Check your key first. */
  [[nodiscard]] std::vector<Beat> beatsOf(std::string_view key,
                                          size_t trackIndex) const;
  /** THE CASCADE'S WHOLE VIRTUAL SPAN, in ms — what track @p trackIndex's
   *  master progress [0,1] maps onto: the moment its last beat closes,
   *  compounded under a nested cascade and read off the table under a cue
   *  table. `beatsOf` reports where each beat OPENS; this is the one
   *  number that says when the whole schedule is over — what a progress
   *  duration must equal for the cascade to run at its authored ms, and
   *  what anything sequenced after the cascade offsets from. It is
   *  computed by the same resolved cascade `beatsOf` and the glyphs read,
   *  so the three cannot disagree about the schedule.
   *
   *  A LOOPING track (`Stagger::loopMs`) answers its PERIOD: the master
   *  maps onto one cycle rather than a one-shot span, so the period is
   *  what a wrapping phase's wall time must span for the schedule to run
   *  at its authored ms.
   *
   *  Valid after a draw(), like `beatsOf`. `Stagger::spanMs` is the
   *  DECLARE-TIME form of the same number, for the site that needs it
   *  before any layout exists — handed its unit count, where this reads
   *  the count off the laid-out text. An unknown key, a node that is not
   *  text, a track index past the node's list and a track carrying no
   *  effect all resolve to 0, silently, as `beatsOf` resolves empty. */
  [[nodiscard]] float cascadeSpanMs(std::string_view key,
                                    size_t trackIndex) const;
  /** Topmost key at a canvas-space point. Valid after a draw().
   *
   *  Paint-order aware (zIndex, then declaration order, topmost first),
   *  transform-aware (rotated, scaled and translated nodes hit in their
   *  visual place), and shape-aware (custom outlines and corner radii
   *  bound the hit region, so the gap between a star's arms misses). A
   *  keyless node's hit resolves to its nearest keyed ancestor, and
   *  clipped subtrees do not hit outside their clip.
   *
   *  It answers for any keyed node whose region contains the point,
   *  whether or not that node painted anything — so a keyed transparent
   *  container will answer for its whole box. See
   *  `Element::hitTestable` for the opt-out. */
  std::optional<std::string> hitTest(SkPoint canvasPoint) const;
  /** The edge store's back-index: keys of route elements (connector()/
   *  rail()) anchored on @p nodeKey, in tree order — the graph query
   *  ("which edges touch this node") for hover highlights and pruned
   *  updates. Keyless routes are anchored but unaddressable, so they are
   *  omitted; give routes keys to see them here. Valid after render(). */
  std::vector<std::string> routesAt(std::string_view nodeKey) const;

  // ---- introspection (cost verification; see the compose_bench target) --
  /** What the retained tree currently holds and what the last frame
   *  did to it. Read for verifying that a description is being reused
   *  rather than rebuilt: describes skipped and nodes kept should
   *  dominate once a tree has settled. */
  struct Stats {
    size_t instances = 0;       ///< live retained nodes
    size_t yogaNodes = 0;       ///< instances carrying a Yoga node —
                                ///< positioned() subtrees carry none
    size_t describedNodes = 0;  ///< element nodes visited last render()
    size_t memoHits = 0;        ///< memo props equal → describe skipped
    size_t patchedNodes = 0;    ///< instances whose props changed
    size_t picturesLive = 0;    ///< auto-cached subtree pictures held
    size_t texturesLive = 0;    ///< Cache::Texture images held
    /** CACHE WRITES last draw() — every recording AND every pixel bake.
     *
     *  The name is narrower than the number: `Cache::Texture` bakes and
     *  library-promoted bakes count here too, so this answers "how much
     *  cache work did that frame do". `texturesBaked` breaks out the
     *  pixel-bake subset. */
    size_t picturesRecorded = 0;
    size_t texturesBaked = 0;  ///< of those, bakes rather than recordings
    size_t nodesPainted = 0;   ///< instances painted live last draw()
    // Per-phase wall time, so a slow frame localizes at a glance. The paint
    // number is where per-pixel cost lives (live materials, re-records);
    // reconcile/layout/volatile are the retained machinery.
    double reconcileMs = 0;  ///< render()/renderSlot() since previous draw()
    double layoutMs = 0;     ///< ensureLayout() inside last draw()
    double volatileMs = 0;   ///< computeVolatile() walk inside last draw()
    double paintMs = 0;      ///< paint traversal inside last draw()
  };
  const Stats& stats() const;

  /** PER-NODE PAINT COST.
   *
   *  `stats().paintMs` says how long the frame spent painting and nothing
   *  about WHERE. This localizes it.
   *
   *      composer.setProfiling(true);
   *      composer.draw(canvas);
   *      for (const auto &row : composer.profile())   // worst first
   *        printf("%7.2f ms  %s\n", row.selfMs, row.label.c_str());
   *
   *  `selfMs` EXCLUDES children, so the number lands on the node that
   *  actually costs rather than on its ancestors.
   *
   *  A CACHED NODE CAN STILL BE THE MOST EXPENSIVE THING ON THE SHEET. A
   *  picture records the DRAW CALLS, so replaying it re-runs every shader
   *  over every pixel; only a texture bake replaces that with a blit. Such
   *  a node shows up here as `cached() == true` with a large `selfMs`.
   *
   *  Off by default: the timing calls are cheap but not free. */
  /** How a node produced its pixels this frame. Picture and Texture are
   *  named separately rather than collapsed into "cached", because they
   *  cost radically different amounts — see the note above. */
  enum class CacheState : uint8_t {
    Live,      ///< painted from scratch
    Picture,   ///< replayed a recording — RE-RUNS every shader, every pixel
    Texture,   ///< blitted a raster bake — the author asked for it
    Promoted,  ///< blitted a raster bake the LIBRARY decided to make
    /** Blitted the node's OWN paint and drew its live children over it.
     *  Volatility is declared per node, so a static ground plane carrying
     *  one moving child shares the child's verdict and loses; this state
     *  says the two were separated. */
    SplitOwn,
    /** Blitted a whole-subtree bake held by `Cache::Group`'s value memo:
     *  the node AND its animated children, composited once into one
     *  unrotated device layer while every bound scalar below holds still.
     *  Distinct from Texture because the thing being asserted is
     *  different — a Texture node is provably static, a Group node is
     *  provably NOT CHANGING RIGHT NOW, and the difference is one frame. */
    Group,
  };
  /** WHY a node is, or is not, a pixel bake.
   *
   *  Promotion refusals are individually correct and individually
   *  invisible, so an expensive live-painted node is otherwise a dead end
   *  for an author. Every profiled node carries its reason here. Each
   *  refusal value names a condition under which a bake would produce
   *  DIFFERENT PIXELS, which is the one thing promotion may never do. */
  enum class Promotion : uint8_t {
    Cheap,        ///< under the cost threshold — promoting it would not pay
    Warming,      ///< expensive, counting the consecutive frames before a bake
    Promoted,     ///< baked by the library
    AskedFor,     ///< Cache::Texture — the author's own bake, not a decision
    OptedOut,     ///< Cache::Picture / Cache::None, or promotion switched off
    Volatile,     ///< its content genuinely changes every frame
    Composited,   ///< opacity < 1 or a non-srcOver blend: a bake would round
                  ///< twice
    Transformed,  ///< rotated, skewed or mirrored — a bake would resample
    Filtered,     ///< layer/backdrop effect or clip on the node itself
    /** Something in the subtree composites against what is already on the
     *  canvas — a non-srcOver blend or a backdrop filter, on this node or
     *  any descendant. A bake would resolve it against transparent black.
     *  Separated from Filtered because the remedy is different: a clip is
     *  the author's own node to change, whereas this can be a blend three
     *  levels down that they will not find without being told. */
    ReadsBackdrop,
    TooBig,  ///< beyond the per-bake area cap or the composer's bake budget
    /** The node's OWN paint is baked and its volatile children are painted
     *  live over the blit. Not a refusal — the outcome for a node whose
     *  static self was being re-rasterized every frame to redraw a moving
     *  child on top of it. */
    SplitBaked,
  };
  /** One node's share of the last frame, with the reason it was or was
   *  not promoted to a cached bake. This is the per-node companion to
   *  Stats: Stats says the tree is re-rasterizing, these say which
   *  node is doing it and what refused the bake. */
  struct NodeCost {
    std::string label;   ///< key() if set, else kind + size — actionable
    double selfMs = 0;   ///< this node's own paint, EXCLUDING children
    double totalMs = 0;  ///< including children
    int depth = 0;
    CacheState cacheState = CacheState::Live;
    Promotion promotion = Promotion::Cheap;
    /** EVERY condition that refused a bake, not just the first one.
     *
     *  `promotion` is a first-match verdict, so a node that is both
     *  volatile and clipped reports only `Volatile`, and fixing the
     *  volatility then reveals a second refusal that was never mentioned.
     *  This mask carries all of them at once; `promotion` stays the
     *  primary outcome.
     *
     *  The bit index IS the Promotion ordinal, so there is no second table
     *  to drift out of sync with the enum. */
    uint16_t refusals = 0;
    bool refused(Promotion p) const {
      return (refusals & (uint16_t)(1u << (unsigned)p)) != 0;
    }
    bool cached() const { return cacheState != CacheState::Live; }
  };
  /** One short phrase for a Promotion, for printing next to a cost. */
  static const char* promotionReason(Promotion p);
  void setProfiling(bool on);
  bool profiling() const;
  /** Rows from the last draw(), sorted by `selfMs` descending. Empty when
   *  profiling is off. */
  const std::vector<NodeCost>& profile() const;

  /** AUTOMATIC TEXTURE PROMOTION. On by default on CPU raster; OFF by
   *  default on a Graphite/GPU surface, because the cost model driving it
   *  measures op-RECORDING time, which describes raster work and not GPU
   *  work. This setter overrides in both directions.
   *
   *  A provably-static `Cache::Auto` subtree already caches as an
   *  SkPicture — but a picture records the DRAW CALLS, so replaying it
   *  re-runs every shader over every pixel, forever. It saves the describe
   *  and the layout, not the pixels. Promotion is what turns an expensive
   *  static node into an actual blit: the composer watches how long each
   *  static node's paint costs, and once a node has been expensive for
   *  several consecutive frames it bakes that subtree ONCE into a raster
   *  image and blits it thereafter.
   *
   *  THREE KINDS OF NODE ARE ELIGIBLE:
   *
   *  1. A cached subtree whose picture replay is expensive.
   *  2. A LEAF that never records a picture at all. Bare boxes are
   *     deliberately excluded from picture recording, since one drawRect
   *     beats a nested recording — but a full-canvas box carrying one
   *     costly shader is exactly such a leaf, and it is often the single
   *     most expensive object in a frame. Leaves are measured.
   *  3. A node whose only volatility is a LIVE MATERIAL that has not
   *     actually moved since the bake. `Material::quantizeTime(hz)` steps
   *     its uniforms hz times a second, so most frames resolve to the SAME
   *     shader and the previous bake is still exact. A material bound to a
   *     continuous Output resolves to a new shader every frame, never
   *     reaches that stability, and stays live — the library measures
   *     which it is rather than assuming.
   *
   *  Re-baking is not free, so a node holds its promotion only while it is
   *  actually stable: a bake per frame would cost more than the replay it
   *  replaced.
   *
   *  IT MUST NOT CHANGE A PIXEL, and that is enforced structurally rather
   *  than hoped for: promotion is refused unless the node maps to device
   *  space with no rotation, mirroring or skew, and the bake is then taken
   *  in DEVICE space at an integer-snapped rect and blitted back with the
   *  matrix reset and no resampling. An integer device-space translation
   *  cannot alter rasterisation, so the blit is a literal copy of the
   *  pixels the live paint would have produced. Anything outside that
   *  envelope keeps painting as it did.
   *
   *  The refusals that look most like missed wins are the honest ones. A
   *  leaf at `opacity(0.13).blend(kSoftLight)` — the paper-grain idiom,
   *  and often the most expensive node in a tree — cannot be promoted:
   *  compositing a bake applies the alpha to an already-rounded 8-bit
   *  colour, while the direct draw applies it to the shader's float
   *  output, and the two agree only to within 1 LSB. Ask for that one
   *  yourself with `.cache(Cache::Texture)` — an author who types it has
   *  accepted the rounding; the library will not accept it on your behalf.
   *
   *  Why a given node was or was not promoted is reported per node as
   *  NodeCost::promotion.
   *
   *  Opting out: globally here, or per node with `.cache(Cache::Picture)`,
   *  which means "record, and never promote". `Cache::Texture` is the
   *  opposite opt-in and is unaffected. */
  void setAutoTexturePromotion(bool on);
  bool autoTexturePromotion() const;

  /** @private */
  struct Impl;

 private:
  friend struct detail::Instance;
  friend sk_sp<SkPicture> snapshot(Element, sigil::weave::FontContext&, SkSize);
  friend SkSize measure(Element, sigil::weave::FontContext&, SkSize);
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::compose
