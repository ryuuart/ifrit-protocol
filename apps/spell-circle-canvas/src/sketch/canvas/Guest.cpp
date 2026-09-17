/** @file
 * The publication a sketch wears: one subscription, the wrap that makes
 * its newest frame an image the canvas draws, and the read that makes it
 * a texture a body is dressed with.
 */

#include <sigilio/publish/Subscription.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/set/Set.h>
#include <sigilskia/graphite/TextureImage.h>

#include <utility>

namespace sigil::sketch {

Guest::Guest(SketchContext& context, std::string name, std::string application)
    : Guest(context.deterministic, std::move(name), std::move(application)) {}

Guest::Guest(SetContext& context, std::string name, std::string application)
    : Guest(context.deterministic, std::move(name), std::move(application)) {}

Guest::Guest(bool deterministic, std::string name, std::string application)
    : m_name(std::move(name)) {
  // A DETERMINISTIC RUN SUBSCRIBES TO NOTHING: a capture that will be
  // diffed must be a function of this sketch's declaration, and what
  // another application happens to be publishing while it is taken is
  // not one.
  if (deterministic) return;
  m_subscription = io::publish::subscribe(m_name, std::move(application),
                                          io::publish::defaultMetalDevice());
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

material::Texture Guest::texture() {
  if (!m_subscription) return {};
  const uint64_t arrived = m_subscription->generation();
  // THE SAME TEXTURE WHILE THE SAME FRAME STANDS, for the reason the
  // wrap has and one more: a read that ran again on a frame nothing had
  // replaced would be the same pixels copied a second time, and the
  // material holding it would compare unequal and patch for nothing.
  if (m_dress.valid() && arrived == m_read && m_subscription->standing())
    return m_dress;
  // ASKING IS ALSO THE RECONNECTION, so it is asked whatever can be done
  // with the answer.
  void* texture = m_subscription->newestFrame();
  m_dress = {};
  m_read = arrived;
  if (!texture) return m_dress;
  // No colour space is stated for the same reason the wrap states none:
  // nothing converts on the way across, and the texels mean what the
  // frame's own format says.
  sk_sp<SkImage> pixels = skia::readImage(texture);
  if (pixels) m_dress = material::Texture::of(std::move(pixels));
  return m_dress;
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
