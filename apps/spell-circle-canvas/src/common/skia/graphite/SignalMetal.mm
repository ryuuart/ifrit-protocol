// The device signal on Metal: a shared event the queue raises at the end
// of the command buffer a submit commits, and a listener that wakes the
// waiting thread when it does.

#import <Metal/Metal.h>

#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "Waits.h"

namespace sigil::skia {

struct DeviceSignal::State {
  id<MTLSharedEvent> event = nil;
  MTLSharedEventListener* listener = nil;
  std::mutex mutex;
  std::condition_variable raised;
  bool reached = false;
};

namespace {

/** The value every signal raises its own event to. */
constexpr uint64_t kRaised = 1;

/** One listener for every signal: its queue only runs the block that
 *  wakes a waiting thread. */
MTLSharedEventListener* sharedListener() {
  static MTLSharedEventListener* listener = [[MTLSharedEventListener alloc] init];
  return listener;
}

}  // namespace

DeviceSignal::DeviceSignal(const skgpu::graphite::Recorder& recorder) {
  if (recorder.backend() != skgpu::BackendApi::kMetal) return;
  id<MTLDevice> device = (__bridge id<MTLDevice>)deviceOf(recorder);
  if (!device) return;
  id<MTLSharedEvent> event = [device newSharedEvent];
  if (!event) return;
  m_state = std::make_unique<State>();
  m_state->event = event;
  m_semaphore = skgpu::graphite::BackendSemaphores::MakeMetal(
      (__bridge CFTypeRef)event, kRaised);
  State* state = m_state.get();
  // The block holds the state by pointer: the destructor waits for the
  // event to be raised, or gives the block nothing to touch, before the
  // state goes.
  [event notifyListener:sharedListener()
                atValue:kRaised
                  block:^(id<MTLSharedEvent>, uint64_t) {
                    const std::lock_guard<std::mutex> lock(state->mutex);
                    state->reached = true;
                    state->raised.notify_all();
                  }];
}

DeviceSignal::~DeviceSignal() {
  if (!m_state) return;
  // A signal that was never raised — a submit that failed — is raised
  // here from the host, so the listener's block runs before the state it
  // reaches goes.
  if (m_state->event.signaledValue < kRaised) m_state->event.signaledValue = kRaised;
  std::unique_lock<std::mutex> lock(m_state->mutex);
  m_state->raised.wait(lock, [&] { return m_state->reached; });
  lock.unlock();
  [m_state->event release];
}

void DeviceSignal::attach(skgpu::graphite::InsertRecordingInfo& insert) {
  if (!m_state) return;
  insert.fNumSignalSemaphores = 1;
  insert.fSignalSemaphores = &m_semaphore;
}

bool DeviceSignal::wait(std::chrono::steady_clock::time_point deadline) const {
  if (!m_state) return false;
  std::unique_lock<std::mutex> lock(m_state->mutex);
  return m_state->raised.wait_until(lock, deadline, [&] { return m_state->reached; });
}

}  // namespace sigil::skia
