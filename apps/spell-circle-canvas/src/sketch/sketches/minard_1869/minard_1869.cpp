// minard 1869: scene assembly and animation.

// TAGS: Data/Charts

#include "Minard1869.h"

auto Minard1869::describe(sketch::SketchContext& ctx) -> Element {
  // The desk's sheet stands for everything described below it, so a kit
  // component four levels down is set in this plate's registers without
  // being handed them.
  sketch::kit::Provide look(sheetLook);
  return box()
      .fill(Paint::solid(kDesk))
      .child(titleStrip())
      .child(sheet(ctx))
      .child(auditColumn())
      .child(consoleStrip());
}

auto Minard1869::setup(sketch::SketchContext& ctx) -> void {
  plate = readSheet(ctx.assets);
  ctx.canvas(kW, kH);
  ctx.background(kDesk);
  fonts = ctx.fonts;
  // A study brings its own canvas, background and captured moment rather
  // than inheriting them: at 20.0 s both bands, the temperature graph and
  // the geography card are complete and the caliper is lit. 29.0 is the
  // quiescent alternative — every beat settled, nothing still moving.
  ctx.captureAt(20.0);

  // The plate's lettering is FOUR systems and they are genuinely
  // different: a looping engraver's ronde for the titles and the legend
  // paragraphs, an upright condensed face for the numbers across the
  // zones, a fine sloped italic for the place names, and spaced roman
  // capitals for MOSCOU alone.
  // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
  // library's own walk: the first installed family wins, and a machine
  // with none of them gets the default face AT THE WEIGHT ASKED FOR
  // rather than silently at Normal.
  faceScript = weave::ports::face({"Snell Roundhand", "Apple Chancery"});
  faceItalic = weave::ports::face({"Baskerville", "Times New Roman"},
                                  SkFontStyle::Italic());
  faceRoman = weave::ports::face({"Baskerville", "Times New Roman"});
  faceNum = weave::ports::face({"Baskerville"});
  faceUi = weave::ports::face({"Helvetica Neue", "Baskerville"});
  faceUiBold = weave::ports::face({"Helvetica Neue", "Baskerville"},
                                  SkFontStyle::kBold_Weight);
  faceMono = sketch::kit::houseFace(sketch::kit::Voice::Terminal);

  // THE DESK'S OWN SHEET, once the faces exist to name it with. Every kit
  // component reads the theme in scope, and this plate is a lithograph on
  // a dark table rather than the house sheet, so it binds its own: the
  // interface face the commentary is set in, the plate's three inks, and
  // the air the masthead's lines stand apart by.
  sheetLook = {};
  sheetLook.palette.ground = kDesk;
  sheetLook.palette.ink = hexColor(0xe3dccd);
  sheetLook.palette.ash = hexColor(0x9a9285);
  sheetLook.palette.rule = hexColor(0x2c2a26);
  sheetLook.palette.figure = hexColor(0xe3dccd);
  sheetLook.type.sans = faceUi;
  sheetLook.type.mono = faceMono;
  sheetLook.type.title = {20, 0};
  sheetLook.type.subtitle = {12, 0};
  sheetLook.type.captionNote = {11, 0};
  sheetLook.spacing.subtitleGap = 6;
  sheetLook.spacing.rowGap = 5;

  // THE PAPER, and it is a FIBRE problem, not a colour problem: pulp
  // grain, the laid lines of a hand-made 19th-century sheet at ~1.2 px
  // pitch, the chain lines at ~26 px, and foxing.
  paperPulp = patterns::speckle(160, 220, 0.35f, 0.9f,
                                {skia::toColor(hexColor(0xb9ad98, 0.20f)),
                                 skia::toColor(hexColor(0xd6cab6, 0.18f))});
  paperPulp.seed(1869);
  laidLines =
      patterns::stripes(0.6f, 0.7f, skia::toColor(hexColor(0xb9ad98, 0.10f)));
  laidLines.rotate(90.0f);
  chainLines =
      patterns::stripes(1.1f, 25.0f, skia::toColor(hexColor(0xb9ad98, 0.13f)));
  chainLines.rotate(90.0f);
  foxing = patterns::speckle(190, 5, 1.6f, 6.0f,
                             {skia::toColor(hexColor(0xa07f55, 0.10f))});
  foxing.seed(91);
  tintSpeckle = patterns::speckle(64, 40, 0.6f, 2.4f,
                                  {skia::toColor(hexColor(0x8f6a55, 0.5f))});
  tintSpeckle.seed(41);

  paperMat = Paint::blend({
      {Paint::solid(kPaperBody), SkBlendMode::kSrc},
      {Paint::recipe(field::grain(0.34f, 2, 7.0f, 0.40f)),
       SkBlendMode::kSoftLight},
      {laidLines.material(), SkBlendMode::kSrcOver},
      {chainLines.material(), SkBlendMode::kSrcOver},
      {paperPulp.material(), SkBlendMode::kSrcOver},
      {foxing.material(), SkBlendMode::kSrcOver},
  });
  // lying under a window: the vignette centre sits slightly ABOVE middle
  vignette = Paint::radialUnit({0.46f, 0.40f}, 1.10f,
                               {{0.0f, hexColor(0xffffff)},
                                {0.70f, hexColor(0xf6f1e6)},
                                {1.0f, hexColor(0xc4b9a4)}});

  runAudits(ctx);

  // ONE Output drives every beat, looping at tLoop. Each beat is
  // bind().window(lo, hi), never from(): outside a window from() feeds
  // the easing curve values outside its domain and ease:: is not total.
  ctx.ticker.add([this, &ticker = ctx.ticker](double) {
    const double t = ticker.elapsed();
    const float s = (float)std::fmod(t, (double)tLoop);
    T = s;
    // the 12.6%: every band morphs from engraved to STATED and back,
    // twice, and it should be unmistakable that the reproduction is
    // lying while it does.
    if (s >= tMorph && s <= tMorphEnd) {
      const float u = (s - tMorph) / (tMorphEnd - tMorph);
      const float w = 0.5f - 0.5f * std::cos(u * 4.0f * kPi);
      mmScale = kMmPer10k + (kStatedMmPer10k - kMmPer10k) * w;
    } else {
      mmScale = kMmPer10k;
    }
    // the sheet dims while the audit argues, and comes back after
    float d = 0.0f;
    if (s > tScale && s < tGeo) {
      d = std::min(1.0f, (s - tScale) / 0.6f);
      d = std::min(d, (tGeo - s) / 0.6f);
    }
    dimAmt = 0.42f * std::clamp(d, 0.0f, 1.0f);
    calAlpha = std::clamp(
        std::min((s - tScale - 0.2f) / 0.3f, (28.6f - s) / 0.9f), 0.0f, 1.0f);
    return true;
  });
  // The caliper walks in CLICKS: one spot reading every 2/3 s on its own
  // fixed-rate lane, independent of the frame rate.
  ctx.ticker.addFixed(
      1.5f,
      [this]() {
        if (T.value() >= tScale && T.value() <= tLigneEnd) ++calStep;
        return true;
      },
      8);

  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("caliper", caliper());
  calShown = calStep;
}

auto Minard1869::update(double, sketch::SketchContext& ctx) -> void {
  if (calStep != calShown) {
    calShown = calStep;
    ctx.composer.renderSlot("caliper", caliper());
  }
}

SIGIL_SKETCH(
    Minard1869, "Study \xc2\xb7 Science",
    "Minard's BnF presentation copy \xe2\x80\x94 the plate audited against "
    "its own printed legend")
