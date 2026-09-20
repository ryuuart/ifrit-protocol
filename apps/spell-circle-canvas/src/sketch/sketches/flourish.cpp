/** @file
 * flourish — a living gilt border round a parchment cartouche,
 * deliberately maximal: it reaches across nearly the whole drawing
 * surface in one scene.
 */

// "Aurelia" — a living flourish border: a gilt-on-oxblood ornamental frame
// around a central parchment cartouche. It is deliberately maximal, reaching
// across nearly the whole SigilCompose surface in one scene:
//   PathFormat rule weights + bead stamps, a ContourWalk acanthus vine
//   (element stamp, recursion L2) + an animatedWalk glow, a Slice
//   nine-slice, onEdges crests, AlongPath/Radial/Scatter layouts, arc +
//   orthogonal connectors, flowAround, Blur/backdrop effects + SkSL fills,
//   image().region(), a timeline draw-on entrance, with() transitions, and
//   the Texture/Picture/None cache partition kept honest (every bound node
//   is a sibling of the bake, never inside it).
//
// The live layers are the ones that carry the piece: spinning medallions,
// the draw-on scrollwork, and the shimmer. Everything else is static, and
// every bound node is a sibling of a bake rather than inside one.

// TAGS: Drawing/Generative, Patterns/Ornament

#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Flourish.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/kit/Routers.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;
namespace path = sigil::geometry::path;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::draw::Pen;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;
using namespace sigil::compose::kit::flourish;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace ch = choreograph;

struct Flourish {
  static constexpr float kW = 900.0f;  // kSceneSize.width()
  static constexpr float kH = 640.0f;  // kSceneSize.height()
  static constexpr float kFrameInset = 34.0f;
  static constexpr float kMedD = 92.0f;

  static constexpr int kRosettes = 44;
  static constexpr int kPetals = 12;
  static constexpr int kSparks = 30;
  static constexpr int kMotes = 90;
  static constexpr int kFriezeTiles = 16;

  FlourishStyle st;

  ch::Output<float> reveal{0.0f};
  ch::Output<float> titleDrop{-18.0f};
  ch::Output<float> titleFade{0.0f};
  ch::Output<float> sealBreathe{1.0f};
  ch::Output<float> spin[4];
  ch::Output<float> breathe[4];
  ch::Output<float> flare{0.0f};

  sk_sp<SkRuntimeEffect> hatch, engraved;
  std::shared_ptr<sigil::image::ImageAsset> carvedFrame, gemAtlas;
  bool accent = false;
  double nextAccent = 4.0;

  // ---- helpers ------------------------------------------------------------

  static sk_sp<SkRuntimeEffect> makeHatch() {
    return SkRuntimeEffect::MakeForShader(SkString(R"(
        half4 main(float2 p) {
          float d = mod(p.x * 0.7 + p.y, 7.0);
          float line = smoothstep(2.4, 3.1, d) - smoothstep(3.1, 3.8, d);
          return half4(0.0, 0.0, 0.0, 0.20 * line);
        })"))
        .effect;
  }
  static sk_sp<SkRuntimeEffect> makeEngraved() {
    return SkRuntimeEffect::MakeForShader(SkString(R"(
        half4 main(float2 p) {
          float d = mod(p.x * 0.6 - p.y * 0.4, 6.0);
          float line = smoothstep(2.2, 3.0, d) - smoothstep(3.0, 3.8, d);
          float r = length(p - float2(46.0, 46.0)) / 64.0;
          half3 base = mix(half3(0.46, 0.31, 0.15), half3(0.24, 0.15, 0.10),
                           clamp(r, 0.0, 1.0));
          half3 gild = half3(0.92, 0.74, 0.38);
          return half4(mix(base, gild, line * 0.6), 1.0);
        })"))
        .effect;
  }

  std::shared_ptr<sigil::image::ImageAsset> makeGemAtlas() const {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
    SkCanvas& c = *s->getCanvas();
    c.clear(SK_ColorTRANSPARENT);
    const SkColor4f gems[4] = {
        st.rubric, {0.20f, 0.36f, 0.52f, 1}, st.leaf, st.goldBright};
    for (int i = 0; i < 4; ++i) {
      const float ox = (float)i * 16;
      drawDiamond(c, {ox + 8, 8}, 6.0f, gems[i]);
      SkColor4f hi = gems[i];
      hi.fR = std::min(1.f, hi.fR + 0.3f);
      hi.fG = std::min(1.f, hi.fG + 0.3f);
      hi.fB = std::min(1.f, hi.fB + 0.3f);
      drawDiamond(c, {ox + 7, 6}, 2.2f, hi);
    }
    return std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(s->makeImageSnapshot()));
  }

  // ---- the static baked frame band ---------------------------------------

  Element frameBand() const {
    ContourWalk crestWalk;
    crestWalk.spacing = 70.0f;
    crestWalk.stamp = box()
                          .width(16)
                          .height(11)
                          .shape(shapes::star(3, 0.5f))
                          .fill(Fill::color(st.gold));

    std::vector<Element> studs;
    studs.reserve(kRosettes);
    for (int i = 0; i < kRosettes; ++i)
      studs.push_back(box()
                          .width(11)
                          .height(11)
                          .shape(shapes::star(6, 0.5f))
                          .fill(Fill::color(st.gold)));

    auto innerRect = [](SkSize s) {
      return SkPath::RRect(SkRRect::MakeRectXY(
          SkRect::MakeLTRB(22, 22, s.width() - 22, s.height() - 22), 16, 16));
    };

    return box()
        .inset(kFrameInset)
        .borderRadius({22})
        .background(sigil::compose::shadow({0, 0, 0, 0.55f}, {0, 5}, 16))
        .foreground(sigil::compose::stroke(2.6f, Fill::color(st.gold)))
        .foreground(flourishVine(st, 17.0f, 24.0f, 17.0f))
        .foreground(onEdges(path::Edge::Top | path::Edge::Bottom,
                            Decoration(crestWalk)))
        .cache(Cache::Texture)
        .children({box()
                       .inset(13)
                       .borderRadius({15})
                       .foreground(beadChain(st.goldBright, 13.0f, 2.3f))
                       .foreground(giltDash(st.gold, 1.2f)),
                   box().inset(22).foreground(
                       sigil::compose::stroke(0.8f, Fill::color(st.bronze))),
                   layout(layouts::AlongPath{innerRect})
                       .inset(0)
                       .children(std::move(studs))});
  }

  Element frameGlow() const {
    ContourWalk glow;
    glow.spacing = 26.0f;
    glow.animatedWalk = true;
    const SkColor4f g = st.goldBright;
    glow.draw = [g](SkCanvas& c, const PathSample& s, const PaintContext& ctx) {
      SkPaint p;
      p.setAntiAlias(true);
      const float w = 0.5f + 0.5f * std::sin(s.fraction * 18.85f +
                                             (float)ctx.elapsedSeconds * 2.4f);
      p.setColor4f({g.fR, g.fG, g.fB, 0.14f + 0.45f * w}, nullptr);
      c.drawCircle(0, 0, 1.0f + 1.5f * w, p);
    };
    return box()
        .inset(kFrameInset)
        .borderRadius({22})
        .foreground(glow)
        .blend(SkBlendMode::kPlus)
        .cache(Cache::None);
  }

  // ---- corner medallions --------------------------------------------------

  struct MedProps {
    int q;
    bool accent;
    bool operator==(const MedProps&) const = default;
  };

  Element medallion(MedProps mp) const {
    const int q = mp.q;
    const float cx = (q == 1 || q == 2) ? kW - kFrameInset : kFrameInset;
    const float cy = (q >= 2) ? kH - kFrameInset : kFrameInset;

    std::vector<Element> petals;
    petals.reserve(kPetals);
    for (int i = 0; i < kPetals; ++i)
      petals.push_back(box()
                           .width(13)
                           .height(18)
                           .shape(shapes::rounded(shapes::star(4, 0.36f), 2))
                           .fill(Fill::color(st.goldBright))
                           .foreground(sigil::compose::stroke(
                               0.7f, Fill::color(st.bronze))));

    Fill disc =
        engraved ? Fill::shader(SkRuntimeShaderBuilder(engraved).makeShader())
                 : Fill::color({0.14f, 0.07f, 0.05f, 1});

    return box()
        .key("med" + std::to_string(q))
        .inset(cx - kMedD / 2, cy - kMedD / 2, kW - (cx + kMedD / 2),
               kH - (cy + kMedD / 2))
        .width(kMedD)
        .height(kMedD)
        .transformOrigin(pct(50), pct(50))
        .rotate(&spin[q])
        .scale(&breathe[q])
        .cache(Cache::Picture)
        .children(
            {box()
                 .inset(15)
                 .shape(shapes::squircle(4.0f))
                 .fill(disc)
                 .foreground(sigil::compose::stroke(2.2f, Fill::color(st.gold)))
                 .foreground(
                     sigil::compose::stroke(0.7f, Fill::color(st.goldBright))),
             layout(layouts::Radial{0.82f})
                 .inset(0)
                 .children(std::move(petals)),
             box()
                 .inset(kMedD / 2 - 9, kMedD / 2 - 9, kMedD / 2 - 9,
                        kMedD / 2 - 9)
                 .shape(shapes::star(8, 0.5f))
                 .fill(Fill::color(mp.accent ? st.goldBright : st.gold))
                 .opacity(&flare)});
  }

  Element filaments() const {
    PathFormat gild;
    gild.width = 1.1f;
    gild.strokeFill = Fill::color({st.gold.fR, st.gold.fG, st.gold.fB, 0.7f});

    SkPathBuilder dot;
    dot.addCircle(0, 0, 1.3f);
    PathFormat beaded;
    beaded.width = 1.0f;
    beaded.strokeFill = Fill::color(st.goldBright);
    beaded.stampPath = dot.detach();
    beaded.stampAdvance = 11.0f;

    auto arc = [&](const char* a, const char* b) {
      return connector(a, b, routers::arc(0.05f)).inset(0).foreground(gild);
    };
    return stack().inset(0).zIndex(2).children(
        {arc("med0", "med1"), arc("med1", "med2"), arc("med2", "med3"),
         arc("med3", "med0"),
         connector("med0", "med2", routers::orthogonal(18.0f))
             .inset(0)
             .foreground(beaded),
         connector("med1", "med3", routers::orthogonal(18.0f))
             .inset(0)
             .foreground(beaded)});
  }

  // ---- the central cartouche (the box being framed) ----------------------

  Element cartouche() const {
    Slice carved = carvedFrameSlice(carvedFrame);

    Decoration hatchDeco{PaintProgram{}};
    if (hatch) {
      auto fx = hatch;
      hatchDeco =
          Decoration(PaintProgram([fx](SkCanvas& c, const PaintContext& ctx) {
            SkPaint p;
            p.setShader(SkRuntimeShaderBuilder(fx).makeShader());
            p.setAlphaf(0.6f);
            c.save();
            c.clipPath(ctx.outline, true);
            c.drawRect(SkRect::MakeSize(ctx.size), p);
            c.restore();
          }));
    }

    std::vector<Element> sparks;
    sparks.reserve(kSparks);
    for (int i = 0; i < kSparks; ++i)
      sparks.push_back(box()
                           .width(3)
                           .height(3)
                           .shape(shapes::star(4, 0.4f))
                           .fill(Fill::color({st.bronze.fR, st.bronze.fG,
                                              st.bronze.fB, 0.5f}))
                           .opacity(0.5f));

    std::vector<Element> frieze;
    frieze.reserve(kFriezeTiles);
    for (int i = 0; i < kFriezeTiles; ++i)
      frieze.push_back(
          image(gemAtlas)
              .region(SkRect::MakeXYWH((float)(i % 4) * 16, 0, 16, 16))
              .width(16)
              .height(16));

    auto titleLayer = [this](SkColor4f color, bool bloom) {
      auto t = text(u8"AURELIA")
                   .font({.size = 34, .track = 5.0f})
                   .ink(color)
                   .key(bloom ? "titleBloom" : "title")
                   .opacity(&titleFade);
      if (bloom)
        t.filter(sigil::material::skia::Effect::filter(
                     SkImageFilters::Blur(6, 6, nullptr)))
            .blend(SkBlendMode::kPlus);
      else
        t.translateY(&titleDrop);
      return kit::centred()
          .inset(0)
          .column()

          .children({std::move(t)});
    };

    // The cartouche is set in the style's ink; the title and the closing
    // line name their own colours over it.
    return box()
        .key("cartouche")
        .ink(st.ink)
        .inset(224, 188, 224, 188)  // ~452×264 centered box
        .borderRadius({16})
        .zIndex(3)
        .clip()
        .backdropFilter(sigil::material::skia::Effect::filter(
            SkImageFilters::Blur(8, 8, nullptr)))
        .background(sigil::compose::shadow({0, 0, 0, 0.5f}, {0, 6}, 16))
        .fill(flourishParchment(st))
        .background(hatchDeco)
        .background(carved)
        .column()
        .padding(30, 26)
        .gap(9)
        .alignItems(Align::Center)
        .children(
            {layout(layouts::Jittered{7, 0.7f})
                 .inset(22)
                 .children(std::move(sparks)),
             stack()
                 .width(258)
                 .height(66)
                 .shape(scallopOutline(12))
                 .fill(Fill::color({st.parchment.fR * 1.05f,
                                    st.parchment.fG * 1.05f,
                                    st.parchment.fB * 1.02f, 1}))
                 .foreground(sigil::compose::stroke(1.3f, Fill::color(st.gold)))
                 .children({titleLayer(st.goldBright, true),
                            titleLayer({0.34f, 0.20f, 0.09f, 1}, false)}),
             box()
                 .key("seal")
                 .width(42)
                 .height(42)
                 .transformOrigin(pct(50), pct(50))
                 .scale(&sealBreathe)
                 .shape(shapes::star(12, 0.66f))
                 .fill(animate(
                     motion::to(Fill::color(accent ? st.rubric : st.bronze)),
                     {600ms}))
                 .foreground(
                     sigil::compose::stroke(1.4f, Fill::color(st.goldBright))),
             text(u8"Framed by a vine that draws itself on, corner by "
                  u8"corner, while the medallions turn and the rules hold "
                  u8"their three weights of gold — every ornament a "
                  u8"different corner of the compose surface, woven around "
                  u8"this seal.")
                 .font({.size = 12.5f})
                 .key("motto")
                 .flowAround("seal", 7),
             box()
                 .row()
                 .gap(2)
                 .justifyContent(Justify::Center)
                 .children(std::move(frieze)),
             text(u8"— a stress test that chose to be beautiful —")
                 .font({.size = 11})
                 .ink(st.rubric)});
  }

  // ---- draw-on scrollwork sweeps (Cache::None, read reveal live) ----------

  Element scrollworkCorner(int q) const {
    // KEYLESS: the sweep reads the reveal live, at Cache::None.
    return sigil::compose::pen([this, q](Pen& p) {
             const float rev = reveal.value();
             const float local =
                 std::clamp((rev - (float)q * 0.16f) / 0.55f, 0.0f, 1.0f);
             const bool right = (q == 1 || q == 2);
             const bool bottom = (q >= 2);
             // THE FOUR CORNERS ARE ONE DRAWING, MIRRORED: the sweep is
             // written once for the top left and the other three are it
             // turned over on one axis or both.
             p.push();
             if (right) {
               p.translate(p.width, 0);
               p.scale(-1, 1);
             }
             if (bottom) {
               p.translate(0, p.height);
               p.scale(1, -1);
             }

             const float armLen = 195.0f;
             const float x0 = kFrameInset + 10, y0 = kFrameInset + 10;
             auto sweep = [&](bool along) {
               std::vector<SkPoint> pts;
               appendCubic(pts, {x0, y0}, {x0 + armLen * 0.16f, y0 - 22},
                           {x0 + armLen * 0.44f, y0 - 8},
                           {x0 + armLen * 0.62f, y0 + 6});
               const SkPoint eye{x0 + armLen * 0.72f, y0 + 16};
               appendSpiral(pts, eye, 12.0f, 1.2f, -1.7f, -1.7f + 7.6f, 32);
               SkMatrix m;
               if (along)
                 m.setRotate(90, x0, y0);
               else
                 m.setIdentity();
               for (auto& p : pts) p = m.mapPoint(p);
               p.noStroke();
               p.fill(st.gold);
               p.shape(taperedStroke(revealed(pts, local), 5.0f));
               std::vector<SkPoint> under;
               appendCubic(under, {x0 + 4, y0 + 20},
                           {x0 + armLen * 0.16f, y0 + 30},
                           {x0 + armLen * 0.30f, y0 + 26},
                           {x0 + armLen * 0.40f, y0 + 22});
               appendSpiral(under, {x0 + armLen * 0.47f, y0 + 18}, 8.0f, 1.0f,
                            2.4f, 2.4f - 6.6f, 28);
               for (auto& p : under) p = m.mapPoint(p);
               p.shape(taperedStroke(revealed(under, local), 2.6f));
               if (local > 0.85f) {
                 const SkPoint e = m.mapPoint(eye);
                 p.fill(st.goldBright);
                 p.circle(e.x(), e.y(), 5.2f);
               }
             };
             sweep(false);
             sweep(true);
             if (local > 0.98f)
               drawDiamond(*p.canvas(), {x0, y0}, 4.5f, st.goldBright);
             p.pop();
           })
        .inset(0)
        .zIndex(4)
        .cache(Cache::None);
  }

  // Truncate a point list to the leading `fraction` for the draw-on reveal.
  static std::vector<SkPoint> revealed(const std::vector<SkPoint>& pts,
                                       float fraction) {
    const size_t keep = std::max<size_t>(
        2, (size_t)std::lround((float)pts.size() *
                               std::clamp(fraction, 0.f, 1.f)));
    if (keep >= pts.size()) return pts;
    return {pts.begin(), pts.begin() + (std::ptrdiff_t)keep};
  }

  // ---- live overlays ------------------------------------------------------

  Element goldDust() const {
    const SkColor4f g = st.goldBright;
    // KEYLESS: every mote's place and alpha is a function of the paint's own
    // clock, which no key can name.
    return sigil::compose::pen([g](Pen& p) {
             const float t = (float)p.millis() * 0.001f;
             p.noStroke();
             for (int i = 0; i < kMotes; ++i) {
               const float fx = (float)i * 137.5f;
               const float x =
                   std::fmod(fx + t * (7.0f + (float)(i % 5)), p.width);
               const float y = std::fmod(fx * 0.618f + 40.0f, p.height);
               p.fill(
                   {g.fR, g.fG, g.fB,
                    0.05f + 0.15f * (0.5f + 0.5f * std::sin(t * 1.6f +
                                                            (float)i * 2.1f))});
               p.circle(x, y + 10.0f * std::sin(t * 0.7f + (float)i),
                        2.2f + (float)(i % 3));
             }
           })
        .inset(0)
        .zIndex(6)
        .cache(Cache::None)
        .blend(SkBlendMode::kPlus);
  }

  Element shimmer() const {
    const SkColor4f g = st.goldBright;
    // KEYLESS: the sweep's position is the paint's own clock.
    return sigil::compose::pen([g](Pen& p) {
             const float w = p.width, h = p.height;
             const float t = (float)p.millis() * 0.001f;
             const float sweep = std::fmod(t * 180.0f, w + h + 300.0f) - 150.0f;
             // A BAND OF LIGHT CROSSING THE PLATE: a gradient the pen
             // takes as its fill, in the pen's own space, so the sweep is
             // one rect and no shader is spelled by hand.
             p.noStroke();
             p.fill(Paint::linear({sweep, 0}, {sweep + 130, h},
                                  {{0.0f, {1, 1, 1, 0}},
                                   {0.5f, {g.fR, g.fG, g.fB, 0.22f}},
                                   {1.0f, {1, 1, 1, 0}}}));
             p.rect(0, 0, w, h);
           })
        .inset(kFrameInset)
        .zIndex(5)
        .cache(Cache::None)
        .blend(SkBlendMode::kPlus);
  }

  // ---- assembly -----------------------------------------------------------

  Element describe() const {
    return stack()
        .fill(sigil::compose::radialGradient({kW / 2, kH / 2}, 620,
                                             {st.velvetCore, st.velvetEdge}))
        .children({frameBand(), frameGlow(), filaments(),
                   memo(MedProps{0, accent},
                        [this](const MedProps& p) { return medallion(p); })
                       .key("med0"),
                   memo(MedProps{1, accent},
                        [this](const MedProps& p) { return medallion(p); })
                       .key("med1"),
                   memo(MedProps{2, accent},
                        [this](const MedProps& p) { return medallion(p); })
                       .key("med2"),
                   memo(MedProps{3, accent},
                        [this](const MedProps& p) { return medallion(p); })
                       .key("med3"),
                   cartouche(), scrollworkCorner(0), scrollworkCorner(1),
                   scrollworkCorner(2), scrollworkCorner(3), shimmer(),
                   goldDust()});
  }

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    sceneTicker = &ticker;
    hatch = makeHatch();
    engraved = makeEngraved();
    carvedFrame = std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(
            makeCarvedFrame(toOrnamentPalette(st), 192)));
    gemAtlas = makeGemAtlas();

    reveal = 0.0f;
    titleDrop = -18.0f;
    titleFade = 0.0f;
    flare = 0.0f;
    accent = false;
    nextAccent = 4.0;
    for (int q = 0; q < 4; ++q) {
      spin[q] = 0.0f;
      breathe[q] = 1.0f;
    }

    ticker.timeline().apply(&reveal).then<ch::RampTo>(1.0f, 2.4f,
                                                      &ch::easeOutQuint);
    ticker.timeline()
        .apply(&titleDrop)
        .then<ch::RampTo>(0.0f, 1.0f, &ch::easeOutQuint);
    ticker.timeline().apply(&titleFade).then<ch::RampTo>(1.0f, 1.2f);
    ticker.timeline().apply(&flare).then<ch::RampTo>(1.0f, 1.3f);

    ticker.add([this, &ticker] {
      const double t = ticker.elapsed();
      for (int q = 0; q < 4; ++q) {
        const float dir = (q == 0 || q == 2) ? 1.0f : -1.0f;
        spin[q] = (float)(t * 7.0) * dir;
        breathe[q] = 1.0f + 0.05f * (float)std::sin(t * 1.3 + q * 1.5707963);
      }
      sealBreathe = 1.0f + 0.06f * (float)std::sin(t * 1.1);
    });

    composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    Composer& composer = ctx.composer;
    if (elapsed < nextAccent) return;
    nextAccent = elapsed + 4.0;
    accent = !accent;
    // The rubric flare: each accent beat re-lights the medallion bosses,
    // then settles back over 1.1 s.
    if (sceneTicker) {
      flare = 1.0f;
      sceneTicker->timeline().apply(&flare).then<ch::RampTo>(0.55f, 1.1f,
                                                             &ch::easeOutQuint);
    }
    composer.render(describe());
  }

  sigil::motion::Ticker* sceneTicker = nullptr;
};

}  // namespace

SIGIL_SKETCH_AS(Flourish, "flourish", "Catalog · Generative",
                "the integration piece — one ornamental border reaching "
                "across the whole compose surface")
