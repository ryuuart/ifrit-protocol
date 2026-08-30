/** @file
 * The GPU executor: the seam value a frame carries when its passes are
 * to be performed on a device, and the dispatch from a stage to the file
 * that performs it.
 *
 * The frame's resources stay on the device. Nothing crosses back until
 * something asks for a resource by name — a declared readback, or the
 * picture being presented — which is what the image source installed on
 * the targets answers.
 */

#include "sigilworld/diligent/Runtime.h"

#include <sigilgeometry/mesh/pop/Pop.h>

#include <memory>
#include <string>
#include <utility>

#include "Gpu.h"

namespace sigil::world::diligent {

namespace {

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two frames carrying copies of one runtime
 *  compare equal while two separately made runtimes do not — they hold
 *  separate textures, separate pipelines and separate uploads. */
class GpuExecutor : public Executor {
 public:
  explicit GpuExecutor(std::shared_ptr<Gpu> gpu) : m_gpu(std::move(gpu)) {}

  bool operator==(const GpuExecutor& other) const {
    return m_gpu == other.m_gpu;
  }

  void beginFrame(Targets& targets) const override {
    m_gpu->resize(targets.extent());
    m_gpu->beginFrame();
    // Where a resource's pixels are, for the frame that asks for one.
    // Installed every frame because the targets a scene holds may have
    // been handed to another runtime in between.
    targets.source(
        [gpu = m_gpu](std::string_view name) { return gpu->read(name); });
  }

  void execute(const PassWork& work, const View& view,
               Targets& targets) const override {
    if (!work.pass) return;
    const Pass& pass = *work.pass;
    // A declared body replaces the stage's own work and keeps the
    // stage's declarations, so the ordering around it is unchanged. It
    // is handed the same view and the same targets whichever executor is
    // running, which is what makes an escape hatch portable.
    if (pass.body()) {
      pass.body()->run(view, targets);
      return;
    }
    switch (pass.stage()) {
      case Stage::Geometry:
        paintGeometry(*m_gpu, work, view, targets);
        return;
      case Stage::Compute:
        cook(work, targets);
        return;
      case Stage::Post:
        applyPost(*m_gpu, work);
        return;
    }
  }

  void endFrame(Targets&) const override { m_gpu->endFrame(); }

 private:
  /** A chain, cooked on the runtime the pass carries. That runtime is
   *  the host one until a point-operator kernel exists for the device;
   *  the cooked points are uploaded like any other geometry when a
   *  stamp is stood at them. */
  static void cook(const PassWork& work, Targets& targets) {
    const Pass& pass = *work.pass;
    const std::span<const std::string> writes = pass.writes();
    if (writes.empty() || pass.chain().empty()) return;
    *targets.points(writes.front()) =
        ::sigil::geometry::mesh::pop::cook(pass.chain(), pass.popRuntime());
  }

  std::shared_ptr<Gpu> m_gpu;
};

}  // namespace

Runtime runtime(Device& device) {
  installSlangCompiler();
  auto gpu = std::make_shared<Gpu>(device);

  dg::SamplerDesc sampler;
  sampler.MinFilter = dg::FILTER_TYPE_LINEAR;
  sampler.MagFilter = dg::FILTER_TYPE_LINEAR;
  sampler.MipFilter = dg::FILTER_TYPE_LINEAR;
  sampler.AddressU = dg::TEXTURE_ADDRESS_CLAMP;
  sampler.AddressV = dg::TEXTURE_ADDRESS_CLAMP;
  sampler.AddressW = dg::TEXTURE_ADDRESS_CLAMP;
  device.renderDevice()->CreateSampler(sampler, &gpu->sampler);

  // What an unfilled sampled slot reads: one white texel, so a body
  // multiplied by a map it was not given is the body.
  const uint32_t white = 0xFFFFFFFFu;
  dg::TextureDesc desc;
  desc.Name = "world white";
  desc.Type = dg::RESOURCE_DIM_TEX_2D;
  desc.Width = 1;
  desc.Height = 1;
  desc.Format = kColorFormat;
  desc.BindFlags = dg::BIND_SHADER_RESOURCE;
  desc.Usage = dg::USAGE_IMMUTABLE;
  dg::TextureSubResData level{&white, sizeof(white)};
  dg::TextureData data{&level, 1};
  device.renderDevice()->CreateTexture(desc, &data, &gpu->white);

  return Runtime{GpuExecutor{std::move(gpu)}};
}

}  // namespace sigil::world::diligent
