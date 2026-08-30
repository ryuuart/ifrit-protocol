/** @file
 * The texture feature under load: an atlas gridded and packed per region
 * count, a file name classified per name shape, and a region cut.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <sigilmaterial/texture/Atlas.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <string>
#include <vector>

using namespace sigil::material;

namespace {

sk_sp<SkImage> solid(int w, int h) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  bm.eraseColor(SK_ColorWHITE);
  bm.setImmutable();
  return bm.asImage();
}

void AtlasGrid(benchmark::State& state) {
  const int side = (int)state.range(0);
  const Texture sheet = Texture::of(solid(side * 8, side * 8));
  for (auto _ : state) {
    Atlas atlas = Atlas::grid(sheet, side, side);
    benchmark::DoNotOptimize(atlas);
  }
  state.SetItemsProcessed(state.iterations() * side * side);
}
BENCHMARK(AtlasGrid)->Arg(4)->Arg(16)->Arg(64);

void AtlasPack(benchmark::State& state) {
  const int count = (int)state.range(0);
  std::vector<std::pair<std::string, sk_sp<SkImage>>> images;
  for (int i = 0; i < count; ++i)
    images.emplace_back("s" + std::to_string(i),
                        solid(8 + (i * 7) % 24, 8 + (i * 11) % 24));
  for (auto _ : state) {
    Atlas atlas = Atlas::pack(images, 1);
    benchmark::DoNotOptimize(atlas);
  }
  state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(AtlasPack)->Arg(16)->Arg(64)->Arg(256);

void AtlasRegionCut(benchmark::State& state) {
  const Atlas atlas = Atlas::grid(Texture::of(solid(256, 256)), 8, 8);
  for (auto _ : state) {
    // A fresh texture each time so the cut is made, not recalled.
    Texture t = atlas.region((size_t)state.iterations() % 64);
    benchmark::DoNotOptimize(t.image());
  }
}
BENCHMARK(AtlasRegionCut);

void ClassifyName(benchmark::State& state) {
  static const char* kNames[] = {
      "Rock_BaseColor.png", "metal_plate_nor_gl_1k.png",
      "Metal049A_1K-PNG_AmbientOcclusion.png", "photo.png"};
  const char* name = kNames[state.range(0)];
  for (auto _ : state) {
    textures::Classified c = textures::classify(name);
    benchmark::DoNotOptimize(c);
  }
}
BENCHMARK(ClassifyName)->Arg(0)->Arg(1)->Arg(2)->Arg(3);

}  // namespace

BENCHMARK_MAIN();
