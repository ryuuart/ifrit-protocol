#include "WinampBase.h"

auto WinampBase::eqWindow() -> Element {
  using namespace wa;
  // the eleven faders: preamp at native x21, bands on an 18 px pitch from x78
  // — the real, non-skinnable positions
  const auto fader = [this](int i) {
    const float x = i == 0 ? 21.0f : 78.0f + 18.0f * (float)(i - 1);
    return sunken(at(box(), x, 38, 14, 63).fill(hexColor(0x14141F)),
                  sigil::material::withAlpha(hexColor(0x4A4A70), 0.55f),
                  hexColor(0x08080E))
        .children(
            {at(box(), 2, 1, 10, 61).fill(faderTrack),
             // thumb 11x11, travel 0..52 native. bind() turns the [-1,1] gain
             // straight into pixels — no second Output in slider units.
             raised(at(box(), 1, 0, 12, 11)
                        .fill(mskia::Paint::linearUnit(
                            {0, 0}, {0, 1},
                            {{0.0f, sigil::material::lighten(kBtnFace, 0.14f)},
                             {1.0f, dark(kBtnFace, 0.32f)}}))
                        .translateY(motion::bind(&gain[(size_t)i])
                                        .source(-1.0f, 1.0f)
                                        .target(n(52), n(0))))
                 .children(
                     {at(box(), 2, 5, 8, 1)
                          .fill(sigil::material::withAlpha(kBtnLo, 0.85f))})});
  };
  // +12dB / 0dB / -12dB, baked pixel art in the real EQMAIN.BMP, printed here
  // in the gap between the preamp and the first band
  struct Mark {
    const char* words;
    float y;
    sigil::material::Color ink;
  };
  const Mark kScale[3] = {{"+12DB", 38, dark(kEqTop, 0.15f)},
                          {"+0DB", 62, dark(kEqMid, 0.28f)},
                          {"-12DB", 89, dark(kEqBot, 0.10f)}};
  // PREAMP + the ten band captions, tight against the fader feet
  const char* kBands[10] = {"60", "170", "310", "600", "1K",
                            "3K", "6K",  "12K", "14K", "16K"};
  // The window's captions — PREAMP and the band frequencies — are set in the
  // body's lavender-grey; a key and the title bar say their own.
  return raised(box().width(n(275)).height(n(116)).ink(kCaption), kWellHi,
                kWellLo)
      .children({box().inset(0).fill(steel).cache(Cache::Texture),
                 titleBar(275, "WINAMP EQUALIZER", false, false),
                 // ON / AUTO / PRESETS
                 lamp(14, 18, 26, 12, "ON", true, 2, 4.4f),
                 lamp(40, 18, 32, 12, "AUTO", false, 2, 4.4f),
                 textKey(217, 18, 44, 12, "PRESETS", 4.4f),
                 // the response graph (native 86,17,113,19) — its curve is the
                 // SAME ten Outputs the faders below ride, so the two widgets
                 // can never disagree
                 sunken(at(box(), 86, 17, 113, 19).fill(graphMat),
                        sigil::material::withAlpha(hexColor(0x4A4A70), 0.6f),
                        hexColor(0x08080E))
                     .children({box().inset(0).fill(graphGrid.material()),
                                eqCurve().inset(0).cache(Cache::None)}),
                 each(11, fader),
                 each(kScale,
                      [this](const Mark& m) {
                        return at(box(), 38, m.y, 38, 7)
                            .justifyContent(Justify::End)
                            .alignItems(Align::Center)
                            .children({t(m.words, pix(3.6f)).ink(m.ink)});
                      }),
                 at(box(), 3, 104, 30, 7)
                     .alignItems(Align::Center)
                     .children({t("PREAMP", pix(3.6f))}),
                 each(kBands, [this](const char* words, size_t i) {
                   return at(box(), 76.0f + 18.0f * (float)i, 104, 18, 7)
                       .justifyContent(Justify::Center)
                       .alignItems(Align::Center)
                       .children({t(words, pix(3.6f))});
                 })});
}

auto WinampBase::eqCurve() -> Element {
  using namespace wa;
  // KEYLESS, AND DRAWN WITH THE PEN: the curve is read off ten live Outputs at
  // paint time, and p5's `curveVertex` IS Catmull-Rom — so the ten band values
  // ARE the curve, and no interpolation is spelled here. The two passes are
  // the phosphor: a hairline trace and a wider wash of the same green under a
  // fifth of the alpha.
  return sigil::compose::pen([this](draw::Pen& pen) {
    const float w = pen.width, h = pen.height, mid = h * 0.5f;
    pen.noStroke();
    pen.fill(sigil::material::withAlpha(kGrid, 0.9f));
    pen.rect(0, mid - n(0.5f), w, n(1));
    // the reveal is a window over a LIVE path, so it is a clip and not a trim
    pen.clip([&] {
      pen.rect(0, 0, w * std::clamp(graphDraw.value(), 0.0f, 1.0f), h);
    });
    pen.noFill();
    const auto band = [this, w, h, mid](int i) {
      return SkPoint{w * ((float)i + 0.5f) / 10.0f,
                     mid - gain[(size_t)i + 1].value() * h * 0.42f};
    };
    const std::pair<float, sigil::material::Color> passes[2] = {
        {n(2.5f), hexColor(0x1AE81A, 0.20f)}, {n(1), hexColor(0x1AE81A)}};
    for (const auto& [weight, ink] : passes) {
      pen.strokeWeight(weight);
      pen.stroke(ink);
      pen.beginShape();
      // the ends are DOUBLED controls, which is how a Catmull-Rom run reaches
      // its own first and last point instead of starting one in
      pen.curveVertex(0, band(0).fY);
      pen.curveVertex(0, band(0).fY);
      for (int i = 0; i < 10; ++i) pen.curveVertex(band(i).fX, band(i).fY);
      pen.curveVertex(w, band(9).fY);
      pen.curveVertex(w, band(9).fY);
      pen.endShape();
    }
  });
}
