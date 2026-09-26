/** @file
 * The publication a sketch wears: one subscription, read as the picture
 * source it is — bound for the recorder a canvas records on, or taken as
 * a texture a body is dressed with.
 */

#include <include/core/SkCanvas.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/set/Set.h>

#include <utility>

namespace sigil::sketch {

Guest::Guest(SketchContext& context, std::string name, std::string application)
    : Guest(context.assets.hub(), context.deterministic, std::move(name),
            std::move(application)) {}

Guest::Guest(SetContext& context, std::string name, std::string application)
    : Guest(context.assets.hub(), context.deterministic, std::move(name),
            std::move(application)) {}

Guest::Guest(io::Hub& hub, bool deterministic, std::string name,
             std::string application)
    : m_name(std::move(name)) {
  // A DETERMINISTIC RUN SUBSCRIBES TO NOTHING: a capture that will be
  // diffed must be a function of this sketch's declaration, and what
  // another application happens to be publishing while it is taken is
  // not one.
  if (deterministic) return;
  m_subscription = hub.subscribe("syphon://" + m_name,
                                 {.application = std::move(application)});
}

Guest::~Guest() = default;

sk_sp<SkImage> Guest::frame(skgpu::graphite::Recorder* recorder) {
  if (!m_subscription) return nullptr;
  // ASKING IS ALSO THE RECONNECTION, so the frame is asked for whatever
  // can be drawn with it; a canvas rasterising on the CPU has no
  // recorder, and a publication stands on a device.
  const media::Frame arrived = m_subscription.frameAt();
  if (!recorder) return nullptr;
  return media::deviceImage(arrived, recorder);
}

sk_sp<SkImage> Guest::frame(SkCanvas& canvas) {
  // The recorder a drawing is being recorded on is the canvas's own, so
  // a caller inside a paint program has it already and does not have to
  // name Graphite to say so.
  return frame(canvas.recorder());
}

material::Texture Guest::texture() {
  if (!m_subscription) return {};
  // ASKING IS ALSO THE RECONNECTION: a publication that appeared after
  // this guest was made is opened onto here, and until one has sent a
  // frame there is no picture to dress anything with.
  if (!m_subscription.frameAt()) return {};
  return material::Texture(media::PixelSource(m_subscription));
}

bool Guest::publishing() const {
  return m_subscription && m_subscription.state().isOpen();
}

std::string_view Guest::name() const { return m_name; }

std::string_view Guest::application() const {
  return m_subscription ? m_subscription.publishingApplication()
                        : std::string_view{};
}

}  // namespace sigil::sketch
