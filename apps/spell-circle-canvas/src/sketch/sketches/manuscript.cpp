/** @file
 * manuscript — one named codex's mise-en-page, reconstructed: the leaf's
 * own rectangle, a written space ruled on it, a versal dropped six lines
 * into the text, and a bianchi girari frieze in the margins.
 */

// THE CODEX. Firenze, Biblioteca Medicea Laurenziana, Plut. 63.10 — Livy's
// first decade, written at Florence in 1458 by PIERO STROZZI for Piero de'
// Medici through the stationer Vespasiano da Bisticci, and illuminated by
// Filippo di Matteo Torelli. Its leaf is 259 x 360 mm. Its preface page
// carries a frieze of BIANCHI GIRARI — white interlaced vine-stems
// reserved out of a blue ground, with green and pink lacunae between the
// stems, gilded bezants and floral sprays over them — and the title above
// the preface is set in gold capitals inside a rectangular frame of
// lozenges drawn in perspective.
//
// WHAT IS THE CODEX'S AND WHAT IS THIS PAGE'S. The shelfmark, the hand,
// the date, the leaf's 259 x 360 mm and the decoration above are the
// codex's, and this page is drawn to them: the CANVAS IS THE LEAF, three
// canvas pixels to the millimetre, so every number below is a millimetre
// of the real object.
//
// The written space and the line count are NOT published within reach, so
// they are a reconstruction and are stated as one: the block is ruled on
// the NINTHS CANON the Florentine humanist folio takes — one ninth of the
// leaf at the spine and at the head, two ninths at the fore-edge and at
// the foot — which leaves a written space of 172.7 x 240.0 mm, and it is
// ruled to 38 long lines, a 6.32 mm pitch. Both fall where a Livy of this
// format falls; neither is a measurement of Plut. 63.10.
//
// THE VERSAL. `kit::dropCap` is the drop cap: an initial keyed and placed,
// and a body that FLOWS AROUND it — the same exclusion the marginal note
// and the vine sprig get, resolved in the same pass. Its depth is its own
// type's size and nothing else, which is why six lines is spelled here as
// six times the pitch. `kit::NestedStyle` carries the paragraph out of the
// initial: the opening words run on in the rubricator's red small capitals
// through the first full stop, stated as a DELIMITER so an edit to the
// copy moves the run with it.
//
// The body is set in a humanist old-style, because the hand of the codex
// is HUMANIST MINUSCULE — the letter the Florentine scribes cut from
// Carolingian models, and the letter every roman face since descends from.
// A blackletter would be a northern book two centuries earlier.
//
// EDIT THESE FIRST
//   kLeafW / kLeafH — the codex's leaf in millimetres. Every distance on
//                     the page is a fraction of these two.
//   kLines          — how many lines the block is ruled to; the pitch, the
//                     body size and the versal's depth all follow it.
//   kCapLines       — how deep the versal is dropped, in lines.
//   kTurnSecs       — how long a page holds before it is turned.

#include <include/core/SkMaskFilter.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Features.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;

namespace {

const weave::kit::PatternHyphenator& hyphenator() {
  static const weave::kit::PatternHyphenator table(
      "en", weave::kit::englishHyphenationPatterns());
  return table;
}

/** The humanist old-styles this page is set in, best first. Every one of
 *  them descends from the minuscule the Florentine scribes wrote, which is
 *  the whole reason a roman face is right here at all. */
constexpr std::initializer_list<const char*> kBookFaces = {
    "Hoefler Text", "Palatino", "Baskerville", "Iowan Old Style",
    "Times New Roman"};

/// How long one page holds before it is turned.
constexpr double kTurnSecs = 7.0;

// ── The leaf, in millimetres, and the canvas that IS it ────────────────
constexpr float kMm = 3.0f;  ///< canvas pixels to the millimetre
constexpr float kLeafW = 259.0f;
constexpr float kLeafH = 360.0f;
constexpr SkSize kSceneSize = {kLeafW * kMm, kLeafH* kMm};

// ── The ninths canon, in millimetres ───────────────────────────────────
constexpr float kSpine = kLeafW / 9.0f;                  // 28.78
constexpr float kHead = kLeafH / 9.0f;                   // 40.00
constexpr float kForeEdge = kLeafW * 2 / 9.0f;           // 57.56
constexpr float kFoot = kLeafH * 2 / 9.0f;               // 80.00
constexpr float kMeasure = kLeafW - kSpine - kForeEdge;  // 172.67
constexpr float kDepth = kLeafH - kHead - kFoot;         // 240.00

/// The ruling: how many lines the block holds, and the pitch that gives.
constexpr int kLines = 38;
constexpr float kPitch = kDepth / (float)kLines;  // 6.316 mm
/// The incipit's own band, in lines, and the versal's depth in lines.
constexpr int kIncipitLines = 3;
constexpr int kCapLines = 6;

/// The body's size: the pitch less the leading a humanist page opens.
constexpr float kBodySize = kPitch / 1.35f;  // 4.68 mm

constexpr float px(float mm) { return mm * kMm; }

/** THE TWO PAGES the codex is opened at. Each is written to run the
 *  block to its foot: a page that stops two thirds of the way down is a
 *  setting abandoned, not a mise-en-page. */
constexpr const char8_t* kPages[2] = {
    u8"Here begins the book of the ember gate, set down in the year of the "
    u8"long tide by the wardens of the flooded causeway. Let every reader "
    u8"carry the coal with a steady hand, for the water remembers what the "
    u8"fire forgets, and the reeds keep time against the slow current where "
    u8"the ferry rope hums. The old paths bend around the salt gardens and "
    u8"meet again beneath the bell, where the keepers pour the last light "
    u8"into copper bowls and wait for the morning wind to carry the ash "
    u8"home. Twelve nights the vine was fed, twelve nights the lanterns "
    u8"answered, and on the last the gate stood open wide enough for one "
    u8"small boat, one warden, and the weight of everything they chose to "
    u8"leave behind on the far shore of the salt gardens. Of the twelve, "
    u8"four were kept by the ferry and four by the bell, and the last four "
    u8"were carried down to the water line and set in the reeds, where the "
    u8"tide could read them. It is written that the wardens did not speak "
    u8"on the crossing, and that the rope was warm under their hands from "
    u8"the hands before them. They counted the pilings twice, once going "
    u8"out and once coming back, and both counts agreed, which the older "
    u8"warden took for a good omen and the younger for nothing at all. "
    u8"Beyond the third piling the water runs black even at noon, and the "
    u8"keepers have never agreed whether that is the depth of it or the "
    u8"weed. On the far bank the reeds stand higher than a man and the "
    u8"path through them is kept open by walking it, which is the only "
    u8"upkeep the causeway has ever had. In the third year a warden of the "
    u8"bell set down the order of the crossing, and it is copied here as she "
    u8"wrote it: the coal first, then the rope, then the count, then the "
    u8"name of whoever is going over, spoken once and not written. She notes "
    u8"that the order matters less than the keeping of it, and that a "
    u8"crossing made out of order has never yet drowned anyone, which she "
    u8"offers as an argument for the order rather than against it. Below "
    u8"the bell the water is shallow enough to stand in and the reeds come "
    u8"up through the stones of the old road, and it is there that the "
    u8"lanterns are lit, out of the wind, before they are carried down. "
    u8"Twice in living memory the tide has come over the causeway at noon, "
    u8"and both times the wardens were on the far bank and both times they "
    u8"waited, which is the whole of what the office asks of anyone. "
    u8"The bell itself was cast inland and carried out on the causeway in "
    u8"pieces, which is why it hangs a little out of true and why the note "
    u8"it gives is the note the wardens listen for and no other bell makes. "
    u8"Whoever comes after, let them find the margin kept, the coal banked, "
    u8"and the gate answering to a steady hand alone.",
    u8"In the second season the wardens counted the lanterns twice, once "
    u8"for the living channel and once for the drowned road beneath it. "
    u8"The gate takes no coin but memory, says the rubric, and gives back "
    u8"the shape of every hand that held the rope. Draw the circle wide, "
    u8"feed the vine its silver hour, and the causeway will hold one "
    u8"crossing more. So it is written at the water line, and so the tide "
    u8"reads it back to us each night. Keep the margin wide for the vine, "
    u8"keep the corner free for the fox, and let no page close on a coal "
    u8"still warm; the book is a causeway too, and every reader crosses it "
    u8"holding somebody's light. What the second season took, the third "
    u8"gave back doubled, and the keepers wrote it down twice for fear of "
    u8"the tide. The first copy went into the chest under the bell and the "
    u8"second was carried inland, which is why the inland copy is the one "
    u8"that survives and why it is the one that is wrong about the "
    u8"lanterns. Read slowly, and keep the lantern low; the ink is young "
    u8"and the water is patient, and a page turned in haste is a crossing "
    u8"missed. The inland copy gives the lantern count as forty and the "
    u8"drowned copy as thirty-six, and the difference is the four the bell "
    u8"keeps, which the inland scribe had never seen and could not have "
    u8"known to add. That is the whole of the error, and it has stood for "
    u8"three seasons because nobody with both copies has ever been in the "
    u8"same room as anybody who could read them. The wardens themselves "
    u8"count by hand each night and write nothing down, which is why the "
    u8"count in the chest is a copy of a copy and the count in the reeds is "
    u8"the tide's. Let the reader take the larger number and light the four "
    u8"anyway; a lantern lit for nothing is a lantern, and the causeway has "
    u8"never once complained of too much light. As for the fox in the "
    u8"corner: no warden has ever seen it and every warden leaves the corner "
    u8"free, which is either a courtesy to an animal or a courtesy to the "
    u8"wardens who came before, and the wardens do not distinguish. The "
    u8"margin is kept for the vine on the same terms. What a page gives up "
    u8"at its edges it keeps at its centre, says the rubric, and the "
    u8"causeway is a page in that respect as in most others. Here ends the "
    u8"second verse "
    u8"of the ember gate, and the water keeps the rest, as it keeps the "
    u8"road, the rope and the names of every warden who ever walked out "
    u8"over it in the dark.",
};

struct Manuscript final : sketch::Sketch {
  int page = 0;
  double nextTurn = 0.0;
  sk_sp<SkTypeface> book;

  // The page turns on a cycle, so the still names its moment. This sits
  // mid-hold on the first page, the one the incipit above the block and
  // the marginal note beside it both belong to.

  weave::TextStyle body(float size, SkColor4f colour) {
    weave::TextStyle style =
        weave::textStyle({.face = book, .size = px(size), .color = colour});
    style.shaping.languageTag = "la";
    return style;
  }

  /** The bianchi girari frieze: eight half-edge bands, each corner
   *  sending a flourish along both of its edges, running the margins the
   *  canon left. The head band is one ninth deep, the foot two, which is
   *  the asymmetry the canon is. */
  Element frieze(const Palette& pal) {
    const auto band = [&](int quadrant, bool vertical, float l, float t,
                          float w, float h) {
      return box()
          .width(px(w))
          .height(px(h))
          .inset(px(l), px(t), kSceneSize.width() - px(l + w),
                 kSceneSize.height() - px(t + h))
          .zIndex(2)
          .child(custom(edgeFlourish(pal, quadrant, vertical)).inset(0));
    };
    const float halfW = kLeafW * 0.5f;
    const float halfH = kLeafH * 0.5f;
    // The bands sit INSIDE the margin they run, standing off the leaf's
    // trimmed edge by a third of the spine margin.
    const float edge = kSpine / 3.0f;
    const float headBand = kHead - edge * 2.0f;
    const float footBand = kFoot * 0.5f;
    const float sideBand = kSpine - edge;
    return stack()
        .inset(0)
        .child(band(0, false, edge, edge, halfW - edge, headBand))
        .child(band(1, false, halfW, edge, halfW - edge, headBand))
        .child(band(3, false, edge, kLeafH - edge - footBand, halfW - edge,
                    footBand))
        .child(band(2, false, halfW, kLeafH - edge - footBand, halfW - edge,
                    footBand))
        .child(
            band(0, true, edge, kHead * 0.6f, sideBand, halfH - kHead * 0.6f))
        .child(band(3, true, edge, halfH, sideBand, halfH - kFoot * 0.6f))
        .child(band(1, true, kLeafW - edge - sideBand, kHead * 0.6f, sideBand,
                    halfH - kHead * 0.6f))
        .child(band(2, true, kLeafW - edge - sideBand, halfH, sideBand,
                    halfH - kFoot * 0.6f));
  }

  /** The title above the preface: gold capitals inside a rectangle of
   *  lozenges drawn in perspective, which is what stands there in the
   *  codex. It occupies the block's first `kIncipitLines` lines. */
  Element incipit(const Palette& pal) {
    PathFormat gilt;
    gilt.width = px(0.55f);
    gilt.strokeFill = Fill::color(pal.gold);
    return box()
        .absolute()
        .left(Dim(px(kSpine)))
        .top(Dim(px(kHead)))
        .width(Dim(px(kMeasure)))
        .height(Dim(px(kPitch * (float)kIncipitLines)))
        .zIndex(1)
        .fill(Fill::color(pal.stem))
        .foreground(gilt)
        .alignItems(Align::Center)
        .justify(Justify::Center)
        // The perspective lozenges: one row of gilded diamonds across the
        // frame, drawn on the frame itself rather than mounted on it.
        .background(SwirlCorners{pal, px(kPitch * 1.4f), px(0.5f)})
        .child(text(u8"T \xc2\xb7 LIVII \xc2\xb7 PATAVINI \xc2\xb7 AB \xc2\xb7 "
                    u8"VRBE \xc2\xb7 CONDITA \xc2\xb7 LIBER \xc2\xb7 PRIMVS",
                    weave::textStyle({.face = book,
                                      .size = px(kBodySize * 0.86f),
                                      .color = pal.gold,
                                      .track = px(0.5f)})));
  }

  Element describe() {
    const Palette pal = azurePalette();
    const Palette rubric = crimsonPalette();

    // THE VERSAL, six lines deep, and the paragraph it opens. The initial
    // is one grapheme in its own type; the body is the remainder, flowing
    // around the box that type occupies. The nested run carries the
    // paragraph out of the initial in the rubricator's small capitals,
    // stated as a delimiter so an edit moves it.
    const std::u8string letter(1, kPages[page][0]);
    const std::u8string rest(kPages[page] + 1);
    weave::TextStyle capitals = body(kBodySize * 0.92f, rubric.stem);
    capitals.shaping.fontFeatures = {weave::features::smallCaps,
                                     weave::features::capitalsToSmallCaps};

    auto [initial, prose] = kit::dropCap(
        letter,
        weave::textStyle({.face = book,
                          .size = px(kPitch * (float)kCapLines * 0.74f),
                          .color = pal.gold}),
        rest, body(kBodySize, pal.ink), "versal", px(2.4f),
        kit::NestedStyle{.until = kit::NestedStyle::Until::Delimiter,
                         .delimiter = u8".",
                         .style = capitals});

    // The versal is a PANEL: a square field of cobalt with the letter
    // reserved in gold and a gold fillet round it. Six lines deep by
    // construction — the box is six times the pitch.
    PathFormat fillet;
    fillet.width = px(0.7f);
    fillet.strokeFill = Fill::color(pal.gold);
    initial.width(Dim(px(kPitch * (float)kCapLines)))
        .height(Dim(px(kPitch * (float)kCapLines)))
        .fill(Fill::color(pal.stem))
        .foreground(fillet)
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .zIndex(3);

    weave::ParagraphStyle block;
    block.leading = weave::Leading::absolute(px(kPitch));
    block.alignment = weave::TextAlignment::kJustify;

    // The block: the written space the canon ruled, less the incipit's own
    // band at its head.
    const float blockTop = kHead + kPitch * (float)kIncipitLines;
    Element written =
        box()
            .absolute()
            .left(Dim(px(kSpine)))
            .top(Dim(px(blockTop)))
            .width(Dim(px(kMeasure)))
            .height(Dim(px(kDepth - kPitch * (float)kIncipitLines)))
            .zIndex(1)
            .child(std::move(initial))
            .child(prose.key("block")
                       .width(Dim(px(kMeasure)))
                       .paragraph(block)
                       .lineBreak(weave::LineBreakStrategy::kKnuthPlass)
                       .hyphenation({.patterns = &hyphenator()})
                       .flowAround("note", px(3.0f))
                       .flowAround("sprig", px(2.4f)));

    // The marginal note the fore-edge margin is for, reaching a little
    // into the block so the text parts around it.
    Element note =
        illuminatedPanel(rubric)
            .key("note")
            .absolute()
            .left(Dim(px(kMeasure - kForeEdge * 0.30f)))
            .top(Dim(px(kPitch * 12.0f)))
            .width(Dim(px(kForeEdge * 0.78f)))
            .height(Dim(px(kPitch * 6.0f)))
            .zIndex(3)
            .padding(px(3.0f))
            .gap(px(1.6f))
            .child(text(u8"nota bene", body(kBodySize * 0.82f, rubric.stem)))
            .child(text(u8"the gate takes no coin but memory",
                        body(kBodySize * 0.78f, rubric.ink)));
    written.child(std::move(note));

    // One vine stem breaking out of the frieze into the block, which is
    // what a bianchi girari border does when it will not stay in the
    // margin. The text parts around it like any other exclusion.
    written.child(box()
                      .key("sprig")
                      .absolute()
                      .left(Dim(px(-kSpine * 0.2f)))
                      .top(Dim(px(kPitch * 22.0f)))
                      .width(Dim(px(kSpine * 0.9f)))
                      .height(Dim(px(kPitch * 5.0f)))
                      .zIndex(3)
                      .rotate(90.0f)
                      .child(custom(sprig(pal)).inset(0)));

    // Everything static lives in one texture-baked stack: the page is
    // dense — a noise ground, hundreds of vine stamps, prose flowed around
    // three exclusions — so replaying it as a picture would re-rasterize
    // all of that every frame. Baked, a frame costs one blit, and the bake
    // is dropped only when the page turns.
    PathFormat rule;
    rule.width = px(0.4f);
    rule.strokeFill =
        Fill::color({pal.gold.fR, pal.gold.fG, pal.gold.fB, 0.45f});
    return stack()
        .inset(0)
        .cache(Cache::Texture)
        .fill(parchmentFill(pal.parchment))
        // The pricking-and-ruling the block was written to, kept faint the
        // way a scribe's frame ruling is.
        .child(box()
                   .absolute()
                   .left(Dim(px(kSpine)))
                   .top(Dim(px(kHead)))
                   .width(Dim(px(kMeasure)))
                   .height(Dim(px(kDepth)))
                   .foreground(rule))
        .child(frieze(pal))
        .child(incipit(pal))
        .child(std::move(written));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.background({0.11f, 0.09f, 0.075f, 1});
    ctx.captureAt(3.5);
    book = weave::ports::pickTypeface(kBookFaces, 400);
    page = 0;
    nextTurn = kTurnSecs;
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    if (elapsed < nextTurn) return;
    nextTurn = elapsed + kTurnSecs;
    page = (page + 1) % 2;
    ctx.composer.render(describe());  // the flow re-runs on the new text
  }
};

}  // namespace

SIGIL_SKETCH_AS(Manuscript, "manuscript", "Catalog \xc2\xb7 Type",
                "Laur. Plut. 63.10's leaf, ruled \xe2\x80\x94 a versal six "
                "lines deep and a vine in the margin")
