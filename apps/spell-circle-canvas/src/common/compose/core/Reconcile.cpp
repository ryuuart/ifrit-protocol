/** @file
 * Reconcile phase, the composer's half: the structural-equality comparators
 * the identity prune is built from, Yoga-style application of layout
 * props, text materialisation, and the key index with the edge store that
 * rides its walk. The reconciler itself — memo resolution, the prune,
 * keyed and positional matching — is SigilCore's, driven through the host
 * operations in ReconcileHost.cpp.
 *
 * The structural equality in this file is the library's entire correctness
 * surface, and it is worth understanding before changing anything here. Two
 * descriptions that compare EQUAL cause the node to prune: nothing is marked
 * dirty, no transition is applied, and the recording made under the previous
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
 * lambdas, routers, custom layouts) compares UNEQUAL and never prunes.
 * Memoization is the tool for those.
 */

#include <algorithm>
#include <cstring>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

YGAlign toYogaAlign(Align a) {
  switch (a) {
    case Align::Auto:
      return YGAlignAuto;
    case Align::Start:
      return YGAlignFlexStart;
    case Align::Center:
      return YGAlignCenter;
    case Align::End:
      return YGAlignFlexEnd;
    case Align::Stretch:
      return YGAlignStretch;
    case Align::Baseline:
      return YGAlignBaseline;
  }
  return YGAlignAuto;
}

YGJustify toYogaJustify(Justify j) {
  switch (j) {
    case Justify::Start:
      return YGJustifyFlexStart;
    case Justify::Center:
      return YGJustifyCenter;
    case Justify::End:
      return YGJustifyFlexEnd;
    case Justify::SpaceBetween:
      return YGJustifySpaceBetween;
    case Justify::SpaceAround:
      return YGJustifySpaceAround;
    case Justify::SpaceEvenly:
      return YGJustifySpaceEvenly;
  }
  return YGJustifyFlexStart;
}

void applyDim(YGNodeRef node, const Dim& d, void (*setPx)(YGNodeRef, float),
              void (*setPct)(YGNodeRef, float)) {
  switch (d.unit) {
    case Dim::Unit::Px:
      setPx(node, d.value);
      break;
    case Dim::Unit::Pct:
      setPct(node, d.value);
      break;
    case Dim::Unit::Auto:
      // Patch reuses the yoga node, so a dim REMOVED from the description
      // must be written back as YGUndefined rather than skipped. Skipping
      // leaves the previous describe's value in the style set, where it
      // sticks for the life of the instance.
      setPx(node, YGUndefined);
      break;
  }
}

// ---- structural equality ---------------------------------------------------
// Equal only when provably identical. Anything carrying a callable the
// library cannot compare (custom programs, decorations, outlines, routers,
// custom layouts) compares unequal and re-patches every describe; the common
// plain cases (boxes, fills, text runs, images) prune for free.

// Transition, BoundFloat and Animatable compare through SigilCore's
// comparators (transitionEqual, boundMapEqual, propEqual), each pinned
// beside its body there; the Effect and the blocks below are this
// library's own.

bool effectEqual(const std::optional<Effect>& a,
                 const std::optional<Effect>& b) {
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
              "is: through propEqual.");
bool textPathEqual(const TextPath& a, const TextPath& b) {
  return a.path == b.path && propEqual(a.at, b.at) && a.align == b.align &&
         a.offset == b.offset && a.autoFlip == b.autoFlip &&
         a.orient == b.orient && a.exactTangent == b.exactTangent;
}

static_assert(kFieldCount<TextData> == 17 && kFieldCount<TextOptions> == 13 &&
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
  // compared where every other animated slot is, through propEqual.
  //
  // variationDrive()'s track rides this comparison too: its effect's key
  // carries the axis tag AND the driven Output's address, so a re-describe
  // naming the same drive prunes and one naming another does not — the
  // binding identity every bound value in the tree is compared by.
  if (ta.tracks.size() != tb.tracks.size()) return false;
  for (size_t i = 0; i < ta.tracks.size(); ++i)
    if (!ta.tracks[i].sameShape(tb.tracks[i]) ||
        !propEqual(ta.tracks[i].progress, tb.tracks[i].progress))
      return false;
  if (ta.utf8 != tb.utf8 || !(ta.style == tb.style)) return false;
  // rich(): a whole mixed paragraph as one comparable value — same base,
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
  return true;
}

static_assert(kFieldCount<DeriveData> == 15,
              "DeriveData gained or lost a field — rule on it in "
              "deriveEqual() below, then bump this count.");
bool deriveEqual(const Box<DeriveData>& a, const Box<DeriveData>& b) {
  if ((bool)a != (bool)b) return false;
  if (!a) return true;
  // Incomparable callables → conservative inequality. A band's authored
  // SPINE rides the Shape seam instead (same rule as shapeFn): comparable
  // generators prune, raw callables stay conservative. A band borrowed by
  // key was always a comparable value.
  if (a->placeFn || b->placeFn || a->router || b->router || a->railRouter ||
      b->railRouter)
    return false;
  if (!(a->bandSpine == b->bandSpine)) return false;
  if (a->bandWidth.has_value() != b->bandWidth.has_value()) return false;
  if (a->bandWidth && !(*a->bandWidth == *b->bandWidth)) return false;
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
              "below (Mask::operator== is in Compose.h), then bump this "
              "count.");
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
              "materialEqual() below (or in propsEqual, which owns the "
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
    const bool panOnlyA =
        a->live->hasBoundOffset() && !a->live->animatedBeyondBoundOffset();
    const bool panOnlyB =
        b->live->hasBoundOffset() && !b->live->animatedBeyondBoundOffset();
    if (panOnlyA != panOnlyB) return false;
    if (!panOnlyA && (a->live->isAnimated() || b->live->isAnimated()))
      return false;
    if (!(*a->live == *b->live)) return false;
  }
  return true;  // ->recipe is handled with the fill compare in propsEqual
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
// ---- the fx() seam's hand-written comparators ------------------------------

/** Two effects are the same effect when their identity — the preset name
 *  or the author's key, the parameters, the operands, the pass material
 *  and the named curves — is; a lambda-valued curve compares unequal,
 *  conservatively, through easeEqual. */
bool TextEffect::operator==(const TextEffect& other) const {
  if (m_state == other.m_state) return true;  // copies of one value
  if (!m_state || !other.m_state) return false;
  if (m_state->name != other.m_state->name ||
      m_state->params != other.m_state->params ||
      m_state->operands != other.m_state->operands)
    return false;
  // A pass compares by its MATERIAL, by value — Material's own recipe
  // equality, so two passes over one source with equal constants prune,
  // and a live pass material never compares equal, conservatively.
  if ((m_state->pass != nullptr) != (other.m_state->pass != nullptr))
    return false;
  if (m_state->pass && !(*m_state->pass == *other.m_state->pass)) return false;
  if (m_state->curves.size() != other.m_state->curves.size()) return false;
  for (size_t i = 0; i < m_state->curves.size(); ++i)
    if (!detail::easeEqual(m_state->curves[i], other.m_state->curves[i]))
      return false;
  return true;
}

bool Selector::operator==(const Selector& other) const {
  if (m_state == other.m_state) return true;
  if (!m_state || !other.m_state) return false;  // one is "everything"
  return *m_state == *other.m_state;
}

static_assert(kFieldCount<Stagger> == 11,
              "Stagger gained or lost a field — rule on it in "
              "Stagger::operator== below, then bump this count. A field left "
              "out makes two different cascades compare equal, the text node "
              "prunes, and it keeps beating to the old ladder forever.");
bool Stagger::operator==(const Stagger& other) const {
  if (eachMs != other.eachMs || amountMs != other.amountMs ||
      durationMs != other.durationMs || loopMs != other.loopMs ||
      from != other.from || seed != other.seed || over != other.over ||
      beatsOver != other.beatsOver || cueMs != other.cueMs)
    return false;
  if (!easeEqual(distribution, other.distribution)) return false;
  if (inner == other.inner) return true;  // both absent, or one shared value
  if (!inner || !other.inner) return false;
  return *inner == *other.inner;
}

static_assert(kFieldCount<Track> == 6,
              "Track gained or lost a field — rule on it in "
              "Track::sameShape() below, then bump this count. `progress` is "
              "deliberately NOT compared there: it is an Animatable, and "
              "textEqual() compares it through propEqual with every other "
              "animated slot.");
bool Track::sameShape(const Track& other) const {
  return where == other.where && effect == other.effect &&
         stagger == other.stagger && reach == other.reach &&
         continuous == other.continuous;
}
bool Track::operator==(const Track& other) const {
  return sameShape(other) && propEqual(progress, other.progress);
}

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
        (!propEqual(a.begin, b.begin) || !propEqual(a.end, b.end) ||
         !propEqual(a.offset, b.offset)))
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
      return angleDeg == other.angleDeg && propEqual(fraction, other.fraction);
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
 *  (env snapshot + the author's own props comparator) and never reaches
 *  here, because `inst.desc` holds the memo's PRODUCED payload; and
 *  `children` are reconciled by key rather than compared — a node that
 *  prunes still walks them. */
static_assert(kFieldCount<ElementNode> == 24 && kFieldCount<PaintProps> == 15 &&
                  kFieldCount<ImageData> == 3 && kFieldCount<CustomData> == 2 &&
                  kFieldCount<MotionPath> == 3 && kFieldCount<Fill> == 3,
              "A struct propsEqual() compares BY HAND gained or lost a "
              "field. Rule on it below — participate, or a stated reason "
              "not to — then bump this count. A miss is silent: the node "
              "prunes, markPaintDirtyUp() never runs, a stale picture "
              "replays, and applyTransitions() never ramps an animate() on "
              "it. Nothing else fails, so no test will catch it for you.");
bool propsEqual(const ElementNode& a, const ElementNode& b) {
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
  const Material* recipeA =
      a.materialData
          ? (a.materialData->recipe ? &*a.materialData->recipe : nullptr)
          : nullptr;
  const Material* recipeB =
      b.materialData
          ? (b.materialData->recipe ? &*b.materialData->recipe : nullptr)
          : nullptr;
  if ((recipeA != nullptr) != (recipeB != nullptr)) return false;
  if (recipeA) {
    if (!(*recipeA == *recipeB)) return false;
  } else if (pa.fill && !propEqual(*pa.fill, *pb.fill)) {
    return false;
  }
  if (!propEqual(pa.opacity, pb.opacity) || pa.blendMode != pb.blendMode ||
      !propEqual(pa.translateX, pb.translateX) ||
      !propEqual(pa.translateY, pb.translateY) ||
      !propEqual(pa.rotate, pb.rotate) || !propEqual(pa.scale, pb.scale) ||
      // Every transform lane appears in this list, including the per-axis
      // scales. Omitting one makes two descriptions that differ only in
      // that lane compare equal, so the patch prunes, the node is never
      // marked paint-dirty, and it keeps the picture recorded at the old
      // value — and applyTransitions runs only inside the `own` branch
      // below, so an animate() on the missing lane never ramps either.
      !propEqual(pa.scaleX, pb.scaleX) || !propEqual(pa.scaleY, pb.scaleY) ||
      !propEqual(pa.skewX, pb.skewX) || !propEqual(pa.skewY, pb.skewY) ||
      pa.originX != pb.originX || pa.originY != pb.originY ||
      pa.originPx != pb.originPx || pa.zIndex != pb.zIndex)
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
                       !propEqual(a.motionData->t, b.motionData->t) ||
                       a.motionData->lookAhead != b.motionData->lookAhead))
    return false;
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
 *  The lanes must mirror propsEqual's transform block plus travel(), which
 *  replaces the translate lanes and adds to rotate. A lane present there and
 *  missing here is a world-space material left on a stale W. */
bool describedTransformEqual(const ElementNode& a, const ElementNode& b) {
  const PaintProps &pa = a.paint, &pb = b.paint;
  if (!propEqual(pa.translateX, pb.translateX) ||
      !propEqual(pa.translateY, pb.translateY) ||
      !propEqual(pa.rotate, pb.rotate) || !propEqual(pa.scale, pb.scale) ||
      !propEqual(pa.scaleX, pb.scaleX) || !propEqual(pa.scaleY, pb.scaleY) ||
      !propEqual(pa.skewX, pb.skewX) || !propEqual(pa.skewY, pb.skewY) ||
      pa.originX != pb.originX || pa.originY != pb.originY ||
      pa.originPx != pb.originPx)
    return false;
  if ((bool)a.motionData != (bool)b.motionData) return false;
  if (a.motionData && (!(a.motionData->path == b.motionData->path) ||
                       !propEqual(a.motionData->t, b.motionData->t) ||
                       a.motionData->lookAhead != b.motionData->lookAhead))
    return false;
  return true;
}

}  // namespace detail

// ---------------------------------------------------------------------------

void Composer::Impl::materializeText(
    Instance& inst, std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns) {
  const TextData& text = *inst.desc->textData;
  inst.paragraph.emplace();
  // Cleared for every content form, so the names a node answers for are
  // exactly the ones its CURRENT content declares.
  inst.textSlotKeys.clear();
  inst.textSlotRects.clear();
  inst.textNamedRuns.clear();
  if (text.paragraphOverride) {
    *inst.paragraph = *text.paragraphOverride;
  } else if (!text.rich.empty()) {
    // The runs concatenate with nothing between them: a rich text's spacing
    // is the author's own, exactly as it is in the strings they wrote.
    for (const RichText::Run& run : text.rich.runs()) {
      if (!run.slotKey.empty()) {
        // A slot run reserves a box instead of setting glyphs. The names go
        // into one list in declaration order, which is the order weave
        // matches its placeholder records to the U+FFFCs in the text.
        inst.textSlotKeys.push_back(run.slotKey);
        inst.paragraph->appendPlaceholder(
            {run.slotSize.width(), run.slotSize.height(), run.slotBaselineDrop},
            run.style);
        continue;
      }
      // The extent a named run occupies, read off the text as it grows: a
      // name is a handle on THIS run's characters, not on the style span it
      // produced, so the restyles below may cut the spans to pieces and
      // sel::style still answers with the run.
      const auto begin = (uint32_t)inst.paragraph->text().size();
      inst.paragraph->appendText(run.utf8, run.style);
      if (!run.styleName.empty())
        inst.textNamedRuns.push_back(
            {run.styleName, {begin, (uint32_t)inst.paragraph->text().size()}});
    }
  } else {
    inst.paragraph->appendText(text.utf8, text.style);
  }
  // The writing mode is the Paragraph's, not the layout options', so the
  // field-masked override lands here: a mode nobody set leaves a passed-in
  // paragraph's own mode standing. A path run has no columns to advance —
  // its baseline IS the geometry — so the path wins and says so.
  if (text.options.set & TextOptions::kWritingMode)
    inst.paragraph->setWritingMode(text.options.writingMode);
  if (text.onPath &&
      inst.paragraph->writingMode() != sigil::weave::WritingMode::kHorizontal) {
    warnWritingModeOnPath();
    inst.paragraph->setWritingMode(sigil::weave::WritingMode::kHorizontal);
  }

  // The restyles run in DECLARATION ORDER over the finished paragraph, so a
  // later one simply overwrites the spans an earlier one wrote wherever the
  // two overlap — which is the "later wins" rule, spelled as span surgery
  // rather than as a merge nobody could predict.
  //
  // LATER WINS PER DIMENSION, and the paint dimension is `spanPaint`'s: a
  // `spanStyle` over text an earlier `spanPaint` coloured applies its
  // other dimensions and leaves that colour standing. Otherwise the two
  // verbs would have to be declared in one particular order to both take
  // effect, with nothing said when they were not — the style, being a
  // whole style, carries a paint whether or not its author was thinking
  // about paint, and it would silently repaint the selection its own
  // default. Written as a REPLAY: the style is applied whole, then every
  // earlier `spanPaint` that reaches the same characters is re-applied
  // over it, in declaration order, so the last one to cover a character
  // is still the one standing.
  //
  // Every selection is resolved up front, against the text as written:
  // a restyle never edits the text, so the ranges hold, and the fold below
  // needs to know what the LATER restyles cover before it decides about an
  // earlier one.
  const size_t restyleCount = text.spanRestyles.size();
  std::vector<std::vector<sigil::weave::CharRange>> resolvedRanges(
      restyleCount);
  // The ranges are the painter's answer: text that carries none — a
  // description built without a text verb — is restyled by nothing.
  const TextPainterOps* painter = textPainterOf(inst);
  for (size_t i = 0; i < restyleCount; ++i)
    if (painter)
      resolvedRanges[i] =
          painter->ranges(text.spanRestyles[i].where, *inst.paragraph, fonts,
                          lines, columns, inst.textNamedRuns);
  if (inst.textState) inst.textState->spanAxisTracks.clear();
  // The intersection of two selections, as the ranges they share.
  const auto overlap = [](std::span<const sigil::weave::CharRange> a,
                          std::span<const sigil::weave::CharRange> b) {
    std::vector<sigil::weave::CharRange> shared;
    for (const sigil::weave::CharRange& x : a)
      for (const sigil::weave::CharRange& y : b)
        if (x.start < y.end && y.start < x.end)
          shared.push_back(
              {std::max(x.start, y.start), std::min(x.end, y.end)});
    return shared;
  };
  // Nothing is carried until a `spanPaint` has actually painted something,
  // and a passage that declares none takes neither the search nor the
  // replay below.
  bool paintDeclared = false;
  for (size_t i = 0; i < restyleCount; ++i) {
    const SpanRestyle& restyle = text.spanRestyles[i];
    const std::vector<sigil::weave::CharRange>& ranges = resolvedRanges[i];
    if (ranges.empty()) continue;
    if (restyle.paintOnly) {
      // The batch form: N ranges cost one span-list rebuild, and shaping
      // keys are untouched, so nothing re-shapes and nothing relayouts.
      inst.paragraph->setPaint(ranges, restyle.style.paint);
      paintDeclared = true;
      continue;
    }
    // The text this restyle covers whose paint an earlier `spanPaint`
    // owns — the merge, resolved as ranges: each piece with the paint that
    // owns it, in declaration order, and the pieces alone for the fold.
    std::vector<std::pair<std::vector<sigil::weave::CharRange>,
                          const sigil::weave::PaintStyle*>>
        carried;
    std::vector<sigil::weave::CharRange> carriedRanges;
    if (paintDeclared)
      for (size_t j = 0; j < i; ++j) {
        if (!text.spanRestyles[j].paintOnly) continue;
        std::vector<sigil::weave::CharRange> shared =
            overlap(ranges, resolvedRanges[j]);
        if (shared.empty()) continue;
        carriedRanges.insert(carriedRanges.end(), shared.begin(), shared.end());
        carried.emplace_back(std::move(shared),
                             &text.spanRestyles[j].style.paint);
      }
    // THE FOLD. A style that differs from the text it covers only in
    // advance-invariant variable-font axes — a grade over the numerals, an
    // optical size over a heading — does not need the words re-shaped to be
    // honoured: the glyphs keep the pen positions shaping gave them and the
    // coordinate reaches them at draw time, as a track. Anything else the
    // style changes is a reshape, and so is an axis the face moves advances
    // on, so the restyle falls through to the span surgery below. The
    // fold keeps the "later wins" rule by declining wherever a LATER
    // reshaping restyle covers the same text: a track deviates whatever
    // the paragraph shaped, and a later style must be the one that stands.
    std::vector<std::pair<std::string, float>> folded;
    if (painter->foldable(inst, restyle.style, ranges, *inst.paragraph,
                          carriedRanges, folded)) {
      bool coveredLater = false;
      for (size_t j = i + 1; j < restyleCount && !coveredLater; ++j) {
        if (text.spanRestyles[j].paintOnly) continue;
        for (const sigil::weave::CharRange& a : ranges)
          for (const sigil::weave::CharRange& b : resolvedRanges[j])
            if (a.start < b.end && b.start < a.end) coveredLater = true;
      }
      if (!coveredLater) {
        for (const auto& [tag, value] : folded) {
          const char axis[5] = {tag[0], tag[1], tag[2], tag[3], '\0'};
          Track track;
          track.where = restyle.where;
          track.effect = TextEffect::variableAxis(axis, value);
          textStateOf(inst).spanAxisTracks.push_back(std::move(track));
        }
        continue;
      }
    }
    for (const sigil::weave::CharRange& range : ranges)
      inst.paragraph->setStyle(range.start, range.end, restyle.style);
    // …and the earlier paints back over it, so the style's own paint
    // stands only where no `spanPaint` reached.
    for (const auto& [where, paint] : carried)
      inst.paragraph->setPaint(where, *paint);
  }
}

sigil::weave::ParagraphLayoutOptions Composer::Impl::textLayoutOptions(
    const Instance& inst) const {
  sigil::weave::ParagraphLayoutOptions options;
  if (!inst.desc || !inst.desc->textData) return options;
  const TextData& text = *inst.desc->textData;
  // The passed value is the ground the setters are written over, so a
  // full-control caller keeps every field no setter named.
  options = text.layoutOptions;
  text.options.applyTo(options);
  // OVERFLOW IS THE NORMAL CASE ON EVERY FRAME BUT THE LAST. A frame that
  // threads into another has a remainder by design, and a marker there
  // would say the text was cut when it was only continued; the last frame
  // of a chain is the one that threads nowhere, and it keeps whatever
  // ellipsis the leaf asked for.
  if (!text.threadTo.empty()) options.overflow.ellipsis.clear();
  // THE BAND A RESERVING READING NEEDS, asked before anything is broken and
  // answered from the reading's own metrics — which is the whole of why a
  // reservation is a layout input and not a cycle. Only the engine can
  // measure a face, so the painter answers; a text that dresses nothing has
  // no annotations either.
  if (!text.annotations.empty()) {
    const TextPainterOps* painter = textPainterOf(inst);
    if (!painter) painter = detail::registeredTextEngine();
    if (painter) {
      const sigil::weave::ReservedBand band =
          painter->reservedBand(const_cast<Instance&>(inst), text.annotations);
      options.reserved.before += band.before;
      options.reserved.after += band.after;
    }
  }
  return options;
}

void Composer::Impl::applyLayoutProps(Instance& inst) {
  if (!inst.yoga)
    return;  // positioned subtree: instanceRect() reads the props directly
  const LayoutProps& l = inst.desc->layout;
  YGNodeRef n = inst.yoga;

  YGNodeStyleSetFlexDirection(
      n, l.row ? YGFlexDirectionRow : YGFlexDirectionColumn);
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
  if (!autoSized || l.width.unit != Dim::Unit::Auto)
    applyDim(n, l.width, &YGNodeStyleSetWidth, &YGNodeStyleSetWidthPercent);
  if (!autoSized || l.height.unit != Dim::Unit::Auto)
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
  applyDim(n, l.basis, &YGNodeStyleSetFlexBasis,
           &YGNodeStyleSetFlexBasisPercent);
  YGNodeStyleSetAlignItems(n, toYogaAlign(l.alignItems));
  // Measured text must not stretch on the cross axis: a stretched text leaf
  // is re-measured against a width it did not ask for, and the box stops
  // fitting the type. Demote a RESOLVED Stretch to Start for text leaves,
  // while letting any explicit alignment — the node's own or inherited from
  // the parent — through untouched.
  Align self = l.alignSelf;
  if (inst.desc->kind == Kind::Text) {
    const Align resolved = self != Align::Auto
                               ? self
                               : (inst.parent && inst.parent->desc
                                      ? inst.parent->desc->layout.alignItems
                                      : Align::Stretch);
    if (resolved == Align::Stretch) self = Align::Start;
  }
  YGNodeStyleSetAlignSelf(n, toYogaAlign(self));
  YGNodeStyleSetJustifyContent(n, toYogaJustify(l.justify));

  // The node's OWN position type. A stack child's is overwritten right
  // after this, in patchChildren() — see the note there.
  YGNodeStyleSetPositionType(
      n, l.absolute ? YGPositionTypeAbsolute : YGPositionTypeRelative);
  if (l.hasInsets) {
    // Per-side Dims: Auto leaves the side UNPINNED (YGUndefined), so
    // `.top(12).right(12)` pins a corner badge without stretching it.
    // Always write all four — patch() reuses the yoga node, and a side
    // that was pinned last describe must actually release.
    auto applyInset = [n](YGEdge edge, const Dim& d) {
      switch (d.unit) {
        case Dim::Unit::Px:
          YGNodeStyleSetPosition(n, edge, d.value);
          break;
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
  pathMarkInstances.clear();
  threadedInstances.clear();
  routesByAnchor.clear();
  hasDerived = false;
  hasCustomLayout = false;
  hasCenterPins = false;
  // The reconciler fills byKey — a memo shell's key first, else the
  // description's — and the edge store (flat derive lists + anchor
  // back-index) and the pass gates ride the same walk. Tree order here IS
  // the derive order.
  if (root)
    reconciler.indexKeys(*root, byKey, [this](Instance& inst) {
      if (inst.desc->kind == Kind::Slot && !inst.desc->key.empty())
        bySlot[inst.desc->key] = &inst;
      const ElementNode& node = *inst.desc;
      if (node.deriveData) {
        const DeriveData& derive = *node.deriveData;
        if (!derive.flowAroundKeys.empty()) flowInstances.push_back(&inst);
        const bool isConnector =
            !derive.connectFrom.empty() && !derive.connectTo.empty();
        const bool isRail = derive.railAnchors.size() >= 2;
        // A borrowed band spine and a spans::fit() gap are the same kind
        // of question a connector asks — "where did that keyed node land"
        // — so they ride the SAME flat derive list rather than growing a
        // phase.
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
          for (const Anchor& anchor : derive.railAnchors) {
            auto& at = routesByAnchor[anchor.nodeKey];
            if (at.empty() || at.back() != &inst)  // rails revisit anchors
              at.push_back(&inst);
          }
        }
        if (derive.placeFn) hasCustomLayout = true;
      }
      if (node.kind == Kind::Text && node.textData && node.textData->onPath &&
          !node.textData->marks.empty())
        pathMarkInstances.push_back(&inst);
      if (node.kind == Kind::Text && node.textData &&
          !node.textData->threadTo.empty())
        threadedInstances.push_back(&inst);
      if (node.layout.centerAt) hasCenterPins = true;
    });
  hasDerived = !routedInstances.empty() || !flowInstances.empty() ||
               !threadedInstances.empty();
}

}  // namespace sigil::compose
