// What the fill verb takes: every surface a `SurfacePaint` holds, a
// material recipe among them, and neither of the two values that are not
// fills.

#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Tile.h>

#include <algorithm>
#include <concepts>
#include <vector>

#include "support/CoreTestSupport.h"

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
concept FillableWith = requires(Element element, Value value) {
  element.fill(value);
};

}  // namespace

TEST(ComposeFill, AMaterialFillsAsTheSurfaceHoldingItDoes) {
  const material::Material grain = material::field::grain(0.08f, 4, 3.0f);
  const auto page = [](Element swatch) {
    return box().children(
        {std::move(swatch).width(96).height(64).absolute().inset(0)});
  };
  Host direct(96, 64);
  direct.composer.render(page(box().fill(grain)));
  direct.frame();
  Host wrapped(96, 64);
  wrapped.composer.render(page(box().fill(SurfacePaint{grain})));
  wrapped.frame();
  const std::vector<SkColor> expected = pixelsOf(wrapped, 96, 64);
  EXPECT_EQ(pixelsOf(direct, 96, 64), expected);
  // And the recipe painted a field, rather than two empty boxes agreeing.
  EXPECT_FALSE(std::ranges::all_of(
      expected, [&](SkColor one) { return one == expected.front(); }));
}

TEST(ComposeFill, NeitherATileNorAPatternIsAFill) {
  static_assert(FillableWith<material::Material>);
  static_assert(!FillableWith<material::pattern::Tile>);
  static_assert(!FillableWith<Pattern>);
}
