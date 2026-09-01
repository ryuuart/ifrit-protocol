/** @file
 * bousen — the furniture a vertical column carries: the sideline beside
 * it, the alternates the face is asked for, a mark anchored to one phrase,
 * and an entrance that beats column by column.
 */

// 傍線 — where tategaki sets the three vertical FORMS against each other,
// this plate is about everything ELSE a column carries once the letters
// stand: the band that runs beside it, the metrics a face keeps for a
// column and hands over only when asked, the anchor a caller hangs off one
// phrase, and the reading order a cascade beats in.
//
// Four things are on the page, and each is one declaration:
//
//   · a red 傍線 down the right of one phrase, and its opposite down the
//     left of another — one Decoration each, on a span, drawn beside the
//     column the way an underline is drawn beneath a line;
//   · a highlight over a third phrase, which in a column covers the column
//     pitch rather than a cap band;
//   · a mark anchored to a fourth, standing in the margin beside the
//     characters it names, because a mark's rect is the union of the
//     advance boxes and those stack DOWN;
//   · a punctuation pair: one column plain, one asking the face for its
//     vertical alternates and kana forms, so what those tags do on the
//     installed face is on the page rather than in a comment.
//
// The cascade is on a strip of its own, beating over unit::Line — one
// COLUMN a beat — because a band and a track do not share a node: a track
// draws its own glyphs in batched buckets and a bucket carries glyphs
// alone. The plate is the settled page.

#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Features.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The fonts this piece measures with. Leaked deliberately: it owns
 *  Skia-backed state, and a static destructor racing Skia teardown is a
 *  class of crash worth not having. */
sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

constexpr SkSize kSceneSize = {900, 640};

namespace bousen {

constexpr SkColor4f kKinari{0.937f, 0.918f, 0.878f, 1};  // unbleached paper
constexpr SkColor4f kKinariLift{0.961f, 0.945f, 0.909f, 1};
constexpr SkColor4f kSumi{0.114f, 0.106f, 0.098f, 1};  // ink
constexpr SkColor4f kAka{0.741f, 0.196f, 0.153f, 1};   // vermilion
constexpr SkColor4f kAi{0.192f, 0.302f, 0.404f, 1};    // indigo
constexpr SkColor4f kUsu{0.612f, 0.588f, 0.545f, 1};   // pale ink

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kBodySize = 27;
constexpr float kBlockW = 300;
constexpr float kBlockH = 430;
constexpr float kBlockRight = 60;

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

inline sigil::weave::TextStyle body(float size, SkColor4f color) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = mincho();
  s.shaping.fontSize = size;
  s.shaping.languageTag = "ja";
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** The same body style asking the face for the metrics and forms it keeps
 *  for a column: punctuation pulled onto the column axis, full-width marks
 *  set to the space their ink needs, small kana cut for a column. What the
 *  installed face answers is what the plate shows. */
inline sigil::weave::TextStyle columnFitted(float size, SkColor4f color) {
  sigil::weave::TextStyle s = body(size, color);
  s.shaping.fontFeatures = {sigil::weave::Features::verticalAlternates,
                            sigil::weave::Features::proportionalVerticalMetrics,
                            sigil::weave::Features::verticalKana};
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

/** A paint that draws its glyphs in @p ink and carries one band beside
 *  the column: `kUnderline` runs down the RIGHT of the column, `kOverline`
 *  down the left, `kHighlight` across the whole pitch. */
inline sigil::weave::PaintStyle banded(SkColor4f ink,
                                       sigil::weave::Decoration::Kind kind,
                                       SkColor4f band, float thickness) {
  sigil::weave::PaintStyle p(ink.toSkColor());
  p.foreground.setAntiAlias(true);
  sigil::weave::Decoration decoration;
  decoration.kind = kind;
  decoration.color = band.toSkColor();
  decoration.thickness = thickness;
  p.addDecoration(decoration);
  return p;
}

}  // namespace bousen

struct Bousen final : sketch::Sketch {
  /// After the columns have assembled: the plate is the finished page.

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background({1, 1, 1, 1});
    ctx.captureAt(2.6);
    ctx.composer.render(describe());
  }

  /** One punctuation column, captioned: the same characters set twice,
   *  once as the face gives them and once as it gives them when asked. */
  Element specimen(const char* caption, const sigil::weave::TextStyle& style) {
    namespace bs = bousen;
    return box()
        .column()
        .gap(10)
        .child(
            text(toU8(caption), bs::label(11, bs::kAi, 1.5f)).width(Dim(96.0f)))
        .child(text(u8"「あっ」、。", style)
                   .width(Dim(42.0f))
                   .height(Dim(150.0f))
                   .writingMode(sigil::weave::WritingMode::kVerticalRL));
  }

  Element describe() {
    namespace bs = bousen;
    namespace ch = choreograph;

    Material ground = Material::linear(
        {0, 0}, {0, bs::kH}, {{0.0f, bs::kKinariLift}, {1.0f, bs::kKinari}});

    auto passage =
        rich(bs::body(bs::kBodySize, bs::kSumi))
            .add(u8"縦組みの本文にも、")
            .add(u8"傍線")
            .add(u8"を引くことができる。線は列の右に立ち、")
            .add(u8"約物")
            .add(u8"は列の心に寄る。")
            .add(u8"小書きの仮名")
            .add(u8"は縦の形に替わり、列は右から左へ組み上がってゆく。");

    return box()
        .fill(std::move(ground))
        .child(
            text(std::move(passage))
                .absolute()
                .inset(bs::kW - bs::kBlockRight - bs::kBlockW, 96,
                       bs::kBlockRight, 0)
                .width(Dim(bs::kBlockW))
                .height(Dim(bs::kBlockH))
                .writingMode(sigil::weave::WritingMode::kVerticalRL)
                // The band the plate is named for: down the RIGHT of the
                // column, the length of the phrase it dresses.
                .spanPaint(
                    sel::text(u8"傍線"),
                    bs::banded(bs::kSumi,
                               sigil::weave::Decoration::Kind::kUnderline,
                               bs::kAka, 2.5f))
                // Its opposite, down the left.
                .spanPaint(sel::text(u8"約物"),
                           bs::banded(bs::kSumi,
                                      sigil::weave::Decoration::Kind::kOverline,
                                      bs::kAi, 2.0f))
                // A highlight covers the column PITCH — there is no cap
                // band across a column to hang one on.
                .spanPaint(
                    sel::text(u8"小書きの仮名"),
                    bs::banded(bs::kSumi,
                               sigil::weave::Decoration::Kind::kHighlight,
                               {bs::kAi.fR, bs::kAi.fG, bs::kAi.fB, 0.13f}, 0))
                // A mark stands in the margin BESIDE the phrase it names:
                // the rect it anchors to is the union of that phrase's
                // advance boxes, and in a column those stack downward, so
                // the note it carries runs down the page beside them.
                .mark(sel::text(u8"列は右から左へ"),
                      box()
                          .key("callout")
                          .left(Dim(-168.0f))
                          .top(pct(0))
                          .width(Dim(150.0f))
                          .child(text(rich(bs::label(10, bs::kUsu))
                                          .add(toU8("mark() "),
                                               bs::label(11, bs::kAka, 1))
                                          .add(toU8("\xe2\x80\x94 anchored to "
                                                    "the phrase,\nnot to a "
                                                    "coordinate")))
                                     .width(Dim(150.0f)))))
        // The plate names itself in the other writing mode, so the two
        // stand side by side.
        .child(
            box()
                .absolute()
                .inset(64, 92, 0, 0)
                .column()
                .gap(10)
                .child(text(toU8("\xe5\x82\x8d\xe7\xb7\x9a"),
                            bs::body(44, bs::kSumi)))
                .child(box()
                           .width(Dim(120.0f))
                           .height(Dim(1.0f))
                           .fill(Fill::color(bs::kAka)))
                .child(text(toU8("THE COLUMN'S FURNITURE"),
                            bs::label(13, bs::kAi, 3)))
                .child(text(toU8("a band beside the column, not beneath a\n"
                                 "line \xc2\xb7 a mark on the phrase it names"),
                            bs::label(13, bs::kSumi, 0.4f))
                           .width(Dim(260.0f)))
                .child(box().height(Dim(20.0f)))
                .child(box()
                           .row()
                           .gap(30)
                           .child(specimen("AS THE FACE GIVES IT",
                                           bs::body(26, bs::kSumi)))
                           .child(specimen("valt \xc2\xb7 vpal \xc2\xb7 vkna",
                                           bs::columnFitted(26, bs::kAka))))
                .child(box().height(Dim(14.0f)))
                .child(text(toU8("the pair is one string set twice: the "
                                 "second asks\nthe face for the metrics it "
                                 "keeps for a column"),
                            bs::label(11, bs::kUsu))
                           .width(Dim(300.0f))))
        // The cascade lives on its own strip, and it wears a band. A track
        // draws its glyphs itself, in batched buckets that carry glyphs
        // alone, so the sideline is drawn beside them at the placement the
        // layout left it: it stands still down the whole column while the
        // letters travel into it.
        .child(
            text(u8"列ごとに文字が現れる。右から左へ。", bs::body(21, bs::kAi))
                .absolute()
                .inset(352, 150, 0, 0)
                .width(Dim(120.0f))
                .height(Dim(300.0f))
                .writingMode(sigil::weave::WritingMode::kVerticalRL)
                .spanPaint(
                    sel::text(u8"右から左へ"),
                    bs::banded(bs::kAi,
                               sigil::weave::Decoration::Kind::kUnderline,
                               bs::kAka, 2.0f))
                .fx({.effect = fx::rise(18),
                     .stagger = {.eachMs = 210,
                                 .durationMs = 520,
                                 .over = unit::Line},
                     .progress = animate(from(0.0f).to(1.0f),
                                         {1400ms, &ch::easeNone, 220ms})}))
        .child(text(toU8("\xe2\x86\x91 this strip's entrance beats over\n"
                         "unit::Line \xe2\x80\x94 one COLUMN a beat,\n"
                         "and its band stands at rest"),
                    bs::label(10, bs::kUsu))
                   .absolute()
                   .inset(300, 466, 0, 0)
                   .width(Dim(180.0f)))
        .child(
            text(toU8("underline \xe2\x86\x92 right of the column  \xc2\xb7  "
                      "overline \xe2\x86\x92 left  \xc2\xb7  highlight "
                      "\xe2\x86\x92 the whole pitch  \xc2\xb7  the entrance "
                      "beats over COLUMNS"),
                 bs::label(12, bs::kUsu))
                .absolute()
                .inset(64, bs::kH - 44, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(Bousen, "bousen", "Catalog \xc2\xb7 Type & grid",
                "vertical columns \xe2\x80\x94 sidelines, alternates, marks")
