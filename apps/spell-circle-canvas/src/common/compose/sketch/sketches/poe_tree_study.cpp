// poe_tree_study.cpp — a STUDY of Path of Exile's passive-tree linework
// (REFERENCES.md §5: skill-tree / node-graph brushes between elements).
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/poe_tree_study.cpp
//
// The grammar transcribed here:
//   the 3-state law ..... every edge is Normal / Intermediate / Active —
//                         brushes::rope(state) is the verified PoE ladder
//                         (#3A332A → #6B5A40 → #8A7248 + halo); edges match
//                         their endpoint states, all three states shown
//   ring-state ladder ... sockets are SDF circles: unallocated pewter
//                         #4E463C ring; canAllocate rim-lit #8A7A5C with a
//                         breathing glow (bound uGlowR); allocated DOUBLE
//                         gold ring #C9A96A + warm halo #FFC970
//   orbit arcs .......... the Orbit{n}{State} idiom — two nodes sharing a
//                         circle are joined by an ARC of that circle: a
//                         stroked circle outline() trimmed to the arc span,
//                         rope-brushed like any straight connector
//   pulse-travel ........ a 40px bright window rides the allocated path at
//                         ~280px/s with a long gap (§5 pulse law): a second
//                         rail whose trim window [phase, phase+0.12] is
//                         bound to a looping Output pair
//   width law ........... state changes shift COLOR dramatically, width
//                         barely — hierarchy in socket size, progress in hue
//
// Headless: ComposeSketch <this> --frame poe.png --at 1.5
//
// REFERENCES.md §5 is the ground truth for every number above.

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>

#include <include/core/SkPathBuilder.h>

#include <array>
#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;
using namespace std::chrono_literals;

namespace {

constexpr float W = 960, H = 640;

// Near-black ground and the PoE ladder (REFERENCES.md §5).
constexpr SkColor4f kBg{0.098f, 0.082f, 0.071f, 1};      // #191512
constexpr SkColor4f kBgLift{0.125f, 0.104f, 0.086f, 1};  // center lift
constexpr SkColor4f kBgSink{0.072f, 0.060f, 0.052f, 1};  // vignette edge
constexpr SkColor4f kSocket{0.129f, 0.106f, 0.082f, 1};  // socket well
constexpr SkColor4f kPewter{0.306f, 0.275f, 0.235f, 1};  // #4E463C
constexpr SkColor4f kRimLit{0.541f, 0.478f, 0.361f, 1};  // #8A7A5C
constexpr SkColor4f kGold{0.788f, 0.663f, 0.416f, 1};    // #C9A96A
constexpr SkColor4f kHalo{1.0f, 0.788f, 0.439f, 1};      // #FFC970
constexpr SkColor4f kSpec{0.922f, 0.839f, 0.643f, 1};    // #EBD6A4
constexpr SkColor4f kBone{0.835f, 0.769f, 0.616f, 1};
constexpr SkColor4f kAsh{0.447f, 0.404f, 0.337f, 1};

// The shared orbit two of the sockets sit on (PoE orbit radii ladder —
// this study's circle sits between the 82/162px rings).
constexpr SkPoint kOrbitC{610, 300};
constexpr float kOrbitR = 150;
// a3 sits at canvas angle 160°, c1 at 250° — the arc spans 90°.
constexpr float kArcStartDeg = 160, kArcSweepDeg = 90;

SkPoint onOrbit(float canvasDeg) {
  const float rad = canvasDeg * 3.14159265f / 180.0f;
  return {kOrbitC.x() + kOrbitR * std::cos(rad),
          kOrbitC.y() + kOrbitR * std::sin(rad)};
}

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** A socket ring: SDF circle chrome sized by the VISIBLE ring diameter —
 *  the box grows by the style's pad (border half-width + glow reach) so
 *  glow never clips. Mirrors sdf::material's own pad formula. */
Element socket(const char *key, SkPoint at, float dia, const sdf::Style &st,
               const ch::Output<float> *breathingGlow = nullptr) {
  const float glowPad = st.glowRadius > 0 ? st.glowRadius * 2.5f : 0.0f;
  const float pad = st.borderWidth * 0.5f + glowPad + 1.0f;
  const float boxSize = dia + 2 * pad, h = boxSize * 0.5f;
  Material m = sdf::material(sdf::circle(), st);
  if (breathingGlow)
    m.uniform("uGlowR", breathingGlow);
  Element e = box().width(boxSize).height(boxSize)
                  .inset(at.x() - h, at.y() - h, W - at.x() - h,
                         H - at.y() - h)
                  .absolute()
                  .fill(std::move(m))
                  .zIndex(3);
  if (key)
    e.key(key);
  return e;
}

/** The bright pulse window riding an Active edge (§5 pulse-travel:
 *  transparent → color → white core). */
LayeredBrush pulseBrush() {
  return LayeredBrush{{
      {14, {kHalo.fR, kHalo.fG, kHalo.fB, 0.14f}, 6, {}, 0, SkBlendMode::kPlus},
      {5, {kSpec.fR, kSpec.fG, kSpec.fB, 0.95f}},
      {1.8f, {1, 1, 1, 0.85f}},
  }};
}

/** Horizontal line outline for the legend swatches. */
std::function<SkPath(SkSize)> hline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
}

} // namespace

struct PoeTreeStudy : sketch::Sketch {
  // One reveal per straight rope edge (draw-on, staggered), one for the
  // orbit arc (ramps to sweep/360), and the bound pulse window pair.
  std::array<ch::Output<float>, 7> edgeReveal;
  ch::Output<float> arcReveal{0};
  ch::Output<float> pulseS{0}, pulseE{0};
  ch::Output<float> breathA{5.5f}, breathB{5.5f};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kBg);

    auto &tl = ctx.ticker.timeline();
    for (size_t i = 0; i < edgeReveal.size(); ++i) {
      edgeReveal[i] = 0.0f;
      tl.apply(&edgeReveal[i])
          .then<ch::Hold>(0.0f, 0.08f + 0.11f * (float)i)
          .then<ch::RampTo>(1.0f, 0.6f, &ch::easeOutQuad);
    }
    arcReveal = 0.0f;
    tl.apply(&arcReveal).then<ch::Hold>(0.0f, 0.5f).then<ch::RampTo>(
        kArcSweepDeg / 360.0f, 0.7f, &ch::easeOutQuad);

    // canAllocate breathing (two phases so the sockets don't sync) and
    // the traveling pulse: window [s, s+0.12] sweeps the ~322px allocated
    // path in 1.15s (≈280px/s, §5), then rests until the 2.6s cycle ends.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      breathA = 5.5f + 3.5f * (float)std::sin(t * 2.1);
      breathB = 5.5f + 3.5f * (float)std::sin(t * 2.1 + 2.2);
      const float cycle = (float)std::fmod(t, 2.6);
      const float u = (cycle - 0.35f) / 1.15f; // travel starts at 0.35s
      const float s = -0.12f + u * 1.12f;
      pulseS = s;
      pulseE = s + 0.12f;
      return true;
    });

    ctx.composer.render(describe());
  }

  /** A rope edge between two sockets, state-matched to its endpoints
   *  (the 3-state law), drawing itself on via a bound trim. */
  Element edge(const char *a, const char *b, int state, size_t reveal) {
    return rail({{a}, {b}}, routers::polyline())
        .absolute().inset(0)
        .trim(0.0f, &edgeReveal[reveal])
        .stroke(brushes::rope(state))
        .zIndex(1);
  }

  /** The orbit arc — Orbit{n}{Intermediate}: a full-circle outline on the
   *  shared orbit, trimmed to the 90° span between a3 and c1. */
  Element orbitArc() {
    const float h = kOrbitR;
    return box().width(2 * kOrbitR).height(2 * kOrbitR)
        .inset(kOrbitC.x() - h, kOrbitC.y() - h, W - kOrbitC.x() - h,
               H - kOrbitC.y() - h)
        .absolute()
        .outline([](SkSize s) {
          SkPathBuilder b;
          b.addArc(SkRect::MakeWH(s.width(), s.height()), kArcStartDeg,
                   359.9f);
          return b.detach();
        })
        .trim(0.0f, &arcReveal)
        .stroke(brushes::rope(1))
        .zIndex(1);
  }

  /** Allocated socket: double gold ring + warm halo (+ a small 4-point
   *  star icon on notables). */
  void allocated(Element &parent, const char *key, SkPoint at, float dia,
                 bool notable) {
    parent.child(socket(key, at, dia,
                        {.fill = kSocket,
                         .borderWidth = 3.0f,
                         .borderColor = kGold,
                         .glowRadius = 8,
                         .glowColor = {kHalo.fR, kHalo.fG, kHalo.fB, 0.5f}}));
    parent.child(socket(nullptr, at, dia - 11,
                        {.fill = {0, 0, 0, 0},
                         .borderWidth = 1.7f,
                         .borderColor = kGold}));
    if (notable) {
      const float h = 8;
      parent.child(
          box().width(2 * h).height(2 * h)
              .inset(at.x() - h, at.y() - h, W - at.x() - h, H - at.y() - h)
              .absolute()
              .fill(sdf::material(
                  sdf::star(4, 2.4f),
                  {.fill = {kGold.fR, kGold.fG, kGold.fB, 0.9f}}))
              .zIndex(4));
    }
  }

  Element describe() {
    // Socket positions — a1→a2→a3 is the allocated path; a3 and c1 share
    // the orbit circle; c* are canAllocate frontier, u* unallocated.
    const SkPoint a1{170, 468}, a2{332, 386};
    const SkPoint a3 = onOrbit(kArcStartDeg);                    // (469,351)
    const SkPoint c1 = onOrbit(kArcStartDeg + kArcSweepDeg);     // (559,159)
    const SkPoint c2{296, 216}, c3 = onOrbit(15);                // (755,339)
    const SkPoint u1{120, 268}, u2{795, 143}, u3{872, 470};

    auto root = stack().fill(
        Material::radial({W * 0.5f, H * 0.48f}, 640,
                         {{0.0f, kBgLift}, {0.55f, kBg}, {1.0f, kBgSink}}));

    // --- ropes beneath everything (state matches endpoints) ---
    root.child(edge("a1", "a2", 2, 0)); // Active: both allocated
    root.child(edge("a2", "a3", 2, 1));
    root.child(edge("a2", "c2", 1, 2)); // Intermediate: one end allocated
    root.child(edge("a3", "c3", 1, 3));
    root.child(edge("c2", "u1", 0, 4)); // Normal: no allocated end
    root.child(edge("c1", "u2", 0, 5));
    root.child(edge("c3", "u3", 0, 6));
    root.child(orbitArc()); // Orbit{Intermediate} arc a3 → c1

    // --- the traveling pulse on the Active path (both trim ends bound) ---
    root.child(rail({{"a1"}, {"a2"}, {"a3"}}, routers::polyline())
                   .absolute().inset(0)
                   .trim(&pulseS, &pulseE)
                   .stroke(pulseBrush())
                   .zIndex(2));

    // --- sockets: the ring-state ladder ---
    allocated(root, "a1", a1, 34, true);
    allocated(root, "a2", a2, 28, false);
    allocated(root, "a3", a3, 34, true);

    const sdf::Style canAlloc{
        .fill = kSocket,
        .borderWidth = 2.5f,
        .borderColor = kRimLit,
        .glowRadius = 9, // pad reserve = the breathing bind's maximum
        .glowColor = {kGold.fR, kGold.fG, kGold.fB, 0.45f}};
    root.child(socket("c1", c1, 28, canAlloc, &breathA));
    root.child(socket("c2", c2, 28, canAlloc, &breathB));
    root.child(socket("c3", c3, 28, canAlloc, &breathA));

    const sdf::Style unalloc{.fill = kSocket,
                             .borderWidth = 2.0f,
                             .borderColor = kPewter};
    root.child(socket("u1", u1, 24, unalloc));
    root.child(socket("u2", u2, 24, unalloc));
    root.child(socket("u3", u3, 26, unalloc));

    // --- title + the 3-state legend ---
    root.child(box().column().absolute().inset(40, 34, 0, 0).zIndex(5)
                   .child(text(toU8("EMBERWOOD REACH"), type(26, kBone, 3)))
                   .child(text(toU8("passive cluster — three-state rope study"),
                               type(13, kAsh, 1.2f))
                              .margin(0, 6, 0, 0)));
    auto swatch = [&](int state, const char *label) {
      return box().row().alignItems(Align::Center).gap(9)
          .child(box().width(56).height(16).outline(hline())
                     .stroke(brushes::rope(state)))
          .child(text(toU8(label), type(12, kAsh, 1)));
    };
    root.child(box().row().gap(26).alignItems(Align::Center)
                   .absolute().inset(40, 588, 0, 0).zIndex(5)
                   .child(swatch(0, "normal"))
                   .child(swatch(1, "intermediate"))
                   .child(swatch(2, "active")));
    return root;
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(PoeTreeStudy)
