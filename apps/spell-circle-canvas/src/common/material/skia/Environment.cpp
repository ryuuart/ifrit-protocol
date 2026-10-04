#include "Environment.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypes.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Terms.h>
#include <sigilshaders/MaterialSkia.h>
#if defined(SK_GRAPHITE)
#include <include/gpu/graphite/Surface.h>
#include <src/gpu/graphite/RecorderPriv.h>
#endif

#include <array>
#include <cmath>
#include <string>

namespace sigil::material::skia {
namespace {

const sk_sp<SkRuntimeEffect>& prefilter() {
  static const auto effect = [] {
    std::string source(termsSource(Target::SkSL));
    source += shaderSource("EnvironmentKernel.sksl");
    source += shaderSource("EnvironmentPrefilter.sksl");
    auto [compiled, error] =
        SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
    if (!compiled)
      SkDebugf("sigilmaterial environment prefilter: %s\n", error.c_str());
    return compiled;
  }();
  return effect;
}

}  // namespace

sk_sp<SkShader> EnvironmentPreparation::shader(
    const sk_sp<SkShader>& source, SkSize domain,
    skgpu::graphite::Recorder* recorder) {
  const std::lock_guard lock(m_mutex);
  uint32_t recorderId = 0;
#if defined(SK_GRAPHITE)
  if (recorder) recorderId = recorder->priv().uniqueID();
#endif
  Entry& entry = m_entries[recorder ? 1 : 0];
  if (source && source == entry.source && domain == entry.domain &&
      recorder == entry.recorder && recorderId == entry.recorderId &&
      entry.prepared)
    return entry.prepared;

  // Failed or changed inputs never keep a previously successful atlas.
  entry = {};
  if (!source || !std::isfinite(domain.width()) ||
      !std::isfinite(domain.height()) || domain.width() < 0 ||
      domain.height() < 0 || !prefilter())
    return nullptr;

  constexpr int width = 258, height = 338;
  const SkImageInfo info = SkImageInfo::Make(
      width, height, recorder ? kRGBA_F16_SkColorType : kRGBA_F32_SkColorType,
      kPremul_SkAlphaType);
  sk_sp<SkSurface> target;
  if (recorder) {
#if defined(SK_GRAPHITE)
    target = SkSurfaces::RenderTarget(recorder, info);
#endif
  } else
    target = SkSurfaces::Raster(info);
  if (!target) return nullptr;

  SkRuntimeShaderBuilder builder(prefilter());
  builder.child("uEnvironment") = source;
  builder.uniform("uEnvironmentSize") =
      std::array<float, 2>{domain.width(), domain.height()};
  const auto convolution = builder.makeShader();
  if (!convolution) return nullptr;
  SkPaint paint;
  paint.setShader(convolution);
  target->getCanvas()->drawPaint(paint);
  const auto image = target->makeImageSnapshot();
  if (!image) return nullptr;
  auto prepared = image->makeShader(SkTileMode::kClamp, SkTileMode::kClamp,
                                    SkSamplingOptions(SkFilterMode::kLinear));
  if (!prepared) return nullptr;
  entry.source = source;
  entry.domain = domain;
  entry.recorder = recorder;
  entry.recorderId = recorderId;
  entry.prepared = std::move(prepared);
  return entry.prepared;
}

}  // namespace sigil::material::skia
