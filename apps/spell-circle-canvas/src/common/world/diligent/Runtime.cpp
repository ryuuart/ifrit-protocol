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
#include <sigilworld/diligent/Pop.h>

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
  GpuExecutor(std::shared_ptr<Gpu> gpu,
              ::sigil::geometry::mesh::pop::Runtime points)
      : m_gpu(std::move(gpu)), m_points(std::move(points)) {}

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
  /** A chain, cooked on the device where the whole of it can be, and on
   *  the host where it cannot.
   *
   *  A pass carries the host runtime until it is given another, so a
   *  pass that named one of its own keeps it — asking a device to cook
   *  what an author sent somewhere else on purpose would be answering a
   *  question nobody asked. Otherwise the device's own runtime takes it,
   *  and only when EVERY operator in the chain has a kernel: a chain
   *  that would stop partway through is one this executor cooks on the
   *  host instead, whole, rather than declining it. */
  void cook(const PassWork& work, Targets& targets) const {
    namespace gm = ::sigil::geometry::mesh;
    const Pass& pass = *work.pass;
    const std::span<const std::string> writes = pass.writes();
    if (writes.empty() || pass.chain().empty()) return;
    const gm::pop::Runtime& declared = pass.popRuntime();
    gm::pop::Runtime on = declared;
    if (m_points && declared == gm::pop::Runtime::cpu()) {
      bool whole = true;
      for (const gm::pop::Op& op : pass.chain())
        whole = whole && m_points->supports(op);
      if (whole) on = m_points;
    }
    *targets.points(writes.front()) = gm::pop::cook(pass.chain(), on);
  }

  std::shared_ptr<Gpu> m_gpu;
  /** The point-operator runtime this device offers a compute pass. */
  ::sigil::geometry::mesh::pop::Runtime m_points;
};

}  // namespace

Runtime runtime(Device& device) {
  installSlangCompiler();
  return Runtime{GpuExecutor{makeGpu(device), popRuntime(device)}};
}

}  // namespace sigil::world::diligent
