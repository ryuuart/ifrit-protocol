/** @file
 * The stamped vertices on the device: one compute pipeline built from
 * the kernel this build compiled, one buffer per lane, one dispatch per
 * stamping, and the readback that brings the vertices home.
 *
 * NOTHING HERE COMPUTES A VERTEX. The arithmetic is the kernel's, the
 * same source the host executor's C++ came out of, so what is written
 * here is the plumbing: which buffer each binding takes, and the one
 * crossing that carries all three lanes back at once.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/pop/device/Stamp.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ComputeKernel.h"

namespace sigil::geometry::mesh::points {

using ::sigil::geometry::device::Device;

namespace dg = Diligent;

namespace {

/** WHAT A DEVICE STAMPING HOLDS between one cloud and the next: the
 *  pipeline built from the kernel, the buffer its arguments are written
 *  into, and the lane buffers of the last stamping. */
struct StampGpu {
  explicit StampGpu(Device& d) : device(&d) {}
  ~StampGpu() {
    if (device && device->context()) device->context()->Flush();
  }

  Device* device = nullptr;
  compute::Kernel computeKernel;
  compute::GrowingLane stampPosition;
  compute::GrowingLane stampNormal;
  compute::GrowingLane stampUv;
  compute::GrowingLane stampColor;
  compute::GrowingLane pointOrigin;
  compute::GrowingLane pointDir;
  compute::GrowingLane pointColor;
  compute::GrowingLane pointTextureWindows;
  compute::GrowingLane outPosition;
  compute::GrowingLane outNormal;
  compute::GrowingLane outColor;
  compute::Readback readback;

  /** The pipeline, built on the first stamping. False when the device
   *  refused the kernel, which stops the dispatch rather than forming
   *  wrong vertices. */
  bool ready();
  bool dispatch(const kernel::StampDispatch& work);
  /** The three output lanes, read back in one crossing. */
  void readBack(size_t count, glm::vec4* positions, glm::vec4* normals,
                glm::vec4* colors);
};

bool StampGpu::ready() {
  if (computeKernel.pipeline) return true;
  dg::IRenderDevice* renderDevice = device->renderDevice();
  if (!renderDevice) return false;
  // Asked of the kernel and not of the build's raw output: the words a
  // driver may fuse a multiply-add in are not the words this dispatch is
  // held to agree with the host about.
  return computeKernel.build(*renderDevice, "stamp kernel",
                             kernel::stampSpirv(), sizeof(kernel::StampArgs));
}

bool StampGpu::dispatch(const kernel::StampDispatch& work) {
  const size_t count = work.vertices();
  dg::IDeviceContext* context = device->context();
  dg::IRenderDevice& renderDevice = *device->renderDevice();

  dg::IBufferView* sp = compute::upload(*device, stampPosition,
                                        "stamp position", work.stampPosition);
  dg::IBufferView* sn =
      compute::upload(*device, stampNormal, "stamp normal", work.stampNormal);
  dg::IBufferView* su =
      compute::upload(*device, stampUv, "stamp uv", work.stampUv);
  dg::IBufferView* sc =
      compute::upload(*device, stampColor, "stamp colour", work.stampColor);
  dg::IBufferView* po = compute::upload(*device, pointOrigin,
                                        "stamp point origin", work.pointOrigin);
  dg::IBufferView* pd =
      compute::upload(*device, pointDir, "stamp point dir", work.pointDir);
  dg::IBufferView* pc = compute::upload(*device, pointColor,
                                        "stamp point colour", work.pointColor);
  dg::IBufferView* pt =
      compute::upload(*device, pointTextureWindows, "stamp point tex",
                      work.pointTextureWindows);
  dg::IBufferView* outPos =
      outPosition.sized(renderDevice, "stamp out position", count);
  dg::IBufferView* outNor =
      outNormal.sized(renderDevice, "stamp out normal", count);
  dg::IBufferView* outCol =
      outColor.sized(renderDevice, "stamp out colour", count);
  if (!sp || !sn || !su || !sc || !po || !pd || !pc || !pt || !outPos ||
      !outNor || !outCol)
    return false;

  context->UpdateBuffer(computeKernel.arguments, 0, sizeof(kernel::StampArgs),
                        &work.args,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  computeKernel.bind("globalParams", computeKernel.arguments);
  computeKernel.bind("stampPosition", sp);
  computeKernel.bind("stampNormal", sn);
  computeKernel.bind("stampUv", su);
  computeKernel.bind("stampColor", sc);
  computeKernel.bind("pointOrigin", po);
  computeKernel.bind("pointDir", pd);
  computeKernel.bind("pointColor", pc);
  computeKernel.bind("pointTextureWindows", pt);
  computeKernel.bind("outPosition", outPos);
  computeKernel.bind("outNormal", outNor);
  computeKernel.bind("outColor", outCol);

  context->SetPipelineState(computeKernel.pipeline);
  context->CommitShaderResources(computeKernel.binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX =
      (dg::Uint32)((count + kernel::kStampGroupSize - 1) /
                   kernel::kStampGroupSize);
  context->DispatchCompute(attribs);
  return true;
}

void StampGpu::readBack(size_t count, glm::vec4* positions, glm::vec4* normals,
                        glm::vec4* colors) {
  dg::IBuffer* const lanes[] = {outPosition.buffer, outNormal.buffer,
                                outColor.buffer};
  glm::vec4* const into[] = {positions, normals, colors};
  compute::readBack(*device, readback, "stamp readback", lanes, count, into);
}

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two stampings through copies of one runtime
 *  meet the same pipeline and the same buffers. */
class DeviceStampExecutor : public StampExecutor {
 public:
  explicit DeviceStampExecutor(std::shared_ptr<StampGpu> gpu)
      : m_gpu(std::move(gpu)) {}

  bool operator==(const DeviceStampExecutor& other) const {
    return m_gpu == other.m_gpu;
  }

  std::string name() const override { return "diligent"; }

  void vertices(const kernel::StampDispatch& work, glm::vec4* positions,
                glm::vec4* normals, glm::vec4* colors) const override {
    const size_t count = work.vertices();
    if (count == 0) return;
    // A DEVICE THAT REFUSED THE KERNEL FORMS THE VERTICES ON THE HOST.
    // The two answers are the same bits, so this is where they were
    // formed and not what they are — which is the one thing a caller
    // holding a runtime must not have to check for.
    if (!m_gpu->ready() || !m_gpu->dispatch(work)) {
      kernel::run(work, positions, normals, colors);
      return;
    }
    m_gpu->readBack(count, positions, normals, colors);
  }

 private:
  std::shared_ptr<StampGpu> m_gpu;
};

}  // namespace

StampRuntime deviceRuntime(Device& device) {
  return StampRuntime{DeviceStampExecutor{std::make_shared<StampGpu>(device)}};
}

}  // namespace sigil::geometry::mesh::points
