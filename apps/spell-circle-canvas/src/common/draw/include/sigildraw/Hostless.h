#pragma once

/** @file
 * @ingroup draw-pen
 *
 * A pen begun where no frame lends one, and the seam that says which
 * device such a pen draws on: what a layer drawn at a prompt, a buffer
 * formed before any window opens or a picture looked at between frames
 * stands on.
 */

#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Pen.h>

class SkCanvas;

namespace sigil::weave {
class FontContext;
}  // namespace sigil::weave

namespace sigil::draw {

/** WHERE A PEN WITH NO FRAME DRAWS: the device a host-less pen's buffers
 *  are formed on, and the two crossings between that device and the CPU.
 *
 *  This library owns no device. With none installed a host-less pen draws
 *  on raster, which is this seam's CPU executor and the one for a machine
 *  with no device; a host that holds a device installs an executor
 *  standing on it with `useHostlessDevice`, so a buffer formed with no
 *  frame open is formed where the frames it is put down in are drawn.
 *  Every call is made on the thread the device is drawn from. */
class HostlessDevice {
 public:
  virtual ~HostlessDevice() = default;
  /** A surface of @p info on the device, or null where the device
   *  declines one; the pen then draws on raster. */
  virtual sk_sp<SkSurface> makeSurface(const SkImageInfo& info) = 0;
  /** @p image, which stands on this device, read back to the CPU; null
   *  where it cannot be read. */
  virtual sk_sp<SkImage> rasterImage(const sk_sp<SkImage>& image) = 0;
  /** Whether @p canvas records onto this device, so an image formed here
   *  is drawn on it as it stands. */
  virtual bool drawsOn(SkCanvas& canvas) = 0;
  /** Hands what host-less pens recorded to the device: called as each
   *  one ends, so a device no frame submits does not hold the work. */
  virtual void submit() {}
};

/** Installs @p device as the process's host-less device, or raster again
 *  for null. Borrowed: the caller keeps it alive while it is installed. */
void useHostlessDevice(HostlessDevice* device);
/** The installed host-less device, or null where pens draw on raster. */
[[nodiscard]] HostlessDevice* hostlessDevice();

/** A PEN BEGUN WITH NO FRAME, standing in for a frame's pen as a host: a
 *  buffer begun under it is formed on the host-less device at @p density,
 *  or on raster where none is installed, and its text shapes with
 *  @p fonts. Its canvas is one pixel and draws nothing itself; its clock
 *  reads zero and it has no input. It ends as it goes out of scope. */
class HostlessPen {
 public:
  HostlessPen(float density, float width, float height,
              weave::FontContext* fonts);
  ~HostlessPen();
  HostlessPen(const HostlessPen&) = delete;
  HostlessPen& operator=(const HostlessPen&) = delete;

  [[nodiscard]] Pen& pen() { return m_pen; }

  /** Whether a pen on @p target draws @p image as it stands: where it is
   *  raster, or @p target records onto the host-less device it stands
   *  on. */
  [[nodiscard]] static bool drawnAsItStands(Pen& target, const SkImage& image);
  /** @p image AS A PEN ON @p target DRAWS IT: itself where
   *  `drawnAsItStands`, and read back to the CPU only where @p target
   *  draws on raster or on another device. */
  [[nodiscard]] static sk_sp<SkImage> drawableOn(Pen& target,
                                                 const sk_sp<SkImage>& image);
  /** @p image on the CPU, read back from the host-less device where it
   *  stands there: what is encoded, compared or handed to a caller that
   *  reads pixels. Null where a device image cannot be read. */
  [[nodiscard]] static sk_sp<SkImage> rasterImage(const sk_sp<SkImage>& image);

 private:
  sk_sp<SkSurface> m_surface;
  int m_saveCount = 0;
  Pen m_pen;
};

}  // namespace sigil::draw
