#pragma once
// The brush-engine showcase (REFERENCES.md section 9), ported from
// sketch/sketches/transit.cpp: one metro map, ten line constructions,
// every route a different brush:
//
//   EMBER LINE ...... Brush{ ops::Rounded } + lines::cased leg -- the classic
//                     two-rail metro pair (rounding from the PIPELINE, the
//                     router stays sharp)
//   STEEL SPUR ...... lines::railwayCarto LayerStyle -- osm-carto's verified
//                     dark line + white 50%-duty dash overlay (NOT ties)
//   CURRENT LINE .... lines::Line with midCap chevrons + terminal arrow --
//                     the polylinedecorator repeat pattern
//   SMOKEWATER ...... the TfL Thames rule: a pale octilinear band ~3.9x the
//                     route weight with thin per-leg ops::Offset bank edges
//   NIGHT BUS ....... asymmetric casing from ops::Offset legs -- amber dashed
//                     bus lane right of travel, thin curb left
//   ORBITAL ......... Brush{ Line leg + ScatterBrush leg } on a circle --
//                     station stamps INSTANCED along the route
//   TWIN SERVICE .... shared running as alternating two-color dashes
//   CABLEWAY ........ PathFormat::stampPath rings on a support cable
//   MILLBROOK ....... brushes::taper -- the topographic source->mouth ribbon
//   PIPELINE TRIO ... identical points, three geometry ops (wave/zig/boxy)
//
// Stations are centerAt() SDF discs (bone fill, ink border); the interchange
// wears the breathing SDF star. Routes draw themselves on via staggered
// timeline trims. Coordinates recomposed from the sketch's 1280x800 onto the
// gallery's 900x640 (octilinear legs re-trued to dx==dy where they are pure
// diagonals; the routers snap the rest).

#include "GalleryCore.h"

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>

namespace compose_gallery {

namespace night_network {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

constexpr SkColor4f kInk{0.030f, 0.034f, 0.060f, 1};
constexpr SkColor4f kInkHigh{0.065f, 0.075f, 0.125f, 1};
constexpr SkColor4f kBone{0.94f, 0.92f, 0.87f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.63f, 1};
constexpr SkColor4f kEmber{0.99f, 0.53f, 0.19f, 1};
constexpr SkColor4f kCyan{0.36f, 0.86f, 0.96f, 1};
constexpr SkColor4f kViolet{0.71f, 0.51f, 0.99f, 1};
constexpr SkColor4f kSteel{0.52f, 0.53f, 0.58f, 1};
constexpr SkColor4f kAmber{0.98f, 0.76f, 0.24f, 1};
constexpr SkColor4f kRose{0.95f, 0.45f, 0.62f, 1};
constexpr SkColor4f kAsphalt{0.23f, 0.25f, 0.32f, 1};

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** Invisible keyed waypoint -- rivers and roads route through pins. */
inline Element pin(const char *key, float x, float y) {
  return box().key(key).width(2).height(2).centerAt({x, y});
}

/** A station: centerAt() SDF disc, bone fill + ink border (the classic
 *  interchange glyph). No glow -- sdf pad charges glowR*3.2 against the
 *  box, which is what erased the sketch's round-1 stations. */
inline Element station(const char *key, float x, float y, float size = 16) {
  return box().key(key).width(size).height(size).centerAt({x, y})
      .fill(sdf::material(sdf::circle(), {.fill = kBone,
                                          .borderWidth = 2.5f,
                                          .borderColor = kInk}))
      .zIndex(6);
}

inline Element label(const char *s, float x, float y, SkColor4f c = kAsh,
                     float size = 13, float track = 1.5f) {
  return text(toU8(s), type(size, c, track))
      .absolute().inset(x, y, 0, 0).zIndex(8);
}

/** Legend row: colored line name + ash construction note. */
inline Element legendRow(const char *name, const char *what, SkColor4f c,
                         float y) {
  return box().row().absolute().inset(30, y, 0, 0).zIndex(8)
      .child(text(toU8(name), type(12.5f, c, 1.4f)))
      .child(text(toU8(what), type(12.5f, kAsh, 0.4f)).margin(10, 0, 0, 0));
}

} // namespace night_network

struct NightNetworkScene final : Scene {
  choreograph::Output<float> emberReveal{0}, railReveal{0}, cyanReveal{0},
      ringReveal{0}, roadReveal{0};
  choreograph::Output<float> hubGlow{0};

  const char *name() const override { return "night network"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace ch = choreograph;

    hubGlow = 4.0f;
    auto &tl = ticker.timeline();
    auto drawOn = [&](choreograph::Output<float> &r, float delay) {
      r = 0.0f; // scenes re-activate: reveals re-zero here
      tl.apply(&r).then<ch::Hold>(0.0f, delay).then<ch::RampTo>(
          1.0f, 1.0f, &ch::easeInOutQuad);
    };
    drawOn(emberReveal, 0.10f);
    drawOn(roadReveal, 0.25f);
    drawOn(railReveal, 0.40f);
    drawOn(cyanReveal, 0.55f);
    drawOn(ringReveal, 0.70f);

    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      hubGlow = 4.0f + 2.0f * (float)std::sin(t * 2.1);
      return true;
    });

    composer.render(describe());
  }

  Element describe() {
    namespace nn = night_network;

    // -- 1. EMBER LINE: the cased pair. Router left SHARP (radius 0);
    //    the Brush pipeline rounds, then the leg lays two rails whose
    //    dashes/params share one centerline (Lines.h keeps them in phase).
    Brush emberBrush;
    emberBrush.op(ops::Rounded{12.0f});
    emberBrush.leg(lines::cased(2.6f, Fill::color(nn::kEmber), 7.0f));

    // -- 3. CURRENT LINE: the section-9 decorator pattern -- repeated
    //    mid-path chevrons + terminal arrow, tip AT the endpoint, body
    //    trimmed under the head so nothing pokes past.
    lines::Line current{.width = 3.5f,
                        .fill = Fill::color(nn::kCyan),
                        .endCap = lines::Cap::Arrow,
                        .capSize = 12.0f,
                        .midCap = lines::Cap::Arrow,
                        .midSpacing = 72.0f};

    // -- 4. SMOKEWATER: the VERIFIED schematic-water convention (TfL
    //    Thames; d3-tube-map encodes the same rule): the river speaks the
    //    map's OWN octilinear language -- never organic -- as a pale band
    //    ~3.9x the route weight with thin bank edges ~0.3x, routes drawn
    //    OVER it. The bank edges are per-leg ops::Offset legs -- one Brush,
    //    one material: "a river with two banks".
    const float routeW = 3.0f;
    Brush river;
    river.leg(lines::Line{.width = routeW * 3.9f,
                          .fill = Fill::color({0.13f, 0.27f, 0.40f, 0.9f})});
    river.leg(lines::Line{.width = routeW * 0.3f,
                          .fill = Fill::color({0.36f, 0.66f, 0.86f, 0.8f})},
              {ops::Offset{.px = routeW * 1.95f}});
    river.leg(lines::Line{.width = routeW * 0.3f,
                          .fill = Fill::color({0.36f, 0.66f, 0.86f, 0.8f})},
              {ops::Offset{.px = -routeW * 1.95f}});

    // -- 5. NIGHT BUS: asymmetric casing from ops::Offset legs. Positive
    //    offset = RIGHT of travel (mapbox line-offset semantics): amber
    //    dashed bus lane one side, thin bone curb the other. One Brush per
    //    side -- the pipeline applies to ALL legs, so per-side treatments
    //    are separate strokes.
    lines::Line roadbed{.width = 13.0f, .fill = Fill::color(nn::kAsphalt)};
    Brush busLane;
    busLane.op(ops::Offset{.px = 9.5f});
    busLane.leg(lines::Line{.width = 3.0f,
                            .fill = Fill::color(nn::kAmber),
                            .dashIntervals = {13, 9}});
    Brush curb;
    curb.op(ops::Offset{.px = -8.5f});
    curb.leg(lines::Line{.width = 1.3f,
                         .fill = Fill::color({0.85f, 0.86f, 0.90f, 0.75f})});

    // -- 6. ORBITAL: line leg + ScatterBrush leg -- real components
    //    INSTANCED along the route (snapshot-baked once, replayed per
    //    slot). dia. 190 circle -> circumference ~597 -> 8 stamps at 74.6.
    Element ringStamp =
        box().width(11).height(11)
            .fill(sdf::material(sdf::circle(),
                                {.fill = nn::kBone,
                                 .borderWidth = 2.0f,
                                 .borderColor = {0.30f, 0.18f, 0.48f, 1}}));
    Brush orbital;
    orbital.leg(lines::Line{.width = 3.2f, .fill = Fill::color(nn::kViolet)});
    orbital.leg(brushes::ScatterBrush{.art = ringStamp,
                                      .spacing = 74.6f,
                                      .alignToPath = false,
                                      .reach = 12.0f});

    // -- 7. TWIN SERVICE: shared running as ALTERNATING two-color dashes
    //    (the network-map convention for two services on one track): two
    //    dashed legs, same body, complementary phases -- dash geometry
    //    shares one arc parameterization, so the colors interlock exactly.
    Brush twin;
    twin.leg(lines::Line{.width = 4.0f,
                         .fill = Fill::color(nn::kRose),
                         .dashIntervals = {14, 14},
                         .dashPhase = 0});
    twin.leg(lines::Line{.width = 4.0f,
                         .fill = Fill::color(nn::kBone),
                         .dashIntervals = {14, 14},
                         .dashPhase = 14});

    // -- 8. CABLEWAY: the STAMPED line (Sk1DPathEffect through
    //    PathFormat::stampPath) -- the cable-car map convention: a thin
    //    support cable with rings stamped along it.
    PathFormat cableRings;
    cableRings.width = 1.4f;
    cableRings.strokeFill = Fill::color(nn::kBone);
    cableRings.stampPath = SkPath::Circle(0, 0, 4.0f);
    cableRings.stampAdvance = 24.0f;
    Brush cableway;
    cableway.leg(lines::Line{.width = 1.8f,
                             .fill = Fill::color({0.66f, 0.68f, 0.74f, 0.95f})});
    cableway.leg(cableRings);

    // -- 9. MILLBROOK CREEK: the CALLIGRAPHIC leg -- variable width the
    //    way real maps use it: topographic rivers TAPER toward the source
    //    (drawn mouth->source, wide->narrow), in the map's own octilinear
    //    grammar per the Thames rule.
    Brush creek;
    creek.leg(brushes::taper(2.2f, 9.0f,
                             Fill::color({0.13f, 0.27f, 0.40f, 0.9f})));

    // -- 10. THE PIPELINE TRIO: three runs over IDENTICAL path points --
    //    only the geometry op differs (squiggly / zigzag / boxy). The
    //    whole point of the pipeline: restyle the line, never the route.
    auto demoRun = [](GeometryOp op, SkColor4f c) {
      Brush b;
      b.op(std::move(op));
      b.leg(lines::Line{.width = 2.2f, .fill = Fill::color(c)});
      return b;
    };
    auto demoPath = [](SkSize sz) { // the SAME points for all three
      SkPathBuilder b;
      b.moveTo(0, sz.height());
      b.lineTo(sz.width() * 0.62f, sz.height());
      b.lineTo(sz.width(), 0);
      return b.detach();
    };

    // The interchange: breathing SDF star (glow bound within its reserve --
    // the 72px reserve stays unscaled so the sdf pad keeps its budget).
    Element hub =
        box().key("hub").width(72).height(72).centerAt({436, 320})
            .fill(sdf::material(sdf::star(8, 3.2f),
                                {.fill = nn::kBone,
                                 .borderWidth = 2,
                                 .borderColor = nn::kInk,
                                 .glowRadius = 6,
                                 .glowColor = nn::kEmber})
                      .uniform("uGlowR", &hubGlow))
            .zIndex(7);

    return stack()
        .fill(Material::linear({0, 0}, {0, nn::kH},
                               {{0.0f, nn::kInkHigh},
                                {0.5f, nn::kInk},
                                {1.0f, nn::kInk}}))
        // ---- the waterway, beneath everything ----
        .child(rail({{"rv0"}, {"rv1"}, {"rv2"}, {"rv3"}, {"rv4"}},
                    routers::octilinear(20)) // the Thames rule: the river
                                             // rides the routes' own grid
                   .absolute().inset(0).stroke(river).zIndex(1))
        // ---- the bus corridor (bridges the river) ----
        .child(rail({{"rd_w"}, {"rd1"}, {"rd2"}, {"rd_e"}},
                    routers::polyline(22))
                   .absolute().inset(0).trim(0.0f, &roadReveal)
                   .stroke(roadbed).stroke(busLane).stroke(curb).zIndex(2))
        // ---- the carto railway ----
        .child(rail({{"rw_w"}, {"rw1"}, {"rw2"}, {"rw_e"}},
                    routers::octilinear(14))
                   .absolute().inset(0).trim(0.0f, &railReveal)
                   .style(lines::railwayCarto(1.6f, nn::kSteel,
                                              {0.95f, 0.94f, 0.90f, 1}))
                   .zIndex(3))
        // ---- the cased metro pair ----
        .child(rail({{"em_w"}, {"em1"}, {"hub"}, {"em2"}, {"em_e"}},
                    routers::octilinear(0))
                   .absolute().inset(0).trim(0.0f, &emberReveal)
                   .stroke(emberBrush).zIndex(4))
        // ---- the one-way line ----
        .child(rail({{"cy_w"}, {"cy1"}, {"hub"}, {"cy2"}, {"cy_e"}},
                    routers::octilinear(8))
                   .absolute().inset(0).trim(0.0f, &cyanReveal)
                   .stroke(current).zIndex(4))
        // ---- the orbital ring with instanced stations ----
        .child(box().width(190).height(190).centerAt({436, 320})
                   .outline(shapes::arc(0.0f, 359.9f))
                   .trim(0.0f, &ringReveal).stroke(orbital).zIndex(5))
        // ---- twin service (bottom-right strip) ----
        .child(rail({{"tw_w"}, {"tw1"}, {"tw_e"}}, routers::octilinear(9))
                   .absolute().inset(0).stroke(twin).zIndex(2))
        // ---- cableway (top gap) ----
        .child(rail({{"cb_w"}, {"cb_e"}}, routers::polyline(0))
                   .absolute().inset(0).stroke(cableway).zIndex(3))
        // ---- millbrook creek (tapers INTO the smokewater) ----
        .child(rail({{"ck_s"}, {"ck2"}, {"ck1"}, {"ck_m"}},
                    routers::octilinear(10)) // source->mouth: narrow->wide
                   .absolute().inset(0).stroke(creek).zIndex(1))
        // ---- the pipeline trio: identical points, different ops ----
        .child(box().absolute().inset(49, 554, nn::kW - 232, nn::kH - 581)
                   .outline(demoPath)
                   .stroke(demoRun(ops::Wave{.amplitude = 4, .wavelength = 28},
                                   nn::kCyan))
                   .zIndex(3))
        .child(box().absolute().inset(49, 580, nn::kW - 232, nn::kH - 607)
                   .outline(demoPath)
                   .stroke(demoRun(ops::Wave{.amplitude = 4,
                                             .wavelength = 28,
                                             .zigzag = true},
                                   nn::kAmber))
                   .zIndex(3))
        .child(box().absolute().inset(49, 606, nn::kW - 232, nn::kH - 633)
                   .outline(demoPath)
                   .stroke(demoRun(ops::Square{.amplitude = 4,
                                               .wavelength = 28},
                                   nn::kViolet))
                   .zIndex(3))
        // ---- waypoint pins (invisible) ----
        .child(nn::pin("rv0", 692, 4))
        .child(nn::pin("rv1", 654, 144))
        .child(nn::pin("rv2", 559, 296))
        .child(nn::pin("rv3", 584, 472))
        .child(nn::pin("rv4", 492, 636))
        .child(nn::pin("rd_w", 84, 552))
        .child(nn::pin("rd1", 366, 520))
        .child(nn::pin("rd2", 633, 552))
        .child(nn::pin("rd_e", 823, 512))
        .child(nn::pin("tw_w", 366, 610))
        .child(nn::pin("tw1", 633, 610))
        .child(nn::pin("tw_e", 830, 589))
        .child(nn::pin("cb_w", 302, 96))
        .child(nn::pin("cb_e", 492, 48))
        .child(nn::pin("ck_m", 579, 412))
        .child(nn::pin("ck1", 636, 412))
        .child(nn::pin("ck2", 676, 372))
        .child(nn::pin("ck_s", 828, 372))
        // ---- stations ----
        .child(nn::station("em_w", 98, 448))
        .child(nn::station("em1", 267, 376))
        .child(nn::station("em2", 661, 320))
        .child(nn::station("em_e", 809, 248))
        .child(nn::station("rw_w", 390, 166, 13))
        .child(nn::station("rw1", 436, 120, 13))
        .child(nn::station("rw2", 591, 120, 13))
        .child(nn::station("rw_e", 830, 164, 13))
        .child(nn::station("cy_w", 105, 304))
        .child(nn::station("cy1", 302, 260))
        .child(nn::station("cy2", 605, 436))
        .child(nn::station("cy_e", 773, 500))
        .child(hub)
        // ---- names ----
        .child(nn::label("EMBER GATE", 461, 298, nn::kBone))
        .child(nn::label("wharf lane", 82, 466))
        .child(nn::label("north quay", 524, 98))
        .child(nn::label("saltmarsh", 693, 477))
        .child(nn::label("the smokewater", 668, 206,
                         {0.45f, 0.62f, 0.78f, 1}))
        // ---- title + legend ----
        .child(box().column().absolute().inset(28, 27, 0, 0).zIndex(8)
                   .child(text(toU8("NIGHT NETWORK"),
                               nn::type(30, nn::kBone, 2)))
                   .child(text(toU8("the brush engine \xe2\x80\x94 six line"
                                    " constructions"),
                               nn::type(14, nn::kAsh, 1))
                              .margin(0, 6, 0, 0)))
        .child(nn::legendRow("EMBER LINE", "cased pair over rounded corners",
                             nn::kEmber, 102))
        .child(nn::legendRow("STEEL SPUR", "carto railway: dark + white dash",
                             nn::kSteel, 124))
        .child(nn::legendRow("CURRENT LINE", "one-way: chevrons + arrow",
                             nn::kCyan, 146))
        .child(nn::legendRow("ORBITAL", "instanced station stamps",
                             nn::kViolet, 168))
        .child(nn::legendRow("NIGHT BUS", "offset legs: lane + curb",
                             nn::kAmber, 190))
        .child(nn::legendRow("SMOKEWATER",
                             "TfL Thames rule: octilinear band + banks",
                             {0.45f, 0.62f, 0.78f, 1}, 212));
  }
};

} // namespace compose_gallery
