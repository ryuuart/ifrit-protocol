#pragma once
// The Path of Exile passive-tree linework study (REFERENCES.md section 5 --
// skill-tree / node-graph brushes between elements), ported from
// sketch/sketches/poe_tree_study.cpp. The grammar transcribed here:
//
//   the 3-state law ..... every edge is Normal / Intermediate / Active --
//                         brushes::rope(state) is the verified PoE ladder
//                         (#3A332A -> #6B5A40 -> #8A7248 + halo); edges match
//                         their endpoint states, all three states shown
//   ring-state ladder ... sockets are SDF circles sized by their VISIBLE
//                         silhouette via sdf::minBoxFor(style, dia):
//                         unallocated pewter #4E463C; canAllocate rim-lit
//                         #8A7A5C + breathing glow (bound uGlowR);
//                         allocated DOUBLE gold ring #C9A96A + halo
//   orbit routing ....... EVERY edge is a rail(...) with
//                         routers::orbit(center): consecutive anchors on
//                         the same radius follow the CIRCLE (four arcs --
//                         one Active, two Intermediate, one Normal);
//                         radius-changing legs stay straight spokes
//   wrap-mode comet ..... the orbit ring itself carries a Wrap-mode trim
//                         window (0.92 -> 1.06 + bound offset) that
//                         MARCHES around the circle and survives the
//                         mod-1 wrap across the path seam in two pieces
//   draw-on entrance .... the allocated rail a1->a2->a3->a4 enters with
//                         trim(0, withFrom(0 -> 1, 800ms)) -- no Output
//                         plumbing, the mount IS the reveal
//   pulse-travel ........ stock brushes::pulse() rides the spoke run of
//                         the allocated path (~280 px/s, long gap)
//   width law ........... state changes shift COLOR dramatically, width
//                         barely -- hierarchy in socket size, progress
//                         in hue
//
// Ported per the ScenesKinetic.h exemplar: the 960x640 sketch canvas is
// recomposed for the 900x640 stage (orbit center + radius scale by ~0.75,
// off-ring nodes re-seated to keep the sweep), constants/helpers live in
// the scene-private skill_tree namespace, and the loop clocks re-zero in
// setup().

#include "GalleryCore.h"

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>

#include <include/core/SkPathBuilder.h>

#include <array>
#include <cmath>
#include <functional>

namespace compose_gallery {

namespace skill_tree {

// Near-black ground and the PoE ladder (REFERENCES.md section 5).
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

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// The shared orbit all in-ring sockets sit on (PoE orbit radii ladder --
// the sketch's 150 px ring at (610, 300) on a 960-wide canvas, rescaled
// for the 900x640 stage). All in-ring edges are ARCS of this circle,
// courtesy of routers::orbit(kOrbitC).
constexpr SkPoint kOrbitC{560, 300};
constexpr float kOrbitR = 112;

inline SkPoint onOrbit(float canvasDeg) {
  const float rad = canvasDeg * 3.14159265f / 180.0f;
  return {kOrbitC.x() + kOrbitR * std::cos(rad),
          kOrbitC.y() + kOrbitR * std::sin(rad)};
}

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** A socket ring: SDF circle chrome sized by the VISIBLE ring diameter --
 *  sdf::minBoxFor grows the box by the style's reserved pad so glow never
 *  clips, and centerAt() pins the measured box on the orbit position. */
inline Element socket(const char *key, SkPoint at, float dia,
                      const sdf::Style &st,
                      const choreograph::Output<float> *breathingGlow =
                          nullptr) {
  const float boxSize = sdf::minBoxFor(st, dia);
  Material m = sdf::material(sdf::circle(), st);
  if (breathingGlow)
    m.uniform("uGlowR", breathingGlow);
  Element e = box().width(Dim(boxSize)).height(Dim(boxSize))
                  .centerAt(at)
                  .fill(std::move(m))
                  .zIndex(3);
  if (key)
    e.key(key);
  return e;
}

/** Full-circle outline for the orbit ring, centered in the node. */
inline std::function<SkPath(SkSize)> circleOutline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.addCircle(s.width() * 0.5f, s.height() * 0.5f, s.width() * 0.5f);
    return b.detach();
  };
}

/** Horizontal line outline for the legend swatches. */
inline std::function<SkPath(SkSize)> hline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
}

} // namespace skill_tree

struct SkillTreeScene final : Scene {
  // Output-driven reveals stay on the frontier edges (staggered Holds --
  // withFrom cannot stagger, so the delays need the timeline); the
  // allocated rail enters via withFrom instead. Plus the pulse window
  // pair, the wrap-comet phase, and the canAllocate breathing.
  std::array<choreograph::Output<float>, 6> edgeReveal;
  choreograph::Output<float> pulseS{0}, pulseE{0};
  choreograph::Output<float> ringPhase{0};
  choreograph::Output<float> breathA{5.5f}, breathB{5.5f};

  const char *name() const override { return "passive tree"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace ch = choreograph;

    pulseS = 0;
    pulseE = 0;
    ringPhase = 0;
    breathA = 5.5f;
    breathB = 5.5f;

    auto &tl = ticker.timeline();
    for (size_t i = 0; i < edgeReveal.size(); ++i) {
      edgeReveal[i] = 0.0f;
      tl.apply(&edgeReveal[i])
          .then<ch::Hold>(0.0f, 0.15f + 0.12f * (float)i)
          .then<ch::RampTo>(1.0f, 0.6f, &ch::easeOutQuad);
    }

    // canAllocate breathing (two phases so the sockets don't sync); the
    // traveling pulse: window [s, s+0.12] sweeps the ~296 px spoke run in
    // 1.05 s (~282 px/s) then rests until the 2.6 s cycle ends; and the
    // wrap-comet phase: 0.5 rev/s, so the trim window marches around the
    // orbit and periodically straddles the circle's seam.
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      breathA = 5.5f + 3.5f * (float)std::sin(t * 2.1 + 3.0);
      breathB = 5.5f + 3.5f * (float)std::sin(t * 2.1 + 0.8);
      const float cycle = (float)std::fmod(t, 2.6);
      const float u = (cycle - 1.02f) / 1.05f;
      const float s = -0.12f + u * 1.12f;
      pulseS = s;
      pulseE = s + 0.12f;
      ringPhase = (float)std::fmod(t * 0.5, 1.0);
      return true;
    });

    composer.render(describe());
  }

  /** A rope edge between two sockets, state-matched to its endpoints
   *  (the 3-state law). ONE router for every edge: routers::orbit --
   *  same-radius anchor pairs become in-ring arcs, everything else a
   *  straight spoke. Frontier edges keep Output reveals because
   *  withFrom has no delay: a stagger still needs the timeline. */
  Element edge(const char *a, const char *b, int state, size_t reveal) {
    namespace pt = skill_tree;
    return rail({{a}, {b}}, routers::orbit(pt::kOrbitC))
        .absolute().inset(0)
        .trim(0.0f, &edgeReveal[reveal])
        .stroke(brushes::rope(state))
        .zIndex(1);
  }

  /** The orbit ring + its wrap-mode comet: a faint guide circle, and on
   *  top the SAME outline trimmed to a fixed [0.92, 1.06] window whose
   *  offset is a bound wrapping phase -- TrimMode::Wrap keeps both pieces
   *  when the window crosses the seam (the path start, canvas angle 0,
   *  which sits in the arc-free stretch of the ring), so the highlight
   *  marches around the circle forever without a blink. */
  void orbitRing(Element &parent) {
    namespace pt = skill_tree;
    auto ringBox = [&] {
      return box().width(Dim(2 * pt::kOrbitR)).height(Dim(2 * pt::kOrbitR))
          .centerAt(pt::kOrbitC)
          .outline(pt::circleOutline());
    };
    parent.child(ringBox()
                     .stroke(LayeredBrush{{{1.2f,
                                            {pt::kPewter.fR, pt::kPewter.fG,
                                             pt::kPewter.fB, 0.35f}}}})
                     .zIndex(0));
    parent.child(ringBox()
                     .trim(0.92f, 1.06f, &ringPhase, TrimMode::Wrap)
                     .stroke(brushes::pulse(
                         {pt::kHalo.fR, pt::kHalo.fG, pt::kHalo.fB, 0.22f},
                         {1, 1, 1, 0.75f}, 0.72f))
                     .zIndex(2));
  }

  /** Allocated socket: double gold ring + warm halo (+ a small 4-point
   *  star icon on notables). Box sized via sdf::minBoxFor. */
  void allocated(Element &parent, const char *key, SkPoint at, float dia,
                 bool notable) {
    namespace pt = skill_tree;
    // Declared glowRadius reserves the pad (exp falloff reaches ~0 before
    // the box edge); the ACTUAL halo runs at 6.5 via a uniform override.
    const sdf::Style gold{.fill = pt::kSocket,
                          .borderWidth = 3.2f,
                          .borderColor = pt::kGold,
                          .glowRadius = 13,
                          .glowColor = {pt::kHalo.fR, pt::kHalo.fG,
                                        pt::kHalo.fB, 0.5f}};
    Material m = sdf::material(sdf::circle(), gold);
    m.uniform("uGlowR", 6.5f);
    const float boxSize = sdf::minBoxFor(gold, dia);
    parent.child(box().width(Dim(boxSize)).height(Dim(boxSize))
                     .centerAt(at)
                     .key(key)
                     .fill(std::move(m))
                     .zIndex(3));
    parent.child(pt::socket(nullptr, at, dia - 12,
                            {.fill = {0, 0, 0, 0},
                             .borderWidth = 1.8f,
                             .borderColor = pt::kGold}));
    if (notable) {
      const float h = 7;
      parent.child(box().width(Dim(2 * h)).height(Dim(2 * h))
                       .centerAt(at)
                       .fill(sdf::material(sdf::star(4, 2.4f),
                                           {.fill = {pt::kGold.fR,
                                                     pt::kGold.fG,
                                                     pt::kGold.fB, 0.9f}}))
                       .zIndex(4));
    }
  }

  Element describe() {
    namespace pt = skill_tree;
    using namespace std::chrono_literals;

    // Socket positions -- a1->a2->a3->a4 is the allocated path (two
    // spokes, then an ARC: a3 and a4 share the orbit). Ring occupancy,
    // walking the circle: c3 @100deg, a3 @160deg, a4 @205deg, c1 @250deg,
    // u2 @320deg -- arcs cover 100->320, leaving the seam stretch
    // (through 0deg) free for the wrap comet. c2/u1/u3 hang off-ring on
    // straight spokes.
    const SkPoint a1{190, 470}, a2{345, 385};
    const SkPoint a3 = pt::onOrbit(160);                // (~455, ~338)
    const SkPoint a4 = pt::onOrbit(205);                // (~458, ~253)
    const SkPoint c1 = pt::onOrbit(250);                // (~522, ~195)
    const SkPoint c2{310, 225};
    const SkPoint c3 = pt::onOrbit(100);                // (~541, ~410)
    const SkPoint u1{130, 270};
    const SkPoint u2 = pt::onOrbit(320);                // (~646, ~228)
    const SkPoint u3{815, 465};

    auto root = stack().fill(Material::radial(
        {pt::kW * 0.5f, pt::kH * 0.48f}, 600,
        {{0.0f, pt::kBgLift}, {0.55f, pt::kBg}, {1.0f, pt::kBgSink}}));

    // --- the orbit ring and its traveling wrap-window highlight ---
    orbitRing(root);

    // --- ropes beneath the sockets (state matches endpoints); the
    //     orbit router turns every same-radius pair into an in-ring arc:
    //     a3->c3, a4->c1 (Intermediate), c1->u2 (Normal) -- and the last
    //     leg of the allocated rail below (Active). Spokes stay straight.
    root.child(edge("a2", "c2", 1, 0)); // Intermediate spoke
    root.child(edge("a3", "c3", 1, 1)); // Intermediate ARC (160 -> 100)
    root.child(edge("a4", "c1", 1, 2)); // Intermediate ARC (205 -> 250)
    root.child(edge("c1", "u2", 0, 3)); // Normal ARC (250 -> 320)
    root.child(edge("c2", "u1", 0, 4)); // Normal spoke
    root.child(edge("c3", "u3", 0, 5)); // Normal spoke

    // --- the allocated path: ONE rail, spokes + the Active arc, drawing
    //     itself on at mount via withFrom -- no Output, no timeline. ---
    root.child(rail({{"a1"}, {"a2"}, {"a3"}, {"a4"}},
                    routers::orbit(pt::kOrbitC))
                   .absolute().inset(0)
                   .trim(0.0f, withFrom(0.0f, 1.0f, {800ms}))
                   .stroke(brushes::rope(2))
                   .zIndex(1));

    // --- the traveling pulse packet on the SPOKE run (stock brush) ---
    root.child(rail({{"a1"}, {"a2"}, {"a3"}}, routers::orbit(pt::kOrbitC))
                   .absolute().inset(0)
                   .trim(&pulseS, &pulseE)
                   .stroke(brushes::pulse({pt::kHalo.fR, pt::kHalo.fG,
                                           pt::kHalo.fB, 0.35f},
                                          {1, 1, 1, 0.9f}, 1.25f))
                   .zIndex(2));

    // --- sockets: the ring-state ladder ---
    allocated(root, "a1", a1, 38, true);
    allocated(root, "a2", a2, 30, false);
    allocated(root, "a3", a3, 38, true);
    allocated(root, "a4", a4, 30, false);

    const sdf::Style canAlloc{
        .fill = pt::kSocket,
        .borderWidth = 2.6f,
        .borderColor = pt::kRimLit,
        .glowRadius = 14, // generous pad reserve; the bind stays <= 9
        .glowColor = {pt::kGold.fR, pt::kGold.fG, pt::kGold.fB, 0.45f}};
    root.child(pt::socket("c1", c1, 30, canAlloc, &breathA));
    root.child(pt::socket("c2", c2, 30, canAlloc, &breathB));
    root.child(pt::socket("c3", c3, 30, canAlloc, &breathA));

    const sdf::Style unalloc{.fill = pt::kSocket,
                             .borderWidth = 2.0f,
                             .borderColor = pt::kPewter};
    root.child(pt::socket("u1", u1, 25, unalloc));
    root.child(pt::socket("u2", u2, 25, unalloc));
    root.child(pt::socket("u3", u3, 27, unalloc));

    // --- HUD chrome on per-side pins (top/left, top/right, bottom/left) ---
    root.child(box().column().top(34).left(40).zIndex(5)
                   .child(text(toU8("EMBERWOOD REACH"),
                               pt::type(26, pt::kBone, 3)))
                   .child(text(toU8("passive cluster \xe2\x80\x94 "
                                    "three-state rope study"),
                               pt::type(13, pt::kAsh, 1.2f))
                              .margin(0, 6, 0, 0)));
    root.child(box().column().alignItems(Align::End).top(34).right(40)
                   .zIndex(5)
                   .child(text(toU8("4 / 123"), pt::type(22, pt::kGold, 2)))
                   .child(text(toU8("passive points"),
                               pt::type(11, pt::kAsh, 1.5f))
                              .margin(0, 4, 0, 0)));
    auto swatch = [&](int state, const char *label) {
      return box().row().alignItems(Align::Center).gap(9)
          .child(box().width(Dim(56.0f)).height(Dim(16.0f))
                     .outline(pt::hline())
                     .stroke(brushes::rope(state)))
          .child(text(toU8(label), pt::type(12, pt::kAsh, 1)));
    };
    root.child(box().row().gap(26).alignItems(Align::Center)
                   .bottom(30).left(40).zIndex(5)
                   .child(swatch(0, "normal"))
                   .child(swatch(1, "intermediate"))
                   .child(swatch(2, "active")));
    return root;
  }
};

} // namespace compose_gallery
