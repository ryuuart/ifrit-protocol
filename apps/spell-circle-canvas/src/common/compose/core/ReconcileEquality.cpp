/** @file
 * The structural-equality comparators the identity prune is built from.
 *
 * This is the library's entire correctness surface, and it is worth
 * understanding before changing anything here. Two descriptions that
 * compare EQUAL cause the node to prune: nothing is marked dirty, no
 * transition is applied, and the recording made under the previous
 * description replays as-is. So a field left out of a comparator does not
 * produce a wrong pixel at the point of the mistake — it produces a stale
 * picture, indefinitely, on a node whose description genuinely changed, with
 * every existing test still passing. That is why the comparators here are
 * hand-written and each is guarded by a static assertion on its struct's
 * field count: adding a field must fail the build and force a decision,
 * because it will not fail anything else.
 *
 * The conservative rule that makes this tractable: anything holding a
 * callable the library cannot compare (custom programs, raw outline
 * lambdas, raw route lambdas, custom layouts) compares UNEQUAL and never
 * prunes. Memoization is the tool for those.
 */

#include <algorithm>
#include <cstring>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

// ---- structural equality ---------------------------------------------------
// Equal only when provably identical. Anything carrying a callable the
// library cannot compare (custom programs, decorations, outlines, routes,
// custom layouts) compares unequal and re-patches every describe; the common
// plain cases (boxes, fills, text runs, images) prune for free.

// Transition, BoundFloat and Animatable compare through SigilCore's
// comparators (transitionEqual, boundMapEqual, propertyEqual), each pinned
// beside its body there; the Effect and the blocks below are this
// library's own.

bool effectEqual(const std::optional<material::skia::Effect>& a,
                 const std::optional<material::skia::Effect>& b) {
  if (a.has_value() != b.has_value()) return false;
  if (!a) return true;
  // Structural (Effect::operator==): static shader recipes compare by
  // (runtime effect, constant uniforms) so a re-described effect prunes
  // when the caller holds one SkRuntimeEffect; live effects and filter()
  // pointer changes stay conservatively unequal.
  return *a == *b;
}

// ---- block equality: presence must match first, then contents; a block
// holding a callable stays conservatively unequal ---------------------------

static_assert(kFieldCount<TextPath> == 7,
              "TextPath gained or lost a field — rule on it in "
              "textPathEqual() below, then bump this count. The comparator "
              "is hand-written at all because `at` is an Animatable, and "
              "an Animatable is compared where every other animated slot "
              "is: through propertyEqual.");
bool textPathEqual(const TextPath& a, const TextPath& b) {
  return a.path == b.path && propertyEqual(a.at, b.at) && a.align == b.align &&
         a.offset == b.offset && a.autoFlip == b.autoFlip &&
         a.orient == b.orient && a.exactTangent == b.exactTangent;
}

static_assert(kFieldCount<TextData> == 19 && kFieldCount<TextOptions> == 21 &&
                  kFieldCount<SpanRestyle> == 3,
              "TextData gained or lost a field — rule on it in textEqual() "
              "below, then bump this count. (`layoutOptions` is the one "
              "field NOT compared, and only because the full-control "
              "overload that sets it also sets paragraphOverride, which is "
              "unconditionally conservative. The fluent setters are "
              "comparable in full and live in `options`. `painter` is "
              "EXCLUDED on purpose: it is the same engine on every text "
              "that carries one, so it says nothing about the picture.)");
bool textEqual(const ElementNode& a, const ElementNode& b) {
  if ((bool)a.textData != (bool)b.textData) return false;
  if (!a.textData) return true;
  const TextData &ta = *a.textData, &tb = *b.textData;
  // fx() tracks are comparable VALUES — selector, effect (preset id plus
  // parameters, or the key an ad-hoc lambda was given), cascade, reach and
  // the continuous opt-out — so text that re-describes the same tracks
  // prunes like any other static leaf. The progress is an Animatable and is
  // compared where every other animated slot is, through propertyEqual.
  //
  // variationDrive()'s track rides this comparison too: its effect's key
  // carries the axis tag AND the driven Output's address, so a re-describe
  // naming the same drive prunes and one naming another does not — the
  // binding identity every bound value in the tree is compared by.
  if (ta.tracks.size() != tb.tracks.size()) return false;
  for (size_t i = 0; i < ta.tracks.size(); ++i)
    if (!ta.tracks[i].sameShape(tb.tracks[i]) ||
        !propertyEqual(ta.tracks[i].progress, tb.tracks[i].progress))
      return false;
  if (ta.utf8 != tb.utf8 || !(ta.style == tb.style)) return false;
  // weave::rich(): a whole mixed paragraph as one comparable value — same base,
  // same runs, same resolved styles — so a component that rebuilds its
  // spans every describe prunes like a static leaf. This is exactly what
  // the shared_ptr<Paragraph> overload below cannot answer.
  if (!(ta.rich == tb.rich)) return false;
  if (ta.hasTextStroke != tb.hasTextStroke ||
      (ta.hasTextStroke && (ta.textStrokeWidth != tb.textStrokeWidth ||
                            !(ta.textStrokeFill == tb.textStrokeFill))))
    return false;
  // The fluent layout-option setters ARE comparable — value plus the mask
  // of which fields were written — so a changed alignment, break strategy,
  // clamp or ellipsis patches on every content form.
  if (a.kind == Kind::Text && !(ta.options == tb.options)) return false;
  // spanPaint()/spanStyle(): comparable selectors and comparable styles, in
  // declaration order, so a re-described restyle list prunes and a changed
  // one re-resolves.
  if (ta.spanRestyles != tb.spanRestyles) return false;
  if (ta.paragraphOverride != tb.paragraphOverride) return false;
  if (ta.paragraphOverride)
    return false;  // layoutOptions aren't comparable — memo these
  // onPath(): the baseline is a Shape, so a run laid on a comparable
  // generator prunes like any other static description — which matters
  // because a ring of labels is one text node per label, all re-recording
  // together. A raw-callable baseline makes the Shape compare false and
  // falls back to never pruning.
  if (ta.onPath.has_value() != tb.onPath.has_value()) return false;
  if (ta.onPath && !textPathEqual(*ta.onPath, *tb.onPath)) return false;
  // textFill(): live never prunes, static compares by recipe.
  if (ta.metricFill.has_value() != tb.metricFill.has_value()) return false;
  if (ta.metricFill) {
    if (ta.metricFill->isAnimated() || tb.metricFill->isAnimated())
      return false;
    if (!(*ta.metricFill == *tb.metricFill)) return false;
  }
  // mark(): a comparable selector and the key of the child it anchors, in
  // declaration order — so a re-described mark list prunes, and a mark
  // pointed at a different unit re-resolves its rect.
  if (ta.marks != tb.marks) return false;
  // annotate(): comparable selectors, readings and styles in declaration
  // order — so a re-described reading list prunes, and a changed reading
  // re-lays the small paragraph it is set in AND, where it reserves, the
  // base whose strut its band is in.
  if (ta.annotations != tb.annotations) return false;
  // thread(): the key of the next frame. A chain that re-describes the
  // same links prunes; one that names a different frame re-fills from
  // there.
  if (ta.threadTo != tb.threadTo) return false;
  // balanceChain(): which frame opens a balanced run, and how much of the
  // story that run must hold. Both change the depth the run is filled to,
  // so a re-described chain that states either differently re-bisects.
  if (ta.balanceChain != tb.balanceChain ||
      ta.balanceThroughLine != tb.balanceThroughLine)
    return false;
  return true;
}

static_assert(kFieldCount<DeriveData> == 19,
              "DeriveData gained or lost a field — rule on it in "
              "deriveEqual() below, then bump this count.");
bool deriveEqual(const Box<DeriveData>& a, const Box<DeriveData>& b) {
  if ((bool)a != (bool)b) return false;
  if (!a) return true;
  // Incomparable callables → conservative inequality. Custom layout is
  // the one left: a band's authored SPINE rides the Shape seam and both
  // ROUTERS ride seams of their own (same rule as shapeFn), so a
  // comparable value prunes and only a raw callable stays conservative.
  // A band borrowed by key was always a comparable value.
  if (a->placeFn || b->placeFn) return false;
  if (!(a->router == b->router)) return false;
  if (!(a->railRouter == b->railRouter)) return false;
  if (!(a->bandSpine == b->bandSpine)) return false;
  if (a->bandWidth.has_value() != b->bandWidth.has_value()) return false;
  if (a->bandWidth && !(*a->bandWidth == *b->bandWidth)) return false;
  // `reads` is EXCLUDED, and the exclusion is a derivation rather than a
  // judgement call: every entry is pushed by the same statement that
  // writes one of the fields compared above (or `TextData::threadTo`,
  // compared with the text block), so two descriptions that differ in a
  // read differ in the field that produced it and are already unequal.
  // A verb that ever declared a read WITHOUT storing the key behind it
  // would break that, and would have to be compared here.
  // area(): the region name a child claims of the scheme above it. Two
  // descriptions that name different regions place the child differently
  // and must not prune into each other. `placeReadsMinSizes` needs no rule
  // of its own: it is a property of the scheme type behind `placeFn`, and
  // a node carrying one is already conservatively unequal above.
  if (a->cellArea != b->cellArea) return false;
  // tether(): where the node hangs and everywhere it may hang instead. A
  // re-described tether that names the same places and the same points
  // prunes; one that moves either re-resolves the position.
  if (a->tether != b->tether) return false;
  return a->railAnchors == b->railAnchors &&
         a->flowAroundKeys == b->flowAroundKeys &&
         a->flowAroundMargin == b->flowAroundMargin &&
         a->connectFrom == b->connectFrom && a->connectTo == b->connectTo &&
         a->connectorGap == b->connectorGap && a->bandAround == b->bandAround &&
         a->bandFormation == b->bandFormation &&
         a->spanFitKeys == b->spanFitKeys &&
         a->borrowedPathKeys == b->borrowedPathKeys;
}

static_assert(kFieldCount<StrokeData> == 2 && kFieldCount<StrokePass> == 4,
              "StrokeData/StrokePass gained or lost a field — rule on it in "
              "strokeEqual() below, then bump this count. `resolver` is "
              "EXCLUDED on purpose: the same engine rides every stroked "
              "node.");
bool strokeEqual(const Box<StrokeData>& a, const Box<StrokeData>& b) {
  if ((bool)a != (bool)b) return false;
  if (!a) return true;
  if (a->passes.size() != b->passes.size()) return false;
  for (size_t i = 0; i < a->passes.size(); ++i) {
    const StrokePass &x = a->passes[i], &y = b->passes[i];
    if (x.name != y.name || x.half != y.half || !(x.where == y.where) ||
        !(x.what == y.what))
      return false;
  }
  return true;
}

static_assert(kFieldCount<FxData> == 8 && kFieldCount<Mask> == 2,
              "FxData/Mask gained or lost a field — rule on it in fxEqual() "
              "below, where a mask compares by its own operator, then bump "
              "this count.");
bool fxEqual(const Box<FxData>& a, const Box<FxData>& b) {
  if ((bool)a != (bool)b) return false;
  if (!a) return true;
  if (a->echoes != b->echoes) return false;
  if (a->staggerChildrenMs != b->staggerChildrenMs ||
      a->staggerFrom != b->staggerFrom)
    return false;
  // A mask is read live at paint, so it must participate in this equality:
  // a mask change that pruned would leave the node showing whatever the
  // mask revealed on the frame the recording was made. This is also why the
  // shape gate takes a Region VALUE rather than an outline generator —
  // a generator could not be compared, and an uncomparable mask would make
  // every masked node re-patch forever.
  if (a->masks.size() != b->masks.size()) return false;
  for (size_t i = 0; i < a->masks.size(); ++i)
    if (!(a->masks[i] == b->masks[i])) return false;
  if (a->markNames != b->markNames) return false;
  if (a->overlays.size() != b->overlays.size()) return false;
  for (size_t i = 0; i < a->overlays.size(); ++i)
    if (!(a->overlays[i] == b->overlays[i])) return false;
  return effectEqual(a->layerEffect, b->layerEffect) &&
         effectEqual(a->backdropEffect, b->backdropEffect);
}

static_assert(kFieldCount<MaterialData> == 2,
              "MaterialData gained or lost a field — rule on it in "
              "materialEqual() below (or in propertiesEqual, which owns the "
              "->recipe half), then bump this count.");
bool materialEqual(const Box<MaterialData>& a, const Box<MaterialData>& b) {
  if ((bool)a != (bool)b) return false;
  if (!a) return true;
  // Material-slot fills: truly live ones (bound/uTime) never prune —
  // conservative, like an incomparable callable. Geometry-dependent-but-
  // static ones (SDF chrome and friends) compare by recipe, so identical
  // re-describes prune like any other static material.
  if (a->live.has_value() != b->live.has_value()) return false;
  if (a->live) {
    // A PAN-ONLY material (a bound offset, nothing else animated) is
    // exactly comparable: image identity, matrix, sampling, and the pan
    // binding compared by pointer all participate in Material::operator==.
    // So an identical re-describe prunes, and a re-BOUND pan patches —
    // which it must, because a pruned swap would leave the old Output
    // driving the pixels for the life of the instance. Everything else
    // that reports isAnimated() stays never-prune, below.
    const bool panOnlyA = a->live->boundOffsetOnly();
    const bool panOnlyB = b->live->boundOffsetOnly();
    if (panOnlyA != panOnlyB) return false;
    if (!panOnlyA && (a->live->isAnimated() || b->live->isAnimated()))
      return false;
    if (!(*a->live == *b->live)) return false;
  }
  return true;  // ->recipe is handled with the fill compare in propertiesEqual
}

static_assert(kFieldCount<DepthData> == 10,
              "DepthData gained or lost a field — rule on it in depthEqual() "
              "below, then bump this count. Every field here is read live at "
              "paint: a lane, an origin or a mode left out prunes into its "
              "predecessor and the plane keeps the turn it was recorded at.");
bool depthEqual(const Box<DepthData>& a, const Box<DepthData>& b) {
  if ((bool)a != (bool)b) return false;
  if (!a) return true;
  // The five lanes compare as every animated slot does, through propertyEqual;
  // the origins and the two modes are plain values.
  return propertyEqual(a->rotateX, b->rotateX) &&
         propertyEqual(a->rotateY, b->rotateY) &&
         propertyEqual(a->translateZ, b->translateZ) &&
         propertyEqual(a->scaleZ, b->scaleZ) &&
         propertyEqual(a->perspective, b->perspective) &&
         a->perspectiveOriginX == b->perspectiveOriginX &&
         a->perspectiveOriginY == b->perspectiveOriginY &&
         a->originZ == b->originZ && a->preserve3d == b->preserve3d &&
         a->backface == b->backface;
}

}  // namespace

/** A Spans value compares like any other description — and its animated
 *  endpoints compare through the SAME comparator every animated property
 *  uses, which is why this body lives here rather than in the header.
 *  Anything an author hands the library and the library then reads live has
 *  to participate in this equality, or a pruned node keeps replaying the
 *  reveal it was recorded with.
 *
 *  The endpoint trio is compared only for the two rules that READ it
 *  (Spans::resolve consults `values[3i..3i+2]` under Range and Wrap and
 *  nowhere else); every other field is unconditional. */
static_assert(kFieldCount<Spans::Term> == 11,
              "Spans::Term gained or lost a field — rule on it below, then "
              "bump this count. A term field left out makes every claim of "
              "that shape compare equal to every other one.");
bool Spans::operator==(const Spans& other) const {
  if (terms.size() != other.terms.size()) return false;
  const auto termEqual = [](const Term& a, const Term& b) {
    if (a.rule != b.rule || a.arm != b.arm || a.angleDeg != b.angleDeg ||
        a.duty != b.duty || a.margin != b.margin || a.count != b.count ||
        a.index != b.index || a.key != b.key)
      return false;
    // The two ENDPOINT-carrying rules. Leaving Wrap out here would make
    // every wrapped window compare equal to every other one and a
    // marching reveal would prune to its first frame forever. `offset`
    // rides with them for the same reason: it is a third live endpoint
    // term, and a claim that only slides would otherwise prune to its
    // first frame.
    if ((a.rule == Rule::Range || a.rule == Rule::Wrap) &&
        (!propertyEqual(a.begin, b.begin) || !propertyEqual(a.end, b.end) ||
         !propertyEqual(a.offset, b.offset)))
      return false;
    return true;
  };
  // ORDER-INSENSITIVE: `corners(8) | at(0,4)` and `at(0,4) | corners(8)`
  // claim the same runs — resolve() unions the terms and never reads their
  // order — so a describe that reorders terms must PRUNE, not patch.
  // (A retained node keeps ITS OWN term order and the values array paired
  // with it, so pruning across a reorder replays correct pixels.) The
  // multiset match is greedy-with-used-flags, which is exact because term
  // equality is an equivalence; the in-order fast path keeps the common
  // identical describe at one pass.
  size_t inOrder = 0;
  while (inOrder < terms.size() &&
         termEqual(terms[inOrder], other.terms[inOrder]))
    ++inOrder;
  if (inOrder == terms.size()) return true;
  std::vector<bool> used(terms.size(), false);
  for (size_t i = inOrder; i < terms.size(); ++i) {
    bool matched = false;
    for (size_t j = inOrder; j < other.terms.size(); ++j) {
      if (used[j] || !termEqual(terms[i], other.terms[j])) continue;
      used[j] = true;
      matched = true;
      break;
    }
    if (!matched) return false;
  }
  return true;
}

/** A Gate compares the same way, and for the same reason — with one extra
 *  clause worth naming. A COVERAGE gate holds a Material; a LIVE material
 *  (uTime or a bound uniform) never compares equal, exactly as a live
 *  material fill never does, because a shader that resolves per frame is
 *  not a value this frame can vouch for. A static one compares by recipe
 *  and prunes like any other.
 *
 *  Kind-scoped by construction — "only the members its Kind reads are
 *  meaningful", the class's own contract — so each arm names exactly the
 *  fields that arm resolves. `outside` is read by TWO arms — it is the one
 *  complement question ("which side of the show set?") asked of a region
 *  and of a coverage source — and leaving it out of either would make a
 *  matte compare equal to its own inverse, so a pruned node would keep
 *  showing the wrong half forever. */
static_assert(kFieldCount<Gate> == 9,
              "Gate gained or lost a field — rule on it below (in the arm "
              "of the Kind that reads it), then bump this count. `resolver` "
              "is EXCLUDED on purpose: every `by::` gate carries the same "
              "engine.");
bool Gate::operator==(const Gate& other) const {
  if (kind != other.kind) return false;
  switch (kind) {
    case Kind::Spans:
      return where == other.where;
    case Kind::Edge:
      return angleDeg == other.angleDeg &&
             propertyEqual(fraction, other.fraction);
    case Kind::Shape:
      return outside == other.outside && region == other.region;
    case Kind::Coverage:
      if (outside != other.outside || channel != other.channel) return false;
      if ((bool)coverage != (bool)other.coverage) return false;
      if (!coverage) return true;
      if (coverage->isAnimated() || other.coverage->isAnimated()) return false;
      return *coverage == *other.coverage;
  }
  return false;
}

namespace detail {

/** THE STRUCTURAL PRUNE. Every field of ElementNode is ruled on here, and
 *  every field of the three blocks it compares INLINE (PaintProps,
 *  ImageData, CustomData, MotionPath) with it; the rest delegate to the
 *  helpers above, each with its own pin.
 *
 *  The two legitimate exclusions, stated rather than assumed:
 *  `memoData` is compared EARLIER and more strictly by resolveMemo()
 *  (environment snapshot + the author's own properties comparator) and never
 * reaches here, because `inst.description` holds the memo's PRODUCED payload;
 * and `children` are reconciled by key rather than compared — a node that
 *  prunes still walks them. */
static_assert(kFieldCount<ElementNode> == 26 && kFieldCount<PaintProps> == 15 &&
                  kFieldCount<ImageData> == 3 && kFieldCount<CustomData> == 2 &&
                  kFieldCount<MotionPath> == 3 && kFieldCount<Fill> == 3,
              "A struct propertiesEqual() compares BY HAND gained or lost a "
              "field. Rule on it below — participate, or a stated reason "
              "not to — then bump this count. A miss is silent: the node "
              "prunes, markPaintDirtyUp() never runs, a stale picture "
              "replays, and applyTransitions() never ramps an animate() on "
              "it. Nothing else fails, so no test will catch it for you.");
bool propertiesEqual(const ElementNode& a, const ElementNode& b) {
  if (a.kind != b.kind || a.key != b.key) return false;
  // Incomparable callables → conservative inequality.
  if (a.hitTestable != b.hitTestable) return false;
  if ((bool)a.customData != (bool)b.customData) return false;
  if (a.customData) {
    // custom(key): equal non-empty keys assert equal programs (the
    // author's contract, like a keyed parametric); unkeyed stays
    // conservatively unequal.
    if (a.customData->key.empty() || a.customData->key != b.customData->key)
      return false;
  }
  // The shape seam: a comparable scheme (any shapes:: generator) prunes;
  // the raw-callable escape hatch compares unequal and stays conservative.
  // Worth keeping comparable — a node whose outline cannot compare never
  // prunes, so it re-records on every describe no matter how static it
  // looks, and an outline can be the most expensive thing on the node.
  if (!(a.shapeFn == b.shapeFn)) return false;
  if (!deriveEqual(a.deriveData, b.deriveData)) return false;
  // Decorations compare when they wrap value-comparable schemes (PathFormat,
  // Slice, Shadow…); an incomparable one (bare program, ContourWalk with a
  // draw lambda) makes Decoration::operator== false, so the node stays
  // conservative — static chrome prunes, live/opaque decorations don't.
  if (a.backgrounds.size() != b.backgrounds.size() ||
      a.foregrounds.size() != b.foregrounds.size())
    return false;
  for (size_t i = 0; i < a.backgrounds.size(); ++i)
    if (!(a.backgrounds[i] == b.backgrounds[i])) return false;
  for (size_t i = 0; i < a.foregrounds.size(); ++i)
    if (!(a.foregrounds[i] == b.foregrounds[i])) return false;
  if (!(a.layout == b.layout) || !(a.corners == b.corners) ||
      a.clipContent != b.clipContent || a.boundary != b.boundary ||
      a.coverageThreshold != b.coverageThreshold ||
      a.cacheMode != b.cacheMode || a.bakeScale != b.bakeScale)
    return false;
  if (!fxEqual(a.fxData, b.fxData)) return false;
  if (!strokeEqual(a.strokeData, b.strokeData)) return false;
  if (a.nodeTransition.has_value() != b.nodeTransition.has_value())
    return false;
  if (a.nodeTransition &&
      !transitionEqual(*a.nodeTransition, *b.nodeTransition))
    return false;
  // Paint.
  const PaintProps &pa = a.paint, &pb = b.paint;
  if (pa.fill.has_value() != pb.fill.has_value()) return false;
  if (!materialEqual(a.materialData, b.materialData)) return false;
  // Material-set fills compare by RECIPE — the structural signature of how
  // the material was built. Equal recipes mean interchangeable shaders even
  // though each describe minted a fresh SkShader, so a re-described gradient
  // prunes instead of being defeated by pointer inequality. Everything else
  // falls through to the plain fill compare (colour values, shader pointers).
  const material::skia::Paint* recipeA =
      a.materialData
          ? (a.materialData->recipe ? &*a.materialData->recipe : nullptr)
          : nullptr;
  const material::skia::Paint* recipeB =
      b.materialData
          ? (b.materialData->recipe ? &*b.materialData->recipe : nullptr)
          : nullptr;
  if ((recipeA != nullptr) != (recipeB != nullptr)) return false;
  if (recipeA) {
    if (!(*recipeA == *recipeB)) return false;
  } else if (pa.fill && !propertyEqual(*pa.fill, *pb.fill)) {
    return false;
  }
  if (!propertyEqual(pa.opacity, pb.opacity) || pa.blendMode != pb.blendMode ||
      !propertyEqual(pa.translateX, pb.translateX) ||
      !propertyEqual(pa.translateY, pb.translateY) ||
      !propertyEqual(pa.rotate, pb.rotate) ||
      !propertyEqual(pa.scale, pb.scale) ||
      // Every transform lane appears in this list, including the per-axis
      // scales. Omitting one makes two descriptions that differ only in
      // that lane compare equal, so the patch prunes, the node is never
      // marked paint-dirty, and it keeps the picture recorded at the old
      // value — and applyTransitions runs only inside the `own` branch
      // below, so an animate() on the missing lane never ramps either.
      !propertyEqual(pa.scaleX, pb.scaleX) ||
      !propertyEqual(pa.scaleY, pb.scaleY) ||
      !propertyEqual(pa.skewX, pb.skewX) ||
      !propertyEqual(pa.skewY, pb.skewY) || pa.originX != pb.originX ||
      pa.originY != pb.originY || pa.originPx != pb.originPx ||
      pa.zIndex != pb.zIndex)
    return false;
  // travel(): a motion path is read live at paint, so every one of its
  // fields participates here or a change to that field prunes into its
  // predecessor. `path` carries the shape seam's contract (a comparable
  // scheme prunes; the raw-callable escape hatch never compares equal),
  // `t` compares as any Animatable lane does, and `lookAhead` is a plain
  // float — easy to overlook precisely because it looks inert, and it
  // changes the node's ORIENTATION.
  if ((bool)a.motionData != (bool)b.motionData) return false;
  if (a.motionData && (!(a.motionData->path == b.motionData->path) ||
                       !propertyEqual(a.motionData->t, b.motionData->t) ||
                       a.motionData->lookAhead != b.motionData->lookAhead))
    return false;
  // The depth lanes, the view and the two modes: read live at paint
  // exactly as the 2D lanes are, and pinned beside depthEqual().
  if (!depthEqual(a.depthData, b.depthData)) return false;
  // Content.
  if (!textEqual(a, b)) return false;
  if ((bool)a.imageData != (bool)b.imageData) return false;
  if (a.imageData && (a.imageData->asset != b.imageData->asset ||
                      a.imageData->region != b.imageData->region ||
                      a.imageData->sampling != b.imageData->sampling))
    return false;
  return true;
}

/** Did the DESCRIBED transform change between two descriptions?
 *
 *  There are three ways a node's node-to-root matrix W can move, and this
 *  covers the one nothing else does. A re-described static rotation on an
 *  ancestor moves every descendant's W while those descendants themselves
 *  prune: no rect changed, so the layout walk sees nothing, and no binding
 *  is connected, so the volatility walk sees nothing either. The patch asks
 *  this question and stales the world-space descendants by hand.
 *
 *  The lanes must mirror propertiesEqual's transform block plus travel(), which
 *  replaces the translate lanes and adds to rotate, plus the depth block —
 *  a re-described turn about y, a view or a space mode moves every
 *  descendant's W as a 2D rotation does. A lane present there and missing
 *  here is a world-space material left on a stale W. */
bool describedTransformEqual(const ElementNode& a, const ElementNode& b) {
  const PaintProps &pa = a.paint, &pb = b.paint;
  if (!propertyEqual(pa.translateX, pb.translateX) ||
      !propertyEqual(pa.translateY, pb.translateY) ||
      !propertyEqual(pa.rotate, pb.rotate) ||
      !propertyEqual(pa.scale, pb.scale) ||
      !propertyEqual(pa.scaleX, pb.scaleX) ||
      !propertyEqual(pa.scaleY, pb.scaleY) ||
      !propertyEqual(pa.skewX, pb.skewX) ||
      !propertyEqual(pa.skewY, pb.skewY) || pa.originX != pb.originX ||
      pa.originY != pb.originY || pa.originPx != pb.originPx)
    return false;
  if ((bool)a.motionData != (bool)b.motionData) return false;
  if (a.motionData && (!(a.motionData->path == b.motionData->path) ||
                       !propertyEqual(a.motionData->t, b.motionData->t) ||
                       a.motionData->lookAhead != b.motionData->lookAhead))
    return false;
  return depthEqual(a.depthData, b.depthData);
}

}  // namespace detail

}  // namespace sigil::compose
