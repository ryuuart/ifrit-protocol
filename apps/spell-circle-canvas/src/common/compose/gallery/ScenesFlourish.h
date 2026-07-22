#pragma once
// "Aurelia" — the living flourish border, extracted from the ComposeSketch
// stress sketch into a gallery scene. A gilt-on-oxblood ornamental frame
// around a central parchment cartouche that fans across nearly the whole
// SigilCompose surface at once:
//   PathFormat rule weights + bead stamps, a ContourWalk acanthus vine
//   (element stamp, recursion L2) + an animatedWalk glow, a Slice
//   nine-slice, onEdges crests, AlongPath/Radial/Scatter layouts, arc +
//   orthogonal connectors, flowAround, Blur/backdrop effects + SkSL fills,
//   image().region(), a timeline draw-on entrance, with() transitions, and
//   the Texture/Picture/None cache partition kept honest (every bound node
//   is a sibling of the bake, never inside it).
//
// The static border lives in FlourishKit (shared with the particle posts);
// this file adds the live layers — spinning medallions, the draw-on
// scrollwork, and the shimmer.

#include "FlourishKit.h"
#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <sigilcompose/Layouts.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace compose_gallery {

namespace ch = choreograph;

struct FlourishScene final : Scene {
  static constexpr float kW = 900.0f; // kSceneSize.width()
  static constexpr float kH = 640.0f; // kSceneSize.height()
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

  const char *name() const override { return "flourish"; }

  // ---- helpers ------------------------------------------------------------

  static sigil::weave::TextStyle glyphs(float size, SkColor4f color,
                                        float tracking = 0.0f) {
    sigil::weave::TextStyle s;
    s.shaping.fontSize = size;
    s.shaping.letterSpacing = tracking;
    s.paint.foreground.setColor(toSk(color));
    s.paint.foreground.setAntiAlias(true);
    return s;
  }

  static sk_sp<SkRuntimeEffect> makeHatch() {
    return SkRuntimeEffect::MakeForShader(SkString(R"(
        half4 main(float2 p) {
          float d = mod(p.x * 0.7 + p.y, 7.0);
          float line = smoothstep(2.4, 3.1, d) - smoothstep(3.1, 3.8, d);
          return half4(0.0, 0.0, 0.0, 0.20 * line);
        })")).effect;
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
        })")).effect;
  }

  std::shared_ptr<sigil::image::ImageAsset> makeGemAtlas() const {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
    SkCanvas &c = *s->getCanvas();
    c.clear(SK_ColorTRANSPARENT);
    const SkColor4f gems[4] = {st.rubric,
                               {0.20f, 0.36f, 0.52f, 1},
                               st.leaf,
                               st.goldBright};
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
    crestWalk.stamp = box().width(16).height(11)
                          .outline(shapes::star(3, 0.5f))
                          .fill(Fill::color(st.gold));

    std::vector<Element> studs;
    studs.reserve(kRosettes);
    for (int i = 0; i < kRosettes; ++i)
      studs.push_back(box().width(11).height(11)
                          .outline(shapes::star(6, 0.5f))
                          .fill(Fill::color(st.gold)));

    auto innerRect = [](SkSize s) {
      return SkPath::RRect(SkRRect::MakeRectXY(
          SkRect::MakeLTRB(22, 22, s.width() - 22, s.height() - 22), 16, 16));
    };

    return box()
        .inset(kFrameInset)
        .corners({22})
        .background(sigil::compose::util::shadow({0, 0, 0, 0.55f}, {0, 5}, 16))
        .foreground(sigil::compose::util::stroke(2.6f, Fill::color(st.gold)))
        .foreground(flourishVine(st, 17.0f, 24.0f, 17.0f))
        .foreground(shapes::onEdges(shapes::Edge::Top | shapes::Edge::Bottom,
                                    Decoration(crestWalk)))
        .cache(Cache::Texture)
        .child(box().inset(13).corners({15})
                   .foreground(beadChain(st.goldBright, 13.0f, 2.3f))
                   .foreground(giltDash(st.gold, 1.2f)))
        .child(box().inset(22)
                   .foreground(sigil::compose::util::stroke(
                       0.8f, Fill::color(st.bronze))))
        .child(layout(layouts::AlongPath{innerRect})
                   .inset(0)
                   .children(std::move(studs)));
  }

  Element frameGlow() const {
    ContourWalk glow;
    glow.spacing = 26.0f;
    glow.animatedWalk = true;
    const SkColor4f g = st.goldBright;
    glow.draw = [g](SkCanvas &c, const PathSample &s, const PaintContext &ctx) {
      SkPaint p;
      p.setAntiAlias(true);
      const float w = 0.5f + 0.5f * std::sin(s.fraction * 18.85f +
                                             (float)ctx.elapsedSeconds * 2.4f);
      p.setColor4f({g.fR, g.fG, g.fB, 0.14f + 0.45f * w}, nullptr);
      c.drawCircle(0, 0, 1.0f + 1.5f * w, p);
    };
    return box().inset(kFrameInset).corners({22})
        .foreground(glow).blend(SkBlendMode::kPlus).cache(Cache::None);
  }

  // ---- corner medallions --------------------------------------------------

  struct MedProps {
    int q;
    bool accent;
    bool operator==(const MedProps &) const = default;
  };

  Element medallion(MedProps mp) const {
    const int q = mp.q;
    const float cx = (q == 1 || q == 2) ? kW - kFrameInset : kFrameInset;
    const float cy = (q >= 2) ? kH - kFrameInset : kFrameInset;

    std::vector<Element> petals;
    petals.reserve(kPetals);
    for (int i = 0; i < kPetals; ++i)
      petals.push_back(box().width(13).height(18)
                           .outline(shapes::rounded(shapes::star(4, 0.36f), 2))
                           .fill(Fill::color(st.goldBright))
                           .foreground(sigil::compose::util::stroke(
                               0.7f, Fill::color(st.bronze))));

    Fill disc = engraved
                    ? Fill::shader(SkRuntimeShaderBuilder(engraved).makeShader())
                    : Fill::color({0.14f, 0.07f, 0.05f, 1});

    return box()
        .key("med" + std::to_string(q))
        .inset(cx - kMedD / 2, cy - kMedD / 2, kW - (cx + kMedD / 2),
               kH - (cy + kMedD / 2))
        .width(kMedD).height(kMedD)
        .transformOrigin(0.5f, 0.5f)
        .rotate(&spin[q]).scale(&breathe[q])
        .cache(Cache::Picture)
        .child(box().inset(15)
                   .outline(shapes::squircle(4.0f))
                   .fill(disc)
                   .foreground(sigil::compose::util::stroke(
                       2.2f, Fill::color(st.gold)))
                   .foreground(sigil::compose::util::stroke(
                       0.7f, Fill::color(st.goldBright))))
        .child(layout(layouts::Radial{0.82f}).inset(0)
                   .children(std::move(petals)))
        .child(box()
                   .inset(kMedD / 2 - 9, kMedD / 2 - 9, kMedD / 2 - 9,
                          kMedD / 2 - 9)
                   .outline(shapes::star(8, 0.5f))
                   .fill(Fill::color(mp.accent ? st.goldBright : st.gold))
                   .opacity(&flare));
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

    auto arc = [&](const char *a, const char *b) {
      return connector(a, b, routers::arc(0.05f)).inset(0)
          .foreground(gild);
    };
    return stack().inset(0).zIndex(2)
        .child(arc("med0", "med1"))
        .child(arc("med1", "med2"))
        .child(arc("med2", "med3"))
        .child(arc("med3", "med0"))
        .child(connector("med0", "med2", routers::orthogonal(18.0f))
                   .inset(0).foreground(beaded))
        .child(connector("med1", "med3", routers::orthogonal(18.0f))
                   .inset(0).foreground(beaded));
  }

  // ---- the central cartouche (the box being framed) ----------------------

  Element cartouche() const {
    Slice carved = carvedFrameSlice(carvedFrame);

    Decoration hatchDeco{PaintProgram{}};
    if (hatch) {
      auto fx = hatch;
      hatchDeco = Decoration(PaintProgram([fx](SkCanvas &c,
                                               const PaintContext &ctx) {
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
      sparks.push_back(box().width(3).height(3)
                           .outline(shapes::star(4, 0.4f))
                           .fill(Fill::color({st.bronze.fR, st.bronze.fG,
                                              st.bronze.fB, 0.5f}))
                           .opacity(0.5f));

    std::vector<Element> frieze;
    frieze.reserve(kFriezeTiles);
    for (int i = 0; i < kFriezeTiles; ++i)
      frieze.push_back(image(gemAtlas)
                           .region(SkRect::MakeXYWH((float)(i % 4) * 16, 0, 16,
                                                    16))
                           .width(16).height(16));

    auto titleLayer = [this](SkColor4f color, bool bloom) {
      auto t = text(u8"AURELIA", glyphs(34, color, 5.0f))
                   .key(bloom ? "titleBloom" : "title").opacity(&titleFade);
      if (bloom)
        t.effect(Effect::filter(SkImageFilters::Blur(6, 6, nullptr)))
            .blend(SkBlendMode::kPlus);
      else
        t.translateY(&titleDrop);
      return box().inset(0).column()
          .alignItems(Align::Center).justify(Justify::Center)
          .child(std::move(t));
    };

    return box()
        .key("cartouche")
        .inset(224, 188, 224, 188) // ~452×264 centered box
        .corners({16}).zIndex(3).clip()
        .backdrop(Effect::filter(SkImageFilters::Blur(8, 8, nullptr)))
        .background(sigil::compose::util::shadow({0, 0, 0, 0.5f}, {0, 6}, 16))
        .fill(flourishParchment(st))
        .background(hatchDeco)
        .background(carved)
        .column().padding(30, 26).gap(9).alignItems(Align::Center)
        .child(layout(layouts::Scatter{7, 0.7f}).inset(22)
                   .children(std::move(sparks)))
        .child(stack().width(258).height(66)
                   .outline(scallopOutline(12))
                   .fill(Fill::color({st.parchment.fR * 1.05f,
                                      st.parchment.fG * 1.05f,
                                      st.parchment.fB * 1.02f, 1}))
                   .foreground(sigil::compose::util::stroke(
                       1.3f, Fill::color(st.gold)))
                   .child(titleLayer(st.goldBright, true))
                   .child(titleLayer({0.34f, 0.20f, 0.09f, 1}, false)))
        .child(box().key("seal").width(42).height(42)
                   .transformOrigin(0.5f, 0.5f).scale(&sealBreathe)
                   .outline(shapes::star(12, 0.66f))
                   .fill(with(Fill::color(accent ? st.rubric : st.bronze),
                             {600ms}))
                   .foreground(sigil::compose::util::stroke(
                       1.4f, Fill::color(st.goldBright))))
        .child(text(u8"Framed by a vine that draws itself on, corner by "
                    u8"corner, while the medallions turn and the rules hold "
                    u8"their three weights of gold — every ornament a "
                    u8"different corner of the compose surface, woven around "
                    u8"this seal.",
                    glyphs(12.5f, st.ink))
                   .key("motto").flowAround("seal", 7))
        .child(box().row().gap(2).justify(Justify::Center)
                   .children(std::move(frieze)))
        .child(text(u8"— a stress test that chose to be beautiful —",
                    glyphs(11, st.rubric)));
  }

  // ---- draw-on scrollwork sweeps (Cache::None, read reveal live) ----------

  Element scrollworkCorner(int q) const {
    return custom([this, q](SkCanvas &c, const PaintContext &ctx) {
             const float rev = reveal.value();
             const float local =
                 std::clamp((rev - (float)q * 0.16f) / 0.55f, 0.0f, 1.0f);
             const float w = ctx.size.width(), h = ctx.size.height();
             const bool right = (q == 1 || q == 2);
             const bool bottom = (q >= 2);
             c.save();
             if (right) { c.translate(w, 0); c.scale(-1, 1); }
             if (bottom) { c.translate(0, h); c.scale(1, -1); }

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
               if (along) m.setRotate(90, x0, y0);
               else m.setIdentity();
               for (auto &p : pts) p = m.mapPoint(p);
               SkPaint gp;
               gp.setAntiAlias(true);
               gp.setColor4f(st.gold, nullptr);
               c.drawPath(taperedStroke(revealed(pts, local), 5.0f), gp);
               std::vector<SkPoint> under;
               appendCubic(under, {x0 + 4, y0 + 20},
                           {x0 + armLen * 0.16f, y0 + 30},
                           {x0 + armLen * 0.30f, y0 + 26},
                           {x0 + armLen * 0.40f, y0 + 22});
               appendSpiral(under, {x0 + armLen * 0.47f, y0 + 18}, 8.0f, 1.0f,
                            2.4f, 2.4f - 6.6f, 28);
               for (auto &p : under) p = m.mapPoint(p);
               c.drawPath(taperedStroke(revealed(under, local), 2.6f), gp);
               if (local > 0.85f) {
                 const SkPoint e = m.mapPoint(eye);
                 SkPaint bp;
                 bp.setAntiAlias(true);
                 bp.setColor4f(st.goldBright, nullptr);
                 c.drawCircle(e.x(), e.y(), 2.6f, bp);
               }
             };
             sweep(false);
             sweep(true);
             if (local > 0.98f)
               drawDiamond(c, {x0, y0}, 4.5f, st.goldBright);
             c.restore();
           })
        .inset(0).zIndex(4).cache(Cache::None);
  }

  // Truncate a point list to the leading `fraction` for the draw-on reveal.
  static std::vector<SkPoint> revealed(const std::vector<SkPoint> &pts,
                                       float fraction) {
    const size_t keep = std::max<size_t>(
        2, (size_t)std::lround((float)pts.size() *
                               std::clamp(fraction, 0.f, 1.f)));
    if (keep >= pts.size())
      return pts;
    return {pts.begin(), pts.begin() + (std::ptrdiff_t)keep};
  }

  // ---- live overlays ------------------------------------------------------

  Element goldDust() const {
    const SkColor4f g = st.goldBright;
    return custom([g](SkCanvas &c, const PaintContext &ctx) {
             SkPaint p;
             p.setAntiAlias(true);
             const double t = ctx.elapsedSeconds;
             for (int i = 0; i < kMotes; ++i) {
               const float fx = (float)i * 137.5f;
               const float x = std::fmod(fx + (float)t * (7.0f + (float)(i % 5)),
                                         ctx.size.width());
               const float y = std::fmod(fx * 0.618f + 40.0f, ctx.size.height());
               const float a = 0.05f + 0.15f * (0.5f + 0.5f *
                               std::sin((float)t * 1.6f + (float)i * 2.1f));
               p.setColor4f({g.fR, g.fG, g.fB, a}, nullptr);
               c.drawCircle(x, y + 10.0f * std::sin((float)t * 0.7f + (float)i),
                            1.1f + (float)(i % 3) * 0.5f, p);
             }
           })
        .inset(0).zIndex(6).cache(Cache::None)
        .blend(SkBlendMode::kPlus);
  }

  Element shimmer() const {
    const SkColor4f g = st.goldBright;
    return custom([g](SkCanvas &c, const PaintContext &ctx) {
             const float w = ctx.size.width(), h = ctx.size.height();
             const float t = (float)ctx.elapsedSeconds;
             const float sweep = std::fmod(t * 180.0f, w + h + 300.0f) - 150.0f;
             SkPoint pts[2] = {{sweep, 0}, {sweep + 130, h}};
             const SkColor4f cols[3] = {{1, 1, 1, 0},
                                        {g.fR, g.fG, g.fB, 0.22f},
                                        {1, 1, 1, 0}};
             const float stops[3] = {0.0f, 0.5f, 1.0f};
             SkPaint p;
             p.setShader(SkShaders::LinearGradient(
                 pts, SkGradient({{cols, 3}, {stops, 3}, SkTileMode::kClamp},
                                 {})));
             c.drawRect(SkRect::MakeWH(w, h), p);
           })
        .inset(kFrameInset).zIndex(5).cache(Cache::None)
        .blend(SkBlendMode::kPlus);
  }

  // ---- assembly -----------------------------------------------------------

  Element describe() const {
    return stack()
        .fill(sigil::compose::util::radialGradient(
            {kW / 2, kH / 2}, 620, {st.velvetCore, st.velvetEdge}))
        .child(frameBand())
        .child(frameGlow())
        .child(filaments())
        .child(memo(MedProps{0, accent},
                    [this](const MedProps &p) { return medallion(p); })
                   .key("med0"))
        .child(memo(MedProps{1, accent},
                    [this](const MedProps &p) { return medallion(p); })
                   .key("med1"))
        .child(memo(MedProps{2, accent},
                    [this](const MedProps &p) { return medallion(p); })
                   .key("med2"))
        .child(memo(MedProps{3, accent},
                    [this](const MedProps &p) { return medallion(p); })
                   .key("med3"))
        .child(cartouche())
        .child(scrollworkCorner(0))
        .child(scrollworkCorner(1))
        .child(scrollworkCorner(2))
        .child(scrollworkCorner(3))
        .child(shimmer())
        .child(goldDust());
  }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    sceneTicker = &ticker;
    hatch = makeHatch();
    engraved = makeEngraved();
    carvedFrame = std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(makeCarvedFrame(toOrnamentPalette(st),
                                                       192)));
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
    ticker.timeline().apply(&titleDrop).then<ch::RampTo>(0.0f, 1.0f,
                                                         &ch::easeOutQuint);
    ticker.timeline().apply(&titleFade).then<ch::RampTo>(1.0f, 1.2f);
    ticker.timeline().apply(&flare).then<ch::RampTo>(1.0f, 1.3f);

    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      for (int q = 0; q < 4; ++q) {
        const float dir = (q == 0 || q == 2) ? 1.0f : -1.0f;
        spin[q] = (float)(t * 7.0) * dir;
        breathe[q] = 1.0f + 0.05f * (float)std::sin(t * 1.3 + q * 1.5707963);
      }
      sealBreathe = 1.0f + 0.06f * (float)std::sin(t * 1.1);
      return true;
    });

    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextAccent)
      return;
    nextAccent = elapsed + 4.0;
    accent = !accent;
    // The sketch's rubric flare: each accent beat re-lights the medallion
    // bosses, then settles back over 1.1 s (flourish_border.cpp update()).
    if (sceneTicker) {
      flare = 1.0f;
      sceneTicker->timeline().apply(&flare).then<ch::RampTo>(
          0.55f, 1.1f, &ch::easeOutQuint);
    }
    composer.render(describe());
  }

  sigil::motion::Ticker *sceneTicker = nullptr;
};

} // namespace compose_gallery
