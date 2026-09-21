/** @file
 * The publication a sketch wears: one subscription, the turn that makes
 * its newest frame an image the canvas draws the right way up, and the
 * read that makes it a texture a body is dressed with.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilio/publish/Subscription.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/set/Set.h>
#include <sigilskia/graphite/TextureImage.h>

#include <utility>

namespace sigil::sketch {

namespace {

/** THE ONE THING DONE TO A FRAME ON THE WAY IN. A publication is carried
 *  on a surface whose FIRST ROW IS THE IMAGE'S BOTTOM — the order every
 *  application sharing textures on this machine writes and reads — and a
 *  canvas draws with its first row at the top, so a frame sampled as it
 *  arrived would be upside down.
 *
 *  It is described on the recorder the drawing is being recorded on, and
 *  once per frame that ARRIVES rather than once per draw of one, so a
 *  scene wearing a publication in several places pays for the turn once.
 *  @p target is the surface the turn lands in, kept by the caller for as
 *  long as the answer is: an image made from a surface names the
 *  surface's texture. Null when no target could be made, which leaves a
 *  scene with no picture rather than with a picture the wrong way up. */
sk_sp<SkImage> turnOver(skgpu::graphite::Recorder& recorder,
                        const sk_sp<SkImage>& carried,
                        sk_sp<SkSurface>& target) {
  target.reset();
  if (!carried) return nullptr;
  target = SkSurfaces::RenderTarget(&recorder, carried->imageInfo());
  if (!target) return nullptr;
  SkCanvas* canvas = target->getCanvas();
  canvas->translate(0, (float)carried->height());
  canvas->scale(1, -1);
  canvas->drawImage(carried, 0, 0, SkSamplingOptions());
  return SkSurfaces::AsImage(target);
}

/** The same turn for pixels already in host memory: the rows of @p read
 *  walked backwards into a bitmap of its own, which is the whole of it —
 *  a row is the same bytes wherever it stands, so nothing is resampled
 *  and no channel moves. */
sk_sp<SkImage> turnOver(const sk_sp<SkImage>& read) {
  if (!read) return nullptr;
  SkBitmap turned;
  if (!turned.tryAllocPixels(read->imageInfo())) return nullptr;
  SkCanvas canvas(turned);
  canvas.translate(0, (float)read->height());
  canvas.scale(1, -1);
  canvas.drawImage(read, 0, 0, SkSamplingOptions());
  turned.setImmutable();
  return turned.asImage();
}

}  // namespace

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
  m_turned.reset();
  m_recorder = recorder;
  m_arrived = arrived;
  if (!texture || !recorder) return nullptr;
  // The frame is premultiplied the way the publisher's canvas wrote it
  // and its texels mean what its own format says, which leaves the
  // colour space: nothing converts on the way across, so none is stated.
  m_picture = turnOver(*recorder, skia::wrapImage(*recorder, texture), m_turned);
  return m_picture;
}

sk_sp<SkImage> Guest::frame(SkCanvas& canvas) {
  // The recorder a drawing is being recorded on is the canvas's own, so
  // a caller inside a paint program has it already and does not have to
  // name Graphite to say so.
  return frame(canvas.recorder());
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
  sk_sp<SkImage> pixels = turnOver(skia::readImage(texture));
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
