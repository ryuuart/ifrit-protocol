// THE FIELD WALK — the runtime half of ComposeInternal.h's FIELD PINS.
//
// `propsEqual()` and its helpers compare a description FIELD BY FIELD, and a
// field left out fails INVISIBLY: two different descriptions compare equal,
// the patch prunes, `markPaintDirtyUp()` never runs, a stale picture replays,
// and `applyTransitions()` — which only runs inside the `own` branch — never
// ramps an `animate()` on that property. Nothing errors; no other test fails.
// `scaleX`/`scaleY` sat in that hole from the day they landed until e37d58d.
//
// The compile-time pin (a structured binding per struct, plus a
// `kFieldCount` assert beside each hand-written comparator) makes ADDING a
// field a build failure. This file is what makes RULING on it mechanical:
// each walk perturbs every tied field in turn and demands the comparator
// notice, so a field is covered the moment it is named in `fields()` — no
// second list to remember, and a field whose type has no perturbation here
// does not compile either.
//
// Comparing the values DIRECTLY rather than counting `stats().patchedNodes`
// is deliberate. A prune is a statement about the SAME node across two
// describes; keyed siblings never prune into one another, so a
// render-two-trees harness can pass while the comparator is broken. That
// trap cost this session nine pins that passed their own positive control.

#include "ComposeTestSupport.h"

#include "../ComposeInternal.h"
// …and the RUNTIME header, for the kSlotSpecs walk at the bottom of the file.
// It is the reason this target links yoga::yogacore (see CMakeLists.txt).
#include "../ComposeRuntime.h"

namespace cd = sigil::compose::detail;

namespace {

// ---- perturbations, ONE PER FIELD TYPE -------------------------------------
// A field whose type is not covered here is a compile error at the walk, and
// that is the point: a new field type is a ruling ("how would a change to
// this even look?"), not a silent skip.

template <class> inline constexpr bool kNoPerturbation = false;

template <class T> void perturb(T &) {
  static_assert(kNoPerturbation<T>,
                "no perturbation is known for this field's type — add one "
                "below (a change the comparator must be able to see), or "
                "move the field to the walk's EXCLUDED table with a reason");
}

void perturb(bool &v) { v = !v; }
void perturb(int &v) { v += 1; }
void perturb(uint32_t &v) { v += 1u; }
void perturb(float &v) { v += 1.0f; }
void perturb(std::string &v) { v += "moved"; }
void perturb(cd::Kind &v) { v = cd::Kind::Stack; }
void perturb(Cache &v) { v = Cache::None; }
void perturb(SkBlendMode &v) { v = SkBlendMode::kMultiply; }
void perturb(Corners &v) { v.topLeft += 1.0f; }
void perturb(cd::LayoutProps &v) { v.gap += 1.0f; }
void perturb(Shape &v) {
  v = Shape([](SkSize) { return SkPath(); }); // the raw-callable escape hatch
}
void perturb(std::optional<Transition> &v) { v = Transition{}; }
void perturb(choreograph::EaseFn &v) { v = &choreograph::easeInQuad; }
void perturb(const choreograph::Output<float> *&v) {
  static choreograph::Output<float> other;
  v = &other;
}
void perturb(Animatable<float> &v) {
  v = (v.plain() ? *v.plain() : 0.0f) + 1.0f;
}
void perturb(std::optional<Animatable<Fill>> &v) {
  v = Animatable<Fill>(Fill::color(SkColor4f{1, 0, 0, 1}));
}
void perturb(std::vector<Decoration> &v) {
  v.push_back(Decoration(PaintProgram{[](SkCanvas &, const PaintContext &) {}}));
}
void perturb(std::vector<Element> &v) { v.push_back(box()); }
void perturb(cd::PaintProps &v) { perturb(v.opacity); }
template <class T> void perturb(cd::Box<T> &v) { v.ensure(); }

// ---- the walk --------------------------------------------------------------

/** Perturb field @p I of a default-constructed @p S and hand the pair to
 *  @p equal. `expected` is what the comparator is ALLOWED to say: false for
 *  a field that must participate, true for one legitimately excluded. */
template <std::size_t I, class S, class Eq>
void checkField(Eq equal, const char *name, bool participates) {
  S base{};
  S moved{};
  perturb(std::get<I>(cd::fields(moved)));
  const bool equalNow = equal(base, moved);
  if (participates)
    EXPECT_FALSE(equalNow)
        << "field #" << I << " (" << name << ") is MISSING from its "
        << "comparator: two descriptions differing only in it compare "
           "EQUAL, so the node prunes, replays a stale picture, and never "
           "ramps an animate() on it";
  else
    EXPECT_TRUE(equalNow)
        << "field #" << I << " (" << name << ") is documented as excluded "
        << "from its comparator but now changes the answer — the exclusion "
           "reason in the table above is stale";
}

template <class S, class Eq, std::size_t... I>
void walkFields(Eq equal, const char *const *names, const bool *participates,
                std::index_sequence<I...>) {
  (checkField<I, S>(equal, names[I], participates[I]), ...);
}

template <class S, class Eq, std::size_t N>
void walkFields(Eq equal, const char *const (&names)[N],
                const bool (&participates)[N]) {
  static_assert(N == cd::kFieldCount<S>,
                "the walk's name/participation table has drifted from the "
                "struct's fields — one row per field, in fields() order");
  walkFields<S>(equal, names, participates, std::make_index_sequence<N>{});
}

} // namespace

TEST(ComposeReconcile, EveryPaintPropsFieldParticipatesInEquality) {
  // ALL FIFTEEN. There is no legitimate exclusion in this block: every field
  // of PaintProps is a lane the painter reads live, so every one of them
  // must reach the comparator. This is the walk `scaleX`/`scaleY` would have
  // failed for the whole time they were missing.
  static const char *const kNames[] = {
      "fill",   "opacity", "blendMode", "translateX", "translateY",
      "rotate", "scale",   "scaleX",    "scaleY",     "skewX",
      "skewY",  "originX", "originY",   "originPx",   "zIndex"};
  static const bool kParticipates[] = {true, true, true, true, true,
                                       true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<cd::PaintProps>(
      [](const cd::PaintProps &a, const cd::PaintProps &b) {
        cd::ElementNode na, nb;
        na.paint = a;
        nb.paint = b;
        return cd::propsEqual(na, nb);
      },
      kNames, kParticipates);
}

TEST(ComposeReconcile, EveryBoundFloatFieldParticipatesInEquality) {
  // ALL SEVENTEEN, against boundMapEqual() directly. The wiggle wave pinned
  // five of them end to end
  // (WiggledBindingsPruneOnlyWhenEveryParameterMatches); the other eleven had
  // no control at all until this walk. `wrapPeriod` (the derive() wave,
  // 2026-07-29) is read live like every other stage, so it participates.
  static const char *const kNames[] = {
      "source",       "inScale",         "inOffset",     "curve",
      "clampInput",   "steps",           "scale",        "offset",
      "clamped",      "lo",              "hi",           "wiggleAmount",
      "wiggleFrequency", "wiggleSeed",   "wiggleOctaves", "wiggleFalloff",
      "wrapPeriod"};
  static const bool kParticipates[] = {true, true, true, true, true, true,
                                       true, true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<BoundFloat>(cd::boundMapEqual, kNames, kParticipates);
}

TEST(ComposePaintBounds, PerAxisScaleReachesTheParentsChildBoundsUnion) {
  // THE SECOND SITE of the scaleX/scaleY miss, filed by the travel() wave
  // and taken here. `recordBounds()` hand-rolled the transform gate that
  // `NodeTransform::pivoted()` already spells for paint() and hitInstance(),
  // and left `sx`/`sy` out of it — so a child whose ONLY transform was a
  // per-axis scale handed its parent UNSCALED bounds, and every consumer
  // sized off them (the effect layer here, plus the opacity layer and the
  // texture bake) truncated the overflow.
  //
  // The parent takes an identity offset() filter purely to force the
  // bounded saveLayer: `saveLayer(&recordBounds(inst), …)` clips, so the
  // scaled-out half of the bar is simply gone when the bounds are wrong.
  Host host(200, 200);
  host.composer.render(
      box().child(box()
                      .absolute()
                      .rect(SkRect::MakeXYWH(20, 20, 40, 40))
                      .effect(
                          Effect::filter(SkImageFilters::Offset(0, 0, nullptr)))
                      .child(box()
                                 .absolute()
                                 .rect(SkRect::MakeXYWH(0, 0, 40, 40))
                                 .transformOrigin(0, 0)
                                 .fill(red())
                                 .scaleX(3.0f))));
  host.frame();
  EXPECT_EQ(host.pixel(30, 40), SK_ColorRED) << "the unscaled part is missing";
  EXPECT_EQ(host.pixel(120, 40), SK_ColorRED)
      << "the scaled-out part of the bar was clipped away — recordBounds() "
         "did not see scaleX on the child";
  EXPECT_EQ(host.pixel(150, 40), SK_ColorBLACK) << "…and it over-reached";
}

TEST(ComposeReconcile, EveryElementNodeFieldParticipatesInEquality) {
  // THE AUDIT TABLE, EXECUTABLE. Twenty-three fields; twenty-one must reach
  // propsEqual(), and the two that must NOT are asserted to be inert here
  // rather than merely described in a comment:
  //
  //  - `memoData` never reaches propsEqual at all. resolveMemo() compares a
  //    memo EARLIER and more strictly (the env snapshot, then the author's
  //    own props comparator) and `inst.desc` holds the memo's PRODUCED
  //    payload, which carries no memo block.
  //  - `children` are reconciled BY KEY, not compared. A node that prunes
  //    still walks them — that is the whole point of the structural prune.
  static const char *const kNames[] = {
      "kind",        "key",        "layout",     "paint",
      "corners",     "shapeFn",    "clipContent", "hitTestable",
      "cacheMode",   "bakeScale",  "nodeTransition", "backgrounds",
      "foregrounds", "textData",   "imageData",  "customData",
      "deriveData",  "fxData",     "materialData", "strokeData",
      "memoData",    "motionData", "children"};
  static const bool kParticipates[] = {
      true, true, true, true, true, true, true, true,
      true, true, true, true, true, true, true, true,
      true, true, true, true,
      false, // memoData — resolveMemo owns it, and it never lands in desc
      true,
      false, // children — reconciled by key, never compared
  };
  walkFields<cd::ElementNode>(cd::propsEqual, kNames, kParticipates);
}

// ---------------------------------------------------------------------------
// THE SLOT WALK — the runtime half of ComposeRuntime.h's kSlotSpecs table.
//
// The table closes the OMISSION hazard by construction (index-aligned rows
// under compile-time asserts, so a new Instance::Slot cannot be added
// without ruling on it). What it opens instead is a MISAIMING hazard that
// twelve separate hand-written call sites did not have: a copy-pasted row
// whose accessor returns the neighbouring field would compile, and then
// `.scaleY(animate(...))` would ramp `scaleX` on every one of the four
// consumers at once. This walk is what makes that fail.

TEST(ComposeSlotPins, EverySlotRowReachesItsOwnFieldAtItsStandingDefault) {
  // Every block a slot can hide behind, PRESENT and defaulted — so every
  // non-Bespoke row must answer with a pointer, and that pointer must be at
  // the field's own default value.
  cd::ElementNode node;
  node.motionData.ensure();                  // travel(): carries kMotionT
  node.textData.ensure().glyphFx.emplace();  // kinetic text: kGlyphProgress

  std::vector<const Animatable<float> *> seen;
  int bespoke = 0, opacityRows = 0;
  for (const cd::SlotSpec &spec : cd::kSlotSpecs) {
    const int index = (int)spec.slot;
    if (spec.role == cd::SlotRole::Bespoke) {
      ++bespoke;
      // The compile-time assert already pairs "no accessor" with "declared
      // Bespoke, with a reason"; this is the runtime half of the same claim.
      EXPECT_EQ(spec.of, nullptr) << "slot " << index;
      EXPECT_EQ(cd::slotValueOf(spec, node), nullptr)
          << "slot " << index << ": a Bespoke row must be INERT through "
             "slotValueOf, so a consumer that walks the table without "
             "special-casing it does nothing rather than crashing";
      continue;
    }
    if (spec.role == cd::SlotRole::Opacity)
      ++opacityRows;
    const Animatable<float> *v = cd::slotValueOf(spec, node);
    ASSERT_NE(v, nullptr) << "slot " << index << "'s accessor reaches nothing "
                             "on a node carrying every block";
    // THE MISAIM. Two rows answering with the same address means one of them
    // is pointed at the other's field, and every consumer inherits the error.
    for (size_t i = 0; i < seen.size(); ++i)
      EXPECT_NE(v, seen[i]) << "slot " << index << " and slot " << i
                            << " aim at the SAME field";
    seen.push_back(v);
    // …and the STANDING endpoint applyTransitions substitutes when a node
    // gains or loses the block really is this field's own default. A wrong
    // number here is a node that jumps on the frame it starts travelling.
    ASSERT_NE(v->plain(), nullptr) << "slot " << index;
    EXPECT_FLOAT_EQ(*v->plain(), spec.standing)
        << "slot " << index << "'s `standing` is not the field's default";
  }
  EXPECT_EQ((int)seen.size() + bespoke, (int)cd::Instance::kSlots);
  EXPECT_EQ(opacityRows, 1)
      << "exactly one slot is applied by paint()'s saveLayer rather than its "
         "matrix; computeVolatile's device-bake refusal reads that split";
}

// ---- §44.2b.1 — maxScaleOf's perspective fallback --------------------------
//
// getMinMaxScales refuses a perspective matrix, and the old fallback read
// the matrix DIAGONAL — wrong twice over: a rotation moves the whole scale
// into the skew terms (at 90° the diagonal is exactly zero), and perspective
// makes scale position-dependent while the diagonal reads no position at
// all. The quantized bake ladder consumes this number, so the wrong answer
// was a wrongly-stepped bake under a host perspective CTM. The fix is local
// linearization: the Jacobian of the projective map, maxed over the center
// and four corners of the node's local bounds. This pin holds the answer to
// a NUMERICALLY-derived ground truth (finite differences of the mapped
// point at the same five samples), which the diagonal misses by a large
// factor.

TEST(ComposeMaxScale, PerspectiveFallbackTracksTheJacobianNotTheDiagonal) {
  // A card tilt WITH a rotation component: setRotate(90) snaps the diagonal
  // to exactly (0, 0), and the perspective row makes the true local scale
  // grow toward the rect's near edge (w = 1 − 0.002·y there).
  SkMatrix m;
  m.setRotate(90);
  SkMatrix persp = SkMatrix::I();
  persp.setPerspX(0.002f);
  m.postConcat(persp);
  ASSERT_TRUE(m.hasPerspective());
  const SkRect rect = SkRect::MakeXYWH(0, 0, 200, 150);

  // Ground truth by finite differences at the same five samples the
  // fallback reads: J's columns are (f(p+εx)−f(p))/ε through the FULL
  // projective map (divide included), σmax by the 2×2 closed form.
  const auto fdSigmaMax = [&m](SkPoint p) {
    const float eps = 1e-2f;
    const SkPoint q0 = m.mapPoint(p);
    const SkPoint qx = m.mapPoint({p.x() + eps, p.y()});
    const SkPoint qy = m.mapPoint({p.x(), p.y() + eps});
    const float j00 = (qx.x() - q0.x()) / eps, j10 = (qx.y() - q0.y()) / eps;
    const float j01 = (qy.x() - q0.x()) / eps, j11 = (qy.y() - q0.y()) / eps;
    const float e = j00 + j11, f = j00 - j11;
    const float g = j10 + j01, h = j10 - j01;
    return 0.5f * (std::sqrt(e * e + h * h) + std::sqrt(f * f + g * g));
  };
  float truth = 0;
  for (SkPoint p : {SkPoint{rect.centerX(), rect.centerY()},
                    SkPoint{rect.left(), rect.top()},
                    SkPoint{rect.right(), rect.top()},
                    SkPoint{rect.right(), rect.bottom()},
                    SkPoint{rect.left(), rect.bottom()}})
    truth = std::max(truth, fdSigmaMax(p));

  const float got = cd::maxScaleOf(m, rect);
  EXPECT_NEAR(got, truth, truth * 0.02f)
      << "the perspective fallback disagrees with the numerical Jacobian";

  // The wrongness the pin exists for: the diagonal answers exactly 0 here
  // (the old fallback would have clamped to the ladder's 0.25 floor), while
  // the true near-edge magnification is past 1.5×.
  const float diagonal =
      std::max(std::abs(m.getScaleX()), std::abs(m.getScaleY()));
  EXPECT_LT(diagonal, 0.1f);
  EXPECT_GT(got, 1.5f);

  // And the affine path is untouched: strip the perspective row (the
  // rotation carried the term into perspY, so clear both) and the same
  // rotation answers 1 by singular values, rect ignored.
  SkMatrix affine = m;
  affine.setPerspX(0);
  affine.setPerspY(0);
  ASSERT_FALSE(affine.hasPerspective());
  EXPECT_NEAR(cd::maxScaleOf(affine, rect), 1.0f, 1e-4f);
}
