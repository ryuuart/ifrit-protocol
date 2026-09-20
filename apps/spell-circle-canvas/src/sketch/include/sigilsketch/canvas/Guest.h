#pragma once

/** @file
 * @ingroup sketch-canvas
 *
 * ANOTHER APPLICATION'S PICTURE, as something a sketch can wear: the
 * newest frame of a publication on this machine, as an image the canvas
 * paints like any other and as a texture a body in the world is dressed
 * with.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/texture/Texture.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::io::publish {
class Subscription;
}

namespace sigil::sketch {

struct SketchContext;
struct SetContext;

/**
 * A PUBLICATION, WORN. Another application on this machine offers its
 * frames under a name; a guest holds that name and answers with the
 * newest frame — as an `SkImage` a canvas paints, and as a
 * `material::Texture` a body in the world is dressed with — so a scene
 * wears somebody else's picture with the same calls it wears one of its
 * own.
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
   *  machine with no Metal device, or while the host is capturing.
   *
   *  It is made from the context a sketch was handed, whichever context
   *  that is: a page declares itself from a `SketchContext` and a set
   *  from a `SetContext`, and what a guest reads off either is whether
   *  the host is capturing. */
  Guest(SketchContext& context, std::string name, std::string application = {});
  Guest(SetContext& context, std::string name, std::string application = {});
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

  /** THE NEWEST PUBLISHED FRAME as a texture a material slot takes —
   *  the picture on a BODY, where `frame()` is the picture on a canvas.
   *  Empty while nothing is publishing — `material::Texture::valid` is
   *  false and there is no picture to dress anything with, which is what
   *  a scene reads before it puts its own stand-in in the slot.
   *
   *  THIS ONE COPIES, and it is the only thing in this door that does. A
   *  body is shaded by the renderer the world stands on, and that
   *  renderer does not stand where a publication arrives: the frame is a
   *  Metal texture and the world draws through Vulkan, so neither side
   *  holds anything the other could be handed. The pixels are read back
   *  into host memory instead — one frame's worth for each frame that
   *  arrives — and the renderer uploads them to its own device like any
   *  other image, which is what makes the slot work on every tier rather
   *  than on none. `frame()` is the case that copies nothing, because a
   *  canvas draws on the device the frame arrived on.
   *
   *  ONE READ PER FRAME THAT ARRIVED, like `frame()`: the same texture
   *  is handed back until another frame arrives or the publication
   *  stops, and two textures taken either side of a still frame compare
   *  equal, so a material holding one prunes.
   *
   *  ASK EVERY FRAME, for the reason `frame()` gives: asking is also
   *  what opens onto a publication that appeared after this guest was
   *  made. */
  [[nodiscard]] material::Texture texture();

  /** True while frames can still arrive from the publication this guest
   *  holds. */
  [[nodiscard]] bool publishing() const;

  /** The name this guest waits on, as it was asked for. */
  [[nodiscard]] std::string_view name() const;

  /** The application the standing publication is drawn in, as it
   *  announced itself; empty while nothing is publishing. */
  [[nodiscard]] std::string_view application() const;

 private:
  /** What both constructors are: a name, and whether the host is
   *  capturing — which is the only thing a guest reads off a context. */
  Guest(bool deterministic, std::string name, std::string application);

  std::string m_name;
  std::unique_ptr<io::publish::Subscription> m_subscription;
  /** The last wrap, and what it was a wrap OF: which frame had arrived
   *  and which recorder it was recorded on. */
  sk_sp<SkImage> m_picture;
  skgpu::graphite::Recorder* m_recorder = nullptr;
  uint64_t m_arrived = 0;
  /** The last read, and which frame had arrived when it was taken. It is
   *  counted apart from the wrap's: a scene may ask for both, and each
   *  is made from whichever frame stood when it was asked for. */
  material::Texture m_dress;
  uint64_t m_read = 0;
};

}  // namespace sigil::sketch
