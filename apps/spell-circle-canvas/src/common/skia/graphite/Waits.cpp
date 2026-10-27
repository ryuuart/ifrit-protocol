// What this library notes about a recorder: the GPU waits its next
// inserted recording carries, and the door that submits a recorder with
// them.

#include "Waits.h"

#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/TextureImage.h>
#ifdef __APPLE__
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>
#endif

#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace sigil::skia {

namespace {

/** One wait: the event and the value it must reach. */
struct Wait {
  void* event = nullptr;
  uint64_t value = 0;
};

/** EVERY RECORDER'S WAITS, under one lock: a wait
 *  is registered on the thread that records the sampling draw and taken
 *  on whichever thread inserts, which is most often the same one but need
 *  not be. */
struct Registry {
  std::mutex mutex;
  boost::unordered_flat_map<const skgpu::graphite::Recorder*, std::vector<Wait>>
      waits;
};

Registry& registry() {
  // Never destroyed: a recorder submitting during static destruction
  // would otherwise reach a map that has gone.
  static auto* store = new Registry();
  return *store;
}

}  // namespace

void forgetRecorder(const skgpu::graphite::Recorder* recorder) {
  Registry& store = registry();
  const std::lock_guard<std::mutex> lock(store.mutex);
  store.waits.erase(recorder);
}

#ifdef __APPLE__
void waitBeforeSampling(skgpu::graphite::Recorder& recorder,
                        void* mtlSharedEvent, uint64_t value) {
  if (!mtlSharedEvent) return;
  Registry& store = registry();
  const std::lock_guard<std::mutex> lock(store.mutex);
  std::vector<Wait>& list = store.waits[&recorder];
  // A LATER VALUE OF THE SAME EVENT COVERS AN EARLIER ONE: an event only
  // rises, so a recorder that never inserted between two presentations
  // waits once, for the second.
  for (Wait& wait : list) {
    if (wait.event != mtlSharedEvent) continue;
    if (value > wait.value) wait.value = value;
    return;
  }
  list.push_back({mtlSharedEvent, value});
}

size_t pendingWaits(const skgpu::graphite::Recorder& recorder) {
  Registry& store = registry();
  const std::lock_guard<std::mutex> lock(store.mutex);
  const auto found = store.waits.find(&recorder);
  return found == store.waits.end() ? 0 : found->second.size();
}
#endif

PendingWaits::PendingWaits(const skgpu::graphite::Recorder* recorder) {
  if (!recorder) return;
  std::vector<Wait> taken;
  {
    Registry& store = registry();
    const std::lock_guard<std::mutex> lock(store.mutex);
    const auto found = store.waits.find(recorder);
    if (found == store.waits.end()) return;
    taken = std::move(found->second);
    store.waits.erase(found);
  }
#ifdef __APPLE__
  m_semaphores.reserve(taken.size());
  for (const Wait& wait : taken)
    m_semaphores.push_back(skgpu::graphite::BackendSemaphores::MakeMetal(
        static_cast<CFTypeRef>(wait.event), wait.value));
#endif
}

void PendingWaits::attach(skgpu::graphite::InsertRecordingInfo& insert) {
  if (m_semaphores.empty()) return;
  insert.fNumWaitSemaphores = m_semaphores.size();
  insert.fWaitSemaphores = m_semaphores.data();
}

bool GraphiteContext::submit(skgpu::graphite::Recorder& recorder,
                             skgpu::graphite::SyncToCpu sync) const {
  std::unique_ptr<skgpu::graphite::Recording> recording = recorder.snap();
  const std::unique_lock<std::mutex> lock = lockContext();
  bool inserted = false;
  if (recording) {
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    // Taken right after the snap, so the waits registered for the draws
    // in this recording ride on this insert and on no later one.
    PendingWaits waits(&recorder);
    waits.attach(insert);
    inserted = static_cast<bool>(m_context->insertRecording(insert));
  }
  m_context->submit(skgpu::graphite::SubmitInfo(sync));
  return inserted;
}

DeviceSignal::DeviceSignal(const skgpu::graphite::Recorder& recorder) {
  switch (recorder.backend()) {
#ifdef __APPLE__
    case skgpu::BackendApi::kMetal:
      m_form = metalSignal(recorder);
      break;
#endif
    case skgpu::BackendApi::kVulkan:
      if (const GraphiteContext* context = contextOf(recorder))
        m_form = vulkanSignal(context->m_device, context->m_vulkanQueue,
                              context->m_vulkanGetDeviceProcAddr);
      break;
    default:
      break;
  }
}

DeviceSignal::~DeviceSignal() = default;

void DeviceSignal::attach(skgpu::graphite::InsertRecordingInfo& insert) {
  if (m_form) m_form->attach(insert);
}

void DeviceSignal::afterSubmit() {
  if (m_form) m_form->afterSubmit();
}

bool DeviceSignal::wait(std::chrono::steady_clock::time_point deadline) const {
  return m_form && m_form->wait(deadline);
}

}  // namespace sigil::skia
