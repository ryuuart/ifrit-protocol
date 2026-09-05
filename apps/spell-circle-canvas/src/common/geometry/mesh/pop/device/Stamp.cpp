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
#include <sigilgeometry/mesh/pop/Stamp.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::geometry::mesh::points {

using ::sigil::geometry::device::Device;

namespace dg = Diligent;

namespace {

/** How many lanes the readback carries home: the position, the normal
 *  and the colour. */
constexpr size_t kOutputLanes = 3;

/** ONE BUFFER, and how many vectors it was made to hold. A stamping
 *  formed every frame would otherwise make and destroy every one of
 *  these every frame, so a buffer already large enough is kept. */
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

/** WHAT A DEVICE STAMPING HOLDS between one cloud and the next: the
 *  pipeline built from the kernel, the buffer its arguments are written
 *  into, and the lane buffers of the last stamping. */
struct StampGpu {
  explicit StampGpu(Device& d) : device(&d) {}
  ~StampGpu() {
    if (device && device->context()) device->context()->Flush();
  }

  Device* device = nullptr;
  dg::RefCntAutoPtr<dg::IPipelineState> pipeline;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> binding;
  dg::RefCntAutoPtr<dg::IBuffer> arguments;
  LaneBuffer stampPosition;
  LaneBuffer stampNormal;
  LaneBuffer stampUv;
  LaneBuffer stampColor;
  LaneBuffer pointOrigin;
  LaneBuffer pointDir;
  LaneBuffer pointColor;
  LaneBuffer pointTex;
  LaneBuffer outPosition;
  LaneBuffer outNormal;
  LaneBuffer outColor;
  dg::RefCntAutoPtr<dg::IBuffer> staging;
  size_t stagingCapacity = 0;

  /** The pipeline, built on the first stamping. False when the device
   *  refused the kernel, which stops the dispatch rather than forming
   *  wrong vertices. */
  bool ready();
  /** @p values uploaded into @p lane, which is grown to hold them. */
  dg::IBufferView* upload(LaneBuffer& lane, const char* label,
                          const std::vector<glm::vec4>& values);
  bool dispatch(const kernel::StampDispatch& work);
  /** The three output lanes, read back in one crossing. */
  void readBack(size_t count, glm::vec4* positions, glm::vec4* normals,
                glm::vec4* colors);
};

bool StampGpu::ready() {
  if (pipeline) return true;
  dg::IRenderDevice* renderDevice = device->renderDevice();
  if (!renderDevice) return false;

  dg::RefCntAutoPtr<dg::IShader> cs;
  dg::ShaderCreateInfo ci;
  ci.Desc.Name = "stamp kernel";
  ci.Desc.ShaderType = dg::SHADER_TYPE_COMPUTE;
  ci.Desc.UseCombinedTextureSamplers = true;
  // Asked of the kernel and not of the build's raw output: the words a
  // driver may fuse a multiply-add in are not the words this dispatch is
  // held to agree with the host about.
  const std::span<const uint32_t> words = kernel::stampSpirv();
  ci.ByteCode = words.data();
  ci.ByteCodeSize = words.size() * sizeof(uint32_t);
  renderDevice->CreateShader(ci, &cs);
  if (!cs) return false;

  dg::ComputePipelineStateCreateInfo info;
  info.PSODesc.Name = "stamp kernel";
  // DYNAMIC, because every one of these is rebound per dispatch: a cloud
  // longer than the last one is a different buffer.
  info.PSODesc.ResourceLayout.DefaultVariableType =
      dg::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
  info.pCS = cs;
  renderDevice->CreateComputePipelineState(info, &pipeline);
  if (!pipeline) return false;
  pipeline->CreateShaderResourceBinding(&binding, true);

  dg::BufferDesc desc;
  desc.Name = "stamp kernel arguments";
  desc.Size = sizeof(kernel::StampArgs);
  desc.BindFlags = dg::BIND_UNIFORM_BUFFER;
  // Written with UpdateBuffer rather than mapped: a stamping is not
  // necessarily inside a frame, and a default buffer's write does not
  // ask to be.
  desc.Usage = dg::USAGE_DEFAULT;
  renderDevice->CreateBuffer(desc, nullptr, &arguments);
  return (bool)arguments && (bool)binding;
}

dg::IBufferView* StampGpu::upload(LaneBuffer& lane, const char* label,
                                  const std::vector<glm::vec4>& values) {
  dg::IBufferView* view =
      lane.sized(*device->renderDevice(), label, values.size());
  if (!view || values.empty()) return view;
  device->context()->UpdateBuffer(
      lane.buffer, 0, values.size() * sizeof(glm::vec4), values.data(),
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return view;
}

bool StampGpu::dispatch(const kernel::StampDispatch& work) {
  const size_t count = work.vertices();
  dg::IDeviceContext* context = device->context();
  dg::IRenderDevice& renderDevice = *device->renderDevice();

  dg::IBufferView* sp =
      upload(stampPosition, "stamp position", work.stampPosition);
  dg::IBufferView* sn = upload(stampNormal, "stamp normal", work.stampNormal);
  dg::IBufferView* su = upload(stampUv, "stamp uv", work.stampUv);
  dg::IBufferView* sc = upload(stampColor, "stamp colour", work.stampColor);
  dg::IBufferView* po =
      upload(pointOrigin, "stamp point origin", work.pointOrigin);
  dg::IBufferView* pd = upload(pointDir, "stamp point dir", work.pointDir);
  dg::IBufferView* pc =
      upload(pointColor, "stamp point colour", work.pointColor);
  dg::IBufferView* pt = upload(pointTex, "stamp point tex", work.pointTex);
  dg::IBufferView* outPos =
      outPosition.sized(renderDevice, "stamp out position", count);
  dg::IBufferView* outNor =
      outNormal.sized(renderDevice, "stamp out normal", count);
  dg::IBufferView* outCol =
      outColor.sized(renderDevice, "stamp out colour", count);
  if (!sp || !sn || !su || !sc || !po || !pd || !pc || !pt || !outPos ||
      !outNor || !outCol)
    return false;

  context->UpdateBuffer(arguments, 0, sizeof(kernel::StampArgs), &work.args,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const auto bind = [&](const char* name, dg::IDeviceObject* object) {
    if (dg::IShaderResourceVariable* variable =
            binding->GetVariableByName(dg::SHADER_TYPE_COMPUTE, name))
      variable->Set(object);
  };
  bind("globalParams", arguments);
  bind("stampPosition", sp);
  bind("stampNormal", sn);
  bind("stampUv", su);
  bind("stampColor", sc);
  bind("pointOrigin", po);
  bind("pointDir", pd);
  bind("pointColor", pc);
  bind("pointTex", pt);
  bind("outPosition", outPos);
  bind("outNormal", outNor);
  bind("outColor", outCol);

  context->SetPipelineState(pipeline);
  context->CommitShaderResources(binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX =
      (dg::Uint32)((count + kernel::kStampGroupSize - 1) / kernel::kStampGroupSize);
  context->DispatchCompute(attribs);
  return true;
}

void StampGpu::readBack(size_t count, glm::vec4* positions, glm::vec4* normals,
                        glm::vec4* colors) {
  dg::IDeviceContext* context = device->context();
  const size_t stride = count * sizeof(glm::vec4);
  const size_t wanted = stride * kOutputLanes;
  if (wanted == 0) return;
  if (!staging || stagingCapacity < wanted) {
    staging.Release();
    dg::BufferDesc desc;
    desc.Name = "stamp readback";
    desc.Size = wanted;
    desc.Usage = dg::USAGE_STAGING;
    desc.CPUAccessFlags = dg::CPU_ACCESS_READ;
    desc.BindFlags = dg::BIND_NONE;
    device->renderDevice()->CreateBuffer(desc, nullptr, &staging);
    stagingCapacity = staging ? wanted : 0;
  }
  if (!staging) return;

  // THE THREE LANES IN ONE CROSSING. A wait per lane would cost a round
  // trip per attribute a stamping happens to write; the copies are
  // queued together and waited on once.
  dg::IBuffer* const lanes[kOutputLanes] = {
      outPosition.buffer, outNormal.buffer, outColor.buffer};
  for (size_t i = 0; i < kOutputLanes; ++i)
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
  std::memcpy(colors, bytes + stride * 2, stride);
  context->UnmapBuffer(staging, dg::MAP_READ);
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
