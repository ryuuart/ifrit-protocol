// poe_tree_study.cpp — a STUDY of Path of Exile's passive-tree linework
// (REFERENCES.md §5: skill-tree / node-graph brushes between elements).
// ROUND 2: the new router/trim/entrance surface.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/poe_tree_study.cpp
//
// The grammar transcribed here:
//   the 3-state law ..... every edge is Normal / Intermediate / Active —
//                         brushes::rope(state) is the verified PoE ladder
//                         (#3A332A → #6B5A40 → #8A7248 + halo); edges match
//                         their endpoint states, all three states shown
//   ring-state ladder ... sockets are SDF circles sized by their VISIBLE
//                         silhouette via public sdf::pad(style):
//                         unallocated pewter #4E463C; canAllocate rim-lit
//                         #8A7A5C + breathing glow (bound uGlowR);
//                         allocated DOUBLE gold ring #C9A96A + halo
//   orbit routing ....... EVERY edge is a rail(...) with
//                         routers::orbit(center): consecutive anchors on
//                         the same radius follow the CIRCLE (the
//                         Orbit{n}{State} arc assets — here four arcs,
//                         one Active, two Intermediate, one Normal);
//                         radius-changing legs stay straight spokes
//   wrap-mode comet ..... the orbit ring itself carries a Wrap-mode trim
//                         window (0.92 → 1.06 + bound offset) that
//                         MARCHES around the circle and straddles the
//                         path seam at the settled capture — proof the
//                         window survives the mod-1 wrap in two pieces
//   draw-on entrance .... the allocated rail a1→a2→a3→a4 enters with
//                         trim(0, withFrom(0→1, 800ms)) — no Output
//                         plumbing, the mount IS the reveal
//   pulse-travel ........ stock brushes::pulse() rides the spoke run of
//                         the allocated path (~280px/s, long gap, §5)
//   width law ........... state changes shift COLOR dramatically, width
//                         barely — hierarchy in socket size, progress in hue
//
// Headless: ComposeSketch <this> --frame poe.png --at 2.0
//           (--at 0.35 catches the withFrom draw-on mid-reveal)
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
constexpr SkColor4f kBone{0.835f, 0.769f, 0.616f, 1};
constexpr SkColor4f kAsh{0.447f, 0.404f, 0.337f, 1};

// The shared orbit five of the sockets sit on (PoE orbit radii ladder —
// this study's circle sits between the 82/162px rings). All in-ring
// edges are ARCS of this circle, courtesy of routers::orbit(kOrbitC).
constexpr SkPoint kOrbitC{610, 300};
constexpr float kOrbitR = 150;

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
 *  the box grows by the style's reserved pad so glow never clips.
 *  sdf::pad(style) is public now; no more hand-copied formula. */
Element socket(const char *key, SkPoint at, float dia, const sdf::Style &st,
               const ch::Output<float> *breathingGlow = nullptr) {
  const float pad = sdf::pad(st);
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

/** Full-circle outline for the orbit ring, centered in the node. */
std::function<SkPath(SkSize)> circleOutline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.addCircle(s.width() * 0.5f, s.height() * 0.5f, s.width() * 0.5f);
    return b.detach();
  };
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
  // Output-driven reveals stay on the frontier edges (staggered Holds —
  // withFrom cannot stagger, see the friction note in edge()); the
  // allocated rail enters via withFrom instead. Plus the pulse window
  // pair, the wrap-comet phase, and the canAllocate breathing.
  std::array<ch::Output<float>, 6> edgeReveal;
  ch::Output<float> pulseS{0}, pulseE{0};
  ch::Output<float> ringPhase{0};
  ch::Output<float> breathA{5.5f}, breathB{5.5f};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kBg);

    auto &tl = ctx.ticker.timeline();
    for (size_t i = 0; i < edgeReveal.size(); ++i) {
      edgeReveal[i] = 0.0f;
      tl.apply(&edgeReveal[i])
          .then<ch::Hold>(0.0f, 0.15f + 0.12f * (float)i)
          .then<ch::RampTo>(1.0f, 0.6f, &ch::easeOutQuad);
    }

    // canAllocate breathing (two phases so the sockets don't sync); the
    // traveling pulse: window [s, s+0.12] sweeps the ~323px spoke run in
    // 1.15s (≈280px/s, §5) then rests until the 2.6s cycle ends; and the
    // wrap-comet phase: 0.5 rev/s, so at --at 2.0 the offset lands on an
    // exact revolution and the window sits straddling the circle's seam.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // Phases chosen so the settled capture (t=2.0) catches group A
      // (c1/c3) near peak glow and group B (c2) dim — the asymmetry is
      // the point of the two-phase breathe.
      breathA = 5.5f + 3.5f * (float)std::sin(t * 2.1 + 3.0);
      breathB = 5.5f + 3.5f * (float)std::sin(t * 2.1 + 0.8);
      const float cycle = (float)std::fmod(t, 2.6);
      const float u = (cycle - 1.02f) / 1.15f; // travel starts at 1.02s
      const float s = -0.12f + u * 1.12f;
      pulseS = s;
      pulseE = s + 0.12f;
      ringPhase = (float)std::fmod(t * 0.5, 1.0);
      return true;
    });

    ctx.composer.render(describe());
  }

  /** A rope edge between two sockets, state-matched to its endpoints
   *  (the 3-state law). ONE router for every edge: routers::orbit —
   *  same-radius anchor pairs become in-ring arcs, everything else a
   *  straight spoke. Frontier edges keep Output reveals because
   *  withFrom has no delay: a stagger still needs the timeline. */
  Element edge(const char *a, const char *b, int state, size_t reveal) {
    return rail({{a}, {b}}, routers::orbit(kOrbitC))
        .absolute().inset(0)
        .trim(0.0f, &edgeReveal[reveal])
        .stroke(brushes::rope(state))
        .zIndex(1);
  }

  /** The orbit ring + its wrap-mode comet: a faint guide circle, and on
   *  top the SAME outline trimmed to a fixed [0.92, 1.06] window whose
   *  offset is a bound wrapping phase — TrimMode::Wrap keeps both pieces
   *  when the window crosses the seam (the path start, canvas angle 0°,
   *  which sits in the arc-free stretch of the ring), so the highlight
   *  marches around the circle forever without a blink. */
  void orbitRing(Element &parent) {
    const float h = kOrbitR;
    auto ringBox = [&] {
      return box().width(2 * kOrbitR).height(2 * kOrbitR)
          .inset(kOrbitC.x() - h, kOrbitC.y() - h, W - kOrbitC.x() - h,
                 H - kOrbitC.y() - h)
          .absolute()
          .outline(circleOutline());
    };
    parent.child(ringBox()
                     .stroke(LayeredBrush{{{1.2f,
                                            {kPewter.fR, kPewter.fG,
                                             kPewter.fB, 0.35f}}}})
                     .zIndex(0));
    parent.child(ringBox()
                     .trim(0.92f, 1.06f, &ringPhase, TrimMode::Wrap)
                     .stroke(brushes::pulse(
                         {kHalo.fR, kHalo.fG, kHalo.fB, 0.22f},
                         {1, 1, 1, 0.75f}, 0.72f))
                     .zIndex(2));
  }

  /** Allocated socket: double gold ring + warm halo (+ a small 4-point
   *  star icon on notables). Box sized via public sdf::pad(style). */
  void allocated(Element &parent, const char *key, SkPoint at, float dia,
                 bool notable) {
    // Declared glowRadius reserves the pad (exp falloff reaches ~0 before
    // the box edge); the ACTUAL halo runs at 6.5 via a uniform override.
    const sdf::Style st{.fill = kSocket,
                        .borderWidth = 3.2f,
                        .borderColor = kGold,
                        .glowRadius = 13,
                        .glowColor = {kHalo.fR, kHalo.fG, kHalo.fB, 0.5f}};
    Material m = sdf::material(sdf::circle(), st);
    m.uniform("uGlowR", 6.5f);
    const float pad = sdf::pad(st);
    const float boxSize = dia + 2 * pad, hh = boxSize * 0.5f;
    parent.child(box().width(boxSize).height(boxSize)
                     .inset(at.x() - hh, at.y() - hh, W - at.x() - hh,
                            H - at.y() - hh)
                     .absolute().key(key).fill(std::move(m)).zIndex(3));
    parent.child(socket(nullptr, at, dia - 13,
                        {.fill = {0, 0, 0, 0},
                         .borderWidth = 1.8f,
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
    // Socket positions — a1→a2→a3→a4 is the allocated path (two spokes,
    // then an ARC: a3 and a4 share the orbit). Ring occupancy, walking
    // the circle: c3 @100°, a3 @160°, a4 @205°, c1 @250°, u2 @320° —
    // arcs cover 100°→320°, leaving the seam stretch (through 0°) free
    // for the wrap comet. c2/u1/u3 hang off-ring on straight spokes.
    const SkPoint a1{170, 468}, a2{332, 386};
    const SkPoint a3 = onOrbit(160);                    // (469, 351)
    const SkPoint a4 = onOrbit(205);                    // (474, 237)
    const SkPoint c1 = onOrbit(250);                    // (559, 159)
    const SkPoint c2{296, 216};
    const SkPoint c3 = onOrbit(100);                    // (584, 448)
    const SkPoint u1{120, 268};
    const SkPoint u2 = onOrbit(320);                    // (725, 204)
    const SkPoint u3{872, 470};

    auto root = stack().fill(
        Material::radial({W * 0.5f, H * 0.48f}, 640,
                         {{0.0f, kBgLift}, {0.55f, kBg}, {1.0f, kBgSink}}));

    // --- the orbit ring and its traveling wrap-window highlight ---
    orbitRing(root);

    // --- ropes beneath the sockets (state matches endpoints); the
    //     orbit router turns every same-radius pair into an in-ring arc:
    //     a3→c3, a4→c1 (Intermediate), c1→u2 (Normal) — and the last leg
    //     of the allocated rail below (Active). Spokes stay straight. ---
    root.child(edge("a2", "c2", 1, 0)); // Intermediate spoke
    root.child(edge("a3", "c3", 1, 1)); // Intermediate ARC (160°→100°)
    root.child(edge("a4", "c1", 1, 2)); // Intermediate ARC (205°→250°)
    root.child(edge("c1", "u2", 0, 3)); // Normal ARC (250°→320°)
    root.child(edge("c2", "u1", 0, 4)); // Normal spoke
    root.child(edge("c3", "u3", 0, 5)); // Normal spoke

    // --- the allocated path: ONE rail, spokes + the Active arc, drawing
    //     itself on at mount via withFrom — no Output, no timeline. ---
    root.child(rail({{"a1"}, {"a2"}, {"a3"}, {"a4"}}, routers::orbit(kOrbitC))
                   .absolute().inset(0)
                   .trim(0.0f, withFrom(0.0f, 1.0f, {800ms}))
                   .stroke(brushes::rope(2))
                   .zIndex(1));

    // --- the traveling pulse packet on the SPOKE run (stock brush) ---
    root.child(rail({{"a1"}, {"a2"}, {"a3"}}, routers::orbit(kOrbitC))
                   .absolute().inset(0)
                   .trim(&pulseS, &pulseE)
                   .stroke(brushes::pulse({kHalo.fR, kHalo.fG, kHalo.fB,
                                           0.35f},
                                          {1, 1, 1, 0.9f}, 1.25f))
                   .zIndex(2));

    // --- sockets: the ring-state ladder ---
    allocated(root, "a1", a1, 40, true);
    allocated(root, "a2", a2, 32, false);
    allocated(root, "a3", a3, 40, true);
    allocated(root, "a4", a4, 32, false);

    const sdf::Style canAlloc{
        .fill = kSocket,
        .borderWidth = 2.6f,
        .borderColor = kRimLit,
        .glowRadius = 14, // generous pad reserve; the bind stays ≤ 9
        .glowColor = {kGold.fR, kGold.fG, kGold.fB, 0.45f}};
    root.child(socket("c1", c1, 32, canAlloc, &breathA));
    root.child(socket("c2", c2, 32, canAlloc, &breathB));
    root.child(socket("c3", c3, 32, canAlloc, &breathA));

    const sdf::Style unalloc{.fill = kSocket,
                             .borderWidth = 2.0f,
                             .borderColor = kPewter};
    root.child(socket("u1", u1, 27, unalloc));
    root.child(socket("u2", u2, 27, unalloc));
    root.child(socket("u3", u3, 29, unalloc));

    // --- HUD chrome on per-side pins (top/left, top/right, bottom/left) ---
    root.child(box().column().top(34).left(40).zIndex(5)
                   .child(text(toU8("EMBERWOOD REACH"), type(26, kBone, 3)))
                   .child(text(toU8("passive cluster — three-state rope study"),
                               type(13, kAsh, 1.2f))
                              .margin(0, 6, 0, 0)));
    root.child(box().column().alignItems(Align::End).top(34).right(40)
                   .zIndex(5)
                   .child(text(toU8("4 / 123"), type(22, kGold, 2)))
                   .child(text(toU8("passive points"), type(11, kAsh, 1.5f))
                              .margin(0, 4, 0, 0)));
    auto swatch = [&](int state, const char *label) {
      return box().row().alignItems(Align::Center).gap(9)
          .child(box().width(56).height(16).outline(hline())
                     .stroke(brushes::rope(state)))
          .child(text(toU8(label), type(12, kAsh, 1)));
    };
    root.child(box().row().gap(26).alignItems(Align::Center)
                   .bottom(30).left(40).zIndex(5)
                   .child(swatch(0, "normal"))
                   .child(swatch(1, "intermediate"))
                   .child(swatch(2, "active")));
    return root;
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(PoeTreeStudy)
