#pragma once
// 縦組み — a vertical-RL CJK plate, the one place the library's writing mode
// is visible as type rather than as geometry.
//
// One paragraph carries all three vertical forms at once, because that is
// what makes the mode legible: ideographs standing upright in their 'vert'
// shapes, a Latin word lying on its side down the column, and two-digit
// numbers set 縦中横 — shaped across and stood upright in the column, which
// is how a date reads in vertical prose.
//
// Over that, the rest of the text surface asked to work down the page: a
// spanPaint highlight on a named phrase, and one settling entrance beating
// cluster by cluster in reading order — down each column, then right to
// left across them.
//
// Ruby and kenten are deliberately absent. Each is a few lines over the
// placed runs of a finished layout rather than a library feature, and the
// shapes they take differ enough per passage that a verb would fit none of
// them.

#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>

#include "GalleryCore.h"

namespace compose_gallery {

namespace tategaki {

constexpr SkColor4f kSumi{0.055f, 0.051f, 0.047f, 1};  // ink ground
constexpr SkColor4f kSumiLift{0.086f, 0.078f, 0.070f, 1};
constexpr SkColor4f kGofun{0.921f, 0.906f, 0.870f, 1};  // shell white
constexpr SkColor4f kAi{0.478f, 0.588f, 0.678f, 1};     // indigo
constexpr SkColor4f kAka{0.847f, 0.294f, 0.216f, 1};    // vermilion

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kBodySize = 30;
constexpr float kColumnBlockW = 420;
constexpr float kColumnBlockH = 436;
constexpr float kColumnBlockRight = 56;

/** The mincho face the plate is set in, or whatever the platform offers.
 *  Resolved once: matchFamilyStyle walks the system cascade. */
inline sk_sp<SkTypeface> mincho() {
  static sk_sp<SkTypeface> face = [] {
    sk_sp<SkTypeface> matched = fonts().fontManager()->matchFamilyStyle(
        "Hiragino Mincho ProN", SkFontStyle());
    return matched ? matched : fonts().defaultTypeface();
  }();
  return face;
}

inline sigil::weave::TextStyle body(
    float size, SkColor4f color,
    sigil::weave::VerticalForm form = sigil::weave::VerticalForm::kAuto) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = mincho();
  s.shaping.fontSize = size;
  s.shaping.languageTag = "ja";
  s.shaping.verticalForm = form;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** Latin captions, which stay horizontal — the plate labels itself in the
 *  other writing mode so the two are side by side. */
inline sigil::weave::TextStyle label(float size, SkColor4f color,
                                     float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

}  // namespace tategaki

struct TategakiScene final : Scene {
  const char* name() const override { return "tategaki"; }

  /// After the cascade has settled: the plate is the finished setting, not
  /// a frame of its entrance.
  double captureSeconds() const override { return 2.4; }

  void setup(Composer& composer, sigil::motion::Ticker&) override {
    composer.render(describe());
  }

  /** One form, named and shown: a Latin caption over a short column set
   *  the way the caption says. The three together are the whole per-span
   *  vocabulary, side by side at a size where the difference reads. */
  Element specimen(const char* caption, RichText run) {
    namespace tg = tategaki;
    return box()
        .column()
        .gap(12)
        .child(text(toU8(caption), tg::label(12, tg::kAi, 2)))
        .child(text(std::move(run))
                   .width(Dim(46.0f))
                   .height(Dim(140.0f))
                   .writingMode(sigil::weave::WritingMode::kVerticalRL));
  }

  Element describe() {
    namespace tg = tategaki;
    namespace ch = choreograph;

    Material ground = Material::linear(
        {0, 0}, {0, tg::kH}, {{0.0f, tg::kSumiLift}, {1.0f, tg::kSumi}});

    // All three vertical forms in one passage. Only the two numbers and the
    // Latin word name a form; everything else takes UTR#50's, which is what
    // stands the ideographs upright and turns the Latin on its side by
    // itself.
    auto passage =
        rich(tg::body(tg::kBodySize, tg::kGofun))
            .add(u8"縦組みの文章は、上から下へ、右から左へと流れる。平成")
            .add(u8"31", tg::body(tg::kBodySize, tg::kGofun,
                                  sigil::weave::VerticalForm::kTateChuYoko))
            .add(u8"年")
            .add(u8"12", tg::body(tg::kBodySize, tg::kGofun,
                                  sigil::weave::VerticalForm::kTateChuYoko))
            .add(u8"月、組版の器")
            .add(u8"SigilWeave", tg::body(tg::kBodySize * 0.86f, tg::kAi,
                                          sigil::weave::VerticalForm::kRotated))
            .add(u8"は縦書きに対応した。字は立ち、欧文は寝る。")
            .add(u8"数字は縦中横に組み、二桁のまま読ませる。")
            .add(u8"行は列となり、列は右から左へ積まれてゆく。");

    return box()
        .fill(std::move(ground))
        .child(text(std::move(passage))
                   .absolute()
                   .inset(tg::kW - tg::kColumnBlockRight - tg::kColumnBlockW,
                          92, tg::kColumnBlockRight, 0)
                   .width(Dim(tg::kColumnBlockW))
                   .height(Dim(tg::kColumnBlockH))
                   .writingMode(sigil::weave::WritingMode::kVerticalRL)
                   // The phrase the plate is about, in vermilion — paint only,
                   // so the glyphs are exactly the glyphs the passage shaped.
                   .spanPaint(sel::text(u8"縦組み"),
                              sigil::weave::PaintStyle(tg::kAka.toSkColor()))
                   // One settling entrance, beating cluster by cluster in
                   // READING ORDER: down each column, then right to left.
                   .fx({.effect = fx::rise(30),
                        .stagger = {.amountMs = 1100, .durationMs = 520},
                        .progress = animate(from(0.0f).to(1.0f),
                                            {1500ms, &ch::easeNone, 180ms})}))
        .child(
            box()
                .absolute()
                .inset(64, 88, 0, 0)
                .column()
                .gap(10)
                .child(text(toU8("\xe7\xb8\xa6\xe7\xb5\x84\xe3\x81\xbf"),
                            tg::body(46, tg::kGofun)))
                .child(box()
                           .width(Dim(120.0f))
                           .height(Dim(1.0f))
                           .fill(Fill::color(tg::kAi)))
                .child(text(toU8("VERTICAL-RL"), tg::label(15, tg::kAi, 4)))
                .child(text(toU8("UTR#50 orientation, 'vert' forms,\n"
                                 "tate-chu-yoko digits, rotated Latin"),
                            tg::label(14, tg::kGofun, 0.5f))
                           .width(Dim(240.0f)))
                .child(box().height(Dim(26.0f)))
                .child(
                    box()
                        .row()
                        .gap(34)
                        .child(specimen(
                            "UPRIGHT",
                            rich(tg::body(28, tg::kGofun,
                                          sigil::weave::VerticalForm::kUpright))
                                .add(u8"字は立つ")))
                        .child(specimen(
                            "ROTATED",
                            rich(tg::body(24, tg::kAi,
                                          sigil::weave::VerticalForm::kRotated))
                                .add(u8"Latin lies")))
                        .child(specimen(
                            "TATE-CHU-YOKO",
                            rich(tg::body(28, tg::kGofun))
                                .add(u8"令和")
                                .add(u8"07",
                                     tg::body(28, tg::kAka,
                                              sigil::weave::VerticalForm::
                                                  kTateChuYoko))
                                .add(u8"年"))))
                .child(box().height(Dim(22.0f)))
                .child(text(toU8("one paragraph \xc2\xb7 one writingMode "
                                 "\xc2\xb7 three forms"),
                            tg::label(13, {0.55f, 0.53f, 0.50f, 1}))
                           .width(Dim(300.0f))))
        .child(text(toU8("cluster-unit entrance staggers DOWN the column, "
                         "columns advance right to left"),
                    tg::label(13, {0.48f, 0.46f, 0.44f, 1}))
                   .absolute()
                   .inset(64, tg::kH - 46, 0, 0));
  }
};

}  // namespace compose_gallery
