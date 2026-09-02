/** @file
 * OCIO to LUT baking: the slices laid side by side in one image, and the
 * three transform factories over the trilinear recipe.
 */

#include "sigilmaterial/ocio/Ocio.h"

#include <OpenColorIO/OpenColorIO.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkTypes.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <string>
#include <vector>

namespace OCIO = OCIO_NAMESPACE;

namespace sigil::material::ocio {

namespace {

/** The 3D LUT lives in a 2D image: N slices of N×N laid side by side
 *  (width N·N, height N). x = slice·N + r-index, y = g-index; the body
 *  does hardware bilinear in-slice and lerps across two slices for the
 *  blue axis. Texel centres bound the in-slice sampling, so slices never
 *  bleed into each other. */
sk_sp<SkImage> bakeLut(const OCIO::ConstCPUProcessorRcPtr& cpu, int n) {
  const int w = n * n, h = n;
  std::vector<float> rgba((size_t)w * h * 4);
  for (int k = 0; k < n; ++k)        // blue slice
    for (int j = 0; j < n; ++j)      // green (y)
      for (int i = 0; i < n; ++i) {  // red (x within slice)
        float* px = &rgba[((size_t)j * w + (size_t)k * n + i) * 4];
        px[0] = (float)i / (float)(n - 1);
        px[1] = (float)j / (float)(n - 1);
        px[2] = (float)k / (float)(n - 1);
        px[3] = 1.0f;
      }
  OCIO::PackedImageDesc desc(rgba.data(), w, h, 4);
  cpu->apply(desc);
  // Display outputs live in [0,1]; clamp defends odd transforms.
  for (size_t i = 0; i < rgba.size(); i += 4) {
    rgba[i] = std::clamp(rgba[i], 0.0f, 1.0f);
    rgba[i + 1] = std::clamp(rgba[i + 1], 0.0f, 1.0f);
    rgba[i + 2] = std::clamp(rgba[i + 2], 0.0f, 1.0f);
  }
  const SkImageInfo f32Info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType);
  const SkPixmap f32Pixels(f32Info, rgba.data(), (size_t)w * 4 * sizeof(float));
  SkBitmap f16;
  if (!f16.tryAllocPixels(SkImageInfo::Make(w, h, kRGBA_F16_SkColorType,
                                            kUnpremul_SkAlphaType)))
    return nullptr;
  if (!f32Pixels.readPixels(f16.pixmap())) return nullptr;
  f16.setImmutable();
  return f16.asImage();
}

Material lutMaterial(sk_sp<SkImage> lutImage, int n) {
  if (!lutImage) return Material(lutRecipe());
  Material m(lutRecipe(), LutParams{(float)n});
  m.child("lut", Texture::of(std::move(lutImage)));
  return m;
}

OCIO::ConstConfigRcPtr loadConfig(std::string_view config) {
  const std::string s(config);
  if (s.rfind("ocio://", 0) == 0)
    return OCIO::Config::CreateFromBuiltinConfig(s.c_str());
  return OCIO::Config::CreateFromFile(s.c_str());
}

Material bake(const OCIO::ConstConfigRcPtr& config,
              const OCIO::ConstTransformRcPtr& transform, int lutSize) {
  const int n = std::clamp(lutSize, 8, 129);
  OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
  OCIO::ConstCPUProcessorRcPtr cpu = proc->getDefaultCPUProcessor();
  return lutMaterial(bakeLut(cpu, n), n);
}

}  // namespace

bool available() {
  try {
    return (bool)OCIO::Config::CreateRaw();
  } catch (const OCIO::Exception&) {
    return false;
  }
}

Material viewTransform(std::string_view config, std::string_view displayName,
                       std::string_view viewName, int lutSize) {
  try {
    OCIO::ConstConfigRcPtr cfg = loadConfig(config);
    OCIO::DisplayViewTransformRcPtr t = OCIO::DisplayViewTransform::Create();
    t->setSrc(OCIO::ROLE_SCENE_LINEAR);
    t->setDisplay(std::string(displayName).c_str());
    t->setView(std::string(viewName).c_str());
    return bake(cfg, t, lutSize);
  } catch (const OCIO::Exception& e) {
    SkDebugf(
        "sigilmaterial ocio::viewTransform(\"%.*s\", \"%.*s\", \"%.*s\"): "
        "%s\n",
        (int)config.size(), config.data(), (int)displayName.size(),
        displayName.data(), (int)viewName.size(), viewName.data(), e.what());
    // Fail soft AND helpfully: list what the config actually offers.
    try {
      OCIO::ConstConfigRcPtr cfg = loadConfig(config);
      for (int d = 0; d < cfg->getNumDisplays(); ++d) {
        const char* disp = cfg->getDisplay(d);
        SkDebugf("  display \"%s\": views:", disp);
        for (int v = 0; v < cfg->getNumViews(disp); ++v)
          SkDebugf(" \"%s\"", cfg->getView(disp, v));
        SkDebugf("\n");
      }
    } catch (...) {
      SkDebugf("  (the config could not be listed)\n");
    }
    return Material(lutRecipe());
  }
}

Material convert(std::string_view config, std::string_view src,
                 std::string_view dst, int lutSize) {
  try {
    OCIO::ConstConfigRcPtr cfg = loadConfig(config);
    OCIO::ColorSpaceTransformRcPtr t = OCIO::ColorSpaceTransform::Create();
    t->setSrc(std::string(src).c_str());
    t->setDst(std::string(dst).c_str());
    return bake(cfg, t, lutSize);
  } catch (const OCIO::Exception& e) {
    SkDebugf("sigilmaterial ocio::convert(\"%.*s\" -> \"%.*s\"): %s\n",
             (int)src.size(), src.data(), (int)dst.size(), dst.data(),
             e.what());
    return Material(lutRecipe());
  }
}

Material exponent(float gamma, int lutSize) {
  try {
    OCIO::ConstConfigRcPtr cfg = OCIO::Config::CreateRaw();
    OCIO::ExponentTransformRcPtr t = OCIO::ExponentTransform::Create();
    const double v[4] = {gamma, gamma, gamma, 1.0};
    t->setValue(v);
    return bake(cfg, t, lutSize);
  } catch (const OCIO::Exception& e) {
    SkDebugf("sigilmaterial ocio::exponent(%f): %s\n", gamma, e.what());
    return Material(lutRecipe());
  }
}

}  // namespace sigil::material::ocio
