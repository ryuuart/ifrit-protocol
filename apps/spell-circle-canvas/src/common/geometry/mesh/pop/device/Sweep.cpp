/** @file
 * The swept ring vertices on the device: one compute pipeline built from
 * the kernel this build compiled, one buffer per lane, one dispatch per
 * sweep, and the readback that brings the vertices home.
 *
 * NOTHING HERE COMPUTES A VERTEX. The arithmetic is the kernel's, the
 * same source the host executor's C++ came out of, so what is written
 * here is the plumbing: which buffer each binding takes, and the one
 * crossing that carries both lanes back at once.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/pop/device/Sweep.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ComputeKernel.h"

namespace sigil::geometry::mesh::pop {

using ::sigil::geometry::device::Device;

namespace dg = Diligent;

namespace {

/** WHAT A DEVICE SWEEP HOLDS between one rail and the next: the pipeline
 *  built from the kernel, the buffer its arguments are written into, and
 *  the lane buffers of the last sweep. */
struct SweepGpu {
  explicit SweepGpu(Device& d) : device(&d) {}
  ~SweepGpu() {
    if (device && device->context()) device->context()->Flush();
  }

  Device* device = nullptr;
  compute::Kernel computeKernel;
  compute::GrowingLane railPosition;
  compute::GrowingLane railNormal;
  compute::GrowingLane railBinormal;
  compute::GrowingLane profile;
  compute::GrowingLane outPosition;
  compute::GrowingLane outNormal;
  compute::Readback readback;

  /** The pipeline, built on the first sweep. False when the device
   *  refused the kernel, which stops the sweep rather than forming wrong
   *  vertices. */
  bool ready();
  bool dispatch(const kernel::SweepDispatch& work);
  /** The two output lanes, read back in one crossing. */
  void readBack(size_t count, glm::vec4* positions, glm::vec4* normals);
};

bool SweepGpu::ready() {
  if (computeKernel.pipeline) return true;
  dg::IRenderDevice* renderDevice = device->renderDevice();
  if (!renderDevice) return false;
  // Asked of the kernel and not of the build's raw output: the words a
  // driver may fuse a multiply-add in are not the words this dispatch is
  // held to agree with the host about.
  return computeKernel.build(*renderDevice, "sweep kernel",
                             kernel::sweepSpirv(), sizeof(kernel::SweepArgs));
}

bool SweepGpu::dispatch(const kernel::SweepDispatch& work) {
  const size_t count = work.vertices();
  dg::IDeviceContext* context = device->context();
  dg::IRenderDevice& renderDevice = *device->renderDevice();

  dg::IBufferView* railPos = compute::upload(
      *device, railPosition, "sweep rail position", work.railPosition);
  dg::IBufferView* railNor = compute::upload(
      *device, railNormal, "sweep rail normal", work.railNormal);
  dg::IBufferView* railBin = compute::upload(
      *device, railBinormal, "sweep rail binormal", work.railBinormal);
  dg::IBufferView* contour =
      compute::upload(*device, profile, "sweep profile", work.profile);
  dg::IBufferView* outPos =
      outPosition.sized(renderDevice, "sweep position", count);
  dg::IBufferView* outNor =
      outNormal.sized(renderDevice, "sweep normal", count);
  if (!railPos || !railNor || !railBin || !contour || !outPos || !outNor)
    return false;

  context->UpdateBuffer(computeKernel.arguments, 0, sizeof(kernel::SweepArgs),
                        &work.args,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  computeKernel.bind("globalParams", computeKernel.arguments);
  computeKernel.bind("railPosition", railPos);
  computeKernel.bind("railNormal", railNor);
  computeKernel.bind("railBinormal", railBin);
  computeKernel.bind("profile", contour);
  computeKernel.bind("outPosition", outPos);
  computeKernel.bind("outNormal", outNor);

  context->SetPipelineState(computeKernel.pipeline);
  context->CommitShaderResources(computeKernel.binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX =
      (dg::Uint32)((count + kernel::kSweepGroupSize - 1) /
                   kernel::kSweepGroupSize);
  context->DispatchCompute(attribs);
  return true;
}

void SweepGpu::readBack(size_t count, glm::vec4* positions,
                        glm::vec4* normals) {
  dg::IBuffer* const lanes[] = {outPosition.buffer, outNormal.buffer};
  glm::vec4* const into[] = {positions, normals};
  compute::readBack(*device, readback, "sweep readback", lanes, count, into);
}

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two sweeps through copies of one runtime meet
 *  the same pipeline and the same buffers. */
class DeviceSweepExecutor : public SweepExecutor {
 public:
  explicit DeviceSweepExecutor(std::shared_ptr<SweepGpu> gpu)
      : m_gpu(std::move(gpu)) {}

  bool operator==(const DeviceSweepExecutor& other) const {
    return m_gpu == other.m_gpu;
  }

  std::string name() const override { return "diligent"; }

  void rings(const kernel::SweepDispatch& work, glm::vec4* positions,
             glm::vec4* normals) const override {
    const size_t count = work.vertices();
    if (count == 0) return;
    // A DEVICE THAT REFUSED THE KERNEL FORMS THE RINGS ON THE HOST. The
    // two answers are the same bits, so this is where the vertices were
    // formed and not what they are — which is the one thing a caller
    // holding a runtime must not have to check for.
    if (!m_gpu->ready() || !m_gpu->dispatch(work)) {
      kernel::run(work, positions, normals);
      return;
    }
    m_gpu->readBack(count, positions, normals);
  }

 private:
  std::shared_ptr<SweepGpu> m_gpu;
};

}  // namespace

SweepRuntime sweepDeviceRuntime(Device& device) {
  return SweepRuntime{DeviceSweepExecutor{std::make_shared<SweepGpu>(device)}};
}

}  // namespace sigil::geometry::mesh::pop
