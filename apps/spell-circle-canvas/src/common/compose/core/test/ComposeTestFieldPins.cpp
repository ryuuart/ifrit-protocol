// THE FIELD WALK — the runtime half of ComposeInternal.h's field pins.
//
// `propsEqual()` and its helpers compare a description FIELD BY FIELD, and a
// field left out fails INVISIBLY: two different descriptions compare equal,
// the patch prunes, `markPaintDirtyUp()` never runs, a stale picture replays,
// and `applyTransitions()` — which only runs inside the `own` branch — never
// ramps an `animate()` on that property. Nothing errors, and no other test
// fails, so the only thing that can catch it is a test built this way.
//
// The compile-time half (a `kFieldCount` assert beside each hand-written
// comparator) makes ADDING a field a build failure. This file makes
// deciding what to do about it mechanical: each walk perturbs every field
// of the struct in turn and demands the comparator notice, so a field is
// covered the moment it is DECLARED — no second list to remember. A field
// whose type has no perturbation below does not compile either.
//
// Comparing the values DIRECTLY rather than counting `stats().patchedNodes`
// is deliberate. A prune is a statement about the SAME node across two
// describes, and keyed siblings never prune into one another — so a harness
// that renders two trees and counts patched nodes can report exactly what a
// correct comparator would while the comparator is in fact broken.

#include <boost/pfr/core.hpp>

#include "../ComposeInternal.h"
#include "support/CoreTestSupport.h"

// …and the RUNTIME header, for the kSlotSpecs walk at the bottom of the file.
// It is the reason this target links yoga::yogacore (see CMakeLists.txt).
#include "../ComposeRuntime.h"

namespace cd = sigil::compose::detail;

namespace {

// ---- perturbations, ONE PER FIELD TYPE -------------------------------------
// A field whose type is not covered here is a compile error at the walk, and
// that is the point: introducing a new field type forces someone to answer
// "what does a change to this even look like?" instead of skipping it.

template <class>
inline constexpr bool kNoPerturbation = false;

template <class T>
void perturb(T&) {
  static_assert(kNoPerturbation<T>,
                "no perturbation is known for this field's type — add one "
                "below (a change the comparator must be able to see), or "
                "move the field to the walk's EXCLUDED table with a reason");
}

void perturb(bool& v) { v = !v; }

void perturb(int& v) { v += 1; }

void perturb(uint32_t& v) { v += 1u; }

void perturb(float& v) { v += 1.0f; }

void perturb(std::string& v) { v += "moved"; }

void perturb(cd::Kind& v) { v = cd::Kind::Stack; }

void perturb(Cache& v) { v = Cache::None; }

void perturb(Boundary& v) { v = Boundary::Glyphs; }

void perturb(Backface& v) { v = Backface::Hidden; }

void perturb(SkBlendMode& v) { v = SkBlendMode::kMultiply; }

void perturb(Corners& v) { v.topLeft += 1.0f; }

void perturb(cd::LayoutProps& v) { v.gap += 1.0f; }

void perturb(Shape& v) {
  v = Shape([](SkSize) { return SkPath(); });  // the raw-callable escape hatch
}

void perturb(std::optional<sigil::motion::Transition>& v) {
  v = motion::Transition{};
}

void perturb(choreograph::EaseFn& v) { v = &choreograph::easeInQuad; }

void perturb(sigil::motion::Envelope& v) {
  v = sigil::motion::Envelope::kCosine;
}

void perturb(const choreograph::Output<float>*& v) {
  static choreograph::Output<float> other;
  v = &other;
}

void perturb(sigil::motion::Animatable<float>& v) {
  v = (v.plain() ? *v.plain() : 0.0f) + 1.0f;
}

void perturb(std::optional<sigil::motion::Animatable<Fill>>& v) {
  v = sigil::motion::Animatable<Fill>(Fill::color(SkColor4f{1, 0, 0, 1}));
}

void perturb(std::vector<Decoration>& v) {
  v.emplace_back(PaintProgram{[](SkCanvas&, const PaintContext&) {}});
}

void perturb(std::vector<Element>& v) { v.push_back(box()); }

void perturb(cd::PaintProps& v) { perturb(v.opacity); }

void perturb(Unit& v) { v = Unit::Line; }

void perturb(Beats& v) { v = Beats::Text; }

void perturb(std::vector<float>& v) { v.push_back(1.0f); }

template <class T>
void perturb(cd::Box<T>& v) {
  v.ensure();
}

// ---- the walk --------------------------------------------------------------

/** Perturb field @p I of a default-constructed @p S and hand the pair to
 *  @p equal. `expected` is what the comparator is ALLOWED to say: false for
 *  a field that must participate, true for one legitimately excluded. */
template <std::size_t I, class S, class Eq>
void checkField(Eq equal, const char* name, bool participates) {
  S base{};
  S moved{};
  perturb(boost::pfr::get<I>(moved));
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
void walkFields(Eq equal, const char* const* names, const bool* participates,
                std::index_sequence<I...>) {
  (checkField<I, S>(equal, names[I], participates[I]), ...);
}

template <class S, class Eq, std::size_t N>
void walkFields(Eq equal, const char* const (&names)[N],
                const bool (&participates)[N]) {
  static_assert(N == cd::kFieldCount<S>,
                "the walk's name/participation table has drifted from the "
                "struct's fields — one row per field, in declaration order");
  walkFields<S>(equal, names, participates, std::make_index_sequence<N>{});
}

}  // namespace

TEST(ComposeReconcile, EveryPaintPropsFieldParticipatesInEquality) {
  // No legitimate exclusion exists in this block: every field of PaintProps
  // is a lane the painter reads live, so every one must reach the comparator.
  // The per-axis scales are the easiest ones to leave out, because the
  // uniform `scale` beside them makes a comparator look complete.
  static const char* const kNames[] = {
      "fill",   "opacity", "blendMode", "translateX", "translateY",
      "rotate", "scale",   "scaleX",    "scaleY",     "skewX",
      "skewY",  "originX", "originY",   "originPx",   "zIndex"};
  static const bool kParticipates[] = {true, true, true, true, true,
                                       true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<cd::PaintProps>(
      [](const cd::PaintProps& a, const cd::PaintProps& b) {
        cd::ElementNode na, nb;
        na.paint = a;
        nb.paint = b;
        return cd::propsEqual(na, nb);
      },
      kNames, kParticipates);
}

TEST(ComposeReconcile, EveryDepthDataFieldParticipatesInEquality) {
  // The depth block is read live at paint exactly as PaintProps is: five
  // lanes, the two origins, the transform origin's depth and the two
  // modes. A lane left out keeps the plane at the turn it was recorded at;
  // a mode left out keeps a space open, or a back drawn, that the author
  // closed.
  static const char* const kNames[] = {
      "rotateX",   "rotateY",   "translateZ",         "scaleZ",
      "perspective", "perspectiveOriginX", "perspectiveOriginY", "originZ",
      "preserve3d", "backface"};
  static const bool kParticipates[] = {true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<cd::DepthData>(
      [](const cd::DepthData& a, const cd::DepthData& b) {
        cd::ElementNode na, nb;
        na.depthData.ensure() = a;
        nb.depthData.ensure() = b;
        return cd::propsEqual(na, nb);
      },
      kNames, kParticipates);
}

TEST(ComposeReconcile, EveryCascadeFieldOfATrackParticipatesInEquality) {
  // A cascade over text is two values: the SCHEDULE, which is
  // SigilMotion's and pinned there, and the three fields that say what a
  // unit IS, which are this library's. Miss one and a re-described track
  // keeps the OLD schedule with no diagnostic — a granularity that never
  // takes, or a `beatsOver` flipped to Text on a paragraph that goes on
  // beating over each half's own selection. Both are silent, and both look
  // exactly like the engine ignoring the author.
  //
  // The pin beside `Track::sameShape()` makes a NEW field a build failure;
  // this makes the decision about it mechanical.
  const Track base{.stagger = {.eachMs = 30}};
  Track over = base;
  over.over = Unit::Line;
  EXPECT_FALSE(base.sameShape(over)) << "over";
  Track innerOver = base;
  innerOver.innerOver = Unit::Line;
  EXPECT_FALSE(base.sameShape(innerOver)) << "innerOver";
  Track beatsOver = base;
  beatsOver.beatsOver = Beats::Text;
  EXPECT_FALSE(base.sameShape(beatsOver)) << "beatsOver";
  Track schedule = base;
  schedule.stagger.eachMs = 31;
  EXPECT_FALSE(base.sameShape(schedule)) << "the schedule itself";
  EXPECT_TRUE(base.sameShape(base));
}

TEST(ComposeReconcile, EveryBoundFloatFieldParticipatesInEquality) {
  // Against boundMapEqual() directly. Every stage of a bound float's shaping
  // map is read live at paint, so every one of them participates — including
  // the wiggle parameters, `wrapPeriod` and the envelope's corners, which are
  // easy to add to the struct and forget in the comparator.
  static const char* const kNames[] = {"source",
                                       "inScale",
                                       "inOffset",
                                       "curve",
                                       "clampInput",
                                       "envelope",
                                       "riseStart",
                                       "holdStart",
                                       "holdEnd",
                                       "fallEnd",
                                       "duty",
                                       "waveFn",
                                       "steps",
                                       "scale",
                                       "offset",
                                       "clamped",
                                       "lo",
                                       "hi",
                                       "wiggleAmount",
                                       "wiggleFrequency",
                                       "wiggleSeed",
                                       "wiggleOctaves",
                                       "wiggleFalloff",
                                       "wrapPeriod"};
  static const bool kParticipates[] = {
      true, true, true, true, true, true, true, true, true, true, true, true,
      true, true, true, true, true, true, true, true, true, true, true, true};
  walkFields<sigil::motion::BoundFloat>(cd::boundMapEqual, kNames,
                                        kParticipates);
}

TEST(ComposePaintBounds, PerAxisScaleReachesTheParentsChildBoundsUnion) {
  // `recordBounds()` decides whether a child's transform widens the parent's
  // bounds, and it must recognise exactly the transforms `NodeTransform`
  // applies for paint() and hitInstance() — per-axis scale included. Miss one
  // and a child whose ONLY transform is a per-axis scale hands its parent
  // unscaled bounds, and every consumer sized off them (the effect layer
  // here, the opacity layer, the texture bake) silently truncates the
  // overflow.
  //
  // The parent takes an identity offset() filter purely to force a bounded
  // saveLayer: that layer clips to recordBounds(), so wrong bounds delete the
  // scaled-out half of the bar instead of merely mis-sizing something.
  Host host(200, 200);
  host.composer.render(
      box().child(box()
                      .absolute()
                      .rect(SkRect::MakeXYWH(20, 20, 40, 40))
                      .effect(material::skia::Effect::filter(
                          SkImageFilters::Offset(0, 0, nullptr)))
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
  // Two fields must NOT reach propsEqual(), and the table asserts their
  // inertness rather than describing it, so an accidental inclusion fails
  // here too:
  //
  //  - `memoData` never reaches propsEqual at all. resolveMemo() compares a
  //    memo EARLIER and more strictly (the env snapshot, then the author's
  //    own props comparator) and `inst.desc` holds the memo's PRODUCED
  //    payload, which carries no memo block.
  //  - `children` are reconciled BY KEY, not compared. A node that prunes
  //    still walks them — that is the whole point of the structural prune.
  static const char* const kNames[] = {
      "kind",        "key",         "layout",     "paint",
      "corners",     "shapeFn",     "boundary",   "clipContent",
      "hitTestable", "cacheMode",   "bakeScale",  "nodeTransition",
      "backgrounds", "foregrounds", "textData",   "imageData",
      "customData",  "deriveData",  "fxData",     "materialData",
      "strokeData",  "memoData",    "motionData", "depthData",
      "children"};
  static const bool kParticipates[] = {
      true,  true, true, true, true, true, true, true, true, true, true,
      true,  true, true, true, true, true, true, true, true, true,
      false,  // memoData — resolveMemo owns it, and it never lands in desc
      true,  true,
      false,  // children — reconciled by key, never compared
  };
  walkFields<cd::ElementNode>(cd::propsEqual, kNames, kParticipates);
}

// ---------------------------------------------------------------------------
// THE SLOT WALK — the runtime half of ComposeRuntime.h's kSlotSpecs table.
//
// The table closes the OMISSION hazard by construction: rows are index-
// aligned with the slot enum under compile-time asserts, so a new
// Instance::Slot cannot be added without a row. What a shared table opens
// instead is a MISAIMING hazard. A copy-pasted row whose accessor returns
// the neighbouring field compiles cleanly, and then `.scaleY(animate(...))`
// ramps `scaleX` in every consumer of the table at once. This walk is what
// makes that fail.

TEST(ComposeSlotPins, EverySlotRowReachesItsOwnFieldAtItsStandingDefault) {
  // Every block a slot can hide behind, PRESENT and defaulted — so every
  // non-Bespoke row must answer with a pointer, and that pointer must be at
  // the field's own default value.
  cd::ElementNode node;
  node.motionData.ensure();                 // travel(): carries kMotionT
  node.textData.ensure().onPath.emplace();  // onPath(): carries kTextPathAt
  node.depthData.ensure();  // the depth lanes: kRotateX … kPerspective

  std::vector<const sigil::motion::Animatable<float>*> seen;
  int bespoke = 0, opacityRows = 0;
  for (const cd::SlotSpec& spec : cd::kSlotSpecs) {
    const int index = (int)spec.slot;
    if (spec.role == cd::SlotRole::Bespoke) {
      ++bespoke;
      // The compile-time assert already pairs "no accessor" with "declared
      // Bespoke, with a reason"; this is the runtime half of that claim.
      EXPECT_EQ(spec.of, nullptr) << "slot " << index;
      EXPECT_EQ(cd::slotValueOf(spec, node), nullptr)
          << "slot " << index
          << ": a Bespoke row must be INERT through "
             "slotValueOf, so a consumer that walks the table without "
             "special-casing it does nothing rather than crashing";
      continue;
    }
    if (spec.role == cd::SlotRole::Opacity) ++opacityRows;
    const sigil::motion::Animatable<float>* v = cd::slotValueOf(spec, node);
    ASSERT_NE(v, nullptr) << "slot " << index
                          << "'s accessor reaches nothing "
                             "on a node carrying every block";
    // THE MISAIM. Two rows answering with the same address means one of them
    // is pointed at the other's field, and every consumer inherits the error.
    for (size_t i = 0; i < seen.size(); ++i)
      EXPECT_NE(v, seen[i])
          << "slot " << index << " and slot " << i << " aim at the SAME field";
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

// ---- maxScaleOf's perspective fallback -------------------------------------
//
// Skia's getMinMaxScales refuses a perspective matrix, so maxScaleOf needs a
// fallback — and the matrix DIAGONAL is the wrong one twice over. A rotation
// moves the whole scale into the skew terms (at 90° the diagonal is exactly
// zero), and perspective makes scale position-dependent while the diagonal
// reads no position at all. The quantized bake ladder consumes this number,
// so a wrong answer is a wrongly-stepped bake whenever the host CTM carries
// perspective.
//
// What maxScaleOf does instead is local linearization: the Jacobian of the
// projective map, maxed over the centre and four corners of the node's local
// bounds. This test holds it to a numerically derived ground truth — finite
// differences of the mapped point at those same five samples.

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
  for (SkPoint p :
       {SkPoint{rect.centerX(), rect.centerY()},
        SkPoint{rect.left(), rect.top()}, SkPoint{rect.right(), rect.top()},
        SkPoint{rect.right(), rect.bottom()},
        SkPoint{rect.left(), rect.bottom()}})
    truth = std::max(truth, fdSigmaMax(p));

  const float got = cd::maxScaleOf(m, rect);
  EXPECT_NEAR(got, truth, truth * 0.02f)
      << "the perspective fallback disagrees with the numerical Jacobian";

  // The gap the test exists to hold open: the diagonal answers essentially 0
  // for this matrix — which the bake ladder would clamp to its 0.25 floor,
  // baking quarter-resolution — while the true near-edge magnification is
  // past 1.5x.
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
