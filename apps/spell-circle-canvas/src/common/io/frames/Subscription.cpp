/** @file
 * The one place that knows what this build can subscribe over.
 * Everything else in this repository holds a `Subscription` or holds
 * nothing.
 */

#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>

#include <mutex>
#include <string>
#include <utility>

#include "Protocol.h"
#include "Received.h"

#if defined(__APPLE__)
#include "SyphonSubscription.h"
#endif

namespace sigil::io {

namespace frames {

namespace detail {

struct Presentation {
  std::mutex mutex;
  uint64_t revision = 0;
  media::Frame frame;
};

}  // namespace detail

Subscription::Subscription(std::shared_ptr<detail::SubscriptionEnd> end)
    : m_end(std::move(end)),
      m_presentation(std::make_shared<detail::Presentation>()) {}

media::Frame Subscription::frameAt(std::chrono::duration<double>) const {
  if (!m_end) return {};
  const FeedState standing = m_end->state();
  std::lock_guard lock(m_presentation->mutex);
  // THE SAME PICTURE WHILE THE SAME FRAME STANDS: a binding holds what
  // it made of an arrival for the arrival's whole life, so presenting a
  // frame nothing has replaced would make the same picture twice.
  if (m_presentation->frame && m_presentation->revision == standing.revision &&
      standing.isOpen())
    return m_presentation->frame;
  // ASKING IS ALSO THE RECONNECTION, so it is asked whatever becomes of
  // the answer.
  const std::optional<Frame> arrival = m_end->latest();
  m_presentation->revision = standing.revision;
  m_presentation->frame = {};
  if (!arrival || !arrival->texture) return {};
  // The arrival is borrowed until the next ask; the frame holds it.
  std::shared_ptr<void> held = detail::retainTexture(arrival->texture);
  if (!held) return {};
  media::Frame frame;
  frame.index = static_cast<int64_t>(standing.revision);
  frame.device.kind = media::DeviceFrame::Kind::Texture;
  frame.device.pointer = arrival->texture;
  frame.device.width = arrival->width;
  frame.device.height = arrival->height;
  frame.device.storage = held;
  frame.device.binding = detail::bindArrival(held);
  m_presentation->frame = frame;
  return frame;
}

std::vector<Publication> publications() {
#if defined(__APPLE__)
  return syphonPublications();
#else
  return {};
#endif
}

void* defaultMetalDevice() {
#if defined(__APPLE__)
  return metalDeviceOfThisMachine();
#else
  return {};
#endif
}

}  // namespace frames

frames::Subscription Hub::subscribe(std::string_view uri,
                                    const frames::SubscribeOptions& options) {
  const std::optional<frames::detail::Carrier> carrier =
      frames::detail::carrierOf(uri, options.device);
  if (!carrier) return {};
#if defined(__APPLE__)
  if (carrier->api == frames::GraphicsApi::Metal)
    return frames::Subscription(frames::makeSyphonSubscription(
        std::string(carrier->name), options.application, carrier->device));
#endif
  return {};
}

}  // namespace sigil::io
