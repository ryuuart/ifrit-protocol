#pragma once

/** @file
 * ANOTHER APPLICATION'S PICTURE, as something a sketch can wear: the
 * newest frame of a publication on this machine, as an image the canvas
 * paints like any other.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::sketch {

struct SketchContext;
class Subscription;

/**
 * A PUBLICATION, WORN. Another application on this machine offers its
 * frames under a name; a guest holds that name and answers with the
 * newest frame as an `SkImage`, so a body draws somebody else's picture
 * with the same call it draws one of its own.
 *
 * IT IS MADE IN `setup()` AND KEPT — held by the sketch, and by the
 * paint programs that read it, so make it a `std::shared_ptr` and
 * capture that. Making one per frame would open a subscription per
 * frame.
 *
 * NOTHING ARRIVES IN A DETERMINISTIC RUN. What another application
 * happens to be publishing is not a function of this sketch's
 * declaration, and a capture that will be diffed has to be — so a guest
 * opened while the host is capturing subscribes to nothing at all, and a
 * scene states what it draws when nobody is publishing. That is also
 * what a plate of it is.
 */
class Guest {
 public:
  /** A guest onto the publication @p name announces — and, where
   *  @p application is not empty, only that application's. Nothing is
   *  subscribed to where this run receives nothing: off macOS, on a
   *  machine with no Metal device, or while the host is capturing. */
  Guest(SketchContext& context, std::string name, std::string application = {});
  ~Guest();
  Guest(const Guest&) = delete;
  Guest& operator=(const Guest&) = delete;

  /** THE NEWEST PUBLISHED FRAME as an image @p recorder can draw, or
   *  null while nothing is publishing.
   *
   *  @p recorder IS THE ONE THE DRAWING IS BEING RECORDED ON —
   *  `canvas.recorder()` inside a paint program. A received frame is a
   *  texture on the device it arrived on, so the image naming it belongs
   *  to the recorder that will sample it and to no other; a canvas
   *  rasterising on the CPU has no recorder, and the answer there is
   *  null.
   *
   *  ONE WRAP PER FRAME THAT ARRIVED. An image holds the texture it
   *  names for its own life, so the same image is handed back until
   *  another frame arrives, the recorder changes, or the publication
   *  stops.
   *
   *  ASK EVERY FRAME, including where nothing can be drawn with the
   *  answer: asking is also what opens onto a publication that appeared
   *  after this guest was made, or came back after its publisher
   *  stopped. */
  sk_sp<SkImage> frame(skgpu::graphite::Recorder* recorder);

  /** True while frames can still arrive from the publication this guest
   *  holds. */
  [[nodiscard]] bool publishing() const;

  /** The name this guest waits on, as it was asked for. */
  [[nodiscard]] std::string_view name() const;

  /** The application the standing publication is drawn in, as it
   *  announced itself; empty while nothing is publishing. */
  [[nodiscard]] std::string_view application() const;

 private:
  std::string m_name;
  std::unique_ptr<Subscription> m_subscription;
  /** The last wrap, and what it was a wrap OF: which frame had arrived
   *  and which recorder it was recorded on. */
  sk_sp<SkImage> m_picture;
  skgpu::graphite::Recorder* m_recorder = nullptr;
  uint64_t m_arrived = 0;
};

}  // namespace sigil::sketch
