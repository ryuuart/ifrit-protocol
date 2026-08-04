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

static_assert(kFieldCount<Transition> == 3,
              "Transition gained or lost a field — rule on it in "
              "transitionEqual() below, then bump this count.");
bool transitionEqual(const Transition &a, const Transition &b) {
  return a.duration == b.duration && a.delay == b.delay &&
         easeEqual(a.easing(), b.easing()); // `ease` is read through easing()
}

} // namespace

namespace detail {

/** Shaped bindings prune like anything else: same Output, same affine,
 *  same curve under easeEqual's conservative rule. A re-describe that
 *  only changes the CURVE must NOT prune — the map is read live, so a
 *  pruned node would keep shaping through the old one forever.
 *
 *  EVERY FIELD OF BoundFloat MUST APPEAR HERE. The failure of an omission
 *  is invisible: two different shapings compare equal, the node prunes,
 *  and the instance keeps applying the OLD map forever while every
 *  existing test still passes. The wiggle() fields (2026-07-29) are the
 *  most recent five.
 *
 *  Three gates keep it honest, and they are the model for every
 *  hand-written comparator in this file (see ComposeInternal.h's FIELD
 *  PINS block):
 *    1. the `kFieldCount` assert below — adding a field to BoundFloat does
 *       not compile until someone bumps the count HERE, next to the list;
 *    2. `ComposeReconcile.EveryBoundFloatFieldParticipatesInEquality` —
 *       the field walk, which perturbs each tied field in turn and demands
 *       this function say false, so a new field is covered the moment it
 *       is named in `fields()`;
 *    3. `ComposeReconcile.WiggledBindingsPruneOnlyWhenEveryParameterMatches`
 *       — the end-to-end pin, through a real re-describe of the SAME node,
 *       for the five wiggle fields. */
static_assert(kFieldCount<BoundFloat> == 17,
              "BoundFloat gained or lost a field. boundMapEqual() below "
              "compares it BY HAND: rule on the new field (participate, or "
              "a stated reason not to), then bump this count. A miss is "
              "silent — the node prunes and keeps shaping through the old "
              "map forever.");
bool boundMapEqual(const BoundFloat &a, const BoundFloat &b) {
  return a.source == b.source && a.inScale == b.inScale &&
         a.inOffset == b.inOffset && a.clampInput == b.clampInput &&
         a.steps == b.steps && a.scale == b.scale &&
         a.offset == b.offset && a.clamped == b.clamped && a.lo == b.lo &&
         a.hi == b.hi && a.wiggleAmount == b.wiggleAmount &&
         a.wiggleFrequency == b.wiggleFrequency &&
         a.wiggleSeed == b.wiggleSeed && a.wiggleOctaves == b.wiggleOctaves &&
         a.wiggleFalloff == b.wiggleFalloff &&
         a.wrapPeriod == b.wrapPeriod && easeEqual(a.curve, b.curve);
}

} // namespace detail

namespace {

static_assert(kFieldCount<Transitioned<float>> == 4,
              "Transitioned gained or lost a field — rule on it in "
              "propEqual() below, then bump this count.");
template <typename T>
bool propEqual(const Animatable<T> &a, const Animatable<T> &b) {
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
  // Structural (Effect::operator==): static shader recipes compare by
  // (runtime effect, constant uniforms) so a re-described effect prunes
  // when the caller holds one SkRuntimeEffect; live effects and filter()
  // pointer changes stay conservatively unequal.
  return *a == *b;
}

// ---- block equality (presence must match; then contents, preserving the
// monolith's exact semantics — callables stay conservatively unequal) ----

static_assert(kFieldCount<TextData> == 12,
              "TextData gained or lost a field — rule on it in textEqual() "
              "below, then bump this count. (`layoutOptions` is the one "
              "field compared in PART, and only because the full-control "
              "overload that can set the rest also sets paragraphOverride, "
              "which is unconditionally conservative.)");
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
  // VariationDrive: BINDING identity, like Animatable bindings — same tag
  // and same Output pointer prune (the value lives outside the tree).
  if (std::memcmp(ta.driveTag, tb.driveTag, 4) != 0 ||
      ta.driveValue != tb.driveValue)
    return false;
  if (ta.utf8 != tb.utf8 || !(ta.style == tb.style))
    return false;
  if (ta.hasTextStroke != tb.hasTextStroke ||
      (ta.hasTextStroke && (ta.textStrokeWidth != tb.textStrokeWidth ||
                            !(ta.textStrokeFill == tb.textStrokeFill))))
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
  // onPath(): the baseline is a Shape, so a run on a comparable generator
  // prunes — 72 radial labels used to re-record every render() for want
  // of this compare (§10e). A raw-callable baseline makes the Shape
  // compare false and keeps the old conservative rule.
  if (ta.onPath.has_value() != tb.onPath.has_value())
    return false;
  if (ta.onPath && !(*ta.onPath == *tb.onPath))
    return false;
  // textFill(): live never prunes, static compares by recipe.
  if (ta.metricFill.has_value() != tb.metricFill.has_value())
    return false;
  if (ta.metricFill) {
    if (ta.metricFill->isAnimated() || tb.metricFill->isAnimated())
      return false;
    if (!(*ta.metricFill == *tb.metricFill))
      return false;
  }
  return true;
}

static_assert(kFieldCount<DeriveData> == 15,
              "DeriveData gained or lost a field — rule on it in "
              "deriveEqual() below, then bump this count.");
bool deriveEqual(const Box<DeriveData> &a, const Box<DeriveData> &b) {
  if ((bool)a != (bool)b)
    return false;
  if (!a)
    return true;
  // Incomparable callables → conservative inequality. A band's authored
  // SPINE rides the Shape seam instead (same rule as shapeFn): comparable
  // generators prune, raw callables stay conservative. A band borrowed by
  // key was always a comparable value.
  if (a->placeFn || b->placeFn || a->router || b->router || a->railRouter ||
      b->railRouter)
    return false;
  if (!(a->bandSpine == b->bandSpine))
    return false;
  if (a->bandWidth.has_value() != b->bandWidth.has_value())
    return false;
  if (a->bandWidth && !(*a->bandWidth == *b->bandWidth))
    return false;
  return a->railAnchors == b->railAnchors &&
         a->flowAroundKeys == b->flowAroundKeys &&
         a->flowAroundMargin == b->flowAroundMargin &&
         a->connectFrom == b->connectFrom && a->connectTo == b->connectTo &&
         a->connectorGap == b->connectorGap &&
         a->bandAround == b->bandAround &&
         a->bandFormation == b->bandFormation &&
         a->spanFitKeys == b->spanFitKeys &&
         a->borrowedPathKeys == b->borrowedPathKeys;
}

static_assert(kFieldCount<StrokeData> == 1 && kFieldCount<StrokePass> == 4,
              "StrokeData/StrokePass gained or lost a field — rule on it in "
              "strokeEqual() below, then bump this count.");
bool strokeEqual(const Box<StrokeData> &a, const Box<StrokeData> &b) {
  if ((bool)a != (bool)b)
    return false;
  if (!a)
    return true;
  if (a->passes.size() != b->passes.size())
    return false;
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
              "below (Mask::operator== is in Compose.h), then bump this "
              "count.");
bool fxEqual(const Box<FxData> &a, const Box<FxData> &b) {
  if ((bool)a != (bool)b)
    return false;
  if (!a)
    return true;
  if (a->echoes != b->echoes)
    return false;
  if (a->staggerChildrenMs != b->staggerChildrenMs ||
      a->staggerFrom != b->staggerFrom)
    return false;
  // The masking family. A mask is read LIVE every frame, so it participates
  // in reconciler equality or a pruned node reveals to its first frame and
  // stays there — §33's comparable-values law, and the reason the shape
  // gate takes a Region value instead of an outline generator.
  if (a->masks.size() != b->masks.size())
    return false;
  for (size_t i = 0; i < a->masks.size(); ++i)
    if (!(a->masks[i] == b->masks[i]))
      return false;
  if (a->markNames != b->markNames)
    return false;
  if (a->overlays.size() != b->overlays.size())
    return false;
  for (size_t i = 0; i < a->overlays.size(); ++i)
    if (!(a->overlays[i] == b->overlays[i]))
      return false;
  return effectEqual(a->layerEffect, b->layerEffect) &&
         effectEqual(a->backdropEffect, b->backdropEffect);
}

static_assert(kFieldCount<MaterialData> == 2,
              "MaterialData gained or lost a field — rule on it in "
              "materialEqual() below (or in propsEqual, which owns the "
              "->recipe half), then bump this count.");
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
    if (a->live->isAnimated() || b->live->isAnimated())
      return false;
    if (!(*a->live == *b->live))
      return false;
  }
  return true; // ->recipe is handled with the fill compare in propsEqual
}

} // namespace

/** A Spans value compares like any other description — and its animated
 *  endpoints compare through the SAME comparator every animated property
 *  uses, which is why this body lives here rather than in the header.
 *  (§33's comparable-values law: anything an author hands the library
 *  participates in reconciler equality, or a pruned node reads a stale
 *  reveal forever.)
 *
 *  The endpoint trio is compared only for the two rules that READ it
 *  (Spans::resolve consults `values[3i..3i+2]` under Range and Wrap and
 *  nowhere else); every other field is unconditional. */
static_assert(kFieldCount<Spans::Term> == 11,
              "Spans::Term gained or lost a field — rule on it below, then "
              "bump this count. A term field left out makes every claim of "
              "that shape compare equal to every other one.");
bool Spans::operator==(const Spans &other) const {
  if (terms.size() != other.terms.size())
    return false;
  const auto termEqual = [](const Term &a, const Term &b) {
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
        (!propEqual(a.begin, b.begin) || !propEqual(a.end, b.end) ||
         !propEqual(a.offset, b.offset)))
      return false;
    return true;
  };
  // ORDER-INSENSITIVE (§33): `corners(8) | at(0,4)` and `at(0,4) |
  // corners(8)` claim the same runs — resolve() unions, it never reads
  // term order — so a describe that reorders terms must PRUNE, not patch.
  // (A retained node keeps ITS OWN term order and the values array paired
  // with it, so pruning across a reorder replays correct pixels.) The
  // multiset match is greedy-with-used-flags, which is exact because term
  // equality is an equivalence; the in-order fast path keeps the common
  // identical describe at one pass.
  size_t inOrder = 0;
  while (inOrder < terms.size() &&
         termEqual(terms[inOrder], other.terms[inOrder]))
    ++inOrder;
  if (inOrder == terms.size())
    return true;
  std::vector<bool> used(terms.size(), false);
  for (size_t i = inOrder; i < terms.size(); ++i) {
    bool matched = false;
    for (size_t j = inOrder; j < other.terms.size(); ++j) {
      if (used[j] || !termEqual(terms[i], other.terms[j]))
        continue;
      used[j] = true;
      matched = true;
      break;
    }
    if (!matched)
      return false;
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
static_assert(kFieldCount<Gate> == 8,
              "Gate gained or lost a field — rule on it below (in the arm "
              "of the Kind that reads it), then bump this count.");
bool Gate::operator==(const Gate &other) const {
  if (kind != other.kind)
    return false;
  switch (kind) {
  case Kind::Spans:
    return where == other.where;
  case Kind::Edge:
    return angleDeg == other.angleDeg && propEqual(fraction, other.fraction);
  case Kind::Shape:
    return outside == other.outside && region == other.region;
  case Kind::Coverage:
    if (outside != other.outside || channel != other.channel)
      return false;
    if ((bool)coverage != (bool)other.coverage)
      return false;
    if (!coverage)
      return true;
    if (coverage->isAnimated() || other.coverage->isAnimated())
      return false;
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
 *  (env snapshot + the author's own props comparator) and never reaches
 *  here, because `inst.desc` holds the memo's PRODUCED payload; and
 *  `children` are reconciled by key rather than compared — a node that
 *  prunes still walks them. */
static_assert(kFieldCount<ElementNode> == 23 && kFieldCount<PaintProps> == 15 &&
                  kFieldCount<ImageData> == 3 &&
                  kFieldCount<CustomData> == 2 &&
                  kFieldCount<MotionPath> == 3 && kFieldCount<Fill> == 3,
              "A struct propsEqual() compares BY HAND gained or lost a "
              "field. Rule on it below — participate, or a stated reason "
              "not to — then bump this count. A miss is silent: the node "
              "prunes, markPaintDirtyUp() never runs, a stale picture "
              "replays, and applyTransitions() never ramps an animate() on "
              "it. That is exactly how scaleX/scaleY were lost from the day "
              "they landed until e37d58d.");
bool propsEqual(const ElementNode &a, const ElementNode &b) {
  if (a.kind != b.kind || a.key != b.key)
    return false;
  // Incomparable callables → conservative inequality.
  if (a.hitTestable != b.hitTestable)
    return false;
  if ((bool)a.customData != (bool)b.customData)
    return false;
  if (a.customData) {
    // custom(key): equal non-empty keys assert equal programs (the
    // author's contract, like a keyed parametric); unkeyed stays
    // conservatively unequal.
    if (a.customData->key.empty() || a.customData->key != b.customData->key)
      return false;
  }
  // The shape seam: a comparable scheme (any shapes:: generator) prunes;
  // the raw-callable escape hatch compares unequal and stays conservative.
  // This WAS a blanket refusal — the §3 wall, 43.4 of 43.5 ms measured on
  // one node whose outline could not compare.
  if (!(a.shapeFn == b.shapeFn))
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
  if (!strokeEqual(a.strokeData, b.strokeData))
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
      // scaleX/scaleY were MISSING from this list since they landed: two
      // descriptions differing only in a per-axis scale compared EQUAL,
      // so the patch pruned, the node was never marked paint-dirty, and a
      // bar re-described at a new width kept the old one's picture (and an
      // animate() on scaleX never ramped, since applyTransitions only runs
      // inside the `own` branch). Found by the travel() equality audit.
      !propEqual(pa.scaleX, pb.scaleX) || !propEqual(pa.scaleY, pb.scaleY) ||
      !propEqual(pa.skewX, pb.skewX) || !propEqual(pa.skewY, pb.skewY) ||
      pa.originX != pb.originX || pa.originY != pb.originY ||
      pa.originPx != pb.originPx || pa.zIndex != pb.zIndex)
    return false;
  // travel(): a motion path is read live at paint, so it participates in
  // reconciler equality — every field, or a change to one of them would
  // prune into its predecessor. `path` carries the shape seam's own
  // contract (a comparable scheme prunes; the raw-callable escape hatch
  // never compares equal), `t` compares as any Animatable lane does, and
  // `lookAhead` is a plain float that changes the ORIENTATION and so
  // cannot be left out — the wiggle-wave trap, one field at a time.
  if ((bool)a.motionData != (bool)b.motionData)
    return false;
  if (a.motionData &&
      (!(a.motionData->path == b.motionData->path) ||
       !propEqual(a.motionData->t, b.motionData->t) ||
       a.motionData->lookAhead != b.motionData->lookAhead))
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

} // namespace detail

// ---------------------------------------------------------------------------

std::shared_ptr<ElementNode>
Composer::Impl::resolveMemo(Instance *existing,
                            const std::shared_ptr<ElementNode> &node,
                            bool &described) {
  if (!node->isMemo()) {
    described = true;
    return node;
  }
  // A memo is a pure function of (props, ENVIRONMENT). The environment is
  // compared first and for the same reason props are: an `env::` binding
  // is read live by the deferred describe, so a memo that hit on props
  // alone would serve the theme it first described under forever — the
  // "anything read live must participate in reconciler equality" law
  // (DESIGN, Growth rules), applied to the one deferred describe there is.
  if (existing && existing->memoShell &&
      envEqual(existing->memoShell->memoData->env, node->memoData->env) &&
      existing->memoShell->memoData->equal(existing->memoShell->memoData->props,
                                           node->memoData->props)) {
    stats.memoHits++;
    described = false;
    return existing->desc; // reuse the previously described payload
  }
  described = true;
  // …and the deferred call runs under the bindings its AUTHOR had, not
  // whatever scope this reconcile happens to sit inside (usually none).
  EnvRestore restore(node->memoData->env);
  Element produced = node->memoData->invoke(node->memoData->props);
  return produced.node();
}

std::unique_ptr<Instance>
Composer::Impl::mount(const std::shared_ptr<ElementNode> &node,
                      Instance *parent) {
  auto inst = std::make_unique<Instance>();
  inst->owner = this;
  inst->parent = parent;
  // Positioned subtrees skip Yoga entirely: a child of a positioned()
  // container — and everything below it — carries its rect in its own
  // description, resolved by instanceRect() with no flex engine behind
  // it. The container itself keeps its node (it lives in its parent's
  // flow); its descendants never get one.
  const bool positionedChild =
      parent && (!parent->yoga ||
                 (parent->desc && parent->desc->layout.positioned));
  if (!positionedChild) {
    inst->yoga = YGNodeNewWithConfig(yogaConfig);
    YGNodeSetContext(inst->yoga, inst.get());
  }
  patch(*inst, node);
  return inst;
}

namespace {

/** §19: does anything this description paints anchor to the composer root
 *  (Material::worldSpace)? Every seam that resolves a Material against the
 *  node's PaintContext is walked — the fill slot, textFill's metric
 *  material, both effects' child materials, the mask coverage materials —
 *  and the answer lands on the INSTANCE as one bool, so the per-relayout
 *  syncLayoutRects walk and the per-frame volatility walk never re-walk a
 *  material tree. */
bool nodeUsesWorldSpace(const ElementNode &n) {
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
    for (const Mask &m : n.fxData->masks)
      if (m.with.coverage && m.with.coverage->usesWorldSpace())
        return true;
  }
  return false;
}

/** §19: did the DESCRIBED transform change between two descriptions? A
 *  re-described static rotation on an ancestor moves every descendant's W
 *  while the descendants themselves prune — no rect changes (layout is
 *  untouched), no binding is connected (volatility never fires), so
 *  neither of the other two movement classes catches it. The patch asks
 *  this and stales the world-space descendants by hand. Lanes mirror
 *  propsEqual's transform block plus travel() (which replaces the
 *  translate lanes and adds to rotate). */
bool describedTransformEqual(const ElementNode &a, const ElementNode &b) {
  const PaintProps &pa = a.paint, &pb = b.paint;
  if (!propEqual(pa.translateX, pb.translateX) ||
      !propEqual(pa.translateY, pb.translateY) ||
      !propEqual(pa.rotate, pb.rotate) || !propEqual(pa.scale, pb.scale) ||
      !propEqual(pa.scaleX, pb.scaleX) || !propEqual(pa.scaleY, pb.scaleY) ||
      !propEqual(pa.skewX, pb.skewX) || !propEqual(pa.skewY, pb.skewY) ||
      pa.originX != pb.originX || pa.originY != pb.originY ||
      pa.originPx != pb.originPx)
    return false;
  if ((bool)a.motionData != (bool)b.motionData)
    return false;
  if (a.motionData &&
      (!(a.motionData->path == b.motionData->path) ||
       !propEqual(a.motionData->t, b.motionData->t) ||
       a.motionData->lookAhead != b.motionData->lookAhead))
    return false;
  return true;
}

/** §19: mark every world-space-carrying descendant's OWN paint dirty —
 *  their recordings baked a W the caller just changed. */
void staleWorldSpaceBelow(Instance &inst) {
  for (auto &child : inst.children) {
    if (child->hasWorldSpaceMaterial)
      child->markPaintDirtyUp();
    staleWorldSpaceBelow(*child);
  }
}

} // namespace

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

    // §19: the instance flag, once per patch (a pruned node keeps its
    // flag — equal props mean equal materials); and the third movement
    // class: a changed DESCRIBED transform moves every descendant's W
    // while those descendants prune, so the world-space ones are staled
    // by hand here (layout moves ride syncLayoutRects, bound transforms
    // ride the volatility walk).
    inst.hasWorldSpaceMaterial = nodeUsesWorldSpace(*resolved);
    if (prev && !describedTransformEqual(*prev, *resolved))
      staleWorldSpaceBelow(inst);

    // Kind change → full remount of content state.
    const bool kindChanged = prev && prev->kind != resolved->kind;
    if (kindChanged) {
      inst.paragraph.reset();
      inst.lines.clear();
      if (inst.yoga)
        YGNodeSetMeasureFunc(inst.yoga, nullptr);
    }

    applyLayoutProps(inst);
    // centerAt lives outside Yoga's style set (resolved in ensureLayout's
    // second pass), so a moved pin must force the layout pass itself.
    if (!prev || prev->layout.centerAt != resolved->layout.centerAt)
      needsLayout = true;
    // A positioned child's rect IS its layout props: a change must run
    // the layout pass so syncLayoutRects stales the recordings that
    // baked the old rect (the job Yoga's dirty bit does elsewhere).
    if (!inst.yoga && prev && !(prev->layout == resolved->layout))
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
        if (inst.yoga) {
          YGNodeSetMeasureFunc(inst.yoga, measureTextNode);
          YGNodeSetBaselineFunc(inst.yoga, baselineOfTextNode);
          YGNodeSetNodeType(inst.yoga, YGNodeTypeText);
          YGNodeMarkDirty(inst.yoga);
        }
        needsLayout = true;
      }
    }

    if (prev)
      applyTransitions(inst, *prev, *resolved);
    else
      applyMountTransitions(inst, *resolved); // animate(from().to()) entrances

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
  // Whether children of THIS parent carry Yoga nodes; a mismatch on a
  // reused instance (the container toggled positioned()) forces a fresh
  // mount below, because a Yoga node's existence is fixed at mount.
  const bool childrenWantYoga =
      inst.yoga != nullptr && !inst.desc->layout.positioned;
  for (auto &child : inst.children) {
    if (child) {
      oldOrder.push_back(child.get());
      if (inst.yoga && child->yoga)
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
    if (match && (match->yoga != nullptr) != childrenWantYoga)
      match.reset(); // unmounts; the fresh mount below picks the right mode

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
    // Stack children overlap: EVERY child is absolute, unconditionally.
    // This runs after mount()/patch() applied the child's own layout props,
    // so it is the last write and a child cannot opt out — which is the
    // container's contract, not an oversight: a stack whose children could
    // individually rejoin the flex flow would lay out as neither. What a
    // child DOES keep is its insets — `.top(12).right(12)` inside a stack
    // pins that corner, because absolute is exactly the mode insets need.
    if (placed.yoga) {
      if (inst.desc->kind == Kind::Stack)
        YGNodeStyleSetPositionType(placed.yoga, YGPositionTypeAbsolute);
      YGNodeInsertChild(inst.yoga, placed.yoga,
                        YGNodeGetChildCount(inst.yoga));
    }
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
  if (!inst.yoga)
    return; // positioned subtree: instanceRect() reads the props directly
  const LayoutProps &l = inst.desc->layout;
  YGNodeRef n = inst.yoga;

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

  // The node's OWN position type. A stack child's is overwritten right
  // after this, in patchChildren() — see the note there.
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
  bySlot.clear();
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
  if (inst.desc->kind == Kind::Slot && !inst.desc->key.empty())
    bySlot[inst.desc->key] = &inst;
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
    // A borrowed band spine and a spans::fit() gap are the same kind of
    // question a connector asks — "where did that keyed node land" — so
    // they ride the SAME flat derive list rather than growing a phase.
    const bool isBorrowed = !derive.bandAround.empty() ||
                            !derive.spanFitKeys.empty() ||
                            !derive.borrowedPathKeys.empty();
    if (isBorrowed && !isConnector && !isRail)
      routedInstances.push_back(&inst);
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
