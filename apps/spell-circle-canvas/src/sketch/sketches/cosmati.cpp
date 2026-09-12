/** @file
 * cosmati — opus sectile: a quincunx of quarried stone with guilloche
 * bands, constructed rather than traced.
 */

// The opus-sectile study: the Cosmatesque pavement tradition — Roman
// marble-workers of the 12th–13th centuries, and specifically the Great
// Pavement laid before the high altar at Westminster Abbey in 1268 by a
// Roman crew under Odoricus.
//
// What the historical record gives us, and what this scene uses:
//   * the QUINCUNX — four roundels around a fifth — is the tradition's
//     governing figure, and the Great Pavement is a quincunx of
//     quincunxes: one at the centre of the field and one in each quarter
//   * the field is 25 medieval Roman feet square (24 ft 10 in, 7.58 m),
//     so the study keeps a SQUARE field and hangs its apparatus beside it
//   * the stones are spolia and each has a place: purple porphyry from
//     Mons Claudianus, green lapis lacedaemonius from Krokeai near
//     Sparta, yellow limestone, white marble, onyx for the centre
//     roundel, and opaque coloured glass in red, turquoise, cobalt and
//     bluish white — the framework at Westminster is Purbeck, not marble
//   * the bands between the roundels are GUILLOCHE: two strands plaited
//     around a row of discs
//
// Everything is generated. The stones are noise and speckle tinted per
// quarry; the guilloche is two phase-shifted sine strands with a disc at
// every crossing; the roundels are rings of lozenges laid on an angular
// step; the quarter fields are a three-colour triangular tessellation.
// A raking light crosses the pavement the way it does at floor level,
// and the whole floor lays itself ring by ring.

// TAGS: Patterns/Tiling

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Legend.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Panel.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;
namespace motion = sigil::motion;
namespace mkit = sigil::material::kit;
namespace mskia = sigil::material::skia;

using namespace sigil::compose;
using sigil::compose::toUtf8;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace cosmati {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

// The quarry list, as stone rather than as decoration.
// THE PAVEMENT IS WORN STONE, not stained glass. Every quarry here is
// read off the Great Pavement photographed flat: warm, dusty and
// low-chroma — ochres, dull reds, greys, dull greens, creams, porphyry a
// dark purple-brown. The header's own warning against rainbow terrazzo
// applies to a saturated reading of the same list, and a black mortar
// under it turns the Purbeck framework — which is the pavement's
// STRONGEST structure — into void.
constexpr SkColor4f kPorphyry = hexColor(0x74494A);  // Mons Claudianus, purple
constexpr SkColor4f kPorphyryLo = hexColor(0x4C2F31);
constexpr SkColor4f kSerpentine = hexColor(0x5B6552);  // lapis lacedaemonius
constexpr SkColor4f kSerpentineLo = hexColor(0x3E4739);
constexpr SkColor4f kGiallo = hexColor(0xC3AA76);  // yellow limestone
constexpr SkColor4f kGialloLo = hexColor(0x9A8455);
constexpr SkColor4f kMarble = hexColor(0xDED6C4);
constexpr SkColor4f kMarbleLo = hexColor(0xB9B0A0);
constexpr SkColor4f kOnyx = hexColor(0xCDBB94);
constexpr SkColor4f kPurbeck = hexColor(0x77756B);  // the framework, not marble
constexpr SkColor4f kPurbeckLo = hexColor(0x4F4E47);
constexpr SkColor4f kGlassRed = hexColor(0x9A5348);
constexpr SkColor4f kGlassTurq = hexColor(0x62867F);
constexpr SkColor4f kGlassCobalt = hexColor(0x4E5A7E);
constexpr SkColor4f kMortar = hexColor(0x6C695F);
constexpr SkColor4f kInk = hexColor(0xE8E1CE);
constexpr SkColor4f kInkDim = hexColor(0x9A9078);

// The field is square because the pavement is square.
constexpr float kFieldSide = 556;
constexpr float kFieldX = 40;
constexpr float kFieldY = (kH - kFieldSide) * 0.5f;
constexpr float kBandW = 40;  // the Purbeck frame carrying the inscription
constexpr float kInner = kFieldSide - 2 * kBandW;

/** A CUT STONE, off the material kit's own recipe: the quarry's two
 *  tones on a diagonal bed, veined with LUMINANCE grain and flecked with
 *  a speckle in the stone's own colours, every field generated per pixel
 *  from these numbers rather than baked into a tile. Luminance grain
 *  rather than fractal RGB is the whole reason it reads as stone: RGB
 *  noise's three channels are independent, so over a coloured surface it
 *  hue-shifts rather than shades, and a Cosmati floor is the one thing
 *  that must never read as rainbow terrazzo. `contrast` is how hard the
 *  veining reads, and the framework band asks for less of it than the
 *  tesserae do so that the Purbeck reads as one stone. */
inline Paint stone(SkColor4f hi, SkColor4f lo, float angleDeg = 24,
                   float contrast = 0.35f) {
  return Paint::recipe(mkit::stone({.hi = mskia::toColor(hi),
                                    .lo = mskia::toColor(lo),
                                    .bedAngle = angleDeg,
                                    .grainContrast = contrast,
                                    .speckle = 0.30f,
                                    .speckleCell = 9.0f,
                                    .speckleAlpha = 0.27f}));
}

/** A regular polygon ring: `count` lozenges laid on a circle of radius
 *  `r`, each pointing outward — the roundel's ring courses. */
inline std::function<SkPath(SkSize)> lozengeRing(int count, float rInner,
                                                 float rOuter, float phase) {
  return [count, rInner, rOuter, phase](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float step =
        arrange::step(6.2831853f, (size_t)count, arrange::Turn::Closed);
    for (int i = 0; i < count; ++i) {
      const float a = arrange::along(phase, 6.2831853f, (size_t)i,
                                     (size_t)count, arrange::Turn::Closed);
      const float half = step * 0.42f;
      const float c0 = std::cos(a - half), s0 = std::sin(a - half);
      const float c1 = std::cos(a + half), s1 = std::sin(a + half);
      const float cm = std::cos(a), sm = std::sin(a);
      b.moveTo(cx + cm * rInner, cy + sm * rInner);
      b.lineTo(cx + c0 * (rInner + rOuter) * 0.5f,
               cy + s0 * (rInner + rOuter) * 0.5f);
      b.lineTo(cx + cm * rOuter, cy + sm * rOuter);
      b.lineTo(cx + c1 * (rInner + rOuter) * 0.5f,
               cy + s1 * (rInner + rOuter) * 0.5f);
      b.close();
    }
    return b.detach();
  };
}

/** The guilloche: two strands plaited about the band's centreline. The
 *  discs at the crossings are drawn separately so they can be stone. */
inline std::function<SkPath(SkSize)> guillocheStrand(float periods, float phase,
                                                     float amplitude) {
  return [periods, phase, amplitude](SkSize s) {
    SkPathBuilder b;
    const float mid = s.height() * 0.5f;
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float x = 0; x <= s.width(); x += 2.0f) {
      const float t = x / std::max(s.width(), 1.0f);
      const float y =
          mid + amplitude * std::sin(t * periods * 6.2831853f + phase);
      if (x == 0)
        b.moveTo(x, y);
      else
        b.lineTo(x, y);
    }
    return b.detach();
  };
}

/** The three-colour triangular tessellation that fills a quarter field. */
inline std::function<SkPath(SkSize)> triangleCourse(int cols, int rows,
                                                    int residue) {
  return [cols, rows, residue](SkSize s) {
    SkPathBuilder b;
    const float w = s.width() / (float)cols;
    const float h = s.height() / (float)rows;
    for (int r = 0; r < rows; ++r)
      for (int c = 0; c < cols * 2; ++c) {
        // two triangles per cell, alternating orientation
        const int index = (r * cols * 2 + c);
        if (index % 3 != residue) continue;
        const int cell = c / 2;
        const float x = (float)cell * w;
        const float y = (float)r * h;
        const bool up = (c % 2 == 0) == (r % 2 == 0);
        if (up) {
          b.moveTo(x, y + h);
          b.lineTo(x + w * 0.5f, y);
          b.lineTo(x + w, y + h);
        } else {
          b.moveTo(x, y);
          b.lineTo(x + w, y);
          b.lineTo(x + w * 0.5f, y + h);
        }
        b.close();
      }
    return b.detach();
  };
}

}  // namespace cosmati

struct Cosmati final : sketch::Sketch {
  choreograph::Output<float> rake{0};  // the raking light's sweep, 0..1
  choreograph::Output<float> lay{0};   // the laying-in progress, 0..1

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    rake = 0;
    lay = 0;
    ticker.add([this, &ticker](double) {
      const double t = ticker.elapsed();
      // A raking light crosses the floor every 7 s: the way polished
      // porphyry actually announces itself in a nave.
      rake = (float)std::fmod(t / 7.0, 1.0);
      lay = (float)std::min(1.0, t / 2.4);
      return true;
    });
    composer.render(describe());
  }

  // ------------------------------------------------------------------

  /** One roundel: an onyx or porphyry eye inside ring courses of
   *  lozenges, bounded by a marble fillet. */
  Element roundel(SkPoint at, float r, SkColor4f eyeHi, SkColor4f eyeLo,
                  int seed) {
    namespace cs = cosmati;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const std::chrono::milliseconds delay{140 + 90 * seed};
    // Baked for the same reason the bed is: the entrance transforms are
    // paint-only, so they still animate over a texture that was
    // rasterized once.
    Element el = stack()
                     .width(Dimension(r * 2))
                     .height(Dimension(r * 2))
                     .centerAt(at)
                     .cache(Cache::Texture)
                     .opacity(animate(motion::from(0.0f).to(1.0f),
                                      {380ms, &ch::easeOutQuad, delay}))
                     .scale(animate(motion::from(0.86f).to(1.0f),
                                    {520ms, &ch::easeOutQuint, delay}));
    // the bed
    el.child(box().inset(0).corners({r}).fill(Paint::solid(cs::kMortar)));
    // outer fillet
    el.child(box().inset(0).corners({r}).foreground(
        stroke(3.0f, Fill::color(cs::kMarble), PathFormat::Align::Inner)));
    // two ring courses of lozenges, counter-phased
    el.child(box()
                 .inset(0)
                 .shape(cs::lozengeRing(12, r * 0.56f, r * 0.90f, 0.0f))
                 .fill(cs::stone(cs::kSerpentine, cs::kSerpentineLo, 40))
                 .stroke(stroke(0.8f, Fill::color(cs::kMortar))));
    el.child(box()
                 .inset(0)
                 .shape(cs::lozengeRing(8, r * 0.30f, r * 0.54f, 0.3926991f))
                 .fill(cs::stone(cs::kGiallo, cs::kGialloLo, 12))
                 .stroke(stroke(0.8f, Fill::color(cs::kMortar))));
    // the eye
    el.child(box()
                 .inset(r * 0.72f)
                 .corners({r * 0.28f})
                 .fill(cs::stone(eyeHi, eyeLo, 60))
                 .foreground(stroke(1.6f, Fill::color(cs::kMarble))));
    return el;
  }

  /** A guilloche band between two roundels: two plaited strands with a
   *  disc at every crossing, on a Purbeck bed. */
  Element guilloche(float x, float y, float w, float h, float degrees,
                    int seed) {
    namespace cs = cosmati;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const int periods = std::max(2, (int)std::round(w / (h * 1.5f)));
    const std::chrono::milliseconds delay{420 + 70 * seed};
    Element band = stack()
                       .width(Dimension(w))
                       .height(Dimension(h))
                       .centerAt({x, y})
                       .rotate(degrees)
                       .cache(Cache::Texture)
                       .opacity(animate(motion::from(0.0f).to(1.0f),
                                        {360ms, &ch::easeOutQuad, delay}));
    band.child(box()
                   .inset(0)
                   .fill(cs::stone(cs::kPurbeck, cs::kPurbeckLo, 8))
                   .foreground(stroke(1.4f, Fill::color(cs::kMarble),
                                      PathFormat::Align::Inner)));
    band.child(box()
                   .inset(0)
                   .shape(cs::guillocheStrand((float)periods, 0.0f, h * 0.26f))
                   .stroke(spans::upTo(&lay),
                           stroke(h * 0.20f, Fill::color(cs::kGiallo))));
    band.child(
        box()
            .inset(0)
            .shape(cs::guillocheStrand((float)periods, 3.14159265f, h * 0.26f))
            .stroke(spans::upTo(&lay),
                    stroke(h * 0.20f, Fill::color(cs::kSerpentine))));
    // the discs the strands plait around
    for (int i = 0; i < periods; ++i) {
      const float cx = w * ((float)i + 0.5f) / (float)periods;
      band.child(box()
                     .left(cx - h * 0.20f)
                     .top(h * 0.30f)
                     .width(Dimension(h * 0.40f))
                     .height(Dimension(h * 0.40f))
                     .corners({h * 0.20f})
                     .fill(cs::stone(cs::kPorphyry, cs::kPorphyryLo, 30))
                     .foreground(stroke(1.0f, Fill::color(cs::kMarble))));
    }
    return band;
  }

  /** A quarter field: the three-colour triangular course, on mortar. */
  Element quarter(float x, float y, float side, int seed) {
    namespace cs = cosmati;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const std::chrono::milliseconds delay{600 + 80 * seed};
    // The bed is the largest stone surface on the plate and every square
    // pixel of it is an SkSL evaluation — three fBm octaves under three
    // gradients. It is static once it has entered, so bake it: the
    // alternative is re-evaluating that shader over the whole quarter on
    // every frame, which the CPU raster backend does literally.
    Element q = stack()
                    .width(Dimension(side))
                    .height(Dimension(side))
                    .left(x)
                    .top(y)
                    .cache(Cache::Texture)
                    .opacity(animate(motion::from(0.0f).to(1.0f),
                                     {420ms, &ch::easeOutQuad, delay}));
    q.child(box().inset(0).fill(Paint::solid(cs::kMortar)));
    // The pavement's tesserae are an order of magnitude smaller than one
    // coarse course: no interstitial area on the Great Pavement reads as
    // a dozen triangles across, and at that size the field reads as a
    // diagram of a tessellation rather than as a mosaic.
    const int cols = 22, rows = 22;
    q.child(box()
                .inset(0)
                .shape(cs::triangleCourse(cols, rows, 0))
                .fill(cs::stone(cs::kPorphyry, cs::kPorphyryLo, 18)));
    q.child(box()
                .inset(0)
                .shape(cs::triangleCourse(cols, rows, 1))
                .fill(cs::stone(cs::kMarble, cs::kMarbleLo, 52)));
    q.child(box()
                .inset(0)
                .shape(cs::triangleCourse(cols, rows, 2))
                .fill(cs::stone(cs::kSerpentine, cs::kSerpentineLo, 34)));
    q.child(box().inset(0).foreground(
        stroke(2.0f, Fill::color(cs::kMarble), PathFormat::Align::Inner)));
    return q;
  }

  /** THE APPARATUS' OWN LOOK, for the quarry key: one register and one
   *  ink, which is the whole of what the key is set in. */
  static sketch::kit::Theme quarryTheme() {
    sketch::kit::Theme paper;
    paper.palette.ink = cosmati::kInkDim;
    paper.type.captionNote = {10.5f, 0.7f};
    return paper;
  }

  Element describe() {
    namespace cs = cosmati;
    namespace ch = choreograph;
    using namespace std::chrono_literals;

    // The apparatus' dim ink, stated once; the title and the ring say
    // theirs.
    auto root = stack()
                    .fill(Paint::linear({0, 0}, {0, cs::kH},
                                        {{0.0f, hexColor(0x14120F)},
                                         {1.0f, hexColor(0x080706)}}))
                    .ink(cs::kInkDim);

    // ---- the pavement ------------------------------------------------
    // The frame's lettering is the plate's own font: the two lines cut
    // into the Purbeck band inherit it whole.
    Element floorPlate = stack()
                             .key("floor")
                             .left(cs::kFieldX)
                             .top(cs::kFieldY)
                             .width(Dimension(cs::kFieldSide))
                             .height(Dimension(cs::kFieldSide))
                             .font({.size = 11, .track = 3.4f});

    // THE PURBECK FRAME IS A FRAME: a shell carrying the inscription
    // band, with the mortar bed set into it by the band's own width on
    // all four sides. The shell's fill covers the whole plate even
    // though only its border shows, and on the raster backend every
    // pixel of it is an SkSL evaluation — one octave for that reason.
    floorPlate.child(
        sketch::kit::frame(
            {.shell = cs::stone(cs::kPurbeck, cs::kPurbeckLo, 6, 0.18f),
             .corners = 0,
             .bezel = cs::kBandW,
             .screen = Paint::solid(cs::kMortar),
             .screenCorners = 0,
             .keyline = Fill::none()})
            .inset(0)
            .foreground(stroke(2.0f, Fill::color(cs::kMarble),
                               PathFormat::Align::Inner))
            .background(styles::dropShadow({0, 0, 0, 0.7f}, {0, 8}, 18)));
    floorPlate.child(text(toUtf8("\xc2\xb7 QVATVOR \xc2\xb7 PRAECEDENTES "
                                 "\xc2\xb7 ET \xc2\xb7 TRES \xc2\xb7"))
                         .left(cs::kBandW)
                         .top(13));
    floorPlate.child(text(toUtf8("\xc2\xb7 ODORICVS \xc2\xb7 FECIT \xc2\xb7 "
                                 "MCCLXVIII \xc2\xb7"))
                         .left(cs::kBandW)
                         .top(cs::kFieldSide - 24));

    // ---- the quincunx of quincunxes ---------------------------------
    const float c = cs::kFieldSide * 0.5f;
    const float big = cs::kInner * 0.185f;    // the central roundel
    const float small = cs::kInner * 0.125f;  // the four around it
    const float arm = cs::kInner * 0.295f;    // centre-to-corner-roundel

    // the triangular course is the BED: a pavement has no bare mortar,
    // the quincunx is set into a field that is already tessellated
    quarterInto(floorPlate, cs::kBandW, cs::kBandW, cs::kInner, 0);

    // the guilloche arms, one to each corner roundel
    const float bandH = cs::kInner * 0.115f;
    const float armLen = arm * 1.414f - big - small + 8;
    for (int i = 0; i < 4; ++i) {
      // The arm is turned onto its own diagonal, so the angle is wanted
      // as well as the place it is centred on.
      const float a = arrange::along(0.7853982f, 6.2831853f, (size_t)i, 4,
                                     arrange::Turn::Closed);
      const SkPoint mid =
          arrange::onEllipse({c, c}, {arm * 0.7071f + 4, arm * 0.7071f + 4}, a);
      const float mx = mid.fX, my = mid.fY;
      floorPlate.child(
          guilloche(mx, my, armLen, bandH, a * 180.0f / 3.14159265f, i));
    }

    // the roundels: four around one
    for (int i = 0; i < 4; ++i)
      floorPlate.child(roundel(
          arrange::onRing((size_t)i, 4, {c, c}, {arm * 1.414f, arm * 1.414f},
                          0.7853982f, 6.2831853f, arrange::Turn::Closed),
          small, cs::kGlassTurq, cs::kGlassCobalt, i + 1));
    floorPlate.child(roundel({c, c}, big, cs::kOnyx, cs::kGialloLo, 0));

    // The circular inscription these pavements carry round their centre
    // roundel. Real Cosmati work sets it in the ring itself; this is one
    // shaped run on a circular baseline — onPath, not 40 hand-placed
    // glyphs — and it does NOT auto-flip, because the letter-cutters
    // didn't: glyph-up points outward the whole way round.
    floorPlate.child(
        text(toUtf8("\xc2\xb7 SPHERICVM \xc2\xb7 ARCHETYPVM "
                    "\xc2\xb7 MVNDVM \xc2\xb7 PRIMVM \xc2\xb7 "
                    "TRIPLEX \xc2\xb7"))
            .font(
                {.size = 9, .color = cs::kGiallo, .track = 2.0f, .weight = 600})
            .width(Dimension(big * 1.50f))
            .height(Dimension(big * 1.50f))
            .centerAt({c, c})
            .onPath({.path = shapes::arc(-90.0f, 359.9f),
                     .at = 0.0f,
                     .align = TextPath::Align::Start,
                     .offset = 0.0f})
            .zIndex(6));

    // the raking light: a soft band crossing the polished floor
    floorPlate.child(
        box()
            .left(-260)
            .top(-40)
            .width(Dimension(210.0f))
            .height(Dimension(cs::kFieldSide + 80))
            .rotate(14.0f)
            .translateX(motion::bind(&rake).target(-260, cosmati::kW + 260))
            .fill(Paint::linear({0, 0}, {210, 0},
                                {{0.0f, {1, 0.96f, 0.88f, 0.0f}},
                                 {0.5f, {1, 0.96f, 0.88f, 0.13f}},
                                 {1.0f, {1, 0.96f, 0.88f, 0.0f}}}))
            .blend(SkBlendMode::kPlus)
            .zIndex(9));
    root.child(std::move(floorPlate));

    // ---- the apparatus beside the floor ------------------------------
    const float px = cs::kFieldX + cs::kFieldSide + 34;
    root.child(
        box()
            .column()
            .left(px)
            .top(cs::kFieldY + 4)
            .child(text(toUtf8("OPUS SECTILE"))
                       .font({.size = 21,
                              .color = cs::kInk,
                              .track = 3.4f,
                              .weight = 640}))
            .child(text(toUtf8("Cosmatesque \xc2\xb7 Westminster "
                               "1268"))
                       .font({.size = 11, .track = 1.4f})
                       .margin(0, 6, 0, 0))
            .child(box()
                       .width(Dimension(190.0f))
                       .height(Dimension(1.0f))
                       .margin(0, 12, 0, 12)
                       .fill(Paint::linear({0, 0}, {190, 0},
                                           {{0.0f,
                                             {cs::kGiallo.fR, cs::kGiallo.fG,
                                              cs::kGiallo.fB, 0.7f}},
                                            {1.0f,
                                             {cs::kGiallo.fR, cs::kGiallo.fG,
                                              cs::kGiallo.fB, 0.0f}}})))
            .child(text(toUtf8("The governing figure is the QUINCUNX "
                               "\xe2\x80\x94 four roundels about a "
                               "fifth. The Great Pavement is a "
                               "quincunx of quincunxes, 25 Roman feet "
                               "square, laid by a Roman crew under "
                               "Odoricus."))
                       .font({.size = 11.5f, .track = 0.2f})
                       .width(Dimension(210.0f))));

    // the quarry legend: every stone named, with a real sample of it
    struct Quarry {
      const char* label;
      SkColor4f hi, lo;
    };
    static const Quarry kQuarries[] = {
        {"porphyry \xc2\xb7 Mons Claudianus", cs::kPorphyry, cs::kPorphyryLo},
        {"lapis lacedaemonius \xc2\xb7 Krokeai", cs::kSerpentine,
         cs::kSerpentineLo},
        {"giallo antico", cs::kGiallo, cs::kGialloLo},
        {"white marble", cs::kMarble, cs::kMarbleLo},
        {"onyx \xc2\xb7 the centre", cs::kOnyx, cs::kGialloLo},
        {"Purbeck \xc2\xb7 the framework", cs::kPurbeck, cs::kPurbeckLo},
        {"glass \xc2\xb7 red", cs::kGlassRed, cs::kPorphyryLo},
        {"glass \xc2\xb7 turquoise", cs::kGlassTurq, cs::kSerpentineLo},
        {"glass \xc2\xb7 cobalt", cs::kGlassCobalt, cs::kPurbeckLo},
    };
    // EACH ENTRY'S MARK IS A REAL SAMPLE OF THE STONE, cut to the
    // proportion a tessera is, so the key is quarried from the same
    // recipes the floor is.
    std::vector<sketch::kit::LegendEntry> quarries;
    for (const Quarry& q : kQuarries)
      quarries.push_back(
          {.label = toUtf8(q.label),
           .mark = box()
                       .width(Dimension(20.0f))
                       .height(Dimension(13.0f))
                       .fill(cs::stone(q.hi, q.lo, 34))
                       .foreground(stroke(
                           1.0f, Fill::color({cs::kMarble.fR, cs::kMarble.fG,
                                              cs::kMarble.fB, 0.55f}))),
           .opacity = animate(motion::from(0.0f).to(1.0f), {320ms}),
           .slide = animate(motion::from(-14.0f).to(0.0f), {400ms})});
    {
      const sketch::kit::Provide look(quarryTheme());
      root.child(sketch::kit::legend(
                     {.entries = std::move(quarries), .gap = 6, .labelGap = 9})
                     .key("quarries")
                     .left(px)
                     .bottom(46)
                     .staggerChildren(60ms));
    }
    return root;
  }

 private:
  void quarterInto(Element& parent, float x, float y, float side, int seed) {
    parent.child(quarter(x, y, side, seed));
  }
};

}  // namespace

SIGIL_SKETCH_AS(Cosmati, "cosmati", "Study \xc2\xb7 Pattern",
                "opus sectile \xe2\x80\x94 quincunx, guilloche, quarried stone")
