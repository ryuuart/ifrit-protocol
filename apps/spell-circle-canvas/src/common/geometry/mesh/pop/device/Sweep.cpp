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

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <sigilgeometry/mesh/pop/Sweep.h>

namespace sigil::geometry::mesh::curve {

using ::sigil::geometry::device::Device;

namespace dg = Diligent;

namespace {

/** ONE BUFFER, and how many vectors it was made to hold. A sweep formed
 *  every frame would otherwise make and destroy every one of these every
 *  frame, so a buffer already large enough is kept. */
struct LaneBuffer {
  dg::RefCntAutoPtr<dg::IBuffer> buffer;
  dg::IBufferView* view = nullptr;
  size_t capacity = 0;

  /** The buffer, grown to hold at least @p count vectors. Null when the
   *  device refused it. */
  dg::IBufferView* sized(dg::IRenderDevice& device, const char* label,
                         size_t count) {
    if (view && capacity >= count) return view;
    buffer.Release();
    view = nullptr;
    capacity = 0;
    if (count == 0) return nullptr;
    dg::BufferDesc desc;
    desc.Name = label;
    desc.Size = count * sizeof(glm::vec4);
    // Read and written by one dispatch, so every lane is one writable
    // resource in one state rather than the same memory claimed two
    // ways.
    desc.BindFlags = dg::BIND_UNORDERED_ACCESS;
    desc.Usage = dg::USAGE_DEFAULT;
    desc.Mode = dg::BUFFER_MODE_STRUCTURED;
    desc.ElementByteStride = sizeof(glm::vec4);
    device.CreateBuffer(desc, nullptr, &buffer);
    if (!buffer) return nullptr;
    capacity = count;
    view = buffer->GetDefaultView(dg::BUFFER_VIEW_UNORDERED_ACCESS);
    return view;
  }
};

/** WHAT A DEVICE SWEEP HOLDS between one rail and the next: the pipeline
 *  built from the kernel, the buffer its arguments are written into, and
 *  the lane buffers of the last sweep. */
struct SweepGpu {
  explicit SweepGpu(Device& d) : device(&d) {}
  ~SweepGpu() {
    if (device && device->context()) device->context()->Flush();
  }

  Device* device = nullptr;
  dg::RefCntAutoPtr<dg::IPipelineState> pipeline;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> binding;
  dg::RefCntAutoPtr<dg::IBuffer> arguments;
  LaneBuffer railPosition;
  LaneBuffer railNormal;
  LaneBuffer railBinormal;
  LaneBuffer profile;
  LaneBuffer outPosition;
  LaneBuffer outNormal;
  dg::RefCntAutoPtr<dg::IBuffer> staging;
  size_t stagingCapacity = 0;

  /** The pipeline, built on the first sweep. False when the device
   *  refused the kernel, which stops the sweep rather than forming wrong
   *  vertices. */
  bool ready();
  /** @p values uploaded into @p lane, which is grown to hold them. */
  dg::IBufferView* upload(LaneBuffer& lane, const char* label,
                          const std::vector<glm::vec4>& values);
  bool dispatch(const kernel::Dispatch& work);
  /** The two output lanes, read back in one crossing. */
  void readBack(size_t count, glm::vec4* positions, glm::vec4* normals);
};

bool SweepGpu::ready() {
  if (pipeline) return true;
  dg::IRenderDevice* renderDevice = device->renderDevice();
  if (!renderDevice) return false;

  dg::RefCntAutoPtr<dg::IShader> cs;
  dg::ShaderCreateInfo ci;
  ci.Desc.Name = "sweep kernel";
  ci.Desc.ShaderType = dg::SHADER_TYPE_COMPUTE;
  ci.Desc.UseCombinedTextureSamplers = true;
  // Asked of the kernel and not of the build's raw output: the words a
  // driver may fuse a multiply-add in are not the words this dispatch is
  // held to agree with the host about.
  const std::span<const uint32_t> words = kernel::spirv();
  ci.ByteCode = words.data();
  ci.ByteCodeSize = words.size() * sizeof(uint32_t);
  renderDevice->CreateShader(ci, &cs);
  if (!cs) return false;

  dg::ComputePipelineStateCreateInfo info;
  info.PSODesc.Name = "sweep kernel";
  // DYNAMIC, because every one of these is rebound per dispatch: a rail
  // longer than the last one is a different buffer.
  info.PSODesc.ResourceLayout.DefaultVariableType =
      dg::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
  info.pCS = cs;
  renderDevice->CreateComputePipelineState(info, &pipeline);
  if (!pipeline) return false;
  pipeline->CreateShaderResourceBinding(&binding, true);

  dg::BufferDesc desc;
  desc.Name = "sweep kernel arguments";
  desc.Size = sizeof(kernel::Args);
  desc.BindFlags = dg::BIND_UNIFORM_BUFFER;
  // Written with UpdateBuffer rather than mapped: a sweep is not
  // necessarily inside a frame, and a default buffer's write does not
  // ask to be.
  desc.Usage = dg::USAGE_DEFAULT;
  renderDevice->CreateBuffer(desc, nullptr, &arguments);
  return (bool)arguments && (bool)binding;
}

dg::IBufferView* SweepGpu::upload(LaneBuffer& lane, const char* label,
                                  const std::vector<glm::vec4>& values) {
  dg::IBufferView* view =
      lane.sized(*device->renderDevice(), label, values.size());
  if (!view || values.empty()) return view;
  device->context()->UpdateBuffer(
      lane.buffer, 0, values.size() * sizeof(glm::vec4), values.data(),
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return view;
}

bool SweepGpu::dispatch(const kernel::Dispatch& work) {
  const size_t count = work.vertices();
  dg::IDeviceContext* context = device->context();
  dg::IRenderDevice& renderDevice = *device->renderDevice();

  dg::IBufferView* railPos =
      upload(railPosition, "sweep rail position", work.railPosition);
  dg::IBufferView* railNor =
      upload(railNormal, "sweep rail normal", work.railNormal);
  dg::IBufferView* railBin =
      upload(railBinormal, "sweep rail binormal", work.railBinormal);
  dg::IBufferView* contour = upload(profile, "sweep profile", work.profile);
  dg::IBufferView* outPos =
      outPosition.sized(renderDevice, "sweep position", count);
  dg::IBufferView* outNor =
      outNormal.sized(renderDevice, "sweep normal", count);
  if (!railPos || !railNor || !railBin || !contour || !outPos || !outNor)
    return false;

  context->UpdateBuffer(arguments, 0, sizeof(kernel::Args), &work.args,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const auto bind = [&](const char* name, dg::IDeviceObject* object) {
    if (dg::IShaderResourceVariable* variable =
            binding->GetVariableByName(dg::SHADER_TYPE_COMPUTE, name))
      variable->Set(object);
  };
  bind("globalParams", arguments);
  bind("railPosition", railPos);
  bind("railNormal", railNor);
  bind("railBinormal", railBin);
  bind("profile", contour);
  bind("outPosition", outPos);
  bind("outNormal", outNor);

  context->SetPipelineState(pipeline);
  context->CommitShaderResources(binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX =
      (dg::Uint32)((count + kernel::kGroupSize - 1) /
                   kernel::kGroupSize);
  context->DispatchCompute(attribs);
  return true;
}

void SweepGpu::readBack(size_t count, glm::vec4* positions,
                        glm::vec4* normals) {
  dg::IDeviceContext* context = device->context();
  const size_t stride = count * sizeof(glm::vec4);
  const size_t wanted = stride * 2;
  if (wanted == 0) return;
  if (!staging || stagingCapacity < wanted) {
    staging.Release();
    dg::BufferDesc desc;
    desc.Name = "sweep readback";
    desc.Size = wanted;
    desc.Usage = dg::USAGE_STAGING;
    desc.CPUAccessFlags = dg::CPU_ACCESS_READ;
    desc.BindFlags = dg::BIND_NONE;
    device->renderDevice()->CreateBuffer(desc, nullptr, &staging);
    stagingCapacity = staging ? wanted : 0;
  }
  if (!staging) return;

  // THE TWO LANES IN ONE CROSSING. A wait per lane would cost a round
  // trip per attribute a sweep happens to write; the copies are queued
  // together and waited on once.
  dg::IBuffer* const lanes[2] = {outPosition.buffer, outNormal.buffer};
  for (size_t i = 0; i < 2; ++i)
    context->CopyBuffer(lanes[i], 0,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging,
                        (dg::Uint64)i * (dg::Uint64)stride, (dg::Uint64)stride,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->WaitForIdle();

  void* mapped = nullptr;
  context->MapBuffer(staging, dg::MAP_READ, dg::MAP_FLAG_DO_NOT_WAIT, mapped);
  if (!mapped) return;
  const auto* bytes = static_cast<const std::byte*>(mapped);
  std::memcpy(positions, bytes, stride);
  std::memcpy(normals, bytes + stride, stride);
  context->UnmapBuffer(staging, dg::MAP_READ);
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

  void rings(const kernel::Dispatch& work, glm::vec4* positions,
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

SweepRuntime deviceRuntime(Device& device) {
  return SweepRuntime{DeviceSweepExecutor{std::make_shared<SweepGpu>(device)}};
}

}  // namespace sigil::geometry::mesh::curve
