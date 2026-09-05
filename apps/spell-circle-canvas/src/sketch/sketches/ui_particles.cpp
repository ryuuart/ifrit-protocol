/** @file
 * ui particles — interface AS particles, from confetti chips to whole
 * posts, none of which look instanced: two atlas sheets, each stamped by
 * one instanced leaf over an entity simulation.
 */

// Scale scene: "UI as particles" — from confetti chips to whole POSTS, and
// none of them look instanced.
//
// TWO instancing::Atlas sheets, each stamped by one instances() leaf
// (Mode::Live) over an EnTT registry sim:
//   - CHIPS: five parameterized components (pill, spiky shout, scalloped
//     seal, carved nine-slice, dashed field note) baked THIRTY-TWO ways —
//     every atlas cell its own hue and content — scattered by the thousand.
//   - POSTS: six bigger multi-paragraph cards (a title + two paragraphs),
//     three border languages — the ornamental flourish border from
//     FlourishKit, a carved nine-slice, and a plain modern card — drifting
//     among the chips. Same instancing technique, from a badge to a feed.
//
// The EnTT registries stay on OUR side of the seam (velocities, drift,
// wrap); each frame the sim is copied into two instancing::Pool SoA stores
// through their spans, and the first-class layer does the baking (2x
// oversample built in) and the drawAtlas stamping.

#include <sigilcompose/core/Instances.h>
#include <sigilcompose/kit/Flourish.h>
#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <entt/entt.hpp>
#include <memory>
#include <random>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;
using namespace sigil::compose::kit::flourish;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

struct UiParticles final : sketch::Sketch {
  // ---- chips (the confetti tier) ------------------------------------------
  static constexpr size_t kChipCount = 820;
  static constexpr float kSprite = 64.0f;
  static constexpr int kVariants = 32;

  // ---- posts (the feed tier) ----------------------------------------------
  static constexpr size_t kPostCount = 30;
  static constexpr float kPostW = 232.0f, kPostH = 148.0f;
  static constexpr int kPostVariants = 12;

  entt::registry chips;
  entt::registry posts;
  std::shared_ptr<instancing::Atlas> chipAtlas, postAtlas;
  std::shared_ptr<instancing::Pool> chipPool, postPool;

  struct Pos {
    float x, y;
  };
  struct Vel {
    float dx, dy;
  };
  struct Look {
    uint8_t sprite;
    float scale;
    float spin;
  };

  // ---- chip components (one component, many skins) ------------------------

  struct ChipTheme {
    SkColor4f fill, edge, ink;
  };
  /** One chip's skin, read off the wheel: the hue is walked rather than
   *  authored, so the fill, the edge and the ink are one hue at three
   *  points of its own tone ladder. */
  static ChipTheme chipTheme(float hueDegrees, bool darkInk) {
    const auto tone = [hueDegrees](float saturation, float value) {
      return material::skia::toSkColor(
          material::hsv(hueDegrees, saturation, value));
    };
    return {tone(0.62f, 0.94f), tone(0.80f, 0.45f),
            darkInk ? tone(0.85f, 0.22f) : SkColor4f{1, 1, 1, 1}};
  }

  Element pill(const ChipTheme& t, std::u8string label) {
    return box()
        .width(kSprite - 10)
        .height(kSprite - 26)
        .corners({14})
        .fill(Fill::color(t.fill))
        .foreground(sigil::compose::stroke(2, Fill::color(t.edge)))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(text(std::move(label),
                    weave::textStyle({.size = 15, .color = t.ink})));
  }
  Element shout(const ChipTheme& t, std::u8string label, int spikes) {
    return box()
        .width(kSprite - 4)
        .height(kSprite - 4)
        .shape(starburstOutline(spikes, 0.32f))
        .fill(sigil::compose::radialGradient({kSprite / 2 - 2, kSprite / 2 - 2},
                                             kSprite / 2,
                                             {{1.0f, 0.92f, 0.55f, 1}, t.fill}))
        .foreground(sigil::compose::stroke(2, Fill::color(t.edge)))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(text(std::move(label),
                    weave::textStyle({.size = 13, .color = t.ink})));
  }
  Element seal(const ChipTheme& t, std::u8string label, float lobe) {
    return box()
        .width(kSprite - 8)
        .height(kSprite - 8)
        .shape(scallopOutline(lobe))
        .fill(Fill::color(t.fill))
        .foreground(sigil::compose::stroke(2, Fill::color(t.edge)))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(text(std::move(label),
                    weave::textStyle({.size = 13, .color = t.ink})));
  }
  Element framed(const Palette& pal, std::u8string label) {
    return box()
        .width(kSprite - 8)
        .height(kSprite - 12)
        .background(carvedFrameSlice(std::make_shared<sigil::image::ImageAsset>(
            sigil::image::ImageAsset::wrap(makeCarvedFrame(pal, 96)))))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(text(std::move(label),
                    weave::textStyle({.size = 15, .color = pal.ink})));
  }
  Element note(const ChipTheme& t, std::u8string line1, std::u8string line2) {
    PathFormat dashed;
    dashed.width = 1.6f;
    dashed.strokeFill = Fill::color(t.edge);
    dashed.dashIntervals = {5, 4};
    SkColor4f paper = t.fill;
    paper.fR = 0.75f + paper.fR * 0.25f;
    paper.fG = 0.75f + paper.fG * 0.25f;
    paper.fB = 0.75f + paper.fB * 0.25f;
    return box()
        .width(kSprite - 10)
        .height(kSprite - 18)
        .corners({8})
        .fill(Fill::color(paper))
        .foreground(dashed)
        .column()
        .gap(2)
        .padding(6)
        .child(text(std::move(line1),
                    weave::textStyle({.size = 12, .color = t.ink})))
        .child(text(std::move(line2),
                    weave::textStyle({.size = 10, .color = t.edge})));
  }

  void buildChipAtlas() {
    chipAtlas =
        std::make_shared<instancing::Atlas>();  // 2x oversample built in

    static constexpr const char8_t* kPillLabels[] = {
        u8"+250", u8"+120", u8"+45", u8"-87",  u8"-12",
        u8"xp",   u8"gg",   u8"♥",   u8"lv 9", u8"rare"};
    static constexpr const char8_t* kShoutLabels[] = {u8"POW", u8"BAM", u8"ZOK",
                                                      u8"CRIT"};
    static constexpr const char8_t* kSealLabels[] = {u8"act I", u8"act II",
                                                     u8"fin", u8"oath"};
    static constexpr const char8_t* kFrameLabels[] = {u8"+1", u8"+3", u8"7",
                                                      u8"key"};
    static constexpr const char8_t* kNoteLines[][2] = {{u8"run!", u8"north"},
                                                       {u8"hide!", u8"east"},
                                                       {u8"loot!", u8"cave"},
                                                       {u8"rest", u8"camp"}};
    const Palette framePals[4] = {oakPalette(), azurePalette(),
                                  crimsonPalette(), emeraldPalette()};

    // a fixed seed; the scene must render the same on every run
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
    std::mt19937 rng{23};
    for (int i = 0; i < kVariants; ++i) {
      // the golden-angle walk, unfolded: hsv() wraps the hue itself
      const float hue = (float)i * 137.5f;
      const ChipTheme theme = chipTheme(hue, (i % 3) != 0);
      Element content = [&]() -> Element {
        switch (i % 5) {
          case 0:
            return pill(theme, kPillLabels[rng() % 10]);
          case 1:
            return shout(theme, kShoutLabels[rng() % 4], 8 + (int)(rng() % 5));
          case 2:
            return seal(theme, kSealLabels[rng() % 4],
                        7.0f + (float)(rng() % 4));
          case 3:
            return framed(framePals[rng() % 4], kFrameLabels[rng() % 4]);
          default: {
            const auto& lines = kNoteLines[rng() % 4];
            return note(theme, lines[0], lines[1]);
          }
        }
      }();
      chipAtlas->cell(box()
                          .alignItems(Align::Center)
                          .justify(Justify::Center)
                          .child(std::move(content)),
                      {kSprite, kSprite});
    }
  }

  // ---- post components (the feed tier) ------------------------------------

  enum class PostKind { Flourish, Carved, Plain };
  struct PostConfig {
    PostKind kind;
    int paletteIndex;  // carved: OrnamentKit palette; plain: accent
    const char8_t* title;
    const char8_t* body1;
    const char8_t* body2;
  };

  Element flourishPost(const PostConfig& cfg) {
    FlourishStyle s;  // gilt-on-parchment
    return flourishCard(s, kPostW - 6, kPostH - 6)
        .child(text(cfg.title, weave::textStyle({.size = 15, .color = s.ink})))
        .child(
            text(cfg.body1, weave::textStyle({.size = 10.5f, .color = s.ink})))
        .child(text(cfg.body2, weave::textStyle(
                                   {.size = 10.5f,
                                    .color = SkColor4f{s.bronze.fR, s.bronze.fG,
                                                       s.bronze.fB, 1}})));
  }
  Element carvedPost(const PostConfig& cfg) {
    const Palette pals[4] = {oakPalette(), azurePalette(), crimsonPalette(),
                             emeraldPalette()};
    const Palette& pal = pals[(unsigned)cfg.paletteIndex & 3u];
    return box()
        .width(kPostW - 6)
        .height(kPostH - 6)
        .background(carvedFrameSlice(std::make_shared<sigil::image::ImageAsset>(
            sigil::image::ImageAsset::wrap(makeCarvedFrame(pal, 128)))))
        .column()
        .padding(30, 26)
        .gap(5)
        .child(
            text(cfg.title, weave::textStyle({.size = 15, .color = pal.stem})))
        .child(text(cfg.body1,
                    weave::textStyle({.size = 10.5f, .color = pal.ink})))
        .child(text(cfg.body2,
                    weave::textStyle({.size = 10.5f, .color = pal.ink})));
  }
  Element plainPost(const PostConfig& cfg) {
    // A modern dark UI card — the counterpoint to the ornate borders.
    const SkColor4f accents[2] = {{0.42f, 0.66f, 0.98f, 1},   // cobalt
                                  {0.98f, 0.72f, 0.34f, 1}};  // amber
    const SkColor4f accent = accents[(unsigned)cfg.paletteIndex & 1u];
    return box()
        .width(kPostW - 6)
        .height(kPostH - 6)
        .corners({12})
        .fill(Fill::color({0.10f, 0.11f, 0.15f, 1}))
        .foreground(sigil::compose::stroke(1.4f, Fill::color(accent)))
        .column()
        .padding(16, 14)
        .gap(6)
        .child(text(cfg.title, weave::textStyle({.size = 15, .color = accent})))
        .child(box().width(pct(38)).height(2).corners({1}).fill(
            Fill::color(accent)))
        .child(text(cfg.body1,
                    weave::textStyle({.size = 10.5f, .color = hex(0xcdd3df)})))
        .child(text(cfg.body2,
                    weave::textStyle({.size = 10.5f, .color = hex(0x9aa3b4)})));
  }

  Element postVariant(const PostConfig& cfg) {
    switch (cfg.kind) {
      case PostKind::Flourish:
        return flourishPost(cfg);
      case PostKind::Carved:
        return carvedPost(cfg);
      default:
        return plainPost(cfg);
    }
  }

  void buildPostAtlas() {
    static constexpr PostConfig kPosts[kPostVariants] = {
        {PostKind::Flourish, 0, u8"Codex I",
         u8"The causeway held through the night; we counted the lanterns "
         u8"twice, once for the living channel and once for the drowned road.",
         u8"Feed the vine its silver hour and the gate stands open one "
         u8"crossing more."},
        {PostKind::Carved, 0, u8"Quartermaster",
         u8"Six barrels of pitch, the copper bowls, and forty fathoms of "
         u8"good rope coiled against the cellar wall.",
         u8"One coal, still warm. Signed at the water line."},
        {PostKind::Plain, 0, u8"Ferryman",
         u8"The channel runs black under the second arch and the lanterns "
         u8"will not sit still on it; steer by the drowned road instead.",
         u8"Count the crossings, not the hours."},
        {PostKind::Flourish, 2, u8"Rubric II",
         u8"The gate takes no coin but memory, and gives back the shape of "
         u8"every hand that held the rope.",
         u8"Draw the circle wide; keep the margin free for the vine."},
        {PostKind::Carved, 3, u8"Warden's Note",
         u8"The reeds keep time against the slow current where the ferry "
         u8"rope hums under a traveler's hand.",
         u8"Twelve nights the lanterns answered; on the last, the gate "
         u8"stood open."},
        {PostKind::Plain, 1, u8"Lampwright",
         u8"Silvered glass and a short wick: the halo is the lamp seen "
         u8"twice, once sharp and once spread.",
         u8"Trim them at dusk and they hold to the turn of the tide."},
        // TWELVE CARDS, NOT SIX. At six variants over thirty posts every
        // card is on the page five times, and at this density the
        // repetition is the first thing the eye finds — which is the one
        // reading a scale study cannot afford, because it looks exactly
        // like the instancing showing through.
        {PostKind::Carved, 1, u8"Salt Garden",
         u8"Between the second arch and the bell the ground goes white by "
         u8"midsummer, and nothing grows in it but the reeds we planted.",
         u8"Cut them short. They come back taller."},
        {PostKind::Flourish, 1, u8"Antiphon",
         u8"Sung at the turn of the tide, once from the near bank and once "
         u8"from the far, so that neither singer hears the other finish.",
         u8"The water carries the second half."},
        {PostKind::Plain, 2, u8"Tide Table",
         u8"High water an hour before the lanterns; low water when the "
         u8"drowned road shows its kerb and the ferry rope goes slack.",
         u8"Nothing crosses on the slack."},
        {PostKind::Carved, 2, u8"Bell Keeper",
         u8"Three strokes for a crossing, two for a warden overdue, and "
         u8"one, held, for a coal gone out on the far shore.",
         u8"It has been rung once in my time."},
        {PostKind::Flourish, 3, u8"Margin Note",
         u8"Whoever copies this book next: keep the margin free. The vine "
         u8"is not decoration, it is where the corrections go.",
         u8"I have left room for yours."},
        {PostKind::Plain, 3, u8"Ropewright",
         u8"Forty fathoms, laid left-handed against the current, and "
         u8"spliced under the arch where the splice stays dry.",
         u8"A rope that has held once will tell you before it fails."},
    };

    postAtlas = std::make_shared<instancing::Atlas>();  // 2x: crisp paragraphs
    for (const auto& post : kPosts)
      postAtlas->cell(box()
                          .alignItems(Align::Center)
                          .justify(Justify::Center)
                          .child(postVariant(post)),
                      {kPostW, kPostH});
  }

  // ---- seeding + stepping -------------------------------------------------

  static void seed(entt::registry& reg, size_t count, int variants,
                   uint32_t rngSeed, float scaleLo, float scaleHi,
                   float velUp) {
    std::mt19937 rng{rngSeed};
    auto unit = [&] { return (float)(rng() % 10000) / 10000.0f; };
    for (size_t i = 0; i < count; ++i) {
      entt::entity e = reg.create();
      reg.emplace<Pos>(e, unit() * kSceneSize.width(),
                       unit() * kSceneSize.height());
      reg.emplace<Vel>(e, unit() * 40 - 20, -velUp * (0.4f + unit()));
      reg.emplace<Look>(e, (uint8_t)(rng() % variants),
                        scaleLo + unit() * (scaleHi - scaleLo),
                        unit() * 0.3f - 0.15f);
    }
  }

  static void step(entt::registry& reg, double dt, float margin) {
    reg.view<Pos, const Vel>().each([dt, margin](Pos& p, const Vel& v) {
      p.x += v.dx * (float)dt;
      p.y += v.dy * (float)dt;
      if (p.y < -margin) p.y += kSceneSize.height() + margin;
      if (p.x < -margin)
        p.x += kSceneSize.width() + margin;
      else if (p.x > kSceneSize.width())
        p.x -= kSceneSize.width() + margin;
    });
  }

  // The EnTT → Pool copy-in: the registry stays the sim, the pool spans
  // are the seam the instances() leaf reads (Mode::Live, every frame).
  static void syncPool(entt::registry& reg, instancing::Pool& pool, double t) {
    auto positions = pool.positions();
    auto rotations = pool.rotations();
    auto scales = pool.scales();
    auto frames = pool.frames();
    size_t i = 0;
    reg.view<const Pos, const Look>().each([&](const Pos& p, const Look& l) {
      positions[i] = {p.x, p.y};
      rotations[i] = l.spin * (float)std::sin(t * 1.6 + p.x * 0.01);
      scales[i] = l.scale;
      frames[i] = l.sprite;
      ++i;
    });
  }

  void update(double t, sketch::SketchContext& ctx) override {
    syncPool(chips, *chipPool, t);
    syncPool(posts, *postPool, t);
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    buildChipAtlas();
    buildPostAtlas();
    chips.clear();
    posts.clear();
    seed(chips, kChipCount, kVariants, 11, 0.35f, 0.9f, 70.0f);
    seed(posts, kPostCount, kPostVariants, 71, 0.42f, 0.62f, 24.0f);
    chipPool = std::make_shared<instancing::Pool>();
    postPool = std::make_shared<instancing::Pool>();
    chipPool->resize(kChipCount);
    postPool->resize(kPostCount);
    syncPool(chips, *chipPool, 0.0);
    syncPool(posts, *postPool, 0.0);

    ticker.add([this](double dt) {
      step(chips, dt, kSprite);
      step(posts, dt, kPostW);
      return true;
    });

    // instances() fills its parent; each tier gets a full-canvas box so
    // pool positions are canvas pixels. Chips behind, posts in front.
    composer.render(
        stack()
            .fill(sigil::compose::linearGradient(
                {0, 0}, {0, 640},
                {{0.05f, 0.04f, 0.12f, 1}, {0.12f, 0.05f, 0.14f, 1}}))
            .child(box().inset(0).child(instancing::instances(
                chipAtlas, chipPool, instancing::Mode::Live)))
            .child(box().inset(0).child(instancing::instances(
                postAtlas, postPool, instancing::Mode::Live)))
            // THE TITLE, ON A SILL. The ground here is not merely crossed
            // by the type, it is the densest thing in the registry: a
            // knockout works against linework and disappears against a
            // field of stamps, so the line stands on an opaque plate of
            // its own.
            .child(
                kit::scrim(text(u8"UI as particles \u2014 820 chips over "
                                u8"30 posts, one instances() stamp a tier",
                                weave::textStyle(
                                    {.size = 17, .color = hex(0xf2f5fb)})),
                           {.fill = Fill::color({0.03f, 0.025f, 0.06f, 0.92f}),
                            .paddingX = 14,
                            .paddingY = 9})
                    .absolute()
                    .left(24)
                    .top(22)
                    .zIndex(2)));
  }
};

}  // namespace

SIGIL_SKETCH_AS(UiParticles, "ui particles", "Specimen",
                "instances() at scale \xe2\x80\x94 two atlases over a "
                "structure-of-arrays simulation")
