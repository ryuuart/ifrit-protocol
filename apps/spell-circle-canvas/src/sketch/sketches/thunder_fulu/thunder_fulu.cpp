// thunder fulu: scene assembly and animation.

// TAGS: Typography/Lettering, Drawing/Brushes

#include "ThunderFulu.h"

auto ThunderFulu::describe(sketch::SketchContext&) -> Element {
  // The plate's type: the terminal face wherever a leaf names none, and
  // three named voices — a section heading in the display face and
  // gold, the italic gloss, and the chant a line is sung in.
  sigil::compose::StyleSheet classes{
      sigil::compose::rule(".heading")
          .font({.face = faceDisplay,
                 .size = 11.5f,
                 .color = kGold,
                 .track = 1.1f}),
      sigil::compose::rule(".gloss").font(
          {.face = faceItalic, .size = 10.5f, .color = hexColor(0x7d6f52)}),
      sigil::compose::rule(".chant").font(
          {.face = faceItalic, .size = 11.0f, .color = kChalk}),
      sigil::compose::rule(".lands").font({.color = hexColor(0xe07a52)}),
      sigil::compose::rule(".lawMark")
          .font({.size = 8.5f, .color = hexColor(0xa89778)}),
      sigil::compose::rule(".plotRule")
          .font({.color = hexColor(0x8b7f66, 0.5f)}),
      sigil::compose::rule(".plotTrace")
          .font({.color = hexColor(0xe6d7ae, 0.95f)}),
      sigil::compose::rule(".ladder").font(
          {.size = 8.5f, .color = hexColor(0x8b7644)}),
      sigil::compose::rule(".station")
          .font({.size = 9.5f, .color = hexColor(0xa48c5c, 0.9f)}),
      sigil::compose::rule(".bayer").font({.face = faceItalic,
                                           .size = 9.0f,
                                           .color = hexColor(0x6f6047, 0.85f)}),
      sigil::compose::rule(".miniName")
          .font({.size = 8.0f, .color = hexColor(0xa89264, 0.95f)}),
      sigil::compose::rule(".miniGloss")
          .font({.face = faceItalic,
                 .size = 8.0f,
                 .color = hexColor(0x776953, 0.9f)}),
      sigil::compose::rule(".register")
          .font({.size = 8.5f, .color = hexColor(0x0b0a09, 0.60f)}),
      sigil::compose::rule(".colophon")
          .font(
              {.face = faceItalic, .size = 10.0f, .color = hexColor(0x8d7f60)}),
      sigil::compose::rule(".note").font(
          {.size = 9.5f, .color = hexColor(0x5d5341)})};
  return box()
      .inset(0)
      .font({.face = faceMono})
      .applyStyleSheet(classes)
      .children({plate(), tread(), marginColumn(), chantPanel(), tempoPanel(),
                 consolePanel(), furniture()});
}

auto ThunderFulu::doc() const -> const data::Json& {
  static const data::Json none;
  return words ? *words : none;
}

auto ThunderFulu::setup(sketch::SketchContext& ctx) -> void {
  font = readFont(ctx);
  words = ctx.assets.json(ctx.local("data/content.json"));

  // The single frame this sketch is photographed at, chosen on the 27 s
  // score: everything through the 19.65 s tap is complete and the foot is
  // about half way through its flying-white sweep, so one still shows both
  // a finished talisman and a stroke being written. Anything before ~8 s
  // catches an almost blank plate; 26.0 gives the fully settled plate with
  // nothing in motion.
  sketch::kit::stage(
      ctx,
      {.size = SkSize::Make(kW, kH), .captureAt = 20.6, .background = kNight});

  // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
  // library's own walk: the first installed family wins, and a machine
  // with none of them gets the default face AT THE WEIGHT ASKED FOR
  // rather than silently at Normal. The three runs the house sheets share
  // are named by VOICE rather than spelled again here, so two plates
  // cannot end up in different faces by a typo.
  faceSerif = sketch::kit::houseFace(sketch::kit::Voice::Book);
  faceItalic = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                      SkFontStyle::kItalic_Slant);
  faceMono = sketch::kit::houseFace(sketch::kit::Voice::Terminal);
  faceDisplay =
      weave::ports::face({"Optima", "Baskerville"}, SkFontStyle::kBold_Weight);

  ironGrain = Paint::recipe(field::grain(2.2f, 4, 1356.0f, 0.55f, 2.6f));
  ironSpeck =
      patterns::speckle(420, 26, 0.7f, 2.6f,
                        {hexColor(0x7c7263, 0.10f), hexColor(0x000000, 0.16f)});
  ironSpeck.seed(1220);

  // brush::Scatter / brush::Pattern art: held as MEMBERS. Built inside a
  // describe they would re-bake their tile every frame — the snapshot
  // cache lives in the value.
  footPrint = box()
                  .width(7)
                  .height(11)
                  .shape(shapes::squircle(2.6f))
                  .fill(Fill::color(hexColor(0xb2914f, 0.42f)));
  hammerTile = box().width(22).height(3).fill(Fill::none());
  hammerCorner =
      box()
          .width(19)
          .height(19)
          .shape(keyedShape(std::string_view("hammer-corner"),
                            [](SkSize s) {
                              SkPathBuilder b;
                              const float w = s.width(), h = s.height();
                              b.moveTo(0, h * 0.5f);
                              b.lineTo(w * 0.42f, h * 0.16f);
                              b.lineTo(w, h * 0.44f);
                              b.lineTo(w * 0.58f, h * 0.88f);
                              b.close();
                              return b.detach();
                            }))
          .fill(Fill::color(hexColor(0x6b6355, 0.55f)))
          .stroke(
              PathFormat{.width = 0.9f,
                         .strokeFill = Fill::color(hexColor(0x100f0f, 0.7f))});

  validateClassifier();
  buildStrokes();
  runChecks();

  ctx.ticker.add([this](double dt) {
    clockT += dt;
    scribe = (float)std::fmod(clockT, (double)kLoop);
  });

  ctx.composer.render(describe(ctx));
}

SIGIL_SKETCH(ThunderFulu, "Study · Esoteric",
             "A Thunder-Rite talisman, WRITTEN — real stroke medians, "
             "and the foot at 7.1× the body's tempo")

// ---------------------------------------------------------------------------
// WHY A WHOLE WRITTEN PERFORMANCE STAYS CHEAP.
//
// 86 marks in 49 nodes (the foot's 38 strokes are one node and one contour),
// every one carrying a Ribbon with a width Profile, and every one bound to
// the SAME Output through `window(t0, t1)`. That last part is what does it: a
// stroke whose window has not opened trims to nothing, and a stroke whose
// window has closed trims to a constant, so on any given frame almost every
// node is a settled fill that the library can keep in a cache and only the
// one or two strokes actually being written are live.
//
// There is no re-describe anywhere in the score. `update()` is empty and the
// whole 27 s performance runs off one `ch::Output<float>` stepped in the
// ticker. Each node is also sized to its OWN bounds rather than to the plate;
// 49 full-plate boxes would move the cost out of the ink and into culling and
// layer composites, since a node's box is what those two are charged for.
//
// Two shapes to know before trying them:
//   1. `cacheScale(0.5f)` on the full-plate grain wash trades a half-area
//      bake for an upscale resample at every composite, and a large soft
//      wash is already the cheapest thing a texture bake can hold.
//   2. removing the plate group's BOUND OPACITY would remove a full-plate
//      saveLayer every frame, but a layer costs what the subtree under it
//      costs to replay, and this subtree is already two texture bakes and a
//      pile of small cached fills. The plate's arrival is part of the score,
//      so the binding stays.
// The one node that genuinely costs is the grain wash: an opacity +
// kSoftLight leaf the library will not promote on its own (see ironWash), so
// its bake is asked for by name.
