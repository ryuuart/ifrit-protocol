/** @file
 * The publication a sketch wears: one subscription, and the one wrap
 * that makes its newest frame an image the canvas draws.
 */

#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/publish/Subscription.h>
#include <sigilskia/graphite/TextureImage.h>

#include <utility>

namespace sigil::sketch {

Guest::Guest(SketchContext& context, std::string name, std::string application)
    : m_name(std::move(name)) {
  // A DETERMINISTIC RUN SUBSCRIBES TO NOTHING: a capture that will be
  // diffed must be a function of this sketch's declaration, and what
  // another application happens to be publishing while it is taken is
  // not one.
  if (context.deterministic) return;
  m_subscription =
      subscribe(m_name, std::move(application), defaultMetalDevice());
}

Guest::~Guest() = default;

sk_sp<SkImage> Guest::frame(skgpu::graphite::Recorder* recorder) {
  if (!m_subscription) return nullptr;
  const uint64_t arrived = m_subscription->generation();
  // THE SAME IMAGE WHILE THE SAME FRAME STANDS. A wrap holds the texture
  // it names for the image's whole life, so wrapping a frame nothing has
  // replaced would be a second handle on one set of pixels — and the
  // draw that samples it would be no different for it.
  if (m_picture && recorder == m_recorder && arrived == m_arrived &&
      m_subscription->standing())
    return m_picture;
  // ASKING IS ALSO THE RECONNECTION, so it is asked whatever can be done
  // with the answer: a publication that appeared after this guest was
  // made, or came back after its publisher stopped, is opened onto here.
  void* texture = m_subscription->newestFrame();
  m_picture.reset();
  m_recorder = recorder;
  m_arrived = arrived;
  if (!texture || !recorder) return nullptr;
  // The frame is premultiplied the way the publisher's canvas wrote it
  // and its texels mean what its own format says, which leaves the
  // colour space: nothing converts on the way across, so none is stated.
  m_picture = skia::wrapImage(*recorder, texture);
  return m_picture;
}

bool Guest::publishing() const {
  return m_subscription && m_subscription->standing();
}

std::string_view Guest::name() const { return m_name; }

std::string_view Guest::application() const {
  return m_subscription ? m_subscription->publishingApplication()
                        : std::string_view{};
}

}  // namespace sigil::sketch
