#pragma once

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>

#include <array>
#include <cstdint>
#include <mutex>

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::material::skia {

// A resolved panorama becomes one bounded atlas of spherical GGX lobes
// and cosine-weighted diffuse light. The original source remains the
// sharp level, and rotation and intensity are applied when it is sampled.
// Diligent's precomputation requires its device and cubemap render targets;
// this executor works on the Skia shader and recorder supplied by its host.
class EnvironmentPreparation {
 public:
  sk_sp<SkShader> shader(const sk_sp<SkShader>& source, SkSize domain,
                         skgpu::graphite::Recorder* recorder);
  sk_sp<SkShader> retainedShader(
      skgpu::graphite::Recorder* recorder = nullptr) {
    const std::lock_guard lock(m_mutex);
    return m_entries[recorder ? 1 : 0].prepared;
  }

 private:
  struct Entry {
    sk_sp<SkShader> source;
    SkSize domain = SkSize::MakeEmpty();
    skgpu::graphite::Recorder* recorder = nullptr;
    uint32_t recorderId = 0;
    sk_sp<SkShader> prepared;
  };
  std::mutex m_mutex;
  // Picture and device resolves can alternate within one paint. Each
  // backend retains its current atlas without replacing the other.
  std::array<Entry, 2> m_entries;
};

}  // namespace sigil::material::skia
