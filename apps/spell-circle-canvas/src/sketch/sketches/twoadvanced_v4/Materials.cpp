#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::available(std::string* why) -> bool {
  return sketch::requireCached(
      {"https://v4prophecy.2advanced.com/images/leftsidepanel.gif",
       "https://v4prophecy.2advanced.com/images/rightsidepanel.gif",
       "https://v4prophecy.2advanced.com/images/sitebackground.gif",
       "https://v4prophecy.2advanced.com/images/sitefooter.gif",
       "https://v4prophecy.2advanced.com/images/2alogobug.svg"},
      why);
}

auto TwoAdvancedV4::stretchFill(
    const std::shared_ptr<const sigil::image::ImageAsset>& asset, float w,
    float h, SkTileMode tx) -> mskia::Paint {
  const sk_sp<SkImage>& img = asset->frames()[0].image;
  return mskia::Paint::image(
      img, tx, SkTileMode::kClamp,
      SkMatrix::Scale(w / (float)img->width(), h / (float)img->height()),
      SkSamplingOptions(SkFilterMode::kLinear));
}

auto TwoAdvancedV4::spectrumFx() -> sk_sp<SkRuntimeEffect> {
  auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float  uTime;
      uniform float  uBars;
      uniform float4 uHot;
      uniform float4 uCool;
      float h(float a, float b) {
        return fract(sin(a * 12.9898 + b * 78.233) * 43758.5453);
      }
      half4 main(float2 xy) {
        float bw = uResolution.x / uBars;
        float i  = floor(xy.x / bw);
        float fx = fract(xy.x / bw);
        float gy = step(fract(xy.y / (uResolution.y / 5.0)), 0.06);
        float4 acc = float4(0.012, 0.075, 0.082, 1.0);
        acc = mix(acc, float4(0.04, 0.19, 0.20, 1.0), gy);
        float gx = step(fract(xy.x / (bw * 4.0)), 0.03);
        acc = mix(acc, float4(0.04, 0.19, 0.20, 1.0), gx);
        if (fx > 0.72) return half4(half3(acc.rgb), 1.0);
        float k  = floor(uTime * 10.0);
        float n  = h(i, k);
        float n2 = h(i * 3.1 + 7.0, k * 0.7 + 5.0);
        float amp = 0.14 + 0.80 * n * (0.35 + 0.65 * n2);
        amp *= 0.55 + 0.55 * sin(i / uBars * 3.14159);
        float top = uResolution.y * (1.0 - clamp(amp, 0.0, 1.0));
        float on  = step(top, xy.y);
        float g = clamp((xy.y - top) / max(uResolution.y - top, 1.0),
                        0.0, 1.0);
        acc = mix(acc, mix(uHot, uCool, g), on);
        float cap = step(abs(xy.y - (top - 3.0)), 1.5);
        acc = mix(acc, float4(1.0, 1.0, 1.0, 1.0), cap * 0.7);
        return half4(half3(acc.rgb), 1.0);
      }
    )"));
  if (!e) SkDebugf("twoadvanced spectrum: %s\n", err.c_str());
  return e;
}

auto TwoAdvancedV4::stripeFx() -> sk_sp<SkRuntimeEffect> {
  auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float  uPan;
      uniform float  uOn;
      uniform float  uPeriod;
      uniform float4 uColor;
      uniform float4 uBase;
      half4 main(float2 xy) {
        float d = (xy.x + xy.y) * 0.70710678 - uPan;
        float f = mod(d, uPeriod);
        float a = clamp(smoothstep(0.0, 1.2, uOn - f), 0.0, 1.0);
        float4 c = mix(uBase, uColor, a);
        return half4(half3(c.rgb), 1.0);
      }
    )"));
  if (!e) SkDebugf("twoadvanced stripe: %s\n", err.c_str());
  return e;
}

auto TwoAdvancedV4::waterFx() -> sk_sp<SkRuntimeEffect> {
  auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float  uTime;
      half4 main(float2 xy) {
        float v = xy.y / max(uResolution.y, 1.0);
        float s = sin(xy.y * 0.85 + sin(xy.x * 0.011 + uTime * 0.45) * 2.4);
        float band = smoothstep(0.62, 1.0, s) * (0.14 + 0.86 * (1.0 - v));
        float3 base = mix(float3(0.000, 0.042, 0.046),
                          float3(0.001, 0.014, 0.017), v);
        float3 c = base + float3(0.02, 0.17, 0.18) * band;
        return half4(half3(c), 1.0);
      }
    )"));
  if (!e) SkDebugf("twoadvanced water: %s\n", err.c_str());
  return e;
}

auto TwoAdvancedV4::tickDots(int cluster, SkColor4f c) -> Element {
  Element r = box().row().gap(4).alignItems(Align::Center);
  for (int i = 0; i < 3; ++i)
    r.child(box().width(5).height(5).fill(c).opacity(
        &dot[(size_t)cluster * 3 + (size_t)i]));
  return r;
}

auto TwoAdvancedV4::radarSweep(int i, SkColor4f tint, float inner) -> Element {
  return box()
      .inset(0)
      .shape(shapes::sector(-100, 78, inner))
      .fill(mskia::Paint::linearUnit({0, 0}, {1, 1},
                                     {{0.0f, mskia::withAlpha(tint, 0.85f)},
                                      {1.0f, mskia::withAlpha(tint, 0.05f)}}))
      .rotate(&gauge[(size_t)i])
      .opacity(&gaugeAlpha[(size_t)i]);
}

auto TwoAdvancedV4::statusBar() -> Element {
  using namespace tav;
  // Two segments meeting on a DIAGONAL seam, not a vertical edge. They
  // drop in one after the other: teal leads, maroon follows 80 ms later.
  Element teal =
      bevelPanel(box()
                     .left(Dim(0))
                     .top(Dim(0))
                     .width(560)
                     .height(40)
                     .shape(shapes::chamfered(40, shapes::Corner::BottomRight))
                     .row()
                     .alignItems(Align::Center)
                     .padding(10, 0)
                     .gap(8),
                 kTealBar)
          .translateY(animate(motion::from(-46.0f).to(0.0f),
                              {380ms, &ch::easeOutQuint, 1450ms}))
          .child(
              box()
                  .width(22)
                  .height(22)
                  .corners({5})
                  .fill(mskia::Paint::radialUnit({0.5f, 0.42f}, 1.15f,
                                                 {{0.0f, kCyanRing},
                                                  {0.55f, kTealBar},
                                                  {1.0f, hexColor(0x0C2A2C)}}))
                  .stroke(stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.7f)),
                                 PathFormat::Align::Inner))
                  .justify(Justify::Center)
                  .alignItems(Align::Center)
                  .child(box().width(9).height(9).corners({5}).stroke(
                      stroke(2, Fill::color(kCyan)))))
          // The teal segment's voice, verbatim from the interface
          // capture: the boot callsign, then the two region labels the
          // page hangs over its modules.
          .child(t("INITREQ 2A", micro(12, kNear, 240)))
          .child(box().width(1).height(14).fill(mskia::withAlpha(kCyan, 0.4f)))
          .child(t("\xe2\x80\xba GLOBAL AMBIENCE",
                   micro(11, mskia::withAlpha(kCyan, 0.9f), 240)))
          .child(box().grow(1))
          .child(box().width(90).height(12).foreground(
              styles::TickRail{mskia::withAlpha(kNear, 0.45f), 6, 3, 8, 1, 3,
                               0.5f, path::Edge::Bottom}))
          .child(box().width(46));

  Element maroon =
      bevelPanel(box()
                     .left(Dim(548))
                     .top(Dim(0))
                     .width(Dim(1892.0f - 548.0f))
                     .height(40)
                     .shape(shapes::chamfered(40, shapes::Corner::TopLeft))
                     .row()
                     .alignItems(Align::Center)
                     .padding(58, 0, 14, 0)
                     .gap(10),
                 kChrome)
          .translateY(animate(motion::from(-46.0f).to(0.0f),
                              {380ms, &ch::easeOutQuint, 1530ms}))
          .child(t("\xe2\x80\xba GLOBAL NAVIGATOR", micro(11, kDust, 260)))
          .child(box().grow(1))
          // V4.PROPHECY sits in its own hairline-outlined plate at the
          // bar's right end — the one piece of type up here that is
          // boxed rather than bare.
          .child(
              box()
                  .height(24)
                  .padding(9, 0)
                  .stroke(stroke(1, Fill::color(mskia::withAlpha(kNear, 0.75f)),
                                 PathFormat::Align::Inner))
                  .row()
                  .alignItems(Align::Center)
                  .child(t("V4.PROPHECY", heavy(14, kNear, 80))));

  return box()
      .left(Dim(24))
      .top(Dim(0))
      .width(1892)
      .height(40)
      .child(maroon)
      .child(teal);
}

auto TwoAdvancedV4::audioModule() -> Element {
  using namespace tav;
  // The playlist verbatim from the interface capture: numbered with
  // periods, track 01 highlighted, no duration column anywhere.
  static const char* tracks[4] = {"01. DESERT TRANCE", "02. NEVERRAIN",
                                  "03. SYNERGY", "04. EXHILE"};
  Element list = box().column().width(268).gap(2);
  for (int i = 0; i < 4; ++i) {
    const bool sel = i == 0;
    list.child(
        box()
            .height(23)
            .row()
            .alignItems(Align::Center)
            .padding(6, 0)
            .gap(6)
            .fill(sel ? kChromeHi : mskia::withAlpha(hexColor(0x2A0A0C), 0.85f))
            .foreground(onEdges(
                path::Edge::Left,
                stroke(
                    2,
                    Fill::color(sel ? kCyan : mskia::withAlpha(kDust, 0.35f)),
                    PathFormat::Align::Inner)))
            .child(t(sel ? "\xe2\x96\xb8" : " ", micro(11, kCyan, 0)))
            .child(t(tracks[i],
                     sigil::weave::kit::tracked(
                         blackFace(), 13, sel ? kNear : kHeadDim, 60, 0.92f))));
  }

  Element scope =
      box()
          .grow(1)
          .height(100)
          .shape(shapes::chamfered(8, shapes::Corner::AntiDiagonal))
          .fill(spectrum)
          .foreground(styles::Scanlines{{0, 0, 0, 0.16f}, 3, 1})
          .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.6f), 10, 2, 3,
                                       shapes::Corner::All})
          .foreground(stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.35f)),
                             PathFormat::Align::Inner));

  auto key = [&](const char* glyph, bool hot) {
    return box()
        .width(38)
        .height(22)
        .shape(shapes::chamfered(6, shapes::Corner::Diagonal))
        .fill(
            mskia::Paint::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, hot ? kCtaHi : hexColor(0x5A2226)},
                                      {0.5f, hot ? kCta : hexColor(0x3A0F12)},
                                      {1.0f, hexColor(0x240607)}}))
        .stroke(stroke(1, Fill::color(mskia::withAlpha(kDust, 0.35f)),
                       PathFormat::Align::Inner))
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t(glyph, micro(11, hot ? kNear : kDust, 0)));
  };
  auto meter = [&](float w, const ch::Output<float>* bind, SkColor4f c) {
    return box()
        .width(Dim(w))
        .height(6)
        .fill(hexColor(0x1B0708))
        .child(box()
                   .left(Dim(0))
                   .top(Dim(0))
                   .width(Dim(w))
                   .height(6)
                   .fill(c)
                   .scaleX(bind)
                   .transformOrigin(0, 0.5f));
  };

  Element panel =
      bevelPanel(box().column().padding(9).gap(6), hexColor(0x3E1013));
  panel.key("audio")
      .area("audio")
      .foreground(styles::Brackets{
          kCyan, 18, 3, 4, shapes::Corner::TopLeft | shapes::Corner::TopRight})
      .foreground(styles::TickRail{mskia::withAlpha(kDust, 0.45f), 7, 3, 6, 1,
                                   4, 0.5f, path::Edge::Bottom})
      .child(box().row().gap(8).height(100).child(list).child(scope))
      .child(box()
                 .row()
                 .gap(5)
                 .alignItems(Align::Center)
                 .child(key("\xe2\x97\x82\xe2\x97\x82", false))
                 .child(key("\xe2\x96\xa0", false))
                 .child(key("\xe2\x96\xb8", true))
                 .child(key("\xe2\x96\xb8\xe2\x96\xb8", false))
                 .child(box().width(8))
                 .child(meter(64, &vuLeft, mskia::withAlpha(kCyan, 0.85f)))
                 .child(box().grow(1))
                 .child(t("VOL", micro(10, kDustDim, 200)))
                 .child(meter(56, &vuRight, mskia::withAlpha(kCyanRing, 0.8f))))
      .child(box()
                 .height(18)
                 .row()
                 .alignItems(Align::Center)
                 .padding(6, 0)
                 .fill(mskia::withAlpha(kChrome, 0.9f))
                 .child(t("AUDIO PREFERENCES", micro(11, kDust, 240)))
                 .child(box().grow(1))
                 .child(t("STREAM 128K \xc2\xb7 STEREO",
                          micro(10, kDustDim, 200))));
  return panel;
}
