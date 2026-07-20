#pragma once
// Ornament showcase scenes: an illuminated manuscript page (procedural
// vine borders, corner flourishes the text weaves between, drop cap,
// parameterized rubric panel) and a nine-slice hall (carved frame
// textures generated on intermediate canvases, stretched over panels
// of every size — one panel breathing live).

#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <include/core/SkMaskFilter.h>

namespace compose_gallery {

// ---- Illuminated manuscript ----------------------------------------------

struct ManuscriptScene final : Scene {
  int verse = 0;
  double nextTurn = 0.0;

  const char *name() const override { return "manuscript"; }

  static constexpr const char8_t *kVerses[2] = {
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
      u8"leave behind on the far shore of the salt gardens.",
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
      u8"verse of the ember gate, and the water keeps the rest."};

  Element describe() {
    const Palette pal = azurePalette();
    const Palette rubric = crimsonPalette();

    // A TRUE drop cap: the verse's first grapheme becomes the
    // illuminated initial, and the body text is the REMAINDER — the
    // paragraph flows around the initial via the derive phase.
    // (SigilWeave's ExclusionFlow provides the geometry; a first-class
    // N-line initial in ParagraphLayoutOptions is the eventual home.)
    const std::u8string letter(1, kVerses[verse][0]);
    const std::u8string body(kVerses[verse] + 1);

    PathFormat goldDash;
    goldDash.width = 1.3f;
    goldDash.strokeFill = Fill::color(pal.gold);
    goldDash.dashIntervals = {16, 8};

    // The scrollwork border: eight half-edge bands (each corner sends
    // a flourish along both adjacent edges), facing spiral eyes near
    // every edge midpoint.
    auto band = [&](int quadrant, bool vertical, float l, float t,
                    float w, float h) {
      return box().width(w).height(h)
          .inset(l, t, kSceneSize.width() - l - w,
                 kSceneSize.height() - t - h)
          .absolute().zIndex(2)
          .child(custom(edgeFlourish(pal, quadrant, vertical)).inset(0));
    };
    // A keyed sprig the body text flows around.
    auto weaveSprig = [&](const char *key, float l, float t, float rot) {
      return box().key(key).width(54).height(64)
          .inset(l, t, kSceneSize.width() - l - 54,
                 kSceneSize.height() - t - 64)
          .absolute().zIndex(2).rotate(rot)
          .child(custom(sprig(pal)).inset(0));
    };

    // Everything static lives in one texture-baked stack: the page is
    // dense (noise fills, hundreds of vine stamps, flowed text), so
    // picture replay would re-rasterize ~20ms/frame on raster — baked,
    // the per-frame cost is one image blit. Re-bakes on verse turns.
    auto pageStack = stack().inset(0).absolute().cache(Cache::Texture)
        // The page: parchment ground, stem-colored rule, vine border.
        .child(box().inset(26, 22, 26, 22).absolute().corners({6})
                   .background(sigil::compose::util::shadow(
                       {0, 0, 0, 0.55f}, {5, 7}, 16))
                   .fill(parchmentFill(pal.parchment))
                   .foreground(sigil::compose::util::stroke(
                       2.2f, Fill::color(pal.stem)))
                   // Inner gilded dashed rule (the broken hairline the
                   // corner flourishes dance around).
                   .child(box().inset(14).absolute()
                              .foreground(goldDash)))
        // Title rubric line.
        .child(text(u8"INCIPIT LIBER PORTAE CINERUM",
                    styleAt(24, toColor(rubric.stem)))
                   .absolute()
                   .inset(200, 88, 200, kSceneSize.height() - 126)
                   .zIndex(1))
        // The illuminated initial: first grapheme on a cobalt block
        // with gilded trim (the watercolor-manuscript treatment).
        .child(box().key("dropcap")
                   .width(92).height(98)
                   .inset(84, 146, kSceneSize.width() - 84 - 92,
                          kSceneSize.height() - 146 - 98)
                   .absolute().zIndex(3).corners({8})
                   .background(sigil::compose::util::shadow(
                       {0, 0, 0, 0.35f}, {2, 3}, 7))
                   .fill(Fill::color(pal.stem))
                   .foreground(sigil::compose::util::stroke(
                       1.6f, Fill::color(pal.gold)))
                   .alignItems(Align::Center).justify(Justify::Center)
                   .child(box().inset(5).absolute().foreground(goldDash))
                   .child(text(letter, styleAt(62, toColor(pal.gold)))))
        // Rubric side panel: the same component, crimson palette.
        .child(illuminatedPanel(rubric).key("rubric")
                   .width(200).height(148)
                   .inset(600, 270, kSceneSize.width() - 600 - 200,
                          kSceneSize.height() - 270 - 148)
                   .absolute().zIndex(3).padding(18).gap(8)
                   .child(text(u8"nota bene",
                               styleAt(15, toColor(rubric.stem))))
                   .child(text(u8"the gate takes no coin but memory",
                               styleAt(16, toColor(rubric.ink)))))
        // Body text weaving between drop cap, rubric, and all four
        // corner flourishes.
        .child(box().inset(100, 132, 100, 82).absolute()
                   .child(text(body, styleAt(19.5f, toColor(pal.ink)))
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
        .child(weaveSprig("sprigL", 96, 300, 90.0f))
        .child(weaveSprig("sprigR", 748, 210, -90.0f))
        .child(weaveSprig("sprigB", 424, 486, 0.0f));

    return stack()
        .fill(Fill::color({0.11f, 0.09f, 0.075f, 1})) // scriptorium desk
        .child(std::move(pageStack))
        // Drifting gold dust: the only per-frame repaint.
        .child(custom([](SkCanvas &c, const PaintContext &ctx) {
                 SkPaint p;
                 p.setAntiAlias(true);
                 const double t = ctx.elapsedSeconds;
                 for (int i = 0; i < 26; ++i) {
                   const float fx = (float)i * 137.5f;
                   const float x = std::fmod(
                       fx + (float)t * (6.0f + (float)(i % 5)),
                       ctx.size.width());
                   const float y =
                       60.0f + std::fmod(fx * 0.61f, 500.0f) +
                       12.0f * std::sin((float)t * 0.8f + (float)i);
                   const float a =
                       0.10f + 0.16f * (0.5f + 0.5f * std::sin(
                                                    (float)t * 1.7f +
                                                    (float)i * 2.1f));
                   p.setColor4f({0.9f, 0.75f, 0.3f, a}, nullptr);
                   c.drawCircle(x, y, 1.6f + (float)(i % 3) * 0.7f, p);
                 }
               })
                   .inset(0).absolute().zIndex(4).cache(Cache::None));
  }

  static SkColor toColor(SkColor4f c) {
    return SkColor4f{c.fR, c.fG, c.fB, c.fA}.toSkColor();
  }

  void setup(Composer &composer, sigil::tick::Ticker &) override {
    verse = 0;
    nextTurn = 7.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextTurn)
      return;
    nextTurn = elapsed + 7.0;
    verse = (verse + 1) % 2;
    composer.render(describe()); // reflow weaves through the exclusions
  }
};

// ---- Nine-slice hall ------------------------------------------------------

struct NineSliceScene final : Scene {
  std::shared_ptr<sigil::image::ImageAsset> oakFrame, azureFrame,
      crimsonFrame;
  float stretch = 0.0f;

  const char *name() const override { return "nineslice"; }

  static std::shared_ptr<sigil::image::ImageAsset>
  generate(const Palette &pal) {
    // The intermediate canvas: draw the carved frame once, wrap the
    // snapshot, stretch it everywhere below. Generated at 2x the
    // on-page band width so the slice bands never magnify (raster
    // textures blur when stretched past their resolution).
    return std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(makeCarvedFrame(pal, 192)));
  }

  Element describe() {
    const Palette oak = oakPalette(), azure = azurePalette(),
                  crimson = crimsonPalette();

    auto panel = [&](const std::shared_ptr<sigil::image::ImageAsset> &f,
                     float l, float t, float w, float h) {
      return box().width(w).height(h)
          .inset(l, t, kSceneSize.width() - l - w,
                 kSceneSize.height() - t - h)
          .absolute()
          .background(carvedFrameSlice(f))
          .padding(24);
    };

    const float breathW = 250 + 66 * stretch;
    const float breathH = 130 + 34 * stretch;

    return stack()
        .fill(sigil::compose::util::linearGradient(
            {0, 0}, {0, 640},
            {{0.09f, 0.07f, 0.10f, 1}, {0.05f, 0.06f, 0.09f, 1}}))
        // The source texture at natural size, labeled.
        .child(box().inset(24, 24, kSceneSize.width() - 24 - 200,
                           kSceneSize.height() - 24 - 150)
                   .absolute().column().gap(8)
                   .child(image(oakFrame).width(96).height(96))
                   .child(text(u8"the source texture — drawn 2x on an "
                               u8"offscreen canvas, wrapped, nine-sliced",
                               styleAt(12, 0xff9aa4bb))
                              .width(190)))
        // Button: oak, small.
        .child(panel(oakFrame, 24, 210, 220, 84)
                   .alignItems(Align::Center).justify(Justify::Center)
                   .child(text(u8"BEGIN QUEST",
                               styleAt(19, 0xff2b1c0b))))
        // Banner: azure, wide.
        .child(panel(azureFrame, 268, 24, 600, 108)
                   .justify(Justify::Center)
                   .child(text(u8"THE HALL OF STRETCHED FRAMES",
                               styleAt(23, 0xff14243a)))
                   .child(text(u8"one texture per palette — any size "
                               u8"without distortion, corners stay carved",
                               styleAt(13.5f, 0xff3a4a63))))
        // Tall dialog: crimson, itemized.
        .child(panel(crimsonFrame, 560, 168, 300, 330)
                   .column().gap(12)
                   .child(text(u8"CELLAR MANIFEST",
                               styleAt(19, 0xff3a1410)))
                   .child(text(u8"◈  six barrels of pitch",
                               styleAt(15, 0xff4a2018)))
                   .child(text(u8"◈  the copper bowls",
                               styleAt(15, 0xff4a2018)))
                   .child(text(u8"◈  rope, forty fathoms",
                               styleAt(15, 0xff4a2018)))
                   .child(text(u8"◈  one coal, still warm",
                               styleAt(15, 0xff4a2018)))
                   .child(box().grow(1))
                   .child(text(u8"signed, the quartermaster",
                               styleAt(13, 0xff6a3a30))))
        // The breathing panel: relaid out every frame — the lattice
        // stretches live while the carved corners hold their shape.
        .child(panel(oakFrame, 60, 380 - (breathHalf(breathH)),
                     breathW, breathH)
                   .alignItems(Align::Center).justify(Justify::Center)
                   .child(text(u8"stretch me", styleAt(17, 0xff2b1c0b))));
  }

  static float breathHalf(float h) { return (h - 130.0f) * 0.5f; }

  void setup(Composer &composer, sigil::tick::Ticker &ticker) override {
    oakFrame = generate(oakPalette());
    azureFrame = generate(azurePalette());
    crimsonFrame = generate(crimsonPalette());
    stretch = 0.0f;
    ticker.add([this](double) { return true; }); // keep clock alive
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    stretch = 0.5f + 0.5f * (float)std::sin(elapsed * 1.4);
    composer.render(describe());
  }
};

} // namespace compose_gallery
