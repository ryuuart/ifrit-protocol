// OCIO → LUT Effect baking. Compiled only when OpenColorIO was found
// (SIGILCOMPOSE_ENABLE_OCIO). See Ocio.h for the pattern rationale.

#include "sigilcompose/Ocio.h"

#include <OpenColorIO/OpenColorIO.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkString.h>
#include <include/core/SkTypes.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <string>
#include <vector>

namespace OCIO = OCIO_NAMESPACE;

namespace sigil::compose::ocio {

namespace {

/** The 3D LUT lives in a 2D image: N slices of N×N laid side by side
 *  (width N·N, height N). x = slice·N + r-index, y = g-index; the SkSL
 *  sampler does hardware bilinear in-slice and lerps across two slices for
 *  the blue axis. Texel centers bound the in-slice sampling, so slices never
 *  bleed into each other. */
sk_sp<SkImage> bakeLut(const OCIO::ConstCPUProcessorRcPtr &cpu, int n) {
  const int w = n * n, h = n;
  std::vector<float> rgba((size_t)w * h * 4);
  for (int k = 0; k < n; ++k)     // blue slice
    for (int j = 0; j < n; ++j)   // green (y)
      for (int i = 0; i < n; ++i) { // red (x within slice)
        float *px = &rgba[((size_t)j * w + (size_t)k * n + i) * 4];
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
  // Bake to F16, not F32: 32-bit-float textures are not linearly
  // filterable on Apple GPUs, so a Graphite host cannot promote an F32
  // LUT to a texture and the whole graded composite would be dropped.
  // Half precision is ample for a display LUT in [0,1].
  const SkImageInfo f32Info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType);
  const SkPixmap f32Pixels(f32Info, rgba.data(), (size_t)w * 4 * sizeof(float));
  SkBitmap f16;
  if (!f16.tryAllocPixels(
          SkImageInfo::Make(w, h, kRGBA_F16_SkColorType, kUnpremul_SkAlphaType)))
    return nullptr;
  if (!f32Pixels.readPixels(f16.pixmap()))
    return nullptr;
  f16.setImmutable();
  return f16.asImage();
}

/** Trilinear 3D-LUT application; unpremul→map→repremul so straight colors go
 *  through the transform (exact for the opaque case). */
constexpr const char *kLutSkSL = R"(
uniform shader content;
uniform shader lut;
uniform float lutSize;
half4 main(float2 xy) {
  half4 c = content.eval(xy);
  float a = max(float(c.a), 0.0001);
  float3 rgb = clamp(float3(c.rgb) / a, 0.0, 1.0);
  float n = lutSize;
  float3 p = rgb * (n - 1.0);
  float zf = floor(p.z);
  float zc = min(zf + 1.0, n - 1.0);
  float t = p.z - zf;
  float2 uv0 = float2(zf * n + p.x + 0.5, p.y + 0.5);
  float2 uv1 = float2(zc * n + p.x + 0.5, p.y + 0.5);
  float3 m = mix(float3(lut.eval(uv0).rgb), float3(lut.eval(uv1).rgb), t);
  return half4(half3(m * a), c.a);
}
)";

Effect lutEffect(sk_sp<SkImage> lutImage, int n) {
  if (!lutImage)
    return {};
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(kLutSkSL));
  if (!effect) {
    SkDebugf("sigilcompose ocio: LUT shader failed to compile: %s\n",
             err.c_str());
    return {};
  }
  SkRuntimeShaderBuilder builder(std::move(effect));
  builder.uniform("lutSize") = (float)n;
  builder.child("lut") = lutImage->makeShader(
      SkTileMode::kClamp, SkTileMode::kClamp,
      SkSamplingOptions(SkFilterMode::kLinear));
  return Effect::filter(
      SkImageFilters::RuntimeShader(builder, "content", nullptr));
}

OCIO::ConstConfigRcPtr loadConfig(std::string_view config) {
  const std::string s(config);
  if (s.rfind("ocio://", 0) == 0)
    return OCIO::Config::CreateFromBuiltinConfig(s.c_str());
  return OCIO::Config::CreateFromFile(s.c_str());
}

Effect bake(const OCIO::ConstConfigRcPtr &config,
            const OCIO::ConstTransformRcPtr &transform, int lutSize) {
  const int n = std::clamp(lutSize, 8, 129);
  OCIO::ConstProcessorRcPtr proc = config->getProcessor(transform);
  OCIO::ConstCPUProcessorRcPtr cpu = proc->getDefaultCPUProcessor();
  return lutEffect(bakeLut(cpu, n), n);
}

} // namespace

bool available() {
  try {
    return (bool)OCIO::Config::CreateRaw();
  } catch (const OCIO::Exception &) {
    return false;
  }
}

Effect display(std::string_view config, std::string_view displayName,
               std::string_view viewName, int lutSize) {
  try {
    OCIO::ConstConfigRcPtr cfg = loadConfig(config);
    OCIO::DisplayViewTransformRcPtr t = OCIO::DisplayViewTransform::Create();
    t->setSrc(OCIO::ROLE_SCENE_LINEAR);
    t->setDisplay(std::string(displayName).c_str());
    t->setView(std::string(viewName).c_str());
    return bake(cfg, t, lutSize);
  } catch (const OCIO::Exception &e) {
    SkDebugf("sigilcompose ocio::display(\"%.*s\", \"%.*s\", \"%.*s\"): %s\n",
             (int)config.size(), config.data(), (int)displayName.size(),
             displayName.data(), (int)viewName.size(), viewName.data(),
             e.what());
    // Fail soft AND helpfully: list what the config actually offers.
    try {
      OCIO::ConstConfigRcPtr cfg = loadConfig(config);
      for (int d = 0; d < cfg->getNumDisplays(); ++d) {
        const char *disp = cfg->getDisplay(d);
        SkDebugf("  display \"%s\": views:", disp);
        for (int v = 0; v < cfg->getNumViews(disp); ++v)
          SkDebugf(" \"%s\"", cfg->getView(disp, v));
        SkDebugf("\n");
      }
    } catch (...) {
    }
    return {};
  }
}

Effect convert(std::string_view config, std::string_view src,
               std::string_view dst, int lutSize) {
  try {
    OCIO::ConstConfigRcPtr cfg = loadConfig(config);
    OCIO::ColorSpaceTransformRcPtr t = OCIO::ColorSpaceTransform::Create();
    t->setSrc(std::string(src).c_str());
    t->setDst(std::string(dst).c_str());
    return bake(cfg, t, lutSize);
  } catch (const OCIO::Exception &e) {
    SkDebugf("sigilcompose ocio::convert(\"%.*s\" -> \"%.*s\"): %s\n",
             (int)src.size(), src.data(), (int)dst.size(), dst.data(),
             e.what());
    return {};
  }
}

Effect exponent(float gamma, int lutSize) {
  try {
    OCIO::ConstConfigRcPtr cfg = OCIO::Config::CreateRaw();
    OCIO::ExponentTransformRcPtr t = OCIO::ExponentTransform::Create();
    const double v[4] = {gamma, gamma, gamma, 1.0};
    t->setValue(v);
    return bake(cfg, t, lutSize);
  } catch (const OCIO::Exception &e) {
    SkDebugf("sigilcompose ocio::exponent(%f): %s\n", gamma, e.what());
    return {};
  }
}

} // namespace sigil::compose::ocio
