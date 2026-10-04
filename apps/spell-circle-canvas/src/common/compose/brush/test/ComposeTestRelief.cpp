#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ReliefTestSupport.h"

namespace {

void expectGlyphCoverage(const Host& ordinary, const Host& dressed, int width,
                         int height) {
  int solidGlyphs = 0, dressedGlyphs = 0, outsideGlyphs = 0;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      if (ordinary.pixel(x, y) == SK_ColorBLACK)
        outsideGlyphs += brightness(dressed.pixel(x, y)) > 90;
      else if (ordinary.pixel(x, y) == SK_ColorWHITE) {
        dressedGlyphs += brightness(dressed.pixel(x, y)) > 600;
        ++solidGlyphs;
      }
    }
  EXPECT_GT(solidGlyphs, 200);
  EXPECT_GT(dressedGlyphs, solidGlyphs * 0.95f);
  EXPECT_LT(outsideGlyphs, solidGlyphs * 0.1f);
}

PaintContext contextFor(const SkPath& outline,
                        const material::Lighting* lighting = nullptr) {
  PaintContext context;
  context.size = {128, 100};
  context.outline = geometry::path::fromSk(outline);
  context.lighting = lighting;
  return context;
}

SkPath rectangle(float left = 20, float top = 20, float right = 100,
                 float bottom = 80) {
  return SkPathBuilder()
      .addRect(SkRect::MakeLTRB(left, top, right, bottom))
      .detach();
}

struct ChangingPicture {
  sk_sp<SkImage> first;
  sk_sp<SkImage> second;
  sigil::media::Frame frameAt(std::chrono::duration<double> time) const {
    return {.image = time.count() < 1 ? first : second, .time = time};
  }
  bool isRunning() const { return true; }
  SkISize size() const { return first->dimensions(); }
  bool operator==(const ChangingPicture&) const = default;
};

Element movingPanel(const motion::Animatable<float>& shift,
                    const material::Material& field, Cache cache,
                    std::optional<Decoration> decoration = {}, int slot = 0,
                    bool curved = false) {
  Element child =
      box().absolute().rect(20, 20, 120, 100).key("panel").cache(cache);
  if (curved) child.shape(geometry::shapes::circle());
  if (!decoration)
    child.fill(field);
  else if (slot == 0)
    child.background(*decoration);
  else if (slot == 1)
    child.foreground(*decoration);
  else if (slot == 2)
    child.overlay(*decoration);
  else if (slot == 3)
    child.background(spans::range(0.0f, 1.0f), *decoration);
  else
    child.stroke(spans::range(0.0f, 1.0f), *decoration);
  child.children(
      {box().absolute().rect(0, 0, 4, 4).fill(material::Color{0, 1, 0, 1})});
  return box()
      .cache(Cache::None)
      .children({box()
                     .absolute()
                     .rect(0, 0, 320, 160)
                     .translateX(shift)
                     .cache(Cache::None)
                     .children({std::move(child)})});
}

void expectPanelMatches(const Host& actual, const Host& expected, int shift) {
  int differences = 0, painted = 0;
  for (int y = 30; y < 110; ++y)
    for (int x = 30 + shift; x < 130 + shift; ++x) {
      const SkColor a = actual.pixel(x, y), b = expected.pixel(x, y);
      differences += std::abs(static_cast<int>(SkColorGetR(a)) -
                              static_cast<int>(SkColorGetR(b))) > 1 ||
                     std::abs(static_cast<int>(SkColorGetB(a)) -
                              static_cast<int>(SkColorGetB(b))) > 1;
      painted += SkColorGetR(b) > 0 || SkColorGetB(b) > 0;
    }
  EXPECT_GT(painted, 100);
  EXPECT_EQ(differences, 0);
  EXPECT_EQ(actual.pixel(shift == 0 ? 210 : 30, 50), SK_ColorBLACK);
}

}  // namespace

TEST(ComposeRelief, SignedDepthTurnsTheActualOffsetOutlineAndPreservesHoles) {
  SkPathBuilder ring;
  ring.addRect(SkRect::MakeLTRB(20, 20, 100, 80));
  ring.addRect(SkRect::MakeLTRB(40, 40, 80, 60), SkPathDirection::kCCW);
  const material::Lighting light = material::studio(
      {.direction = 90.0f, .elevation = 25.0f, .ambient = 0.1f});
  const PaintContext context = contextFor(ring.detach(), &light);
  Host raised(128, 100), impressed(128, 100), flat(128, 100);
  paintWithPen(relief(greySurface(), {.shoulder = 6, .depth = 1}),
               *raised.surface->getCanvas(), context);
  paintWithPen(relief(greySurface(), {.shoulder = 6, .depth = -1}),
               *impressed.surface->getCanvas(), context);
  paintWithPen(relief(greySurface(), {.shoulder = 6, .depth = 0}),
               *flat.surface->getCanvas(), context);
  EXPECT_GT(brightness(raised.pixel(60, 22)),
            brightness(raised.pixel(60, 77)) + 30);
  EXPECT_GT(brightness(impressed.pixel(60, 77)),
            brightness(impressed.pixel(60, 22)) + 30);
  EXPECT_EQ(flat.pixel(60, 22), flat.pixel(60, 77));
  EXPECT_EQ(raised.pixel(60, 50), SK_ColorBLACK);
  EXPECT_EQ(impressed.pixel(60, 50), SK_ColorBLACK);
  EXPECT_EQ(raised.pixel(10, 50), SK_ColorBLACK);
}

TEST(ComposeRelief, ASharedBrushKeepsDistinctOutlineBakesWhileItsLightMoves) {
  auto reads = std::make_shared<int>(0);
  const auto sampled = [&](SkColor colour) {
    return material::image(
        sigil::media::PixelSource(CountedPicture{reads, greyPicture(colour)}));
  };
  const Relief mark =
      relief(sampled(SkColorSetRGB(150, 150, 150))
                 .surface({.roughness = sampled(SkColorSetRGB(204, 204, 204)),
                           .normal = sampled(SkColorSetRGB(128, 128, 255))}),
             {.shoulder = 6});
  motion::Animatable<float> direction = motion::animatable(180.0f);
  const material::Lighting light = material::studio(
      {.direction = direction, .elevation = 25.0f, .ambient = 0.1f});
  const PaintContext first = contextFor(rectangle(10, 10, 60, 90), &light);
  const PaintContext second = contextFor(rectangle(80, 30, 120, 70), &light);
  Host host(128, 100);
  const auto paintBoth = [&] {
    host.surface->getCanvas()->clear(SK_ColorBLACK);
    paintWithPen(mark, *host.surface->getCanvas(), first);
    paintWithPen(mark, *host.surface->getCanvas(), second);
  };
  paintBoth();
  const int firstReads = *reads;
  ASSERT_GT(firstReads, 0);
  EXPECT_GT(brightness(host.pixel(12, 50)),
            brightness(host.pixel(57, 50)) + 30);
  EXPECT_GT(brightness(host.pixel(82, 50)),
            brightness(host.pixel(117, 50)) + 30);
  direction = 0.0f;
  paintBoth();
  EXPECT_GT(brightness(host.pixel(57, 50)),
            brightness(host.pixel(12, 50)) + 30);
  EXPECT_GT(brightness(host.pixel(117, 50)),
            brightness(host.pixel(82, 50)) + 30);
  EXPECT_EQ(*reads, firstReads);
  for (const float angle : {180.0f, 0.0f}) {
    const material::Lighting snapshot = material::studio(
        {.direction = angle, .elevation = 25.0f, .ambient = 0.1f});
    PaintContext firstSnapshot = first, secondSnapshot = second;
    firstSnapshot.lighting = secondSnapshot.lighting = &snapshot;
    host.surface->getCanvas()->clear(SK_ColorBLACK);
    paintWithPen(mark, *host.surface->getCanvas(), firstSnapshot);
    paintWithPen(mark, *host.surface->getCanvas(), secondSnapshot);
    const int lit = angle == 180.0f ? 12 : 57;
    const int dark = angle == 180.0f ? 57 : 12;
    EXPECT_GT(brightness(host.pixel(lit, 50)),
              brightness(host.pixel(dark, 50)) + 30);
    EXPECT_EQ(*reads, firstReads);
  }
}

TEST(ComposeRelief, AuthoredNormalDetailAndItsConventionSurviveTheShoulder) {
  const material::Lighting light = material::studio(
      {.direction = 90.0f, .elevation = 25.0f, .ambient = 0.1f});
  const material::Material finish =
      material::from(material::Color{0.6f, 0.6f, 0.6f, 1})
          .surface({.roughness = 0.8f,
                    .normal = material::Color{0.5f, 0.8f, 0.9f, 1},
                    .normalDirectX = false});
  const PaintContext context = contextFor(rectangle(), &light);
  Host detailOnly(128, 100), combined(128, 100), noDetail(128, 100);
  paintWithPen(relief(finish, {.shoulder = 6, .depth = 0}),
               *detailOnly.surface->getCanvas(), context);
  paintWithPen(relief(finish, {.shoulder = 6, .depth = 1}),
               *combined.surface->getCanvas(), context);
  paintWithPen(relief(greySurface(), {.shoulder = 6, .depth = 0}),
               *noDetail.surface->getCanvas(), context);
  EXPECT_GT(brightness(detailOnly.pixel(60, 50)),
            brightness(noDetail.pixel(60, 50)) + 30);
  EXPECT_NEAR(brightness(combined.pixel(60, 50)),
              brightness(detailOnly.pixel(60, 50)), 12);
  EXPECT_GT(brightness(combined.pixel(60, 22)),
            brightness(combined.pixel(60, 77)) + 30);
}

TEST(ComposeRelief, BakeDensityKeepsLogicalShouldersAndOutlinePlacement) {
  const material::Lighting light = material::studio(
      {.direction = 90.0f, .elevation = 25.0f, .ambient = 0.1f});
  const Relief mark = relief(greySurface(), {.shoulder = 6});
  for (const float density : {0.5f, 1.0f, 2.0f}) {
    Host host(128, 100);
    PaintContext context = contextFor(rectangle(), &light);
    context.bakeDensity = density;
    paintWithPen(mark, *host.surface->getCanvas(), context);
    EXPECT_GT(brightness(host.pixel(60, 22)),
              brightness(host.pixel(60, 77)) + 30);
    EXPECT_EQ(host.pixel(10, 50), SK_ColorBLACK);
    EXPECT_GT(brightness(host.pixel(60, 50)), 0);
  }
}

TEST(ComposeRelief, AComposerTracksInheritedLightingWithoutAnotherDescribe) {
  motion::Animatable<float> direction = motion::animatable(180.0f);
  Host host(120, 100);
  host.composer.render(
      box()
          .lighting(material::studio(
              {.direction = direction, .elevation = 25.0f, .ambient = 0.1f}))
          .children({box()
                         .absolute()
                         .left(20)
                         .top(20)
                         .width(80)
                         .height(60)
                         .background(relief(greySurface(), {.shoulder = 6}))}));
  host.frame();
  EXPECT_GT(brightness(host.pixel(22, 50)),
            brightness(host.pixel(97, 50)) + 30);
  direction = 0.0f;
  EXPECT_TRUE(host.composer.isRunning());
  host.frame();
  EXPECT_GT(brightness(host.pixel(97, 50)),
            brightness(host.pixel(22, 50)) + 30);
}

TEST(ComposeRelief, TimeAndSurfaceMapBindingsRemainLiveUnderAutomaticCaching) {
  ASSERT_TRUE(timeColour());
  Host timed(120, 100);
  timed.composer.render(box().children({box().width(80).height(60).background(
      relief(material::skia::base(material::skia::sksl(timeColour()))))}));
  timed.frame();
  EXPECT_EQ(timed.pixel(30, 30), SK_ColorRED);
  EXPECT_TRUE(timed.composer.isRunning());
  timed.frame(1.0);
  EXPECT_EQ(timed.pixel(30, 30), SK_ColorBLUE);

  ASSERT_TRUE(scalarMap());
  motion::Animatable<float> roughness = motion::animatable(0.05f);
  material::Paint roughMap = material::skia::sksl(scalarMap());
  roughMap.bind("value", roughness);
  const material::Material surface =
      material::from(material::Color{0.6f, 0.6f, 0.6f, 1})
          .surface(
              {.metallic = 1.0f, .roughness = material::skia::base(roughMap)});
  Host mapped(120, 100);
  mapped.composer.render(
      box()
          .lighting(material::studio({.elevation = 90.0f}))
          .children({box().width(80).height(60).background(relief(surface))}));
  mapped.frame();
  const SkColor polished = mapped.pixel(40, 30);
  roughness = 1.0f;
  EXPECT_TRUE(mapped.composer.isRunning());
  mapped.frame();
  EXPECT_NE(mapped.pixel(40, 30), polished);
}

TEST(ComposeRelief, AMovingPixelSourceRemainsLiveThroughLoweredPaint) {
  SkBitmap redFrame, blueFrame;
  redFrame.allocN32Pixels(80, 60, true);
  blueFrame.allocN32Pixels(80, 60, true);
  redFrame.eraseColor(SK_ColorRED);
  blueFrame.eraseColor(SK_ColorBLUE);
  redFrame.setImmutable();
  blueFrame.setImmutable();
  const material::Texture source(sigil::media::PixelSource(
      ChangingPicture{redFrame.asImage(), blueFrame.asImage()}));
  const Relief mark = relief(material::image(source).surface({.unlit = true}));
  Host host(120, 100);
  host.composer.render(
      box().children({box().width(80).height(60).background(mark)}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  EXPECT_TRUE(host.composer.isRunning());
  host.frame(2.0);
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLUE);
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLUE);
}

namespace {

struct RootReliefCase {
  Cache cache;
  Composer::CacheState state;
  int slot;
  std::string name;
};

class HeldRootRelief : public testing::TestWithParam<RootReliefCase> {};

std::vector<RootReliefCase> rootReliefCases() {
  struct Arm {
    Cache cache;
    Composer::CacheState state;
    const char* name;
  };
  const Arm arms[] = {
      {Cache::None, Composer::CacheState::Live, "Live"},
      {Cache::Picture, Composer::CacheState::Picture, "Picture"},
      {Cache::Texture, Composer::CacheState::Texture, "Texture"},
      {Cache::Auto, Composer::CacheState::Promoted, "Eager"}};
  const char* slots[] = {"Background", "Foreground", "Overlay",
                         "SpanBackground", "SpanForeground"};
  std::vector<RootReliefCase> cases;
  for (const Arm& arm : arms)
    for (int slot = 0; slot < 5; ++slot)
      cases.push_back({arm.cache, arm.state, slot,
                       std::string(arm.name) + "_" + slots[slot]});
  return cases;
}

struct RootWashCase {
  int wrapper;
  const char* name;
};

class HeldRootWash : public testing::TestWithParam<RootWashCase> {};

}  // namespace

TEST_P(HeldRootRelief, ReanchorsWhenOnlyItsAncestorMoves) {
  const RootReliefCase& arm = GetParam();
  const auto field = rootGradient();
  const Relief contour = relief(field, {.shoulder = 0, .depth = 0});
  ASSERT_TRUE(contour.usesWorldSpace());
  ASSERT_FALSE(contour.isRunning());
  auto shift = motion::animatable(0.0f);
  Host retained(320, 160), ordinary(320, 160);
  retained.composer.setProfiling(true);
  retained.composer.setAutoTexturePromotion(
      arm.cache == Cache::Auto ? Composer::PromotionPolicy::Eager
                               : Composer::PromotionPolicy::Off);
  ordinary.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  retained.composer.render(
      movingPanel(shift, field, arm.cache, contour, arm.slot));
  ordinary.composer.render(movingPanel(shift, field, Cache::None));
  for (int frame = 0; frame < 16; ++frame) {
    retained.frame(1.0 / 60.0);
    ordinary.frame(1.0 / 60.0);
  }
  expectPanelMatches(retained, ordinary, 0);
  ASSERT_EQ(panelCacheState(retained), arm.state);
  // Only a retained ancestor input changes: neither child is described
  // again, including after the content scalar's stable release.
  for (const int offset : {80, 80, 0}) {
    shift = static_cast<float>(offset);
    retained.frame(1.0 / 60.0);
    ordinary.frame(1.0 / 60.0);
    expectPanelMatches(retained, ordinary, offset);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ComposeRelief, HeldRootRelief, testing::ValuesIn(rootReliefCases()),
    [](const testing::TestParamInfo<RootReliefCase>& info) {
      return info.param.name;
    });

TEST_P(HeldRootWash, ReanchorsThroughItsDecorationWrapper) {
  const auto field = rootGradient();
  const Decoration wash = decorations::wash(field);
  const auto unchangedOutline = geometry::path::Shaper::incomparable(
      [](const geometry::path::Outline& outline) { return outline; });
  const std::vector<Decoration> wrappers = {
      wash,
      DecorationStack{{wash}},
      brush::layers({wash}),
      Brush{}.layer(wash),
      brush::restyle(unchangedOutline, wash),
      onEdges(geometry::path::Edge::All, wash),
      inset(0, wash),
  };
  const Decoration& mark = wrappers[GetParam().wrapper];
  ASSERT_TRUE(mark.usesWorldSpace());
  ASSERT_FALSE(mark.isRunning());
  auto shift = motion::animatable(0.0f);
  Host retained(320, 160), ordinary(320, 160);
  retained.composer.setProfiling(true);
  retained.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  ordinary.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  retained.composer.render(
      movingPanel(shift, field, Cache::Picture, mark, 1, true));
  ordinary.composer.render(
      movingPanel(shift, field, Cache::None, mark, 1, true));
  for (int frame = 0; frame < 16; ++frame) {
    retained.frame(1.0 / 60.0);
    ordinary.frame(1.0 / 60.0);
  }
  expectPanelMatches(retained, ordinary, 0);
  ASSERT_EQ(panelCacheState(retained), Composer::CacheState::Picture);
  for (const int offset : {80, 80, 0}) {
    shift = static_cast<float>(offset);
    retained.frame(1.0 / 60.0);
    ordinary.frame(1.0 / 60.0);
    expectPanelMatches(retained, ordinary, offset);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ComposeRelief, HeldRootWash,
    testing::Values(RootWashCase{0, "Wash"}, RootWashCase{1, "Stack"},
                    RootWashCase{2, "Weave"}, RootWashCase{3, "Brush"},
                    RootWashCase{4, "Restyled"}, RootWashCase{5, "EdgeSlice"},
                    RootWashCase{6, "Inset"}),
    [](const testing::TestParamInfo<RootWashCase>& info) {
      return info.param.name;
    });

TEST(ComposeRelief, LocalMaterialsDoNotDeclareRootSpaceSampling) {
  EXPECT_FALSE(Decoration(relief(greySurface())).usesWorldSpace());
  EXPECT_FALSE(Decoration(decorations::wash(greySurface())).usesWorldSpace());
}

TEST(ComposeRelief, MaterialLayersCoverageAndPixelEffectsArePreserved) {
  const material::Material material =
      material::from(material::Color{0, 0, 1, 1})
          .layer(material::Color{1, 0, 0, 1})
          .surface({.unlit = true})
          .effects(material::Filter::shadow({0, 1, 0, 1}, {.offset = {0, 8}})
                       .then(material::Filter::stroke(
                           {1, 1, 0, 1},
                           {.width = 3,
                            .position = material::StrokePosition::Inside}))
                       .then(material::Filter::saturate(0)));
  Host host(128, 100);
  host.composer.render(box().children(
      {box().absolute().left(20).top(20).width(80).height(60).background(
          relief(material))}));
  host.frame();
  const SkColor centre = host.pixel(60, 50);
  EXPECT_GT(SkColorGetR(centre), 0);
  EXPECT_EQ(SkColorGetR(centre), SkColorGetG(centre));
  EXPECT_EQ(SkColorGetG(centre), SkColorGetB(centre));
  EXPECT_GT(brightness(host.pixel(21, 50)), brightness(centre));
  EXPECT_GT(brightness(host.pixel(60, 84)), 0);
  EXPECT_EQ(SkColorGetR(host.pixel(60, 84)), SkColorGetG(host.pixel(60, 84)));
  EXPECT_EQ(host.pixel(60, 90), SK_ColorBLACK);
}

TEST(ComposeRelief, GlyphForegroundFollowsTheSameCoverageAsTextInk) {
  for (const bool fixed : {false, true}) {
    SCOPED_TRACE(fixed ? "fixed text box" : "intrinsic text box");
    Host ordinary(200, 100), dressed(200, 100);
    const auto describe = [fixed](bool decorated) {
      Element letters = text(u8"HO", whiteStyle(64));
      if (fixed) letters.absolute().rect(14, 7, 174, 88);
      if (decorated)
        letters.ink(material::Color{0, 0, 0, 0})
            .decorationOutline(Boundary::Glyphs)
            .foreground(relief(material::Color{1, 1, 1, 1}));
      else
        letters.ink({1, 1, 1, 1});
      return box().children({letters});
    };
    ordinary.composer.render(describe(false));
    dressed.composer.render(describe(true));
    for (int frame = 0; frame < 2; ++frame) {
      SCOPED_TRACE(frame);
      ordinary.frame();
      dressed.frame();
      expectGlyphCoverage(ordinary, dressed, 200, 100);
    }
  }
}

TEST(ComposeRelief, GlyphForegroundTracksAlignmentAfterItsFixedBoxChanges) {
  Host ordinary(260, 110), dressed(260, 110);
  const auto describe = [](bool decorated, float width) {
    Element letters = text(u8"HO", whiteStyle(64))
                          .absolute()
                          .rect(10, 8, width, 90)
                          .textAlign(weave::TextAlignment::kCenter);
    if (decorated)
      letters.ink(material::Color{0, 0, 0, 0})
          .decorationOutline(Boundary::Glyphs)
          .foreground(relief(material::Color{1, 1, 1, 1}));
    else
      letters.ink({1, 1, 1, 1});
    return box().children({letters});
  };
  for (const float width : {140.0f, 230.0f, 140.0f}) {
    SCOPED_TRACE(width);
    ordinary.composer.render(describe(false, width));
    dressed.composer.render(describe(true, width));
    ordinary.frame();
    dressed.frame();
    expectGlyphCoverage(ordinary, dressed, 260, 110);
  }
}

TEST(ComposeRelief, EmptyAndWhitespaceGlyphBoundariesPaintNoRelief) {
  for (const char8_t* source : {u8"", u8"   ", u8"O"}) {
    SCOPED_TRACE(reinterpret_cast<const char*>(source));
    Host host(200, 100);
    host.composer.render(
        box().children({text(source, whiteStyle(64))
                            .absolute()
                            .rect(14, 7, 174, 88)
                            .ink(material::Color{0, 0, 0, 0})
                            .decorationOutline(Boundary::Glyphs)
                            .foreground(relief(material::Color{1, 1, 1, 1}))}));
    for (int frame = 0; frame < 2; ++frame) {
      SCOPED_TRACE(frame);
      host.frame();
      int covered = 0;
      for (int y = 0; y < 100; ++y)
        for (int x = 0; x < 200; ++x)
          covered += host.pixel(x, y) != SK_ColorBLACK;
      if (source[0] == u8'O') {
        EXPECT_GT(covered, 200);
        EXPECT_LT(covered, 174 * 88 / 2);
      } else {
        EXPECT_EQ(covered, 0);
      }
    }
  }

  Host shape(80, 60);
  shape.composer.render(
      box().children({box()
                          .width(40)
                          .height(30)
                          .decorationOutline(Boundary::Glyphs)
                          .foreground(relief(material::Color{1, 1, 1, 1}))}));
  shape.frame();
  EXPECT_EQ(shape.pixel(20, 15), SK_ColorWHITE);
  EXPECT_EQ(shape.pixel(50, 15), SK_ColorBLACK);
}

TEST(ComposeRelief, NormalBakesDoNotParticipateInStructuralEquality) {
  const Relief mark = relief(greySurface());
  Host host(128, 100);
  paintWithPen(mark, *host.surface->getCanvas(), contextFor(rectangle()));
  EXPECT_EQ(Decoration(mark), Decoration(relief(greySurface())));
  const auto scene = [] {
    return box().width(80).height(60).background(relief(greySurface()));
  };
  host.composer.render(scene());
  host.frame();
  host.composer.render(scene());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeRelief, InvalidAndOversizedBakesFallBackToFlatFinitePaint) {
  for (const ReliefOptions options :
       {ReliefOptions{.shoulder = std::numeric_limits<float>::infinity()},
        ReliefOptions{.depth = std::numeric_limits<float>::quiet_NaN()},
        ReliefOptions{.depth = std::numeric_limits<float>::max()},
        ReliefOptions{.shoulder = -1}}) {
    Host host(128, 100);
    paintWithPen(relief(material::Color{1, 0, 0, 1}, options),
                 *host.surface->getCanvas(), contextFor(rectangle()));
    EXPECT_EQ(host.pixel(60, 50), SK_ColorRED);
    EXPECT_EQ(host.pixel(10, 50), SK_ColorBLACK);
  }
  Host host(128, 100);
  PaintContext context = contextFor(rectangle());
  context.bakeDensity = std::numeric_limits<float>::max();
  paintWithPen(relief(material::Color{1, 0, 0, 1}), *host.surface->getCanvas(),
               context);
  EXPECT_EQ(host.pixel(60, 50), SK_ColorRED);
}
