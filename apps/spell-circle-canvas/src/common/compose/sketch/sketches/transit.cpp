// transit.cpp — "NIGHT NETWORK", the brush-engine showcase.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/transit.cpp
//
// One metro map, six line constructions — every route a different brush,
// all grounded in REFERENCES.md §9:
//
//   EMBER LINE ...... Brush{ ops::Rounded } + lines::cased leg — the classic
//                     two-rail metro pair (rounding from the PIPELINE, the
//                     router stays sharp)
//   STEEL SPUR ...... lines::railwayCarto LayerStyle — osm-carto's verified
//                     dark line + white 50%-duty dash overlay (NOT ties)
//   CURRENT LINE .... lines::Line with midCap chevrons + terminal arrow —
//                     the polylinedecorator repeat pattern; tip AT the
//                     endpoint, body trimmed under the head
//   SMOKEWATER ...... Brush{ Wave then sketchy jitter } + a soft
//                     LayeredBrush band — the organic river against the
//                     octilinear grid (SketchyOpen below works around an
//                     ops::Sketchy fill-rec bug that closes open contours)
//   NIGHT BUS ....... asymmetric casing from ops::Offset legs at +9.5/-8.5 —
//                     amber dashed bus lane right of travel, thin curb left
//                     (mapbox line-offset semantics)
//   ORBITAL ......... Brush{ Line leg + ScatterBrush leg } on a circle
//                     outline — station stamps INSTANCED along the route
//
// Stations are centerAt() SDF discs (bone fill, ink border — sized past the
// sdf pad: pad = borderW/2 + glowR*3.2 + 1); the interchange wears the
// breathing SDF star. Routes draw themselves on via staggered trims.
//
// Headless: ComposeSketch <this> --frame transit_v2.png --at 2.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#if defined(SIGILCOMPOSE_ENABLE_OCIO)
#include <sigilcompose/Ocio.h>
#endif

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;

namespace {

constexpr float W = 1280, H = 800;

constexpr SkColor4f kInk{0.030f, 0.034f, 0.060f, 1};
constexpr SkColor4f kInkHigh{0.065f, 0.075f, 0.125f, 1};
constexpr SkColor4f kBone{0.94f, 0.92f, 0.87f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.63f, 1};
constexpr SkColor4f kEmber{0.99f, 0.53f, 0.19f, 1};
constexpr SkColor4f kCyan{0.36f, 0.86f, 0.96f, 1};
constexpr SkColor4f kViolet{0.71f, 0.51f, 0.99f, 1};
constexpr SkColor4f kSteel{0.52f, 0.53f, 0.58f, 1};
constexpr SkColor4f kAmber{0.98f, 0.76f, 0.24f, 1};
constexpr SkColor4f kAsphalt{0.23f, 0.25f, 0.32f, 1};
constexpr SkColor4f kRiverDeep{0.10f, 0.22f, 0.34f, 0.55f};
constexpr SkColor4f kRiverMid{0.16f, 0.34f, 0.50f, 0.85f};
constexpr SkColor4f kRiverLite{0.55f, 0.75f, 0.90f, 0.45f};

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** Invisible keyed waypoint — rivers and roads route through pins. */
Element pin(const char *key, float x, float y) {
  return box().key(key).width(2).height(2).centerAt({x, y});
}

/** FOUND UPSTREAM (worked around here): ops::Sketchy hands
 *  SkDiscretePathEffect a FILL SkStrokeRec, and under fill the effect
 *  FORCE-CLOSES open contours — a jittered open route grows a straight
 *  return chord (verified: 869.7px river became a 1696.6px CLOSED loop,
 *  painting a phantom second channel). Same op with a hairline rec keeps
 *  the contour open. Belongs in Brushes.h as the ops::Sketchy default. */
struct SketchyOpen {
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const SketchyOpen &) const = default;
  float bleed() const { return deviation * 2; }
  SkPath apply(const SkPath &p) const {
    SkPathBuilder out;
    SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
    if (sk_sp<SkPathEffect> fx =
            SkDiscretePathEffect::Make(segLength, deviation, seed);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** A station: centerAt() SDF disc, bone fill + ink border (the classic
 *  interchange glyph). No glow — sdf pad charges glowR*3.2 against the
 *  box, which is what erased round 1's stations. */
Element station(const char *key, float x, float y, float size = 16) {
  return box().key(key).width(size).height(size).centerAt({x, y})
      .fill(sdf::material(sdf::circle(), {.fill = kBone,
                                          .borderWidth = 2.5f,
                                          .borderColor = kInk}))
      .zIndex(6);
}

Element label(const char *s, float x, float y, SkColor4f c = kAsh,
              float size = 13, float track = 1.5f) {
  return text(toU8(s), type(size, c, track))
      .absolute().inset(x, y, 0, 0).zIndex(8);
}

/** Legend row: colored line name + ash construction note. */
Element legendRow(const char *name, const char *what, SkColor4f c, float y) {
  return box().row().absolute().inset(42, y, 0, 0).zIndex(8)
      .child(text(toU8(name), type(12.5f, c, 1.4f)))
      .child(text(toU8(what), type(12.5f, kAsh, 0.4f)).margin(10, 0, 0, 0));
}

} // namespace

struct Transit : sketch::Sketch {
  ch::Output<float> emberReveal{0}, railReveal{0}, cyanReveal{0},
      ringReveal{0}, roadReveal{0};
  ch::Output<float> hubGlow{0};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kInk);

    auto &tl = ctx.ticker.timeline();
    auto drawOn = [&](ch::Output<float> &r, float delay) {
      r = 0.0f;
      tl.apply(&r).then<ch::Hold>(0.0f, delay).then<ch::RampTo>(
          1.0f, 1.0f, &ch::easeInOutQuad);
    };
    drawOn(emberReveal, 0.10f);
    drawOn(roadReveal, 0.25f);
    drawOn(railReveal, 0.40f);
    drawOn(cyanReveal, 0.55f);
    drawOn(ringReveal, 0.70f);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      hubGlow = 4.0f + 2.0f * (float)std::sin(t * 2.1);
      return true;
    });

#if defined(SIGILCOMPOSE_ENABLE_OCIO)
    ctx.composer.setView(ocio::exponent(1.10f));
#endif
    ctx.composer.render(describe());
  }

  Element describe() {
    // -- 1. EMBER LINE: the cased pair. Router left SHARP (radius 0);
    //    the Brush pipeline rounds, then the leg lays two rails whose
    //    dashes/params share one centerline (Lines.h keeps them in phase).
    Brush emberBrush;
    emberBrush.op(ops::Rounded{16.0f});
    emberBrush.leg(lines::cased(2.6f, Fill::color(kEmber), 7.0f));

    // -- 3. CURRENT LINE: the §9 decorator pattern — repeated mid-path
    //    chevrons + terminal arrow, tip AT the endpoint, 60° apex, body
    //    trimmed under the head so nothing pokes past.
    lines::Line current{.width = 3.5f,
                        .fill = Fill::color(kCyan),
                        .endCap = lines::Cap::Arrow,
                        .capSize = 12.0f,
                        .midCap = lines::Cap::Arrow,
                        .midSpacing = 96.0f};

    // -- 4. SMOKEWATER: geometry pipeline (meander wave, then hand-drawn
    //    jitter) under a soft three-layer band + dashed current thread.
    Brush river;
    river.op(ops::Wave{.amplitude = 6.0f, .wavelength = 120.0f});
    river.op(SketchyOpen{.segLength = 14.0f, .deviation = 1.6f, .seed = 11});
    river.leg(LayeredBrush{{
        {30.0f, kRiverDeep, 8.0f},
        {15.0f, kRiverMid, 3.0f},
        {2.2f, kRiverLite, 0.0f, {16, 12}},
    }});

    // -- 5. NIGHT BUS: asymmetric casing from ops::Offset legs. Positive
    //    offset = RIGHT of travel (mapbox line-offset semantics): amber
    //    dashed bus lane one side, thin bone curb the other. One Brush per
    //    side — the pipeline applies to ALL legs, so per-side treatments
    //    are separate strokes.
    lines::Line roadbed{.width = 13.0f, .fill = Fill::color(kAsphalt)};
    Brush busLane;
    busLane.op(ops::Offset{.px = 9.5f});
    busLane.leg(lines::Line{.width = 3.0f,
                            .fill = Fill::color(kAmber),
                            .dashIntervals = {13, 9}});
    Brush curb;
    curb.op(ops::Offset{.px = -8.5f});
    curb.leg(lines::Line{.width = 1.3f,
                         .fill = Fill::color({0.85f, 0.86f, 0.90f, 0.75f})});

    // -- 6. ORBITAL: line leg + ScatterBrush leg — real components
    //    INSTANCED along the route (snapshot-baked once, replayed per
    //    slot). ø250 circle → circumference ≈ 785 → 8 stamps at 98.2.
    Element ringStamp =
        box().width(11).height(11)
            .fill(sdf::material(sdf::circle(),
                                {.fill = kBone,
                                 .borderWidth = 2.0f,
                                 .borderColor = {0.30f, 0.18f, 0.48f, 1}}));
    Brush orbital;
    orbital.leg(lines::Line{.width = 3.2f, .fill = Fill::color(kViolet)});
    orbital.leg(brushes::ScatterBrush{.art = ringStamp,
                                      .spacing = 98.2f,
                                      .alignToPath = false,
                                      .reach = 12.0f});

    // The interchange: breathing SDF star (glow bound within its reserve).
    Element hub =
        box().key("hub").width(72).height(72).centerAt({620, 400})
            .fill(sdf::material(sdf::star(8, 3.2f),
                                {.fill = kBone,
                                 .borderWidth = 2,
                                 .borderColor = kInk,
                                 .glowRadius = 6,
                                 .glowColor = kEmber})
                      .uniform("uGlowR", &hubGlow))
            .zIndex(7);

    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, kInkHigh}, {0.5f, kInk}, {1.0f, kInk}}))
        // ---- the waterway, beneath everything ----
        .child(rail({{"rv0"}, {"rv1"}, {"rv2"}, {"rv3"}, {"rv4"}},
                    routers::polyline(70))
                   .absolute().inset(0).stroke(river).zIndex(1))
        // ---- the bus corridor (bridges the river) ----
        .child(rail({{"rd_w"}, {"rd1"}, {"rd2"}, {"rd_e"}},
                    routers::polyline(30))
                   .absolute().inset(0).trim(0.0f, &roadReveal)
                   .stroke(roadbed).stroke(busLane).stroke(curb).zIndex(2))
        // ---- the carto railway ----
        .child(rail({{"rw_w"}, {"rw1"}, {"rw2"}, {"rw_e"}},
                    routers::octilinear(18))
                   .absolute().inset(0).trim(0.0f, &railReveal)
                   .style(lines::railwayCarto(1.6f, kSteel,
                                              {0.95f, 0.94f, 0.90f, 1}))
                   .zIndex(3))
        // ---- the cased metro pair ----
        .child(rail({{"em_w"}, {"em1"}, {"hub"}, {"em2"}, {"em_e"}},
                    routers::octilinear(0))
                   .absolute().inset(0).trim(0.0f, &emberReveal)
                   .stroke(emberBrush).zIndex(4))
        // ---- the one-way line ----
        .child(rail({{"cy_w"}, {"cy1"}, {"hub"}, {"cy2"}, {"cy_e"}},
                    routers::octilinear(10))
                   .absolute().inset(0).trim(0.0f, &cyanReveal)
                   .stroke(current).zIndex(4))
        // ---- the orbital ring with instanced stations ----
        .child(box().width(250).height(250).centerAt({620, 400})
                   .outline(shapes::arc(0.0f, 359.9f))
                   .trim(0.0f, &ringReveal).stroke(orbital).zIndex(5))
        // ---- waypoint pins (invisible) ----
        .child(pin("rv0", 985, 4))
        .child(pin("rv1", 930, 180))
        .child(pin("rv2", 795, 370))
        .child(pin("rv3", 830, 590))
        .child(pin("rv4", 700, 796))
        .child(pin("rd_w", 120, 690))
        .child(pin("rd1", 520, 650))
        .child(pin("rd2", 900, 690))
        .child(pin("rd_e", 1170, 640))
        // ---- stations ----
        .child(station("em_w", 140, 560))
        .child(station("em1", 380, 470))
        .child(station("em2", 940, 400))
        .child(station("em_e", 1150, 310))
        .child(station("rw_w", 390, 205, 13))
        .child(station("rw1", 620, 150, 13))
        .child(station("rw2", 840, 150, 13))
        .child(station("rw_e", 1180, 205, 13))
        .child(station("cy_w", 170, 300))
        .child(station("cy1", 430, 325))
        .child(station("cy2", 860, 545))
        .child(station("cy_e", 1100, 625))
        .child(hub)
        // ---- names ----
        .child(label("EMBER GATE", 655, 372, kBone))
        .child(label("wharf lane", 118, 582))
        .child(label("north quay", 745, 122))
        .child(label("saltmarsh", 985, 596))
        .child(label("the smokewater", 950, 258, {0.45f, 0.62f, 0.78f, 1}))
        // ---- title + legend ----
        .child(box().column().absolute().inset(40, 34, 0, 0).zIndex(8)
                   .child(text(toU8("NIGHT NETWORK"), type(34, kBone, 2)))
                   .child(text(toU8("the brush engine — six line"
                                    " constructions"),
                               type(15, kAsh, 1))
                              .margin(0, 6, 0, 0)))
        .child(legendRow("EMBER LINE", "cased pair over rounded corners",
                         kEmber, 128))
        .child(legendRow("STEEL SPUR", "carto railway: dark + white dash",
                         kSteel, 150))
        .child(legendRow("CURRENT LINE", "one-way: chevrons + arrow", kCyan,
                         172))
        .child(legendRow("ORBITAL", "instanced station stamps", kViolet,
                         194))
        .child(legendRow("NIGHT BUS", "offset legs: lane + curb", kAmber,
                         216))
        .child(legendRow("SMOKEWATER", "wave + sketchy pipeline",
                         {0.45f, 0.62f, 0.78f, 1}, 238));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(Transit)
