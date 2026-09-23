// THE FIELD WALK — the runtime half of ComposeInternal.h's field pins.
//
// `propertiesEqual()` and its helpers compare a description FIELD BY FIELD, and
// a field left out fails INVISIBLY: two different descriptions compare equal,
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
//
// THIS FILE REACHES THE LIBRARY'S OWN SOURCE DIRECTORY, as do
// ComposeTestSlotConsumers.cpp and ComposeTestDeclareDoor.cpp beside it and
// nothing else. That is a
// stated exception rather than an oversight: a field left out of a
// comparator is unfalsifiable from outside, because from outside the two
// descriptions simply compare equal and the tree simply prunes. Every case
// here needs the comparator or the slot table itself; a case that needs
// neither belongs in the binary whose tier owns its subject, not here. What
// the slot table's CONSUMERS do with a row — whether the property ramps, is
// volatile, refuses a group's bake — is scenes rendered through a host, and
// lives in that file.

#include <boost/pfr/core.hpp>

#include "../ComposeInternal.h"
#include "support/CoreTestSupport.h"

// …and the RUNTIME header, for the kSlotSpecs walk at the bottom of the file.
// It is the reason this target links yoga::yogacore (see CMakeLists.txt).
#include "../ComposeRuntime.h"

namespace cd = sigil::compose::detail;

/** The walk's way into the declared state: one part of it put on a node
 *  with nothing else, which no writer can do. */
struct cd::DeclaredFieldsTestAccess {
  static DeclaredFields::Storage& storage(DeclaredFields& fields) {
    return fields.m_storage;
  }
};

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

void perturb(material::Backface& v) { v = material::Backface::Hidden; }

void perturb(SkBlendMode& v) { v = SkBlendMode::kMultiply; }

void perturb(BackgroundOrigin& v) { v = BackgroundOrigin::ContentBox; }

void perturb(Corners& v) { v.topLeft += 1.0f; }

void perturb(Dimension& v) { v = Dimension(v.value + 1.0f); }

void perturb(cd::LayoutProps& v) { v.gap = Dimension(v.gap.value + 1.0f); }

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
  v.emplace_back(PaintProgram{[] {}});
}

void perturb(std::vector<Element>& v) { v.push_back(box()); }

void perturb(cd::PaintProps& v) { perturb(v.opacity); }

// A value and its bit together, as a writer moves them; the walk over
// DeclaredFields::Storage moves each part alone.
void perturb(cd::DeclaredFields& v) {
  v.gap() = Dimension(v.layout().gap.value + 1.0f);
}

void perturb(PropertyMask& v) { v.set(Property::Gap); }

void perturb(sigil::weave::Unit& v) { v = sigil::weave::Unit::Line; }

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

// The cascade pass re-folds every node that writes a keyword, on every
// pass, and asks this comparator whether the second fold moved anything
// before it invalidates. A field left out of it reads as a property that
// did not move, and the picture recorded from the old value replays —
// the same invisible failure the description's comparator has, one phase
// later.

TEST(ComposeDeclarations, EveryComputedStyleFieldParticipatesInEquality) {
  static const char* const kNames[] = {"layout", "paint", "corners",
                                       "clipContent"};
  static const bool kParticipates[] = {true, true, true, true};
  walkFields<cd::ComputedStyle>(cd::computedStyleEqual, kNames, kParticipates);
}

TEST(ComposeDeclarations, EveryPaintPropsFieldParticipatesInTheStyleCompare) {
  // The paint block is the half of the computed style with a hand-written
  // comparison, so it is walked field by field there as it is here.
  static const char* const kNames[] = {
      "fill",       "opacity", "blendMode", "backgroundOrigin", "translateX",
      "translateY", "rotate",  "scale",     "scaleX",           "scaleY",
      "skewX",      "skewY",   "originX",   "originY",          "zIndex"};
  static const bool kParticipates[] = {true, true, true, true, true,
                                       true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<cd::PaintProps>(
      [](const cd::PaintProps& a, const cd::PaintProps& b) {
        cd::ComputedStyle sa, sb;
        sa.paint = a;
        sb.paint = b;
        return cd::computedStyleEqual(sa, sb);
      },
      kNames, kParticipates);
}

TEST(ComposeReconcile, EveryPaintPropsFieldParticipatesInEquality) {
  // No legitimate exclusion exists in this block: every field of PaintProps
  // is a lane the painter reads live, so every one must reach the comparator.
  // The per-axis scales are the easiest ones to leave out, because the
  // uniform `scale` beside them makes a comparator look complete.
  static const char* const kNames[] = {
      "fill",       "opacity", "blendMode", "backgroundOrigin", "translateX",
      "translateY", "rotate",  "scale",     "scaleX",           "scaleY",
      "skewX",      "skewY",   "originX",   "originY",          "zIndex"};
  static const bool kParticipates[] = {true, true, true, true, true,
                                       true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<cd::PaintProps>(
      [](const cd::PaintProps& a, const cd::PaintProps& b) {
        cd::ElementNode na, nb;
        cd::DeclaredFieldsTestAccess::storage(na.fields).paint = a;
        cd::DeclaredFieldsTestAccess::storage(nb.fields).paint = b;
        return cd::propertiesEqual(na, nb);
      },
      kNames, kParticipates);
}

TEST(ComposeReconcile, EveryDeclaredFieldsPartParticipatesInEquality) {
  // Each part moved ALONE — a value with no bit, a bit with no value, an
  // empty keyword table — so the compare must read every part itself and
  // cannot pass on the mask that a writer moves beside every value.
  static const char* const kNames[] = {"layout",      "paint",    "corners",
                                       "clipContent", "declared", "keywords"};
  static const bool kParticipates[] = {true, true, true, true, true, true};
  walkFields<cd::DeclaredFields::Storage>(
      [](const cd::DeclaredFields::Storage& a,
         const cd::DeclaredFields::Storage& b) {
        cd::ElementNode na, nb;
        cd::DeclaredFieldsTestAccess::storage(na.fields) = a;
        cd::DeclaredFieldsTestAccess::storage(nb.fields) = b;
        return cd::propertiesEqual(na, nb);
      },
      kNames, kParticipates);
}

TEST(ComposeReconcile, EveryDepthDataFieldParticipatesInEquality) {
  // The depth block is read live at paint exactly as PaintProps is: five
  // lanes, the two origins, the transform origin's depth and the two
  // modes. A lane left out keeps the plane at the turn it was recorded at;
  // a mode left out keeps a space open, or a back drawn, that the author
  // closed.
  static const char* const kNames[] = {"rotateX",
                                       "rotateY",
                                       "translateZ",
                                       "scaleZ",
                                       "perspective",
                                       "perspectiveOriginX",
                                       "perspectiveOriginY",
                                       "originZ",
                                       "preserve3d",
                                       "backface"};
  static const bool kParticipates[] = {true, true, true, true, true,
                                       true, true, true, true, true};
  walkFields<cd::DepthData>(
      [](const cd::DepthData& a, const cd::DepthData& b) {
        cd::ElementNode na, nb;
        na.depthData.ensure() = a;
        nb.depthData.ensure() = b;
        return cd::propertiesEqual(na, nb);
      },
      kNames, kParticipates);
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
                                       "waveFunction",
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

TEST(ComposeReconcile, EveryElementNodeFieldParticipatesInEquality) {
  // Two fields must NOT reach propertiesEqual(), and the table asserts their
  // inertness rather than describing it, so an accidental inclusion fails
  // here too:
  //
  //  - `memoData` never reaches propertiesEqual at all. resolveMemo() compares
  //  a
  //    memo EARLIER and more strictly (the environment snapshot, then the
  //    author's own properties comparator) and `inst.description` holds the
  //    memo's PRODUCED payload, which carries no memo block.
  //  - `children` are reconciled BY KEY, not compared. A node that prunes
  //    still walks them — that is the whole point of the structural prune.
  static const char* const kNames[] = {
      "kind",       "boundary",       "coverageThreshold", "key",
      "fields",     "shapeFn",        "hitTestable",       "cacheMode",
      "bakeScale",  "nodeTransition", "backgrounds",       "foregrounds",
      "textData",   "imageData",      "customData",        "deriveData",
      "fxData",     "materialData",   "strokeData",        "memoData",
      "motionData", "depthData",      "cascadeData",       "operatorData",
      "children"};
  static const bool kParticipates[] = {
      true,
      true,
      true,
      true,
      true,  // fields — the declared values, the mask of which properties
             // were stated, and the keywords, each walked alone above
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      true,
      false,  // memoData — resolveMemo owns it, and it never lands in
              // description
      true,
      true,
      true,   // cascadeData — the font, the ink's property and the custom
              // properties a node declares for everything under it
      true,   // operatorData — the facts a node states and the operators
              // it runs; a present block with neither is still a change
      false,  // children — reconciled by key, never compared
  };
  walkFields<cd::ElementNode>(cd::propertiesEqual, kNames, kParticipates);
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
  node.textData.ensure().onPath.emplace();  // textOnPath(): carries kTextPathAt
  node.depthData.ensure();  // the depth lanes: kRotateX … kPerspective
  // A row reads a PROPERTY lane off the computed style and a positional one
  // off the description's block, so the walk hands it both.
  cd::ComputedStyle style;
  cd::resolveStyle(nullptr, node, style);
  const cd::StyledNode styled{style, node};

  std::vector<const sigil::motion::Animatable<float>*> seen;
  int bespoke = 0, opacityRows = 0;
  for (const cd::SlotSpec& spec : cd::kSlotSpecs) {
    const int index = (int)spec.slot;
    if (spec.role == cd::SlotRole::Bespoke) {
      ++bespoke;
      // The compile-time assert already pairs "no accessor" with "declared
      // Bespoke, with a reason"; this is the runtime half of that claim.
      EXPECT_EQ(spec.of, nullptr) << "slot " << index;
      EXPECT_EQ(cd::slotValueOf(spec, styled), nullptr)
          << "slot " << index
          << ": a Bespoke row must be INERT through "
             "slotValueOf, so a consumer that walks the table without "
             "special-casing it does nothing rather than crashing";
      continue;
    }
    if (spec.role == cd::SlotRole::Opacity) ++opacityRows;
    const sigil::motion::Animatable<float>* v = cd::slotValueOf(spec, styled);
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

// A property row must read the COMPUTED style and not the description it was
// filled from. The two hold the same value today, so a row still pointed at
// the description passes every assertion above — and then, the moment a rule
// or an inherited value writes an answer the description never carried, that
// property snaps where every other one eases, silently.
//
// What tells the two apart is the ADDRESS, not the value: the style here is
// a separate object standing beside the node, so a row reading the style
// answers inside `style.paint` and a row still reading the node answers
// inside `node.paint`. A row left on the node is then counted as positional
// and the total falls short.
TEST(ComposeSlotPins, EveryPropertyRowReadsTheComputedStyleAndNotTheNode) {
  cd::ElementNode node;
  node.motionData.ensure();
  node.textData.ensure().onPath.emplace();
  node.depthData.ensure();
  cd::ComputedStyle style;
  cd::resolveStyle(nullptr, node, style);

  // One address per lane of the style's own paint block, so a row that
  // answers with one of them is reading the style.
  const void* paintLanes[] = {
      &style.paint.opacity, &style.paint.translateX, &style.paint.translateY,
      &style.paint.rotate,  &style.paint.scale,      &style.paint.scaleX,
      &style.paint.scaleY,  &style.paint.skewX,      &style.paint.skewY};
  const cd::StyledNode styled{style, node};
  int propertyRows = 0;
  for (const cd::SlotSpec& spec : cd::kSlotSpecs) {
    const sigil::motion::Animatable<float>* v = cd::slotValueOf(spec, styled);
    if (v == nullptr) continue;
    bool fromStyle = false;
    for (const void* lane : paintLanes) fromStyle |= (lane == v);
    if (!fromStyle) continue;  // a positional lane, read off the block
    ++propertyRows;
  }
  EXPECT_EQ(propertyRows, (int)std::size(paintLanes))
      << "every transform and opacity lane must come from the computed "
         "style; a row still reading the description's own paint block "
         "leaves that property unable to ramp from a resolved value";
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
