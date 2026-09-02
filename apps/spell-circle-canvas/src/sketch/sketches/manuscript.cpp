/** @file
 * manuscript — an illuminated page in the Florentine humanist manner:
 * a bianchi girari margin, a versal on a cobalt ground, and a text that
 * flows around both.
 */

// THE BOOK THIS IS SET AFTER
//
// The bianchi girari ("white vine-stem") book of the Florentine humanist
// workshops, c. 1450–1480 — Vespasiano da Bisticci's among them: white
// interlaced vine-stems reserved out of a blue, green and vermilion
// ground, running the margin and knotting at the corners; a gold versal
// on a cobalt panel; a vermilion incipit above the text; and the body in
// HUMANIST MINUSCULE, the hand those scribes cut from Carolingian models
// and the hand every roman typeface since is descended from.
//
// That descent is why the face here is a humanist old-style rather than
// a blackletter: the page is a fifteenth-century Florentine book and not
// a thirteenth-century northern one, and the letterform the scribes used
// is the one this line of type still carries. A grotesque is the one
// thing it cannot be — a book hand is the first thing a reader of an
// illuminated page sees.
//
// A TRUE DROP CAP, and where its geometry comes from: there is no
// dedicated drop-cap facility. The verse's first grapheme is split off
// as its own element and the remainder is a paragraph that FLOWS AROUND
// it, through the same exclusion machinery the sprigs and the rubric
// panel use — so the initial's box is the only thing that decides where
// the first lines start, and moving it re-runs the flow.
//
// EDIT THESE FIRST
//   kBookFaces  — the humanist old-style the body is set in, in order of
//                 preference; the first one installed wins.
//   kTurnSecs   — how long a verse holds before the page turns.
//   azurePalette() / crimsonPalette() — the page's two colour sets, one
//                 for the margin and versal and one for the rubric.
//   the flowAround margins below — the white a line keeps from each
//                 exclusion it meets.

#include <include/core/SkMaskFilter.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

#include <initializer_list>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;

namespace {
/** The humanist old-styles this page is set in, best first. Every one of
 *  them descends from the minuscule the Florentine scribes wrote, which
 *  is the whole reason a roman face is right here at all. */
constexpr std::initializer_list<const char*> kBookFaces = {
    "Hoefler Text", "Palatino", "Baskerville", "Iowan Old Style",
    "Times New Roman"};

/// How long one verse holds before the page turns.
constexpr double kTurnSecs = 7.0;

/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

struct Manuscript final : sketch::Sketch {
  int verse = 0;
  double nextTurn = 0.0;

  // The page turns between two verses on a cycle, so the still names its
  // moment. This sits mid-hold on verse 0, which is the incipit the fixed
  // rubric in the margin announces — the only verse the rest of the page
  // agrees with.

  static constexpr const char8_t* kVerses[2] = {
      u8"Here begins the book of the ember gate, set down in the year "
      u8"of the long tide by the wardens of the flooded causeway. Let "
      u8"every reader carry the coal with a steady hand, for the water "
      u8"remembers what the fire forgets, and the reeds keep time "
      u8"against the slow current where the ferry rope hums. The old "
      u8"paths bend around the salt gardens and meet again beneath the "
      u8"bell, where the keepers pour the last light into copper bowls "
      u8"and wait for the morning wind to carry the ash home. Twelve "
      u8"nights the vine was fed, twelve nights the lanterns answered, "
      u8"and on the last the gate stood open wide enough for one small "
      u8"boat, one warden, and the weight of everything they chose to "
      u8"leave behind on the far shore of the salt gardens. Of the "
      u8"twelve, four were kept by the ferry and four by the bell, and "
      u8"the last four were carried down to the water line and set in "
      u8"the reeds, where the tide could read them. It is written that "
      u8"the wardens did not speak on the crossing, and that the rope "
      u8"was warm under their hands from the hands before them. Whoever "
      u8"comes after, let them find the margin kept, the coal banked, "
      u8"and the gate answering to a steady hand alone.",
      u8"In the second season the wardens counted the lanterns twice, "
      u8"once for the living channel and once for the drowned road "
      u8"beneath it. The gate takes no coin but memory, says the "
      u8"rubric, and gives back the shape of every hand that held the "
      u8"rope. Draw the circle wide, feed the vine its silver hour, "
      u8"and the causeway will hold one crossing more. So it is "
      u8"written at the water line, and so the tide reads it back to "
      u8"us each night. Keep the margin wide for the vine, keep the "
      u8"corner free for the fox, and let no page close on a coal "
      u8"still warm; the book is a causeway too, and every reader "
      u8"crosses it holding somebody's light. Here ends the second "
      u8"verse of the ember gate, and the water keeps the rest. What "
      u8"the second season took, the third gave back doubled, and the "
      u8"keepers wrote it down twice for fear of the tide. Read slowly, "
      u8"and keep the lantern low; the ink is young and the water is "
      u8"patient, and a page turned in haste is a crossing missed."};

  sk_sp<SkTypeface> book;

  Element describe() {
    const Palette pal = azurePalette();
    const Palette rubric = crimsonPalette();

    // A TRUE drop cap: the verse's first grapheme becomes the illuminated
    // initial and the body text is the REMAINDER, with the paragraph flowing
    // around the initial through the derive phase. There is no dedicated
    // drop-cap facility; the geometry comes from SigilWeave's exclusion flow,
    // which is why the split is done here on the string.
    const std::u8string letter(1, kVerses[verse][0]);
    const std::u8string body(kVerses[verse] + 1);

    PathFormat goldDash;
    goldDash.width = 1.3f;
    goldDash.strokeFill = Fill::color(pal.gold);
    goldDash.dashIntervals = {16, 8};

    // The scrollwork border: eight half-edge bands (each corner sends
    // a flourish along both adjacent edges), facing spiral eyes near
    // every edge midpoint.
    auto band = [&](int quadrant, bool vertical, float l, float t, float w,
                    float h) {
      return box()
          .width(w)
          .height(h)
          .inset(l, t, kSceneSize.width() - l - w, kSceneSize.height() - t - h)
          .zIndex(2)
          .child(custom(edgeFlourish(pal, quadrant, vertical)).inset(0));
    };
    // A keyed sprig the body text flows around.
    auto weaveSprig = [&](const char* key, float l, float t, float rot) {
      return box()
          .key(key)
          .width(54)
          .height(64)
          .inset(l, t, kSceneSize.width() - l - 54,
                 kSceneSize.height() - t - 64)
          .zIndex(2)
          .rotate(rot)
          .child(custom(sprig(pal)).inset(0));
    };

    // Everything static lives in one texture-baked stack. The page is dense
    // — noise fills, hundreds of vine stamps, text flowed around exclusions —
    // so replaying it as a picture would re-rasterize all of that every
    // frame; baked, the per-frame cost is one image blit. The bake is dropped
    // and retaken only on a verse turn.
    auto pageStack =
        stack()
            .inset(0)
            .cache(Cache::Texture)
            // The page: parchment ground, stem-colored rule, vine border.
            .child(box()
                       .inset(26, 22, 26, 22)
                       .corners({6})
                       .background(
                           sigil::compose::shadow({0, 0, 0, 0.55f}, {5, 7}, 16))
                       .fill(parchmentFill(pal.parchment))
                       .foreground(
                           sigil::compose::stroke(2.2f, Fill::color(pal.stem)))
                       // Inner gilded dashed rule (the broken hairline the
                       // corner flourishes dance around).
                       .child(box().inset(14).foreground(goldDash)))
            // Title rubric line.
            .child(text(u8"INCIPIT LIBER PORTAE CINERUM",
                        type({.face = book, .size = 24, .color = rubric.stem}))
                       .inset(200, 88, 200, kSceneSize.height() - 126)
                       .zIndex(1))
            // The illuminated initial: first grapheme on a cobalt block
            // with gilded trim (the watercolor-manuscript treatment).
            .child(box()
                       .key("dropcap")
                       .width(92)
                       .height(98)
                       .inset(84, 146, kSceneSize.width() - 84 - 92,
                              kSceneSize.height() - 146 - 98)
                       .zIndex(3)
                       // A versal is a PANEL: a square field of colour
                       // with a gold letter reserved in it and a gold
                       // fillet round the edge. The rounded corners and
                       // the dashed inner rule it carried are a modern
                       // callout's furniture and read as one.
                       .background(
                           sigil::compose::shadow({0, 0, 0, 0.35f}, {2, 3}, 7))
                       .fill(Fill::color(pal.stem))
                       .foreground(
                           sigil::compose::stroke(2.2f, Fill::color(pal.gold)))
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .child(text(letter, type({.face = book,
                                                 .size = 66,
                                                 .color = pal.gold}))))
            // Rubric side panel: the same component, crimson palette.
            .child(
                illuminatedPanel(rubric)
                    .key("rubric")
                    .width(200)
                    .height(148)
                    .inset(600, 270, kSceneSize.width() - 600 - 200,
                           kSceneSize.height() - 270 - 148)
                    .zIndex(3)
                    .padding(18)
                    .gap(8)
                    .child(text(
                        u8"nota bene",
                        type({.face = book, .size = 15, .color = rubric.stem})))
                    .child(text(
                        u8"the gate takes no coin but memory",
                        type({.face = book, .size = 16, .color = rubric.ink}))))
            // Body text weaving between drop cap, rubric, and all four
            // corner flourishes.
            .child(box()
                       .inset(100, 132, 100, 82)
                       .child(text(body, type({.face = book,
                                               .size = 19.5f,
                                               .color = pal.ink}))
                                  .key("body")
                                  .flowAround("dropcap", 14)
                                  .flowAround("rubric", 14)
                                  .flowAround("sprigL", 10)
                                  .flowAround("sprigR", 10)
                                  .flowAround("sprigB", 10))
                       .zIndex(1))
            .child(band(0, false, 40, 34, 400, 52))
            .child(band(1, false, 460, 34, 400, 52))
            .child(band(3, false, 40, 554, 400, 52))
            .child(band(2, false, 460, 554, 400, 52))
            .child(band(0, true, 34, 40, 52, 260))
            .child(band(3, true, 34, 340, 52, 260))
            .child(band(1, true, 814, 40, 52, 260))
            .child(band(2, true, 814, 340, 52, 260))
            // Keyed sprigs the body text weaves around (with the drop cap
            // and rubric, the multi-exclusion flow demo).
            // Seated against the foot of the versal. Left lower, it opened
            // exactly one line of full measure between the two
            // exclusions, and a single line jutting five ems past its
            // neighbours reads as a broken column rather than as a flow.
            .child(weaveSprig("sprigL", 96, 246, 90.0f))
            .child(weaveSprig("sprigR", 748, 210, -90.0f))
            .child(weaveSprig("sprigB", 424, 486, 0.0f));

    return stack()
        .fill(Fill::color({0.11f, 0.09f, 0.075f, 1}))  // scriptorium desk
        .child(std::move(pageStack))
        // Drifting gold dust: the only per-frame repaint.
        .child(
            custom([](SkCanvas& c, const PaintContext& ctx) {
              SkPaint p;
              p.setAntiAlias(true);
              const double t = ctx.elapsedSeconds;
              for (int i = 0; i < 26; ++i) {
                const float fx = (float)i * 137.5f;
                const float x = std::fmod(
                    fx + (float)t * (6.0f + (float)(i % 5)), ctx.size.width());
                const float y = 60.0f + std::fmod(fx * 0.61f, 500.0f) +
                                12.0f * std::sin((float)t * 0.8f + (float)i);
                const float a =
                    0.10f + 0.16f * (0.5f + 0.5f * std::sin((float)t * 1.7f +
                                                            (float)i * 2.1f));
                p.setColor4f({0.9f, 0.75f, 0.3f, a}, nullptr);
                c.drawCircle(x, y, 1.6f + (float)(i % 3) * 0.7f, p);
              }
            })
                .inset(0)
                .zIndex(4)
                .cache(Cache::None));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(3.5);
    Composer& composer = ctx.composer;
    book = pickFace(kBookFaces, 400);
    verse = 0;
    nextTurn = kTurnSecs;
    composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    if (elapsed < nextTurn) return;
    nextTurn = elapsed + kTurnSecs;
    verse = (verse + 1) % 2;
    composer.render(describe());  // reflow weaves through the exclusions
  }
};

}  // namespace

SIGIL_SKETCH_AS(Manuscript, "manuscript", "Catalog \xc2\xb7 Type & grid",
                "text flowing around the flourishes, and a true drop cap")
