// What the fill verb takes: every surface a `Fill` holds, a
// material recipe among them, and neither of the two values that are not
// fills. Every other verb that takes a surface takes a material as well.

#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/core/Text.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Tile.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstring>
#include <vector>

#include "cache/MarkStates.h"
#include "paint/MaterialEffects.h"
#include "paint/PenOnCanvas.h"
#include "paint/SurfaceMap.h"
#include "support/CoreTestSupport.h"
#include "support/Rows.h"

namespace {

/** Every pixel of @p host's surface, row by row. */
std::vector<SkColor> pixelsOf(Host& host, int width, int height) {
  std::vector<SkColor> out;
  out.reserve((size_t)(width * height));
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) out.push_back(host.pixel(x, y));
  return out;
}

template <class Value>
concept FillableWith =
    requires(Element element, Value value) { element.fill(value); };

template <class Value>
concept InkableWith =
    requires(Element element, Value value) { element.ink(value); };

template <class Value>
concept TextStrokableWith =
    requires(Text text, Value value) { text.textStroke(1.0f, value); };

template <class Value>
concept RuleFillableWith = requires(Rule rule, Value value) {
  rule.fill(value);
  rule.ink(value);
  rule.textStroke(1.0f, value);
};

}  // namespace

TEST(ComposeFill, NeitherATileNorAPatternIsAFill) {
  static_assert(FillableWith<material::Material>);
  static_assert(!FillableWith<material::pattern::Tile>);
  static_assert(!FillableWith<Pattern>);
}

TEST(ComposeFill, EveryVerbThatTakesASurfaceTakesAMaterial) {
  sigil::compose::test::checkRows({
      {"AMaterialFillsAsAFillHoldingItDoes",
       [&] {
         const material::Material grain =
             material::field::grain(0.08f, 4, 3.0f);
         const auto page = [](Element swatch) {
           return box().children(
               {std::move(swatch).width(96).height(64).absolute().inset(0)});
         };
         Host direct(96, 64);
         direct.composer.render(page(box().fill(grain)));
         direct.frame();
         Host wrapped(96, 64);
         wrapped.composer.render(page(box().fill(Fill{grain})));
         wrapped.frame();
         const std::vector<SkColor> expected = pixelsOf(wrapped, 96, 64);
         EXPECT_EQ(pixelsOf(direct, 96, 64), expected);
         // And the recipe painted a field, rather than two empty boxes
         // agreeing.
         EXPECT_FALSE(std::ranges::all_of(
             expected, [&](SkColor one) { return one == expected.front(); }));
       }},
      {"InkTextStrokeAndARuleTakeOne",
       [&] {
         static_assert(InkableWith<material::Material>);
         static_assert(TextStrokableWith<material::Material>);
         static_assert(RuleFillableWith<material::Material>);
       }},
  });
}

// ---- a lit keyline's paint state --------------------------------------------

namespace {

/** A lit finish: metal three quarters, roughness one quarter. */
material::Material litFinish() {
  return material::from(material::Color{.8f, .4f, .2f, 1})
      .surface({.metallic = .75f, .roughness = .25f});
}

/** A plate dressed only by a lit keyline six pixels wide inside it. */
Element keylined() {
  return box().width(64).height(64).children(
      {box()
           .absolute()
           .rect(8, 8, 48, 48)
           .stroke(
               litFinish(),
               {.width = 6, .position = material::StrokePosition::Inside})});
}

}  // namespace

TEST(ComposeMaterialStroke, AKeylineDescribedOncePaintsEachComposersOwnMap) {
  sigil::compose::test::checkRows({
      {"OneDescriptionOnTwoComposers",
       [&] {
         // One description handed to two composers painting different surface
         // maps, frame after frame in turn: each keyline is its own composer's
         // map, as a composer that alone was handed the description paints it.
         const Element described = keylined();
         Host rough(64, 64), metal(64, 64);
         rough.composer.setSurfaceMap(material::texture::Role::Roughness);
         metal.composer.setSurfaceMap(material::texture::Role::Metallic);
         rough.composer.render(described);
         metal.composer.render(described);
         for (int frame = 0; frame < 3; ++frame) {
           rough.frame();
           metal.frame();
         }
         const SkColor4f roughKeyline =
             SkColor4f::FromColor(rough.pixel(10, 32));
         const SkColor4f metalKeyline =
             SkColor4f::FromColor(metal.pixel(10, 32));
         EXPECT_NEAR(roughKeyline.fG, .25f, .012f);
         EXPECT_NEAR(metalKeyline.fG, .75f, .012f);
         Host roughAlone(64, 64), metalAlone(64, 64);
         roughAlone.composer.setSurfaceMap(material::texture::Role::Roughness);
         metalAlone.composer.setSurfaceMap(material::texture::Role::Metallic);
         roughAlone.composer.render(keylined());
         metalAlone.composer.render(keylined());
         roughAlone.frame();
         metalAlone.frame();
         EXPECT_TRUE(identicalPixels(rough, roughAlone, 64, 64));
         EXPECT_TRUE(identicalPixels(metal, metalAlone, 64, 64));
       }},
      {"PaintingLeavesTheDescriptionUnchanged",
       [&] {
         // Described once and painted as a surface map — outside a composer and
         // on a composer's instance alike — the mark's value is the bytes it
         // was described as: what the paint prepared stands with the painter.
         const material::Material finish = litFinish();
         const detail::MaterialStroke described{
             material::skia::paint(finish),
             {.width = 6, .position = material::StrokePosition::Inside},
             detail::surfacedForMaps(finish)};
         ASSERT_TRUE(described.surfaced.has_value());
         std::array<unsigned char, sizeof(detail::MaterialStroke)> before{};
         std::memcpy(before.data(), &described, sizeof(described));

         SkBitmap pixels;
         pixels.allocN32Pixels(64, 64);
         SkCanvas canvas(pixels);
         PaintContext context;
         context.size = {48, 48};
         context.outline = geometry::path::fromSk(SkPath::Rect({0, 0, 48, 48}));
         context.surfaceMap = material::texture::Role::Roughness;
         detail::MarkState state;
         for (detail::MarkState* kept : {(detail::MarkState*)nullptr, &state}) {
           context.markState = kept;
           detail::PenOnCanvas pen(canvas, context);
           described.paint(pen.pen(), context);
         }
         EXPECT_NE(state.stroke, nullptr)
             << "the instance keeps what was prepared";
         EXPECT_EQ(std::memcmp(before.data(), &described, sizeof(described)), 0)
             << "painting wrote into the description";
         EXPECT_NEAR(pixels.getColor4f(2, 24).fG, .25f, .012f);
       }},
  });
}
