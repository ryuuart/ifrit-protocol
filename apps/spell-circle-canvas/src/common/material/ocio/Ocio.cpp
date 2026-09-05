/** @file
 * OCIO baking: the test that decides whether a transform's channels are
 * independent, the response row for one that is and the LUT slices laid
 * side by side for one that is not, and the three transform factories
 * over them.
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
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace OCIO = OCIO_NAMESPACE;

namespace sigil::material::ocio {

namespace {

/** Display outputs live in [0,1]; clamping defends odd transforms. Alpha
 *  is left as the identity the bake wrote. */
void clampRgb(std::vector<float>& rgba) {
  for (size_t i = 0; i < rgba.size(); i += 4) {
    rgba[i] = std::clamp(rgba[i], 0.0f, 1.0f);
    rgba[i + 1] = std::clamp(rgba[i + 1], 0.0f, 1.0f);
    rgba[i + 2] = std::clamp(rgba[i + 2], 0.0f, 1.0f);
  }
}

/** @p rgba as an immutable unpremultiplied F16 image, the storage every
 *  bake is held in: F32 textures are not linearly filterable on Apple
 *  GPUs, so a sampler over one would fall back to point sampling and
 *  band. */
sk_sp<SkImage> f16Image(const std::vector<float>& rgba, int w, int h) {
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
  clampRgb(rgba);
  return f16Image(rgba, w, h);
}

/** Samples along a response row — one per eight-bit code, so a table
 *  drawn from the row is exact at every code an eight-bit surface can
 *  carry. */
constexpr int kResponseSize = 256;
/** How far the composed responses may sit from the transform before the
 *  channels are called dependent: half an eight-bit code, the largest
 *  error that can never change a code on an eight-bit surface. */
constexpr float kResponseTolerance = 0.5f / 255.0f;
/** Codes between the lattice values the independence proof samples —
 *  sixteen per axis, 0 through 255, every one of them a row sample, so
 *  the comparison reads the responses rather than interpolating them. */
constexpr int kProofStride = 17;

/** The row @p cpu answers on the grey ramp, WHEN the transform's
 *  channels are independent, and nothing when they are not.
 *
 *  A transform whose channels are independent is f(r, g, b) = (fr(r),
 *  fg(g), fb(b)), so it answers (fr(t), fg(t), fb(t)) at (t, t, t): one
 *  pass over the grey ramp already carries all three responses, in the
 *  channels they belong to. The ramp alone proves nothing, though —
 *  independence is a claim about MIXED colours, which the diagonal never
 *  visits — so the row is checked against a lattice of them: every
 *  sampled colour must equal its own three responses composed. Anything
 *  that mixes channels, a matrix or a 3D LUT of its own, parts from that
 *  somewhere on the lattice. */
std::optional<std::vector<float>> bakeResponse(
    const OCIO::ConstCPUProcessorRcPtr& cpu) {
  std::vector<float> row((size_t)kResponseSize * 4);
  for (int i = 0; i < kResponseSize; ++i) {
    const float t = (float)i / (float)(kResponseSize - 1);
    float* px = &row[(size_t)i * 4];
    px[0] = px[1] = px[2] = t;
    px[3] = 1.0f;
  }
  OCIO::PackedImageDesc rowDesc(row.data(), kResponseSize, 1, 4);
  cpu->apply(rowDesc);
  clampRgb(row);

  constexpr int kAxis = (kResponseSize - 1) / kProofStride + 1;
  std::vector<float> grid((size_t)kAxis * kAxis * kAxis * 4);
  size_t at = 0;
  for (int r = 0; r < kAxis; ++r)
    for (int g = 0; g < kAxis; ++g)
      for (int b = 0; b < kAxis; ++b) {
        grid[at++] = (float)(r * kProofStride) / (float)(kResponseSize - 1);
        grid[at++] = (float)(g * kProofStride) / (float)(kResponseSize - 1);
        grid[at++] = (float)(b * kProofStride) / (float)(kResponseSize - 1);
        grid[at++] = 1.0f;
      }
  OCIO::PackedImageDesc gridDesc(grid.data(), kAxis * kAxis * kAxis, 1, 4);
  cpu->apply(gridDesc);
  clampRgb(grid);

  at = 0;
  for (int r = 0; r < kAxis; ++r)
    for (int g = 0; g < kAxis; ++g)
      for (int b = 0; b < kAxis; ++b) {
        const float expected[3] = {row[(size_t)(r * kProofStride) * 4],
                                   row[(size_t)(g * kProofStride) * 4 + 1],
                                   row[(size_t)(b * kProofStride) * 4 + 2]};
        for (int c = 0; c < 3; ++c)
          if (std::fabs(grid[at + (size_t)c] - expected[c]) >
              kResponseTolerance)
            return std::nullopt;
        at += 4;
      }
  return row;
}

Material lutMaterial(sk_sp<SkImage> lutImage, int n) {
  if (!lutImage) return Material(lutRecipe());
  Material m(lutRecipe(), LutParams{(float)n});
  m.child("lut", Texture::of(std::move(lutImage)));
  return m;
}

Material responseMaterial(sk_sp<SkImage> rowImage) {
  if (!rowImage) return Material(lutRecipe());
  Material m(responseRecipe(), LutParams{(float)kResponseSize});
  m.child("lut", Texture::of(std::move(rowImage)));
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
  // A transform whose channels are independent carries no more than one
  // response per channel: bake the row, which a renderer on an eight-bit
  // surface can run as a table with no program at all. The volume is for
  // transforms that actually need it.
  if (std::optional<std::vector<float>> row = bakeResponse(cpu))
    return responseMaterial(f16Image(*row, kResponseSize, 1));
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
