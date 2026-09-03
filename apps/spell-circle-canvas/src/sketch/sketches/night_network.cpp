/** @file
 * night network — one metro map, twelve constructions, every route a
 * different brush: the brush engine put through everything it has.
 */

// The brush engine put through everything it has: one metro map, twelve
// constructions, every route a different brush:
//
//   EMBER LINE ...... Brush{ .shaped(shapers::Rounded) } + a cased layer --
//                     the classic two-rail metro pair (rounding from the
//                     PIPELINE, the router stays sharp)
//   STEEL SPUR ...... brush::presets::railwayCarto LayerStyle -- osm-carto's verified
//                     dark line + white 50%-duty dash overlay (NOT ties)
//   CURRENT LINE .... lines::Line with midCap chevrons + terminal arrow --
//                     the polylinedecorator repeat pattern
//   SMOKEWATER ...... the TfL Thames rule: a pale octilinear band ~3.9x the
//                     route weight with thin per-layer shapers::Offset edges
//   NIGHT BUS ....... asymmetric casing from shapers::Offset pipelines --
//                     amber dashed bus lane right of travel, thin curb left
//   ORBITAL ......... Brush{ Line layer + brush::Scatter layer } on a circle --
//                     station stamps INSTANCED along the route
//   TWIN SERVICE .... shared running as alternating two-color dashes
//   CABLEWAY ........ PathFormat::stampPath rings on a support cable
//   MILLBROOK ....... brush::taper -- the topographic source->mouth ribbon
//   PIPELINE TRIO ... identical points, three geometry ops (wave/zig/boxy)
//
// Stations are centerAt() SDF discs (bone fill, ink border); the interchange
// wears the breathing SDF star. Routes draw themselves on via staggered
// timeline trims. The octilinear legs hold dx == dy exactly where they are
// pure diagonals; the routers snap the rest.
//
// EDIT THESE FIRST
//   the five reveal delays in setup() — the order the map draws itself
//                    on, and therefore what a plate taken before the last
//                    one lands would be missing.
//   routeW           — the weight every construction is measured in: the
//                    Thames band is a multiple of it, the casings are
//                    fractions of it, and the legend's swatches follow.
//   the palette block — one colour per line, and the legend reads them.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilweave/style/Type.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilgeometry/path/Shaper.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace mskia = sigil::material::skia;
namespace shapes = sigil::geometry::shapes;
namespace shapers = sigil::geometry::shapers;
namespace sdf = sigil::material::sdf;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::skia::Paint;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

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

/** This study's type colour reaches the paint as 8-bit sRGB, so a tint
 *  computed per frame lands on the same 256-step ladder as a quoted one.
 *  `compose::type` carries the float through instead, and the device
 *  raster resolves the two differently. */
inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  return sigil::weave::textStyle(
      {.size = size, .color = color, .track = tracking, .color8 = true});
}

/** Invisible keyed waypoint -- rivers and roads route through pins. */
inline Element pin(const char* key, float x, float y) {
  return box().key(key).width(2).height(2).centerAt({x, y});
}

/** A station: centerAt() SDF disc, bone fill + ink border (the classic
 *  interchange glyph). No glow: sdf pad charges glowR*3.2 against the box,
 *  which at this size leaves no box to draw. */
inline Element station(const char* key, float x, float y, float size = 16) {
  return box()
      .key(key)
      .width(size)
      .height(size)
      .centerAt({x, y})
      .fill(Paint::recipe(sdf::material(
          sdf::circle(),
          {.fill = mskia::toColor(kBone), .borderWidth = 2.5f, .borderColor = mskia::toColor(kInk)})))
      .zIndex(6);
}

inline Element label(const char* s, float x, float y, SkColor4f c = kAsh,
                     float size = 13, float track = 1.5f) {
  return text(toU8(s), type(size, c, track)).inset(x, y, 0, 0).zIndex(8);
}

/** The ARTLINE art cell: a stem with alternating leaf lenses — reads as a
 *  living vine once the art warp bends it (logical 48x16). */
inline Element vineArt() {
  constexpr SkColor4f kMoss{0.55f, 0.80f, 0.47f, 1};
  constexpr SkColor4f kMossDeep{0.34f, 0.60f, 0.36f, 1};
  auto leaf = [&](float x, float y, float deg, SkColor4f c) {
    return box()
        .width(13)
        .height(7)
        .inset(x, y, 48.0f - x - 13.0f, 16.0f - y - 7.0f)
        .corners({6.5f, 0, 6.5f, 0})
        .rotate(deg)
        .fill(Fill::color(c));
  };
  return stack()
      .width(48)
      .height(16)
      .child(box()
                 .inset(0, 6.8f, 0, 6.8f)
                 .corners({1.2f})
                 .fill(Fill::color(kMossDeep)))
      .child(leaf(4, 0, -28, kMoss))
      .child(leaf(18, 9, 152, kMossDeep))
      .child(leaf(31, 0, -24, kMoss));
}

/** Legend row: coloured line name + ash construction note. */
inline Element legendRow(const char* name, const char* what, SkColor4f c,
                         float y) {
  return box()
      .row()
      .inset(30, y, 0, 0)
      .zIndex(8)
      .child(text(toU8(name), type(12.5f, c, 1.4f)))
      .child(text(toU8(what), type(12.5f, kAsh, 0.4f)).margin(10, 0, 0, 0));
}

}  // namespace night_network

struct NightNetwork final : sketch::Sketch {
  choreograph::Output<float> emberReveal{0}, railReveal{0}, cyanReveal{0},
      ringReveal{0}, roadReveal{0};
  choreograph::Output<float> hubGlow{0};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.captureAt(6.0);
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    namespace ch = choreograph;

    hubGlow = 4.0f;
    auto& tl = ticker.timeline();
    auto drawOn = [&](choreograph::Output<float>& r, float delay) {
      r = 0.0f;  // scenes re-activate: reveals re-zero here
      tl.apply(&r)
          .then<ch::Hold>(0.0f, delay)
          .then<ch::RampTo>(1.0f, 1.0f, &ch::easeInOutQuad);
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
    //    the Brush pipeline rounds, then the layer lays two rails whose
    //    dashes/params share one centerline (Lines.h keeps them in phase).
    Brush emberBrush;
    emberBrush.shaped(shapers::Rounded{12.0f});
    emberBrush.layer(lines::cased(2.6f, Fill::color(nn::kEmber), 7.0f));

    // -- 3. CURRENT LINE: the decorator pattern -- repeated
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
    //    OVER it. The bank edges are per-layer shapers::Offset -- one Brush,
    //    one material: "a river with two banks".
    const float routeW = 3.0f;
    Brush river;
    river.layer(lines::Line{.width = routeW * 3.9f,
                            .fill = Fill::color({0.13f, 0.27f, 0.40f, 0.9f})});
    river.layer(lines::Line{.width = routeW * 0.3f,
                            .fill = Fill::color({0.36f, 0.66f, 0.86f, 0.8f})},
                {shapers::Offset{.px = -routeW * 1.95f}});
    river.layer(lines::Line{.width = routeW * 0.3f,
                            .fill = Fill::color({0.36f, 0.66f, 0.86f, 0.8f})},
                {shapers::Offset{.px = routeW * 1.95f}});

    // -- 5. NIGHT BUS: asymmetric casing from shapers::Offset. Positive
    //    offset = LEFT of travel, which is the engine's one convention: amber
    //    dashed bus lane one side, thin bone curb the other. One Brush per
    //    side -- the pipeline applies to ALL layers, so per-side treatments
    //    are separate strokes.
    lines::Line roadbed{.width = 13.0f, .fill = Fill::color(nn::kAsphalt)};
    Brush busLane;
    busLane.shaped(shapers::Offset{.px = -9.5f});
    busLane.layer(lines::Line{.width = 3.0f,
                              .fill = Fill::color(nn::kAmber),
                              .dashIntervals = {13, 9}});
    Brush curb;
    curb.shaped(shapers::Offset{.px = 8.5f});
    curb.layer(lines::Line{.width = 1.3f,
                           .fill = Fill::color({0.85f, 0.86f, 0.90f, 0.75f})});

    // -- 6. ORBITAL: line layer + brush::Scatter layer -- real components
    //    INSTANCED along the route (snapshot-baked once, replayed per
    //    slot). dia. 190 circle -> circumference ~597 -> 8 stamps at 74.6.
    Element ringStamp = box().width(11).height(11).fill(Paint::recipe(sdf::material(
        sdf::circle(), {.fill = mskia::toColor(nn::kBone),
                        .borderWidth = 2.0f,
                        .borderColor = {0.30f, 0.18f, 0.48f, 1}})));
    Brush orbital;
    orbital.layer(lines::Line{.width = 3.2f, .fill = Fill::color(nn::kViolet)});
    orbital.layer(brush::Scatter{.art = ringStamp,
                                 .spacing = 74.6f,
                                 .alignToPath = false,
                                 .reach = 12.0f});

    // -- 7. TWIN SERVICE: shared running as ALTERNATING two-color dashes
    //    (the network-map convention for two services on one track): two
    //    dashed layers, same body, complementary phases -- dash geometry
    //    shares one arc parameterization, so the colors interlock exactly.
    Brush twin;
    twin.layer(lines::Line{.width = 4.0f,
                           .fill = Fill::color(nn::kRose),
                           .dashIntervals = {14, 14},
                           .dashPhase = 0});
    twin.layer(lines::Line{.width = 4.0f,
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
    cableway.layer(lines::Line{
        .width = 1.8f, .fill = Fill::color({0.66f, 0.68f, 0.74f, 0.95f})});
    cableway.layer(cableRings);

    // -- 9. MILLBROOK CREEK: the CALLIGRAPHIC layer -- variable width the
    //    way real maps use it: topographic rivers TAPER toward the source
    //    (drawn mouth->source, wide->narrow), in the map's own octilinear
    //    grammar per the Thames rule.
    Brush creek;
    creek.layer(
        brush::taper(2.2f, 9.0f, Fill::color({0.13f, 0.27f, 0.40f, 0.9f})));

    // -- 10. THE PIPELINE TRIO: three runs over IDENTICAL path points --
    //    only the geometry op differs (squiggly / zigzag / boxy). The
    //    whole point of the pipeline: restyle the line, never the route.
    auto demoRun = [](path::Shaper op, SkColor4f c) {
      Brush b;
      b.shaped(std::move(op));
      b.layer(lines::Line{.width = 2.2f, .fill = Fill::color(c)});
      return b;
    };
    auto demoPath = [](SkSize sz) {  // the SAME points for all three
      SkPathBuilder b;
      b.moveTo(0, sz.height());
      b.lineTo(sz.width() * 0.62f, sz.height());
      b.lineTo(sz.width(), 0);
      return b.detach();
    };

    // The interchange: breathing SDF star (glow bound within its reserve --
    // the 72px reserve stays unscaled so the sdf pad keeps its budget).
    Element hub =
        box()
            .key("hub")
            .width(72)
            .height(72)
            .centerAt({436, 320})
            .fill(Paint::recipe(sdf::material(sdf::star(8, 3.2f), {.fill = mskia::toColor(nn::kBone),
                                                     .borderWidth = 2,
                                                     .borderColor = mskia::toColor(nn::kInk),
                                                     .glowRadius = 6,
                                                     .glowColor = mskia::toColor(nn::kEmber)}))
                      .uniform("uGlowR", &hubGlow))
            .zIndex(7);

    return stack()
        .fill(Paint::linear(
            {0, 0}, {0, nn::kH},
            {{0.0f, nn::kInkHigh}, {0.5f, nn::kInk}, {1.0f, nn::kInk}}))
        // ---- the waterway, beneath everything ----
        .child(rail({{"rv0"}, {"rv1"}, {"rv2"}, {"rv3"}, {"rv4"}},
                    routers::octilinear(20))  // the Thames rule: the river
                                              // rides the routes' own grid
                   .inset(0)
                   .stroke(river)
                   .zIndex(1))
        // ---- the bus corridor (bridges the river) ----
        .child(
            rail({{"rd_w"}, {"rd1"}, {"rd2"}, {"rd_e"}}, routers::polyline(22))
                .inset(0)
                .mask(by::spans(spans::upTo(&roadReveal)))
                .stroke(roadbed)
                .stroke(busLane)
                .stroke(curb)
                .zIndex(2))
        // ---- the carto railway ----
        .child(rail({{"rw_w"}, {"rw1"}, {"rw2"}, {"rw_e"}},
                    routers::octilinear(14))
                   .inset(0)
                   .mask(by::spans(spans::upTo(&railReveal)))
                   .style(brush::presets::railwayCarto(1.6f, nn::kSteel,
                                              {0.95f, 0.94f, 0.90f, 1}))
                   .zIndex(3))
        // ---- the cased metro pair ----
        .child(rail({{"em_w"}, {"em1"}, {"hub"}, {"em2"}, {"em_e"}},
                    routers::octilinear(0))
                   .inset(0)
                   .stroke(spans::upTo(&emberReveal), emberBrush)
                   .zIndex(4))
        // ---- the one-way line ----
        .child(rail({{"cy_w"}, {"cy1"}, {"hub"}, {"cy2"}, {"cy_e"}},
                    routers::octilinear(8))
                   .inset(0)
                   .stroke(spans::upTo(&cyanReveal), current)
                   .zIndex(4))
        // ---- the orbital ring with instanced stations ----
        .child(box()
                   .width(190)
                   .height(190)
                   .centerAt({436, 320})
                   .shape(shapes::arc(0.0f, 359.9f))
                   .stroke(spans::upTo(&ringReveal), orbital)
                   .zIndex(5))
        // ---- twin service (bottom-right strip) ----
        .child(rail({{"tw_w"}, {"tw1"}, {"tw_e"}}, routers::octilinear(9))
                   .inset(0)
                   .stroke(twin)
                   .zIndex(2))
        // ---- cableway (top gap) ----
        .child(rail({{"cb_w"}, {"cb_e"}}, routers::polyline(0))
                   .inset(0)
                   .stroke(cableway)
                   .zIndex(3))
        // ---- millbrook creek (tapers INTO the smokewater) ----
        .child(rail({{"ck_s"}, {"ck2"}, {"ck1"}, {"ck_m"}},
                    routers::octilinear(10))  // source->mouth: narrow->wide
                   .inset(0)
                   .stroke(creek)
                   .zIndex(1))
        // ---- ARTLINE: the SkVertices art warp (brush::artAlong) — one
        // leaf-vine cell stretched and BENT along the S-curve; rigid
        // stamps can't follow this curvature continuously ----
        .child(box()
                   .inset(58, 452, nn::kW - 430, nn::kH - 548)
                   .shape([](SkSize sz) {
                     SkPathBuilder b;
                     b.moveTo(0, sz.height() * 0.72f);
                     b.cubicTo(sz.width() * 0.24f, sz.height() * -0.25f,
                               sz.width() * 0.40f, sz.height() * 1.30f,
                               sz.width() * 0.64f, sz.height() * 0.42f);
                     b.cubicTo(sz.width() * 0.80f, sz.height() * -0.15f,
                               sz.width() * 0.90f, sz.height() * 0.75f,
                               sz.width() * 1.0f, sz.height() * 0.35f);
                     return b.detach();
                   })
                   .foreground(brush::artAlong(nn::vineArt(), 14, 5))
                   .zIndex(3))
        // ---- the saltmarsh: Sk2D lattice hatch on a blob field ----
        .child(box()
                   .width(120)
                   .height(74)
                   .centerAt({760, 524})
                   .shape(shapes::blob(7, 0.16f))
                   .fill(Fill::color({0.10f, 0.20f, 0.20f, 0.55f}))
                   .background(lines::hatch(
                       Fill::color({0.36f, 0.72f, 0.62f, 0.5f}), 7, 1.1f, -32))
                   .zIndex(1))
        // ---- the pipeline trio: identical points, different ops ----
        .child(box()
                   .inset(49, 554, nn::kW - 232, nn::kH - 581)
                   .shape(demoPath)
                   .stroke(demoRun(shapers::Wave{.amplitude = 4,
                                                             .wavelength = 28},
                                   nn::kCyan))
                   .zIndex(3))
        .child(
            box()
                .inset(49, 580, nn::kW - 232, nn::kH - 607)
                .shape(demoPath)
                .stroke(demoRun(shapers::Zigzag{.amplitude = 4,
                                                            .wavelength = 28},
                                nn::kAmber))
                .zIndex(3))
        .child(
            box()
                .inset(49, 606, nn::kW - 232, nn::kH - 633)
                .shape(demoPath)
                .stroke(demoRun(shapers::Square{.amplitude = 4,
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
        // The cableway ran from (302, 96) to (492, 48), which is through
        // the legend panel: its cable and its station rings crossed the
        // TWIN SERVICE and CABLEWAY rows and collided with their type. It
        // spans the clear band above the carto railway instead.
        .child(nn::pin("cb_w", 508, 56))
        .child(nn::pin("cb_e", 842, 34))
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
        .child(nn::label("the smokewater", 668, 206, {0.45f, 0.62f, 0.78f, 1}))
        // ---- title + legend ----
        .child(
            box()
                .column()
                .inset(28, 27, 0, 0)
                .zIndex(8)
                .child(text(toU8("NIGHT NETWORK"), nn::type(30, nn::kBone, 2)))
                .child(text(toU8("the brush engine \xe2\x80\x94 twelve"
                                 " constructions"),
                            nn::type(14, nn::kAsh, 1))
                           .margin(0, 6, 0, 0)))
        // Ten rows reach into the map now — a feathered ink backing keeps
        // the routes from striking through the legend type.
        .child(box()
                   .inset(18, 92, 0, 0)
                   .width(430)
                   .height(276)
                   .corners({10})
                   .fill(Fill::color({0.043f, 0.051f, 0.11f, 0.82f}))
                   .zIndex(7))
        .child(nn::legendRow("EMBER LINE", "cased pair over rounded corners",
                             nn::kEmber, 102))
        .child(nn::legendRow("STEEL SPUR", "carto railway: dark + white dash",
                             nn::kSteel, 124))
        .child(nn::legendRow("CURRENT LINE", "one-way: chevrons + arrow",
                             nn::kCyan, 146))
        .child(nn::legendRow("ORBITAL", "instanced station stamps", nn::kViolet,
                             168))
        .child(nn::legendRow("NIGHT BUS", "offset legs: lane + curb",
                             nn::kAmber, 190))
        .child(nn::legendRow("SMOKEWATER",
                             "TfL Thames rule: octilinear band + banks",
                             {0.45f, 0.62f, 0.78f, 1}, 212))
        .child(nn::legendRow("TWIN SERVICE",
                             "shared running: alternating dashes", nn::kRose,
                             234))
        .child(nn::legendRow("CABLEWAY", "stamped rings on a support cable",
                             nn::kAsh, 256))
        .child(nn::legendRow("MILLBROOK", "topo taper: calligraphic ribbon",
                             {0.36f, 0.66f, 0.86f, 1}, 278))
        .child(nn::legendRow("PIPELINE TRIO",
                             "same points: wave / zigzag / square", nn::kBone,
                             300))
        .child(nn::legendRow("ARTLINE", "SkVertices art warp: one bent vine",
                             {0.55f, 0.80f, 0.47f, 1}, 322))
        .child(nn::legendRow("SALTMARSH", "Sk2D lattice hatch on a blob",
                             {0.36f, 0.72f, 0.62f, 1}, 344));
  }
};

}  // namespace

SIGIL_SKETCH_AS(NightNetwork, "night network", "Catalog \xc2\xb7 Generative",
                "the brush engine, twelve constructions")
