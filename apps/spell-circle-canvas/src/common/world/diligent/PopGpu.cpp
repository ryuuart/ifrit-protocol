/** @file
 * The point operators on the device: one compute pipeline built from the
 * kernel this build compiled, one buffer per lane, and one dispatch per
 * operator in chain order.
 *
 * NOTHING HERE COMPUTES AN OPERATOR either. The arithmetic is the
 * kernel's, the same source the host executor's C++ came out of, so what
 * is written here is the plumbing: which buffer each binding role takes,
 * a barrier between one dispatch and the next, and the one readback that
 * brings the cooked lanes home.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <sigilgeometry/mesh/pop/Kernel.h>
#include <sigilgeometry/device/Device.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <cstring>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "sigilworld/diligent/Pop.h"

namespace sigil::world::diligent {

using ::sigil::geometry::device::Device;

namespace dg = Diligent;
namespace gm = ::sigil::geometry::mesh;

namespace {

using gm::Cloud;
using gm::pop;
using gm::kernel::Args;
using gm::kernel::Dispatch;

/** How many lanes one dispatched group covers. It is the kernel's own
 *  `numthreads`, and the kernel drops the lanes past the point count
 *  itself, so a count that is not a multiple of it needs no second
 *  path. */
constexpr uint32_t kGroupSize = 64;

/** One lane on the device, and which cook last filled it. */
struct LaneBuffer {
  dg::RefCntAutoPtr<dg::IBuffer> buffer;
  dg::IBufferView* view = nullptr;
  /** The cook whose values it holds. A buffer kept from an earlier cook
   *  holds that cook's answer, and a lane must start every cook at what
   *  the generator seeded or at its own fill — so a stamp behind the
   *  current one means "fill me before you read me". */
  uint64_t stamp = 0;
};

/** WHAT A DEVICE COOK HOLDS between one chain and the next: the pipeline
 *  built from the kernel, the buffer its arguments are written into, and
 *  the lane buffers of the last cook — kept while their size still fits,
 *  because a chain cooked every frame would otherwise make and destroy
 *  every one of them every frame. */
struct PopGpu {
  explicit PopGpu(Device& d) : device(&d) {}
  ~PopGpu() {
    if (device && device->context()) device->context()->Flush();
  }

  Device* device = nullptr;
  dg::RefCntAutoPtr<dg::IPipelineState> pipeline;
  dg::RefCntAutoPtr<dg::IShaderResourceBinding> binding;
  dg::RefCntAutoPtr<dg::IBuffer> arguments;
  dg::RefCntAutoPtr<dg::IBuffer> table;
  size_t tableCapacity = 0;
  dg::RefCntAutoPtr<dg::IBuffer> staging;
  size_t stagingCapacity = 0;
  std::map<std::string, LaneBuffer, std::less<>> lanes;
  size_t held = 0;     ///< how many points the held lane buffers are sized for
  uint64_t cooks = 0;  ///< which cook is running, for the lane stamps

  /** Opens a cook over @p count points: the held lane buffers are kept
   *  when they are the right size and dropped when they are not, and
   *  every one of them is marked as holding the cook before's answer. */
  void beginCook(size_t count);

  /** The pipeline, built on the first cook. Null when the device refused
   *  the kernel, which stops the cook rather than cooking a wrong
   *  answer. */
  bool ready();
  /** The buffer lane @p name is held in, made on first touch and filled
   *  with @p values — the seeded lane where the generator wrote one, and
   *  the lane's own fill where it did not. */
  LaneBuffer* lane(const std::string& name, size_t count,
                   const std::vector<glm::vec4>* values);
  /** The stop table @p stops, uploaded. Never empty: a buffer of no
   *  elements is not a buffer a binding can take, and the kernel does
   *  not read a table it was told is empty. */
  dg::IBufferView* stopTable(const std::vector<glm::vec4>& stops);
  void dispatch(const Dispatch& work, size_t count);
  /** Every lane read back into @p into. */
  void readBack(pop::Lanes& into, size_t count);
};

bool PopGpu::ready() {
  if (pipeline) return true;
  dg::IRenderDevice* renderDevice = device->renderDevice();
  if (!renderDevice) return false;

  dg::RefCntAutoPtr<dg::IShader> cs;
  dg::ShaderCreateInfo ci;
  ci.Desc.Name = "pop kernel";
  ci.Desc.ShaderType = dg::SHADER_TYPE_COMPUTE;
  ci.Desc.UseCombinedTextureSamplers = true;
  // Asked of the kernel and not of the build's raw output: the words a
  // driver may fuse a multiply-add in are not the words this dispatch is
  // held to agree with the host about.
  const std::span<const uint32_t> words = gm::kernel::spirv();
  ci.ByteCode = words.data();
  ci.ByteCodeSize = words.size() * sizeof(uint32_t);
  renderDevice->CreateShader(ci, &cs);
  if (!cs) return false;

  dg::ComputePipelineStateCreateInfo info;
  info.PSODesc.Name = "pop kernel";
  // DYNAMIC, because every one of these is rebound per dispatch: the
  // lanes an operator binds change from one operator to the next.
  info.PSODesc.ResourceLayout.DefaultVariableType =
      dg::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
  info.pCS = cs;
  renderDevice->CreateComputePipelineState(info, &pipeline);
  if (!pipeline) return false;
  pipeline->CreateShaderResourceBinding(&binding, true);

  dg::BufferDesc desc;
  desc.Name = "pop kernel arguments";
  desc.Size = sizeof(Args);
  desc.BindFlags = dg::BIND_UNIFORM_BUFFER;
  // Written with UpdateBuffer rather than mapped: a cook is not
  // necessarily inside a frame, and a default buffer's write does not
  // ask to be.
  desc.Usage = dg::USAGE_DEFAULT;
  renderDevice->CreateBuffer(desc, nullptr, &arguments);
  return (bool)arguments && (bool)binding;
}

void PopGpu::beginCook(size_t count) {
  ++cooks;
  if (held != count) {
    lanes.clear();
    held = count;
  }
}

LaneBuffer* PopGpu::lane(const std::string& name, size_t count,
                         const std::vector<glm::vec4>* values) {
  std::vector<glm::vec4> fill;
  if (!values || values->size() != count)
    fill.assign(count, pop::laneFill(name));
  const glm::vec4* source =
      values && values->size() == count ? values->data() : fill.data();

  const auto found = lanes.find(name);
  if (found != lanes.end()) {
    if (found->second.stamp != cooks) {
      device->context()->UpdateBuffer(
          found->second.buffer, 0, count * sizeof(glm::vec4), source,
          dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      found->second.stamp = cooks;
    }
    return &found->second;
  }

  LaneBuffer made;
  made.stamp = cooks;
  dg::BufferDesc desc;
  desc.Name = "pop lane";
  desc.Size = count * sizeof(glm::vec4);
  // Read and written by the same dispatch, which is what a filter that
  // edits a lane in place is, so every lane is one writable resource in
  // one state rather than the same memory claimed two ways.
  desc.BindFlags = dg::BIND_UNORDERED_ACCESS;
  desc.Usage = dg::USAGE_DEFAULT;
  desc.Mode = dg::BUFFER_MODE_STRUCTURED;
  desc.ElementByteStride = sizeof(glm::vec4);
  dg::BufferData data{source, desc.Size};
  device->renderDevice()->CreateBuffer(desc, &data, &made.buffer);
  if (!made.buffer) return nullptr;
  made.view = made.buffer->GetDefaultView(dg::BUFFER_VIEW_UNORDERED_ACCESS);
  return &lanes.emplace(name, std::move(made)).first->second;
}

dg::IBufferView* PopGpu::stopTable(const std::vector<glm::vec4>& stops) {
  const size_t wanted = stops.empty() ? 1 : stops.size();
  if (!table || tableCapacity < wanted) {
    table.Release();
    dg::BufferDesc desc;
    desc.Name = "pop stop table";
    desc.Size = wanted * sizeof(glm::vec4);
    desc.BindFlags = dg::BIND_UNORDERED_ACCESS;
    desc.Usage = dg::USAGE_DEFAULT;
    desc.Mode = dg::BUFFER_MODE_STRUCTURED;
    desc.ElementByteStride = sizeof(glm::vec4);
    device->renderDevice()->CreateBuffer(desc, nullptr, &table);
    tableCapacity = table ? wanted : 0;
  }
  if (!table) return nullptr;
  if (!stops.empty())
    device->context()->UpdateBuffer(
        table, 0, stops.size() * sizeof(glm::vec4), stops.data(),
        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  return table->GetDefaultView(dg::BUFFER_VIEW_UNORDERED_ACCESS);
}

void PopGpu::dispatch(const Dispatch& work, size_t count) {
  dg::IDeviceContext* context = device->context();
  LaneBuffer* dst = lane(work.dst, count, nullptr);
  if (!dst) return;
  const auto role = [&](const std::string& name) -> dg::IBufferView* {
    if (name.empty()) return dst->view;
    LaneBuffer* held = lane(name, count, nullptr);
    return held ? held->view : dst->view;
  };
  dg::IBufferView* a = role(work.a);
  dg::IBufferView* b = role(work.b);
  dg::IBufferView* c = role(work.c);
  dg::IBufferView* mask = role(work.mask);
  dg::IBufferView* stops = stopTable(work.table);
  if (!stops) return;

  context->UpdateBuffer(arguments, 0, sizeof(Args), &work.args,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const auto bind = [&](const char* name, dg::IDeviceObject* object) {
    if (dg::IShaderResourceVariable* variable =
            binding->GetVariableByName(dg::SHADER_TYPE_COMPUTE, name))
      variable->Set(object);
  };
  bind("globalParams", arguments);
  bind("dst", dst->view);
  bind("srcA", a);
  bind("srcB", b);
  bind("srcC", c);
  bind("mask", mask);
  bind("table", stops);

  context->SetPipelineState(pipeline);
  context->CommitShaderResources(binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX =
      (dg::Uint32)((count + kGroupSize - 1) / kGroupSize);
  context->DispatchCompute(attribs);

  // ONE DISPATCH READS WHAT THE ONE BEFORE IT WROTE, and a lane never
  // changes state between them, so nothing about the bindings tells the
  // driver to wait. A transition from a state to itself is what does:
  // it is the barrier, and without it an operator would read the lane as
  // the operator before it found it.
  std::vector<dg::StateTransitionDesc> barriers;
  barriers.reserve(lanes.size());
  for (auto& [name, held] : lanes)
    barriers.emplace_back(
        held.buffer.RawPtr(), dg::RESOURCE_STATE_UNORDERED_ACCESS,
        dg::RESOURCE_STATE_UNORDERED_ACCESS, dg::STATE_TRANSITION_FLAG_NONE);
  if (!barriers.empty())
    context->TransitionResourceStates((dg::Uint32)barriers.size(),
                                      barriers.data());
}

void PopGpu::readBack(pop::Lanes& into, size_t count) {
  dg::IDeviceContext* context = device->context();
  const size_t stride = count * sizeof(glm::vec4);
  const size_t wanted = stride * lanes.size();
  if (wanted == 0) return;
  if (!staging || stagingCapacity < wanted) {
    staging.Release();
    dg::BufferDesc desc;
    desc.Name = "pop readback";
    desc.Size = wanted;
    desc.Usage = dg::USAGE_STAGING;
    desc.CPUAccessFlags = dg::CPU_ACCESS_READ;
    desc.BindFlags = dg::BIND_NONE;
    device->renderDevice()->CreateBuffer(desc, nullptr, &staging);
    stagingCapacity = staging ? wanted : 0;
  }
  if (!staging) return;

  // EVERY LANE IN ONE CROSSING. A wait per lane would cost a round trip
  // per attribute a chain happens to touch; the copies are queued
  // together and waited on once.
  size_t at = 0;
  for (auto& [name, held] : lanes) {
    context->CopyBuffer(held.buffer, 0,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, staging,
                        (dg::Uint64)at, (dg::Uint64)stride,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    at += stride;
  }
  context->WaitForIdle();

  void* mapped = nullptr;
  context->MapBuffer(staging, dg::MAP_READ, dg::MAP_FLAG_DO_NOT_WAIT, mapped);
  if (!mapped) return;
  at = 0;
  for (auto& [name, held] : lanes) {
    std::vector<glm::vec4>& values = into[name];
    values.resize(count);
    std::memcpy(values.data(), static_cast<const std::byte*>(mapped) + at,
                stride);
    at += stride;
  }
  context->UnmapBuffer(staging, dg::MAP_READ);
}

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two chains cooked through copies of one runtime
 *  meet the same pipeline and the same buffers. */
class PopExecutor : public pop::Executor {
 public:
  explicit PopExecutor(std::shared_ptr<PopGpu> gpu) : m_gpu(std::move(gpu)) {}

  bool operator==(const PopExecutor& other) const {
    return m_gpu == other.m_gpu;
  }

  std::string name() const override { return "diligent"; }

  bool supports(const pop::Op& op) const override {
    // A generator is not a map over points: it is run on the host and
    // uploaded, which is what makes the seed the two tiers share
    // bit-identical. Everything after it needs a kernel.
    const bool leads = std::holds_alternative<pop::SplineScatter>(op) ||
                       std::holds_alternative<pop::MeshScatter>(op) ||
                       std::holds_alternative<pop::PointSet>(op);
    return leads || gm::kernel::has(op);
  }

  Cloud cook(const pop::Chain& chain) const override {
    pop::Lanes lanes;
    const size_t count = pop::seedLanes(chain, &lanes);
    if (count == 0) return {};
    if (!m_gpu->ready()) return {};

    // The lanes the generator seeded are uploaded as they stand; every
    // other lane an operator names springs into being on the device
    // filled the way it would have on the host.
    m_gpu->beginCook(count);
    for (const auto& [name, values] : lanes) m_gpu->lane(name, count, &values);

    for (size_t opIndex = 1; opIndex < chain.size(); ++opIndex) {
      Dispatch work;
      if (!gm::kernel::describe(chain[opIndex], count, &work)) continue;
      m_gpu->dispatch(work, count);
    }

    m_gpu->readBack(lanes, count);
    return pop::exportLanes(lanes, count);
  }

 private:
  std::shared_ptr<PopGpu> m_gpu;
};

}  // namespace

pop::Runtime popRuntime(Device& device) {
  return pop::Runtime{PopExecutor{std::make_shared<PopGpu>(device)}};
}

}  // namespace sigil::world::diligent
