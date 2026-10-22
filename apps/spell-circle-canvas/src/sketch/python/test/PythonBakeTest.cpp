/** @file
 * A Python sketch that bakes a 3D frame: the pages its cards wear, a
 * page's stated emission range, a sized light's shadow, a page described
 * before its first paint, a keyframed turn baked at a moment, and the
 * density a still is baked at.
 */

#include <gtest/gtest.h>
#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::assets;
using sketch::test::fonts;
using sketch::test::PythonSource;
using sketch::test::SketchPythonBakes;

TEST_F(SketchPythonBakes, APageThePythonSketchPaintsIsWornByTheBakedCard) {
  PythonSource source("sigil_python_page_bake");
  // One green page that takes no light, worn by a card under no light at
  // all: what the card shows is the page, and the bake fills the canvas.
  source.write(
      "entry.py",
      "from sigil import compose, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(128, 96), background='#1a1a1a', captureAt=0)\n"
      "class Paged:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        self.page = ctx.page((32, 32))\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        card = (world.mesh(mesh.quad(100, 60)).key('card')\n"
      "                .fill(self.page.surface()))\n"
      "        tree = compose.box().width(32).height(32)"
      ".fill('#00ff00')\n"
      "        sun = world.directionalLight((0, 0, -1)).key('sun')\n"
      "        frame = (world.Frame(world.group(sun, card).key('set'))\n"
      "                 .pass_(self.page.render(tree)))\n"
      "        still = ctx.bakeScene3D(frame, (128, 96), "
      "camera=lens, background='#1a1a1a')\n"
      "        ctx.render(compose.image(still).width(128).height(96)\n"
      "                   if still is not None else compose.box())\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  ASSERT_TRUE(kind->needsDevice());
  auto session = kind->open(fonts(), assets(), true, "paged");
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 96));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(128, 96));
  ASSERT_TRUE(surface->readPixels(pixels, 0, 0));
  // The page's green is the card's base colour, lit by the sun.
  const SkColor middle = pixels.getColor(64, 48);
  EXPECT_GT(SkColorGetG(middle), 100u);
  EXPECT_GT(SkColorGetG(middle), SkColorGetR(middle) + 60);
  EXPECT_GT(SkColorGetG(middle), SkColorGetB(middle) + 60);
  // Off the card is the ground the bake was cleared to, not the page.
  EXPECT_LT(SkColorGetG(pixels.getColor(1, 1)), 50u);
}

/** The still a Python source bakes, drawn into a raster canvas of
 *  @p width by @p height and read back. */
SkBitmap bakedStill(const PythonSource& source, std::string_view key, int width,
                    int height) {
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, std::string(key));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  if (!surface->readPixels(pixels, 0, 0)) return {};
  return pixels;
}

TEST_F(SketchPythonBakes, TwoCardsWearTheTwoHalvesOfOnePage) {
  PythonSource source("sigil_python_page_regions");
  // One page, red on its left half and blue on its right, and two cards
  // side by side, each wearing one half: each card shows its own half's
  // colour across the whole of it.
  source.write(
      "entry.py",
      "from sigil import compose, material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(128, 64), background='#000000', captureAt=0)\n"
      "class Halves:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        page = ctx.page((64, 32))\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        left = material.PixelRect(x=0, y=0, width=32, height=32)\n"
      "        right = material.PixelRect(x=32, y=0, width=32, height=32)\n"
      "        cards = [\n"
      "            world.mesh(mesh.quad(100, 100)).key(name).at((x, 0, 0))\n"
      "            .fill(page.surface(region))\n"
      "            for name, x, region in (('left', -60, left),\n"
      "                                    ('right', 60, right))]\n"
      "        tree = compose.box().width(64).height(32).children(\n"
      "            compose.box().absolute().rect(0, 0, 32, 32)"
      ".fill('#ff0000'),\n"
      "            compose.box().absolute().rect(32, 0, 32, 32)"
      ".fill('#0000ff'))\n"
      "        sun = world.directionalLight((0, 0, -1)).key('sun')\n"
      "        frame = (world.Frame(world.group(sun, *cards).key('set'))\n"
      "                 .pass_(page.render(tree)))\n"
      "        still = ctx.bakeScene3D(frame, (128, 64), camera=lens,\n"
      "                                background='#000000')\n"
      "        ctx.render(compose.image(still).width(128).height(64)\n"
      "                   if still is not None else compose.box())\n");
  const SkBitmap pixels = bakedStill(source, "halves", 128, 64);
  ASSERT_FALSE(pixels.isNull());
  const SkColor left = pixels.getColor(46, 32);
  const SkColor right = pixels.getColor(82, 32);
  EXPECT_GT(SkColorGetR(left), SkColorGetB(left) + 60)
      << "the left card wears the page's red half";
  EXPECT_GT(SkColorGetB(right), SkColorGetR(right) + 60)
      << "the right card wears the page's blue half";
}

TEST_F(SketchPythonBakes, AStatedEmissionRangeKeepsABrighterEmissionUnclipped) {
  PythonSource source("sigil_python_page_range");
  // Two pages painting the same emission of three, one stating its
  // emissive map's white at four and one at one. Seen through an exposure
  // of a fifth on a linear curve, the first card shows the three it was
  // painted with and the second the one its range clips it to.
  source.write(
      "entry.py",
      "from sigil import compose, material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(128, 64), background='#000000', captureAt=0)\n"
      "class Ranged:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        wide = ctx.page((32, 32), compose.SurfaceRange(emission=4))\n"
      "        narrow = ctx.page((32, 32),\n"
      "                          range=compose.SurfaceRange(emission=1))\n"
      "        glow = material.from_(material.Color.srgb(0, 0, 0, 1)).surface(\n"
      "            emission=material.Color.srgb(1, 1, 1, 1), emissionStrength=3)\n"
      "        tree = compose.box().width(32).height(32).fill(glow)\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        eye = world.camera(lens, exposure=0.2,\n"
      "                           "
      "toneCurve=world.ToneCurve.Linear).key('eye')\n"
      "        cards = [\n"
      "            world.mesh(mesh.quad(100, 100)).key(name).at((x, 0, 0))\n"
      "            .fill(page.surface())\n"
      "            for name, x, page in (('wide', -60, wide),\n"
      "                                  ('narrow', 60, narrow))]\n"
      "        frame = (world.Frame(world.group(eye, *cards).key('set'))\n"
      "                 .pass_(wide.render(tree))\n"
      "                 .pass_(narrow.render(tree)))\n"
      "        still = ctx.bakeScene3D(frame, (128, 64), camera=lens,\n"
      "                                background='#000000')\n"
      "        ctx.render(compose.image(still).width(128).height(64)\n"
      "                   if still is not None else compose.box())\n");
  const SkBitmap pixels = bakedStill(source, "ranged", 128, 64);
  ASSERT_FALSE(pixels.isNull());
  const SkColor wide = pixels.getColor(46, 32);
  const SkColor narrow = pixels.getColor(82, 32);
  EXPECT_GT(SkColorGetG(narrow), 40u) << "the clipped card still glows";
  EXPECT_GT(SkColorGetG(wide), SkColorGetG(narrow) + 40)
      << "the range of four keeps the emission of three it was painted with";
}

TEST_F(SketchPythonBakes, ASizedLightsShadowIsSofterFartherFromItsCaster) {
  PythonSource source("sigil_python_shadow_size");
  // Seen from straight above, a white ground lit at forty-five degrees
  // from the left by a sun casting shadows, and two slabs hung over it at
  // the left, one low over the lower half and one high over the upper:
  // each throws its shadow to the right by its height. The sun spans
  // eight degrees in the right-hand bake and is a point in the left-hand
  // one, so on the right the high slab's shadow edge is soft and the low
  // one's nearly sharp, and on the left both are sharp.
  source.write(
      "entry.py",
      "from sigil import material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(512, 256), background='#000000', captureAt=0)\n"
      "class Shadowed:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 400)\n"
      "        lens.target = (0, 0, 0)\n"
      "        matte = material.from_('#e0e0e0').surface(roughness=1.0)\n"
      "        def scene(size):\n"
      "            ground = world.mesh(mesh.quad(600, 600)).key('ground')"
      ".fill(matte)\n"
      "            low = world.mesh(mesh.box((-110, -90, 30), (-50, -30, 40)))"
      ".key('low').fill(matte)\n"
      "            high = world.mesh(mesh.box((-110, 30, 120), (-50, 90, 130)))"
      ".key('high').fill(matte)\n"
      "            sun = world.directionalLight((1, 0, -1), castShadow=True,\n"
      "                shadow=world.LightShadow(size=size)).key('sun')\n"
      "            return world.Frame(world.group(sun, ground, low, high)"
      ".key('set'))\n"
      "        self.stills = [ctx.bakeScene3D(scene(size), (256, 256),\n"
      "                                       camera=lens,\n"
      "                                       background='#000000')\n"
      "                       for size in (0.0, 8.0)]\n"
      "\n"
      "    def draw(self, pen, ctx):\n"
      "        for index, still in enumerate(self.stills):\n"
      "            if still is not None:\n"
      "                pen.image(still, 256 * index, 0, 256, 256)\n");
  const SkBitmap pixels = bakedStill(source, "shadowed", 512, 256);
  ASSERT_FALSE(pixels.isNull());
  // Where a world x on the ground lands across one 256-pixel bake: the
  // lens sees 2·400·tan(20°) units across it.
  const double unitsAcross = 2 * 400 * std::tan(20.0 * M_PI / 180.0);
  const auto column = [&](int bake, double x) {
    return bake * 256 + int(std::lround(128 + x * 256 / unitsAcross));
  };
  const auto row = [&](double y) {
    return int(std::lround(128 - y * 256 / unitsAcross));
  };
  const auto lightness = [&](int x, int y) {
    return int(SkColorGetG(pixels.getColor(x, y)));
  };
  // How many pixels a shadow's right edge takes to rise from a fifth of
  // the lit ground to four fifths of it, walking right from inside it.
  const auto edgeWidth = [&](int bake, double shadowX, double y) {
    const int lit = lightness(column(bake, 135), row(y));
    int dark = -1;
    int bright = -1;
    for (int x = column(bake, shadowX); x < column(bake, 135); ++x) {
      const int value = lightness(x, row(y));
      if (dark < 0 && value > lit / 5) dark = x;
      if (value > lit * 4 / 5) {
        bright = x;
        break;
      }
    }
    return dark < 0 || bright < 0 ? -1 : bright - dark;
  };
  ASSERT_GT(lightness(column(1, 135), row(60)), 60) << "the ground is lit";
  ASSERT_LT(lightness(column(1, 45), row(60)), 30)
      << "the high slab shades the ground at its right";
  const int pointLow = edgeWidth(0, -30, -60);
  const int pointHigh = edgeWidth(0, 45, 60);
  const int sizedLow = edgeWidth(1, -30, -60);
  const int sizedHigh = edgeWidth(1, 45, 60);
  ASSERT_GE(pointLow, 0);
  ASSERT_GE(pointHigh, 0);
  ASSERT_GE(sizedLow, 0);
  ASSERT_GE(sizedHigh, 0);
  EXPECT_GT(sizedHigh, sizedLow + 5)
      << "a sized sun's penumbra widens with the caster's height";
  EXPECT_GT(sizedHigh, pointHigh + 5)
      << "a point sun casts the same shadow sharp";
  EXPECT_LE(pointHigh, pointLow + 3)
      << "a point sun's edge is as sharp high as low";
}

/** A FRESH PAGE WEARING @p content, described before it first paints and
 *  baked once, beside the same page baked from a frame described after
 *  that paint: what the two halves of the picture show. */
std::pair<SkBitmap, SkBitmap> firstAndSecondDescriptions(
    std::string_view name, const std::string& content) {
  PythonSource source(name);
  source.write(
      "entry.py",
      "from sigil import compose, material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(192, 96), background='#000000', captureAt=0)\n"
      "class Described:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        page = ctx.page((64, 64))\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 200)\n"
      "        lens.target = (0, 0, 0)\n"
      "        tree = " +
          content +
          "\n"
          "        def frame():\n"
          "            eye = world.camera(lens, "
          "toneCurve=world.ToneCurve.Linear)"
          ".key('eye')\n"
          "            lamp = "
          "world.pointLight(intensity=world.intensityAt(0.5, "
          "150.0)).key('lamp').at((20, 30, 150))\n"
          "            card = world.mesh(mesh.quad(120, 90)).key('card')"
          ".fill(page.surface())\n"
          "            return (world.Frame(world.group(eye, lamp, card)"
          ".key('set'))\n"
          "                    .pass_(page.render(tree)))\n"
          "        self.stills = [ctx.bakeScene3D(frame(), (96, 96), "
          "camera=lens, background='#000000') for _ in range(2)]\n"
          "\n"
          "    def draw(self, pen, ctx):\n"
          "        for index, still in enumerate(self.stills):\n"
          "            if still is not None:\n"
          "                pen.image(still, 96 * index, 0, 96, 96)\n");
  const SkBitmap both = bakedStill(source, name, 192, 96);
  SkBitmap first, second;
  if (both.isNull()) return {};
  both.extractSubset(&first, SkIRect::MakeXYWH(0, 0, 96, 96));
  both.extractSubset(&second, SkIRect::MakeXYWH(96, 0, 96, 96));
  return {first, second};
}

/** How far apart two pictures of one size are: the largest difference of
 *  any channel of any pixel. */
int largestDifference(const SkBitmap& first, const SkBitmap& second) {
  int largest = 0;
  for (int y = 0; y < first.height(); ++y)
    for (int x = 0; x < first.width(); ++x) {
      const SkColor a = first.getColor(x, y);
      const SkColor b = second.getColor(x, y);
      for (int shift : {0, 8, 16, 24})
        largest = std::max(largest, std::abs(int((a >> shift) & 0xff) -
                                             int((b >> shift) & 0xff)));
    }
  return largest;
}

TEST_F(SketchPythonBakes,
       APageDescribedBeforeItsFirstPaintIsWornWithTheLobesItPaints) {
  const std::vector<std::pair<std::string, std::string>> contents = {
      {"brushed",
       "compose.box().width(64).height(64).fill(material.from_('#e6e6e6')"
       ".surface(metallic=1.0, roughness=0.35).brushed(strength=0.9))"},
      {"anisotropic",
       "compose.box().width(64).height(64).fill(material.from_('#e6e6e6')"
       ".surface(metallic=1.0, roughness=0.35, anisotropy=0.9))"},
  };
  for (const auto& [name, content] : contents) {
    SCOPED_TRACE(name);
    const auto [first, second] = firstAndSecondDescriptions(
        "sigil_python_first_description_" + name, content);
    ASSERT_FALSE(first.isNull());
    EXPECT_LE(largestDifference(first, second), 1)
        << "the first bake wears the page as a later description does";
  }
}

TEST_F(SketchPythonBakes, AKeyframedTurnBakedAtAMomentStandsBetweenItsSteps) {
  PythonSource source("sigil_python_keyframed_bake");
  // A green card turned about y by a path of three keyframes and no
  // `from_`: it starts at -80 degrees, reaches 0 a second later and 80 a
  // second after that. Baked at four moments side by side, the card's
  // width on the middle row is how far it has turned: thin at the start
  // and at rest, whole at 0, and in between half way through the first
  // step.
  source.write(
      "entry.py",
      "from sigil import motion, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.material import surface as surfaces\n"
      "from sigil.motion import ease\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "MOMENTS = (0.0, 0.5, 1.0, 5.0)\n"
      "\n"
      "@sketch(size=(384, 96), background='#000000', captureAt=0)\n"
      "class Turned:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        turn = motion.animate(keyframes=[\n"
      "            motion.Keyframe(-80, 0.0),\n"
      "            motion.Keyframe(0, 1.0, ease.linear),\n"
      "            motion.Keyframe(80, 1.0, ease.linear)])\n"
      "        card = (world.mesh(mesh.quad(160, 100)).key('card')\n"
      "                .rotateY(turn)\n"
      "                .fill(surfaces.unlit(\n"
      "                    surfaces.SurfaceParameters(baseColor='#00ff00'))))\n"
      "        frame = world.Frame(world.group(card).key('set'))\n"
      "        self.stills = [ctx.bakeScene3D(frame, (96, 96), camera=lens,\n"
      "                                       background='#000000', at=at)\n"
      "                       for at in MOMENTS]\n"
      "\n"
      "    def draw(self, pen, ctx):\n"
      "        for index, still in enumerate(self.stills):\n"
      "            if still is not None:\n"
      "                pen.image(still, 96 * index, 0, 96, 96)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "turned");
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(384, 96));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(384, 96));
  ASSERT_TRUE(surface->readPixels(pixels, 0, 0));
  const auto widthAt = [&pixels](int panel) {
    int width = 0;
    for (int x = panel * 96; x < (panel + 1) * 96; ++x) {
      const SkColor colour = pixels.getColor(x, 48);
      if (SkColorGetG(colour) > 160 && SkColorGetR(colour) < 80) ++width;
    }
    return width;
  };
  const int start = widthAt(0);
  const int turned = widthAt(1);
  const int whole = widthAt(2);
  const int rest = widthAt(3);
  ASSERT_GT(whole, 30) << "the card square on fills its share of the row";
  EXPECT_LT(start, whole * 35 / 100) << "the path starts at its first step";
  EXPECT_LT(rest, whole * 35 / 100) << "the path rests at its last step";
  EXPECT_GT(turned, whole * 60 / 100)
      << "half way through the first step the card has turned toward 0";
  EXPECT_LT(turned, whole * 92 / 100)
      << "half way through the first step the card is still turned";
}

TEST_F(SketchPythonBakes, AStillIsItsSizeTimesTheDensityTheSketchIsDrawnAt) {
  PythonSource source("sigil_python_bake_density");
  // One frame baked at one size in canvas units: at the density the sketch
  // is drawn at before a host declares one, which is the scene2d runtime's
  // own; at two densities stated; and again after the sketch declares an
  // oversample of its own, which is then the density it is drawn at.
  source.write(
      "entry.py",
      "import builtins\n"
      "from sigil import compose, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.material import surface as surfaces\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(64, 48), background='#000000', captureAt=0)\n"
      "class Dense:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        card = (world.mesh(mesh.quad(100, 60)).key('card')\n"
      "                .fill(surfaces.unlit(\n"
      "                    surfaces.SurfaceParameters(baseColor='#00ff00'))))\n"
      "        frame = world.Frame(world.group(card).key('set'))\n"
      "        def size(**stated):\n"
      "            still = ctx.bakeScene3D(frame, (40, 30), camera=lens,\n"
      "                                    **stated)\n"
      "            return (still.width(), still.height())\n"
      "        sizes = [size(), size(density=1), size(density=2.5)]\n"
      "        ctx.oversample(3)\n"
      "        sizes.append(size())\n"
      "        builtins._sigil_bake_density = sizes\n"
      "        ctx.render(compose.box())\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "dense");
  const pybind11::gil_scoped_acquire lock;
  const pybind11::module_ builtins = pybind11::module_::import("builtins");
  const auto sizes = builtins.attr("_sigil_bake_density")
                         .cast<std::vector<std::pair<int, int>>>();
  builtins.attr("__dict__").attr("pop")("_sigil_bake_density");
  ASSERT_EQ(sizes.size(), 4u);
  const auto scaled = [](float density) {
    return std::pair<int, int>((int)std::lround(40 * density),
                               (int)std::lround(30 * density));
  };
  EXPECT_EQ(sizes[0], scaled(sketch::kScene2DOversample))
      << "a still stating no density is baked at the scene2d runtime's own";
  EXPECT_EQ(sizes[1], scaled(1.0f)) << "a stated density is the still's";
  EXPECT_EQ(sizes[2], scaled(2.5f)) << "a stated density need not be whole";
  EXPECT_EQ(sizes[3], scaled(3.0f))
      << "an oversample the sketch declared is the density it bakes at";
}

}  // namespace
