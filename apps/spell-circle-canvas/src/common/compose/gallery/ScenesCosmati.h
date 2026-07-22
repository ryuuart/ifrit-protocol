#pragma once
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

#include "GalleryCore.h"

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace compose_gallery {

namespace cosmati {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f,
          a};
}

// The quarry list, as stone rather than as decoration.
constexpr SkColor4f kPorphyry = C(0x6B2F3C);    // Mons Claudianus, purple
constexpr SkColor4f kPorphyryLo = C(0x3E1A24);
constexpr SkColor4f kSerpentine = C(0x35563C);  // lapis lacedaemonius
constexpr SkColor4f kSerpentineLo = C(0x1D3122);
constexpr SkColor4f kGiallo = C(0xC49A4E);      // yellow limestone
constexpr SkColor4f kGialloLo = C(0x8A6A2E);
constexpr SkColor4f kMarble = C(0xE4DED0);
constexpr SkColor4f kMarbleLo = C(0xBDB4A2);
constexpr SkColor4f kOnyx = C(0xD8C79B);
constexpr SkColor4f kPurbeck = C(0x4A4B46);     // the framework, not marble
constexpr SkColor4f kPurbeckLo = C(0x2C2D2A);
constexpr SkColor4f kGlassRed = C(0xA82A28);
constexpr SkColor4f kGlassTurq = C(0x2E8C8C);
constexpr SkColor4f kGlassCobalt = C(0x2A3E8C);
constexpr SkColor4f kMortar = C(0x191713);
constexpr SkColor4f kInk = C(0xE8E1CE);
constexpr SkColor4f kInkDim = C(0x9A9078);

// The field is square because the pavement is square.
constexpr float kFieldSide = 556;
constexpr float kFieldX = 40;
constexpr float kFieldY = (kH - kFieldSide) * 0.5f;
constexpr float kBandW = 40;  // the Purbeck frame carrying the inscription
constexpr float kInner = kFieldSide - 2 * kBandW;

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, float weight = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  if (weight > 0)
    s.shaping.variations = {sigil::weave::FontVariation("wght", weight)};
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** A cut stone: the quarry's two tones on a diagonal bed, veined with
 *  LUMINANCE grain and flecked with a speckle in the stone's own colours.
 *  patterns::grain() exists because patterns::noise() is fractal RGB
 *  noise whose three channels are independent — overlaid on a coloured
 *  surface it hue-shifts rather than shades, and turned every quarry here
 *  into rainbow terrazzo, which is the one thing a Cosmati floor is not. */
inline Material stone(SkColor4f hi, SkColor4f lo, float angleDeg = 24,
                      int octaves = 3) {
  const float a = angleDeg * 3.14159265f / 180.0f;
  const float dx = std::cos(a) * 52.0f, dy = std::sin(a) * 52.0f;
  auto mix = [](SkColor4f c, float k, float alpha) {
    return SkColor4f{std::min(1.0f, c.fR * k), std::min(1.0f, c.fG * k),
                     std::min(1.0f, c.fB * k), alpha};
  };
  return Material::blend(
      {{Material::linear({0, 0}, {dx, dy},
                         {{0.0f, hi}, {0.52f, lo}, {1.0f, hi}}),
        SkBlendMode::kSrc},
       {patterns::grain(0.055f, octaves, 7.0f), SkBlendMode::kOverlay},
       {patterns::speckle(34, 16, 0.5f, 1.9f,
                          {mix(hi, 1.45f, 0.26f), mix(lo, 0.55f, 0.28f)})
            .material(),
        SkBlendMode::kSrcOver}});
}

/** A regular polygon ring: `count` lozenges laid on a circle of radius
 *  `r`, each pointing outward — the roundel's ring courses. */
inline std::function<SkPath(SkSize)> lozengeRing(int count, float rInner,
                                                 float rOuter, float phase) {
  return [count, rInner, rOuter, phase](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float step = 6.2831853f / (float)count;
    for (int i = 0; i < count; ++i) {
      const float a = phase + step * (float)i;
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
inline std::function<SkPath(SkSize)> guillocheStrand(float periods,
                                                     float phase,
                                                     float amplitude) {
  return [periods, phase, amplitude](SkSize s) {
    SkPathBuilder b;
    const float mid = s.height() * 0.5f;
    for (float x = 0; x <= s.width(); x += 2.0f) {
      const float t = x / std::max(s.width(), 1.0f);
      const float y = mid + amplitude *
                                std::sin(t * periods * 6.2831853f + phase);
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
        if (index % 3 != residue)
          continue;
        const float x = (float)(c / 2) * w;
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

} // namespace cosmati

struct CosmatiScene final : Scene {
  choreograph::Output<float> rake{0};   // the raking light's sweep, 0..1
  choreograph::Output<float> lay{0};    // the laying-in progress, 0..1
  choreograph::Output<float> plait{0};  // the guilloche phase

  const char *name() const override { return "cosmati"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    rake = 0;
    lay = 0;
    plait = 0;
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // A raking light crosses the floor every 7 s: the way polished
      // porphyry actually announces itself in a nave.
      rake = (float)std::fmod(t / 7.0, 1.0);
      plait = (float)std::fmod(t * 0.10, 1.0);
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
    Element el = stack().width(Dim(r * 2)).height(Dim(r * 2)).centerAt(at)
                     .cache(Cache::Texture)
                     .opacity(withFrom(0.0f, 1.0f, {380ms, &ch::easeOutQuad,
                                                    delay}))
                     .scale(withFrom(0.86f, 1.0f, {520ms, &ch::easeOutQuint,
                                                   delay}));
    // the bed
    el.child(box().inset(0).corners({r})
                 .fill(Material::solid(cs::kMortar)));
    // outer fillet
    el.child(box().inset(0).corners({r})
                 .foreground(util::stroke(3.0f,
                                          Fill::color(cs::kMarble),
                                          PathFormat::Align::Inner)));
    // two ring courses of lozenges, counter-phased
    el.child(box().inset(0)
                 .outline(cs::lozengeRing(12, r * 0.56f, r * 0.90f, 0.0f))
                 .fill(cs::stone(cs::kSerpentine, cs::kSerpentineLo, 40))
                 .stroke(util::stroke(0.8f, Fill::color(cs::kMortar))));
    el.child(box().inset(0)
                 .outline(cs::lozengeRing(8, r * 0.30f, r * 0.54f,
                                          0.3926991f))
                 .fill(cs::stone(cs::kGiallo, cs::kGialloLo, 12))
                 .stroke(util::stroke(0.8f, Fill::color(cs::kMortar))));
    // the eye
    el.child(box().inset(r * 0.72f).corners({r * 0.28f})
                 .fill(cs::stone(eyeHi, eyeLo, 60))
                 .foreground(util::stroke(1.6f, Fill::color(cs::kMarble))));
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
    Element band = stack().width(Dim(w)).height(Dim(h))
                       .centerAt({x, y}).rotate(degrees)
                       .cache(Cache::Texture)
                       .opacity(withFrom(0.0f, 1.0f,
                                         {360ms, &ch::easeOutQuad, delay}));
    band.child(box().inset(0)
                   .fill(cs::stone(cs::kPurbeck, cs::kPurbeckLo, 8))
                   .foreground(util::stroke(1.4f, Fill::color(cs::kMarble),
                                            PathFormat::Align::Inner)));
    band.child(box().inset(0)
                   .outline(cs::guillocheStrand((float)periods, 0.0f,
                                                h * 0.26f))
                   .trim(0.0f, &lay)
                   .stroke(util::stroke(h * 0.20f,
                                        Fill::color(cs::kGiallo))));
    band.child(box().inset(0)
                   .outline(cs::guillocheStrand((float)periods, 3.14159265f,
                                                h * 0.26f))
                   .trim(0.0f, &lay)
                   .stroke(util::stroke(h * 0.20f,
                                        Fill::color(cs::kSerpentine))));
    // the discs the strands plait around
    for (int i = 0; i < periods; ++i) {
      const float cx = w * ((float)i + 0.5f) / (float)periods;
      band.child(box().left(cx - h * 0.20f).top(h * 0.30f)
                     .width(Dim(h * 0.40f)).height(Dim(h * 0.40f))
                     .corners({h * 0.20f})
                     .fill(cs::stone(cs::kPorphyry, cs::kPorphyryLo, 30))
                     .foreground(util::stroke(1.0f,
                                              Fill::color(cs::kMarble))));
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
    // gradients. Static once it has entered, so bake it: on GPU the
    // difference is noise, on the CPU raster backend it is the difference
    // between 12 fps and 100.
    Element q = stack().width(Dim(side)).height(Dim(side))
                    .left(x).top(y)
                    .cache(Cache::Texture)
                    .opacity(withFrom(0.0f, 1.0f,
                                      {420ms, &ch::easeOutQuad, delay}));
    q.child(box().inset(0).fill(Material::solid(cs::kMortar)));
    const int cols = 11, rows = 11;
    q.child(box().inset(0)
                .outline(cs::triangleCourse(cols, rows, 0))
                .fill(cs::stone(cs::kPorphyry, cs::kPorphyryLo, 18)));
    q.child(box().inset(0)
                .outline(cs::triangleCourse(cols, rows, 1))
                .fill(cs::stone(cs::kMarble, cs::kMarbleLo, 52)));
    q.child(box().inset(0)
                .outline(cs::triangleCourse(cols, rows, 2))
                .fill(cs::stone(cs::kSerpentine, cs::kSerpentineLo, 34)));
    q.child(box().inset(0)
                .foreground(util::stroke(2.0f, Fill::color(cs::kMarble),
                                         PathFormat::Align::Inner)));
    return q;
  }

  Element describe() {
    namespace cs = cosmati;
    namespace ch = choreograph;
    using namespace std::chrono_literals;

    auto root = stack().fill(Material::linear(
        {0, 0}, {0, cs::kH},
        {{0.0f, cosmati::C(0x14120F)}, {1.0f, cosmati::C(0x080706)}}));

    // ---- the pavement ------------------------------------------------
    Element floorPlate =
        stack().key("floor")
            .left(cs::kFieldX).top(cs::kFieldY)
            .width(Dim(cs::kFieldSide)).height(Dim(cs::kFieldSide));

    // the Purbeck frame carrying the inscription band
    floorPlate.child(box().inset(0)
                         // one octave: this fill covers the whole plate
                         // even though only its border shows, and on the
                         // raster backend every pixel of it is an SkSL
                         // evaluation
                         .fill(cs::stone(cs::kPurbeck, cs::kPurbeckLo, 6, 1))
                         .foreground(util::stroke(
                             2.0f, Fill::color(cs::kMarble),
                             PathFormat::Align::Inner))
                         .background(styles::dropShadow({0, 0, 0, 0.7f},
                                                        {0, 8}, 18)));
    floorPlate.child(text(toU8("\xc2\xb7 QVATVOR \xc2\xb7 PRAECEDENTES "
                               "\xc2\xb7 ET \xc2\xb7 TRES \xc2\xb7"),
                          cs::type(11, cs::kInkDim, 3.4f))
                         .left(cs::kBandW).top(13));
    floorPlate.child(text(toU8("\xc2\xb7 ODORICVS \xc2\xb7 FECIT \xc2\xb7 "
                               "MCCLXVIII \xc2\xb7"),
                          cs::type(11, cs::kInkDim, 3.4f))
                         .left(cs::kBandW)
                         .top(cs::kFieldSide - 24));

    // the mortar bed inside the frame
    floorPlate.child(box().inset(cs::kBandW)
                         .fill(Material::solid(cs::kMortar)));

    // ---- the quincunx of quincunxes ---------------------------------
    const float c = cs::kFieldSide * 0.5f;
    const float big = cs::kInner * 0.185f;   // the central roundel
    const float small = cs::kInner * 0.125f; // the four around it
    const float arm = cs::kInner * 0.295f;   // centre-to-corner-roundel

    // the triangular course is the BED: a pavement has no bare mortar,
    // the quincunx is set into a field that is already tessellated
    quarterInto(floorPlate, cs::kBandW, cs::kBandW, cs::kInner, 0);

    // the guilloche arms, one to each corner roundel
    const float bandH = cs::kInner * 0.115f;
    const float armLen = arm * 1.414f - big - small + 8;
    for (int i = 0; i < 4; ++i) {
      const float a = 0.7853982f + 1.5707963f * (float)i; // the diagonals
      const float mx = c + std::cos(a) * (arm * 0.7071f + 4);
      const float my = c + std::sin(a) * (arm * 0.7071f + 4);
      floorPlate.child(guilloche(mx, my, armLen, bandH,
                                 a * 180.0f / 3.14159265f, i));
    }

    // the roundels: four around one
    for (int i = 0; i < 4; ++i) {
      const float a = 0.7853982f + 1.5707963f * (float)i;
      floorPlate.child(roundel({c + std::cos(a) * arm * 1.414f,
                                c + std::sin(a) * arm * 1.414f},
                               small, cs::kGlassTurq, cs::kGlassCobalt,
                               i + 1));
    }
    floorPlate.child(roundel({c, c}, big, cs::kOnyx, cs::kGialloLo, 0));

    // The circular inscription these pavements carry round their centre
    // roundel. Real Cosmati work sets it in the ring itself; this is one
    // shaped run on a circular baseline — onPath, not 40 hand-placed
    // glyphs — and it does NOT auto-flip, because the letter-cutters
    // didn't: glyph-up points outward the whole way round.
    floorPlate.child(text(toU8("\xc2\xb7 SPHERICVM \xc2\xb7 ARCHETYPVM "
                              "\xc2\xb7 MVNDVM \xc2\xb7 PRIMVM \xc2\xb7 "
                              "TRIPLEX \xc2\xb7"),
                          [] {
                            namespace cs = cosmati;
                            auto t = cs::type(9, cs::kGiallo, 2.0f, 600);
                            // a cut letter on a busy floor needs its own
                            // shadow to read at all
                            sigil::weave::PaintLayer cut;
                            cut.paint.setAntiAlias(true);
                            cut.paint.setStyle(SkPaint::kStroke_Style);
                            cut.paint.setStrokeWidth(2.6f);
                            cut.paint.setStrokeJoin(SkPaint::kRound_Join);
                            cut.paint.setColor4f({0, 0, 0, 0.82f}, nullptr);
                            (void)cut;
                            return t;
                          }())
                         .width(Dim(big * 1.50f)).height(Dim(big * 1.50f))
                         .centerAt({c, c})
                         .onPath({.path = shapes::arc(-90.0f, 359.9f),
                                  .at = 0.0f,
                                  .align = TextPath::Align::Start,
                                  .offset = 0.0f})
                         .zIndex(6));

    // the raking light: a soft band crossing the polished floor
    floorPlate.child(box().left(-260).top(-40)
                         .width(Dim(210.0f))
                         .height(Dim(cs::kFieldSide + 80))
                         .rotate(14.0f)
                         .translateX(bind(&rake).to(-260, cosmati::kW + 260))
                         .fill(Material::linear(
                             {0, 0}, {210, 0},
                             {{0.0f, {1, 0.96f, 0.88f, 0.0f}},
                              {0.5f, {1, 0.96f, 0.88f, 0.13f}},
                              {1.0f, {1, 0.96f, 0.88f, 0.0f}}}))
                         .blend(SkBlendMode::kPlus)
                         .zIndex(9));
    root.child(std::move(floorPlate));

    // ---- the apparatus beside the floor ------------------------------
    const float px = cs::kFieldX + cs::kFieldSide + 34;
    root.child(box().column().left(px).top(cs::kFieldY + 4)
                   .child(text(toU8("OPUS SECTILE"),
                               cs::type(21, cs::kInk, 3.4f, 640)))
                   .child(text(toU8("Cosmatesque \xc2\xb7 Westminster "
                                    "1268"),
                               cs::type(11, cs::kInkDim, 1.4f))
                              .margin(0, 6, 0, 0))
                   .child(box().width(Dim(190.0f)).height(Dim(1.0f))
                              .margin(0, 12, 0, 12)
                              .fill(Material::linear(
                                  {0, 0}, {190, 0},
                                  {{0.0f, {cs::kGiallo.fR, cs::kGiallo.fG,
                                           cs::kGiallo.fB, 0.7f}},
                                   {1.0f, {cs::kGiallo.fR, cs::kGiallo.fG,
                                           cs::kGiallo.fB, 0.0f}}})))
                   .child(text(toU8("The governing figure is the QUINCUNX "
                                    "\xe2\x80\x94 four roundels about a "
                                    "fifth. The Great Pavement is a "
                                    "quincunx of quincunxes, 25 Roman feet "
                                    "square, laid by a Roman crew under "
                                    "Odoricus."),
                               cs::type(11.5f, cs::kInkDim, 0.2f))
                              .width(Dim(210.0f))));

    // the quarry legend: every stone named, with a real sample of it
    struct Quarry { const char *label; SkColor4f hi, lo; };
    static const Quarry kQuarries[] = {
        {"porphyry \xc2\xb7 Mons Claudianus", cs::kPorphyry,
         cs::kPorphyryLo},
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
    Element legend = box().key("quarries").column().gap(6)
                         .left(px).bottom(46)
                         .staggerChildren(60ms);
    for (const Quarry &q : kQuarries)
      legend.child(box().row().alignItems(Align::Center).gap(9)
                       .opacity(withFrom(0.0f, 1.0f, {320ms}))
                       .translateX(withFrom(-14.0f, 0.0f, {400ms}))
                       .child(box().width(Dim(20.0f)).height(Dim(13.0f))
                                  .fill(cs::stone(q.hi, q.lo, 34))
                                  .foreground(util::stroke(
                                      1.0f,
                                      Fill::color({cs::kMarble.fR,
                                                   cs::kMarble.fG,
                                                   cs::kMarble.fB, 0.55f}))))
                       .child(text(toU8(q.label),
                                   cs::type(10.5f, cs::kInkDim, 0.7f))));
    root.child(std::move(legend));
    return root;
  }

private:
  void quarterInto(Element &parent, float x, float y, float side, int seed) {
    parent.child(quarter(x, y, side, seed));
  }
};

} // namespace compose_gallery
