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
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/pop/Kernel.h>
#include <sigilgeometry/mesh/pop/device/Cook.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <boost/container/map.hpp>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "ComputeKernel.h"

namespace sigil::geometry::mesh {

using ::sigil::geometry::device::Device;

namespace dg = Diligent;

namespace {

using kernel::OperationArguments;
using kernel::OperationDispatch;

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
  compute::Kernel computeKernel;
  dg::RefCntAutoPtr<dg::IBuffer> table;
  size_t tableCapacity = 0;
  compute::Readback readback;
  boost::container::map<std::string, LaneBuffer, std::less<>> lanes;
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
  void dispatch(const OperationDispatch& work, size_t count);
  /** Every lane read back into @p into. */
  void readBack(pop::Lanes& into, size_t count);
};

bool PopGpu::ready() {
  if (computeKernel.pipeline) return true;
  dg::IRenderDevice* renderDevice = device->renderDevice();
  if (!renderDevice) return false;
  // Asked of the kernel and not of the build's raw output: the words a
  // driver may fuse a multiply-add in are not the words this dispatch is
  // held to agree with the host about.
  return computeKernel.build(*renderDevice, "pop kernel",
                             kernel::operationSpirv(),
                             sizeof(OperationArguments));
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

void PopGpu::dispatch(const OperationDispatch& work, size_t count) {
  dg::IDeviceContext* context = device->context();
  LaneBuffer* destination = lane(work.destination, count, nullptr);
  if (!destination) return;
  const auto role = [&](const std::string& name) -> dg::IBufferView* {
    if (name.empty()) return destination->view;
    LaneBuffer* held = lane(name, count, nullptr);
    return held ? held->view : destination->view;
  };
  dg::IBufferView* a = role(work.a);
  dg::IBufferView* b = role(work.b);
  dg::IBufferView* c = role(work.c);
  dg::IBufferView* mask = role(work.mask);
  dg::IBufferView* stops = stopTable(work.table);
  if (!stops) return;

  context->UpdateBuffer(computeKernel.arguments, 0, sizeof(OperationArguments),
                        &work.arguments,
                        dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  computeKernel.bind("globalParams", computeKernel.arguments);
  computeKernel.bind("destination", destination->view);
  computeKernel.bind("sourceA", a);
  computeKernel.bind("sourceB", b);
  computeKernel.bind("sourceC", c);
  computeKernel.bind("mask", mask);
  computeKernel.bind("table", stops);

  context->SetPipelineState(computeKernel.pipeline);
  context->CommitShaderResources(computeKernel.binding,
                                 dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DispatchComputeAttribs attribs;
  attribs.ThreadGroupCountX =
      (dg::Uint32)((count + kGroupSize - 1) / kGroupSize);
  context->DispatchCompute(attribs);

  // ONE DISPATCH READS WHAT THE ONE BEFORE IT WROTE, and the bindings
  // say nothing about that: the same lane is bound the same way to both.
  // A barrier over every lane is what makes the second wait, and without
  // it an operator would read the lane as the operator before it found
  // it.
  //
  // ITS OLD STATE IS THE BUFFER'S OWN, which is what leaving it unknown
  // asks for, because a lane does not reach a dispatch from one place. A
  // lane the dispatch before wrote stands in UNORDERED_ACCESS; one
  // re-uploaded at the head of this cook stands in COPY_DEST; one the
  // cook before read back stands in COPY_SOURCE. Naming a single state
  // for all three is wrong for two of them, and the backend says so and
  // then barriers from the state it was told rather than the state the
  // buffer is in — so the wait the barrier exists to impose is imposed
  // between the wrong pair of stages.
  std::vector<dg::StateTransitionDesc> barriers;
  barriers.reserve(lanes.size());
  for (auto& [name, held] : lanes)
    barriers.emplace_back(held.buffer.RawPtr(), dg::RESOURCE_STATE_UNKNOWN,
                          dg::RESOURCE_STATE_UNORDERED_ACCESS,
                          dg::STATE_TRANSITION_FLAG_NONE);
  if (!barriers.empty())
    context->TransitionResourceStates((dg::Uint32)barriers.size(),
                                      barriers.data());
}

void PopGpu::readBack(pop::Lanes& into, size_t count) {
  // ONLY THE LANES THIS COOK TOUCHED. A buffer kept from an earlier
  // chain still holds that chain's answer, and pouring it into the
  // export would hand the caller a custom lane no operator in this chain
  // ever named. The stamp is what says which is which.
  std::vector<dg::IBuffer*> mine;
  std::vector<glm::vec4*> destinations;
  for (auto& [name, held] : lanes) {
    if (held.stamp != cooks) continue;
    std::vector<glm::vec4>& values = into[name];
    values.resize(count);
    mine.push_back(held.buffer);
    destinations.push_back(values.data());
  }
  compute::readBack(*device, readback, "pop readback", mine, count,
                    destinations);
}

/** THE EXECUTOR. Its device state is shared rather than held, so the
 *  value is copyable and two chains cooked through copies of one runtime
 *  meet the same pipeline and the same buffers. */
class DeviceExecutor : public pop::Executor {
 public:
  explicit DeviceExecutor(std::shared_ptr<PopGpu> gpu)
      : m_gpu(std::move(gpu)) {}

  bool operator==(const DeviceExecutor& other) const {
    return m_gpu == other.m_gpu;
  }

  std::string name() const override { return "diligent"; }

  bool supports(const pop::Operation& operation) const override {
    // A generator is not a map over points: it is run on the host and
    // uploaded, which is what makes the seed the two tiers share
    // bit-identical. Everything after it needs a kernel.
    const bool leads = std::holds_alternative<pop::SplineScatter>(operation) ||
                       std::holds_alternative<pop::MeshScatter>(operation) ||
                       std::holds_alternative<pop::PointSet>(operation);
    return leads || kernel::has(operation);
  }

  Cloud cook(const pop::Chain& chain) const override {
    pop::Lanes lanes;
    const size_t count = pop::seedLanes(chain, &lanes);
    if (count == 0) return {};
    // A DEVICE THAT REFUSED THE KERNEL COOKS THE CHAIN ON THE HOST, the
    // way the stamping and the swept executors beside it do. The two
    // answers are the same bits, so this is where the chain was cooked
    // and not what it cooked to — which is the one thing a caller
    // holding a runtime must not have to check for.
    if (!m_gpu->ready()) return pop::cook(chain, pop::Runtime::cpu());

    // The lanes the generator seeded are uploaded as they stand; every
    // other lane an operator names springs into being on the device
    // filled the way it would have on the host.
    m_gpu->beginCook(count);
    for (const auto& [name, values] : lanes) m_gpu->lane(name, count, &values);

    for (size_t opIndex = 1; opIndex < chain.size(); ++opIndex) {
      OperationDispatch work;
      if (!kernel::describe(chain[opIndex], count, &work)) continue;
      m_gpu->dispatch(work, count);
    }

    m_gpu->readBack(lanes, count);
    return pop::exportLanes(lanes, count);
  }

 private:
  std::shared_ptr<PopGpu> m_gpu;
};

}  // namespace

pop::Runtime pop::deviceRuntime(Device& device) {
  return pop::Runtime{DeviceExecutor{std::make_shared<PopGpu>(device)}};
}

}  // namespace sigil::geometry::mesh
