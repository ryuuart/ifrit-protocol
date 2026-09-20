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
//   STEEL SPUR ...... brush::presets::railwayCarto LayerStyle -- osm-carto's
//   verified
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

// TAGS: Drawing/Brushes, Drawing/Generative

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Shaper.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

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
      .fill(Paint::recipe(
          sdf::material(sdf::circle(), {.fill = mskia::toColor(kBone),
                                        .borderWidth = 2.5f,
                                        .borderColor = mskia::toColor(kInk)})))
      .zIndex(6);
}

/** A place name, in the ink in force: the root's ash unless the caller
 *  names another. */
inline Element label(const char* s, float x, float y) {
  return text(s).font({.size = 13, .track = 1.5f}).inset(x, y, 0, 0).zIndex(8);
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
        .borderRadius({6.5f, 0, 6.5f, 0})
        .rotate(deg)
        .fill(Fill::color(c));
  };
  return stack().width(48).height(16).children(
      {box()
           .inset(0, 6.8f, 0, 6.8f)
           .borderRadius({1.2f})
           .fill(Fill::color(kMossDeep)),
       leaf(4, 0, -28, kMoss), leaf(18, 9, 152, kMossDeep),
       leaf(31, 0, -24, kMoss)});
}

/** THE LEGEND, one row per construction, in the order the map draws them: the
 *  line's name in its own colour and the construction it is made of. The row
 *  pitch is the legend's own and not each row's, so a row inserted here moves
 *  the ones under it. */
struct Legend {
  const char* name;
  const char* what;
  SkColor4f ink;
};

inline const Legend kLegend[] = {
    {"EMBER LINE", "cased pair over rounded corners", kEmber},
    {"STEEL SPUR", "carto railway: dark + white dash", kSteel},
    {"CURRENT LINE", "one-way: chevrons + arrow", kCyan},
    {"ORBITAL", "instanced station stamps", kViolet},
    {"NIGHT BUS", "offset legs: lane + curb", kAmber},
    {"SMOKEWATER",
     "TfL Thames rule: octilinear band + banks",
     {0.45f, 0.62f, 0.78f, 1}},
    {"TWIN SERVICE", "shared running: alternating dashes", kRose},
    {"CABLEWAY", "stamped rings on a support cable", kAsh},
    {"MILLBROOK", "topo taper: calligraphic ribbon", {0.36f, 0.66f, 0.86f, 1}},
    {"PIPELINE TRIO", "same points: wave / zigzag / square", kBone},
    {"ARTLINE", "SkVertices art warp: one bent vine", {0.55f, 0.80f, 0.47f, 1}},
    {"SALTMARSH", "Sk2D lattice hatch on a blob", {0.36f, 0.72f, 0.62f, 1}}};

/** Legend row: the line's name in its own colour, the construction note
 *  in the root's ash, both one size. */
inline Element legendRow(const char* name, const char* what, SkColor4f c,
                         float y) {
  return box()
      .row()
      .inset(30, y, 0, 0)
      .zIndex(8)
      .font({.size = 12.5f})
      .children({text(name).font({.track = 1.4f}).ink(c),
                 text(what).font({.track = 0.4f}).margin(10, 0, 0, 0)});
}

}  // namespace night_network

struct NightNetwork {
  choreograph::Output<float> emberReveal{0}, railReveal{0}, cyanReveal{0},
      ringReveal{0}, roadReveal{0};
  choreograph::Output<float> hubGlow{0};

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});
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

    ticker.add([this, &ticker] {
      const double t = ticker.elapsed();
      hubGlow = 4.0f + 2.0f * (float)std::sin(t * 2.1);
    });

    composer.render(describe());
  }

  Element describe() {
    namespace nn = night_network;

    // -- 1. EMBER LINE: the cased pair. Router left SHARP (radius 0);
    //    the Brush pipeline rounds, then the layer lays two rails whose
    //    dashes/parameters share one centerline (Lines.h keeps them in phase).
    Brush emberBrush;
    emberBrush.shaped(shapers::Rounded{12.0f});
    emberBrush.layer(
        lines::presets::cased(2.6f, Fill::color(nn::kEmber), 7.0f));

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
    Element ringStamp =
        box().width(11).height(11).fill(Paint::recipe(sdf::material(
            sdf::circle(), {.fill = mskia::toColor(nn::kBone),
                            .borderWidth = 2.0f,
                            .borderColor = {0.30f, 0.18f, 0.48f, 1}})));
    Brush orbital;
    orbital.layer(lines::Line{.width = 3.2f, .fill = Fill::color(nn::kViolet)});
    orbital.layer(brush::Scatter{.art = ringStamp,
                                 .spacing = 74.6f,
                                 .alignToPath = false,
                                 .bleedPx = 12.0f});

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
    creek.layer(brush::presets::taper(
        2.2f, 9.0f, Fill::color({0.13f, 0.27f, 0.40f, 0.9f})));

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
    // one row of the trio, at the trio's own 26 px pitch
    auto demoRow = [demoPath](int row, Brush run) {
      const float y = 554.0f + 26.0f * (float)row;
      return box()
          .inset(49, y, nn::kW - 232, nn::kH - y - 27.0f)
          .shape(demoPath)
          .stroke(std::move(run))
          .zIndex(3);
    };

    // The interchange: breathing SDF star (glow bound within its reserve --
    // the 72px reserve stays unscaled so the sdf pad keeps its budget).
    Element hub =
        box()
            .key("hub")
            .width(72)
            .height(72)
            .centerAt({436, 320})
            .fill(Paint::recipe(
                      sdf::material(sdf::star(8, 3.2f),
                                    {.fill = mskia::toColor(nn::kBone),
                                     .borderWidth = 2,
                                     .borderColor = mskia::toColor(nn::kInk),
                                     .glowRadius = 6,
                                     .glowColor = mskia::toColor(nn::kEmber)}))
                      .uniform("uGlowR", &hubGlow))
            .zIndex(7);

    // The type is stated once, here: every line inherits the ash and the
    // 8-bit colour ladder and says only what differs. The colour reaches
    // the paint as 8-bit sRGB so a tint computed per frame lands on the
    // same 256-step ladder as a quoted one; a float carried through
    // resolves differently on the device raster.
    return stack()
        .font({.color8 = true})
        .ink(nn::kAsh)
        .fill(Paint::linear(
            {0, 0}, {0, nn::kH},
            {{0.0f, nn::kInkHigh}, {0.5f, nn::kInk}, {1.0f, nn::kInk}}))
        // ---- the waterway, beneath everything ----
        .children(
            {rail({{"rv0"}, {"rv1"}, {"rv2"}, {"rv3"}, {"rv4"}},
                  routers::octilinear(20))  // the Thames rule: the river
                                            // rides the routes' own grid
                 .inset(0)
                 .stroke(river)
                 .zIndex(1),
             // ---- the bus corridor (bridges the river) ----
             rail({{"rd_w"}, {"rd1"}, {"rd2"}, {"rd_e"}}, routers::polyline(22))
                 .inset(0)
                 .mask(by::spans(spans::upTo(&roadReveal)))
                 .stroke(roadbed)
                 .stroke(busLane)
                 .stroke(curb)
                 .zIndex(2),
             // ---- the carto railway ----
             rail({{"rw_w"}, {"rw1"}, {"rw2"}, {"rw_e"}},
                  routers::octilinear(14))
                 .inset(0)
                 .mask(by::spans(spans::upTo(&railReveal)))
                 .style(brush::presets::railwayCarto(1.6f, nn::kSteel,
                                                     {0.95f, 0.94f, 0.90f, 1}))
                 .zIndex(3),
             // ---- the cased metro pair ----
             rail({{"em_w"}, {"em1"}, {"hub"}, {"em2"}, {"em_e"}},
                  routers::octilinear(0))
                 .inset(0)
                 .stroke(spans::upTo(&emberReveal), emberBrush)
                 .zIndex(4),
             // ---- the one-way line ----
             rail({{"cy_w"}, {"cy1"}, {"hub"}, {"cy2"}, {"cy_e"}},
                  routers::octilinear(8))
                 .inset(0)
                 .stroke(spans::upTo(&cyanReveal), current)
                 .zIndex(4),
             // ---- the orbital ring with instanced stations ----
             box()
                 .width(190)
                 .height(190)
                 .centerAt({436, 320})
                 .shape(shapes::arc(0.0f, 359.9f))
                 .stroke(spans::upTo(&ringReveal), orbital)
                 .zIndex(5),
             // ---- twin service (bottom-right strip) ----
             rail({{"tw_w"}, {"tw1"}, {"tw_e"}}, routers::octilinear(9))
                 .inset(0)
                 .stroke(twin)
                 .zIndex(2),
             // ---- cableway (top gap) ----
             rail({{"cb_w"}, {"cb_e"}}, routers::polyline(0))
                 .inset(0)
                 .stroke(cableway)
                 .zIndex(3),
             // ---- millbrook creek (tapers INTO the smokewater) ----
             rail({{"ck_s"}, {"ck2"}, {"ck1"}, {"ck_m"}},
                  routers::octilinear(10))  // source->mouth: narrow->wide
                 .inset(0)
                 .stroke(creek)
                 .zIndex(1),
             // ---- ARTLINE: the SkVertices art warp (brush::artAlong) — one
             // leaf-vine cell stretched and BENT along the S-curve; rigid
             // stamps can't follow this curvature continuously ----
             box()
                 .inset(58, 452, nn::kW - 430, nn::kH - 548)
                 .shape(keyedShape(
                     std::string_view("vine"),
                     [](SkSize sz) {
                       SkPathBuilder b;
                       b.moveTo(0, sz.height() * 0.72f);
                       b.cubicTo(sz.width() * 0.24f, sz.height() * -0.25f,
                                 sz.width() * 0.40f, sz.height() * 1.30f,
                                 sz.width() * 0.64f, sz.height() * 0.42f);
                       b.cubicTo(sz.width() * 0.80f, sz.height() * -0.15f,
                                 sz.width() * 0.90f, sz.height() * 0.75f,
                                 sz.width() * 1.0f, sz.height() * 0.35f);
                       return b.detach();
                     }))
                 .foreground(brush::artAlong(nn::vineArt(), 14, 5))
                 .zIndex(3),
             // ---- the saltmarsh: Sk2D lattice hatch on a blob field ----
             box()
                 .width(120)
                 .height(74)
                 .centerAt({760, 524})
                 .shape(shapes::blob(7, 0.16f))
                 .fill(Fill::color({0.10f, 0.20f, 0.20f, 0.55f}))
                 .background(lines::presets::hatch(
                     Fill::color({0.36f, 0.72f, 0.62f, 0.5f}), 7, 1.1f, -32))
                 .zIndex(1),
             // ---- the pipeline trio: IDENTICAL POINTS, three operations,
             // one row each at the trio's own pitch ----
             demoRow(0, demoRun(shapers::Wave{.amplitude = 4, .wavelength = 28},
                                nn::kCyan)),
             demoRow(1,
                     demoRun(shapers::Zigzag{.amplitude = 4, .wavelength = 28},
                             nn::kAmber)),
             demoRow(2,
                     demoRun(shapers::Square{.amplitude = 4, .wavelength = 28},
                             nn::kViolet)),
             // ---- waypoint pins (invisible) ----
             nn::pin("rv0", 692, 4), nn::pin("rv1", 654, 144),
             nn::pin("rv2", 559, 296), nn::pin("rv3", 584, 472),
             nn::pin("rv4", 492, 636), nn::pin("rd_w", 84, 552),
             nn::pin("rd1", 366, 520), nn::pin("rd2", 633, 552),
             nn::pin("rd_e", 823, 512), nn::pin("tw_w", 366, 610),
             nn::pin("tw1", 633, 610), nn::pin("tw_e", 830, 589),
             // The cableway ran from (302, 96) to (492, 48), which is through
             // the legend panel: its cable and its station rings crossed the
             // TWIN SERVICE and CABLEWAY rows and collided with their type. It
             // spans the clear band above the carto railway instead.
             nn::pin("cb_w", 508, 56), nn::pin("cb_e", 842, 34),
             nn::pin("ck_m", 579, 412), nn::pin("ck1", 636, 412),
             nn::pin("ck2", 676, 372), nn::pin("ck_s", 828, 372),
             // ---- stations ----
             nn::station("em_w", 98, 448), nn::station("em1", 267, 376),
             nn::station("em2", 661, 320), nn::station("em_e", 809, 248),
             nn::station("rw_w", 390, 166, 13),
             nn::station("rw1", 436, 120, 13), nn::station("rw2", 591, 120, 13),
             nn::station("rw_e", 830, 164, 13), nn::station("cy_w", 105, 304),
             nn::station("cy1", 302, 260), nn::station("cy2", 605, 436),
             nn::station("cy_e", 773, 500), hub,
             // ---- names ----
             nn::label("EMBER GATE", 461, 298).ink(nn::kBone),
             nn::label("wharf lane", 82, 466), nn::label("north quay", 524, 98),
             nn::label("saltmarsh", 693, 477),
             nn::label("the smokewater", 668, 206)
                 .ink(SkColor4f{0.45f, 0.62f, 0.78f, 1}),
             // ---- title + legend ----
             box()
                 .column()
                 .inset(28, 27, 0, 0)
                 .zIndex(8)
                 .children({text("NIGHT NETWORK")
                                .font({.size = 30, .track = 2})
                                .ink(nn::kBone),
                            text("the brush engine — twelve"
                                 " constructions")
                                .font({.size = 14, .track = 1})
                                .margin(0, 6, 0, 0)}),
             // Ten rows reach into the map now — a feathered ink backing keeps
             // the routes from striking through the legend type.
             box()
                 .inset(18, 92, 0, 0)
                 .width(430)
                 .height(276)
                 .borderRadius({10})
                 .fill(Fill::color({0.043f, 0.051f, 0.11f, 0.82f}))
                 .zIndex(7),
             each(nn::kLegend, [](const nn::Legend& l, size_t i) {
               return nn::legendRow(l.name, l.what, l.ink,
                                    102.0f + 22.0f * (float)i);
             })});
  }
};

}  // namespace

SIGIL_SKETCH_AS(NightNetwork, "night network", "Catalog · Generative",
                "the brush engine, twelve constructions")
