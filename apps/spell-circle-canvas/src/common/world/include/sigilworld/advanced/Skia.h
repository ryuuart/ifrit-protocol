#pragma once

/** @file
 * @ingroup world-frame
 * THE ONE DOOR BETWEEN THIS LIBRARY AND SKIA, included by name by an
 * executor, a pass body or a host: `Targets`, the resources a frame's
 * passes write and read — the images, answered by the executor that holds
 * their pixels, and the point sets a compute pass cooks; `draw`, the
 * picture a scene's passes wrote presented on a host's canvas; and the
 * crossings of a colour, a blend, a size and a uv matrix into Skia's own.
 * Every other header here speaks Material's colour and blend, Media's
 * picture and glm.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilworld/advanced/Geometry.h>
#include <sigilworld/advanced/Stats.h>

#include <atomic>
#include <boost/container/map.hpp>
#include <cstdint>
#include <functional>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::world {

/** THE FRAME'S RESOURCES, by name: the images its passes wrote, read
 *  from the executor that holds their pixels, and the point sets a compute
 *  pass cooks, with the instanced geometry formed from them. The pixels stay
 *  where they were drawn, and nothing crosses back until something asks
 *  for an image by name. */
class Targets {
 public:
  Targets() = default;

  /** The size every image here is made at. */
  void extent(glm::ivec2 size) { m_extent = size; }
  /** The size every image here is made at; empty until one is set. */
  [[nodiscard]] glm::ivec2 extent() const { return m_extent; }

  /** @p name as it stands now, from the executor that drew it. Null when
   *  no executor answers for this frame or none painted the name. */
  [[nodiscard]] sk_sp<SkImage> image(std::string_view name);

  /** The point set @p name, made empty on the first ask. */
  geometry::mesh::Cloud* points(std::string_view name);
  /** The point set @p name as it stands; null when nothing made it. */
  [[nodiscard]] const geometry::mesh::Cloud* points(
      std::string_view name) const;

  /** @p stamp STANDING AT EVERY POINT of @p cloud as instanced geometry,
   *  placed by the point operators' own table, formed once per distinct
   *  pair and kept while both stand; null when there is nothing to stamp. The
   *  answer is held under `stampKey`, a fold over the two VALUES, so a
   *  still scene is neither formed twice nor uploaded twice, and what a
   *  frame does not ask for is let go at the end of it. @p key receives
   *  the number the stamping is held under, for an executor that keys an
   *  upload by the same one. */
  const geometry::mesh::Instances* stamped(const geometry::mesh::Cloud& cloud,
                                           const geometry::mesh::Mesh& stamp,
                                           uint64_t* key = nullptr);

  /** How many stampings have been FORMED here, over the store's
   *  whole life. A frame drawing a scene that has not moved must not move
   *  this number. */
  [[nodiscard]] uint64_t stampings() const { return m_stampings; }

  /** WHERE AN IMAGE COMES FROM: the executor that performed the frame's
   *  passes, answering for one name at a time, so only what something
   *  asks for costs the crossing back. An executor installs it at the
   *  start of every frame it runs. */
  using ImageSource = std::function<sk_sp<SkImage>(std::string_view)>;
  /** Installs @p source, on the terms above. */
  void source(ImageSource source) { m_source = std::move(source); }

  /** @p name as an image @p canvas draws by sampling the executor's own
   *  texture where it stands, with nothing read back: what presenting a
   *  picture costs when the canvas's queue can reach the device. Null
   *  when no executor offers that for this canvas, which a caller
   *  answers with `image()`. */
  [[nodiscard]] sk_sp<SkImage> onDevice(std::string_view name,
                                        SkCanvas& canvas);
  /** WHERE A PICTURE IS SAMPLED IN PLACE: the executor that holds the
   *  pixels, asked for one name and the canvas that will draw it. An
   *  executor that can hand a canvas its texture installs it at the
   *  start of every frame it runs, beside the image source. */
  using DeviceSource =
      std::function<sk_sp<SkImage>(std::string_view, SkCanvas&)>;
  /** Installs @p source, on the terms above. */
  void deviceSource(DeviceSource source) { m_deviceSource = std::move(source); }

  /** HOW MANY SHADOW MAPS THE EXECUTOR DREW, which the scene's own plan
   *  cannot see: a map nothing moved in is kept from the frame before and
   *  not drawn. An executor adds what each pass drew; the scene takes the
   *  sum once its passes have run, which starts the next sum at zero. */
  void addShadowMapsDrawn(int64_t count) { m_shadowMapsDrawn += count; }
  [[nodiscard]] int64_t takeShadowMapsDrawn() {
    return std::exchange(m_shadowMapsDrawn, 0);
  }
  /** HOW MANY BODIES THE EXECUTOR DREW AS A STAND-IN because their
   *  program was still compiling, on the same terms. */
  void addProgramsPending(int64_t count) { m_programsPending += count; }
  [[nodiscard]] int64_t takeProgramsPending() {
    return std::exchange(m_programsPending, 0);
  }

  /** THE DEVICE'S STAGE TIMES, which only the executor can read: it
   *  states the newest timed frame that has finished on the device, and
   *  the scene takes them once its passes have run into its stats. */
  void deviceStages(std::vector<DeviceStage> stages, DeviceTiming timing,
                    int64_t frame) {
    m_deviceStages = std::move(stages);
    m_deviceTiming = timing;
    m_deviceStagesFrame = frame;
  }
  /** A number no other targets made in this process carry: what an
   *  executor serving several scenes tells their frames apart by. */
  [[nodiscard]] uint64_t serial() const { return m_serial; }
  /** Moves what the executor stated into @p stats, leaving nothing here. */
  void takeDeviceStages(SceneStats& stats) {
    stats.deviceStages = std::exchange(m_deviceStages, {});
    stats.deviceTiming = std::exchange(m_deviceTiming, DeviceTiming::Off);
    stats.deviceStagesFrame = std::exchange(m_deviceStagesFrame, -1);
  }

  /** Close the frame: a stamping no pass asked for in it is let go. */
  void endFrame();

 private:
  glm::ivec2 m_extent{0, 0};
  boost::container::map<std::string, geometry::mesh::Cloud> m_points;
  /** A formed stamping and the frame it was last asked for in. */
  struct Stamping {
    geometry::mesh::Instances instances;
    /** The pair it was formed from, kept so that a lookup landing on
     *  this number is confirmed by value rather than by the fold. */
    geometry::mesh::Cloud cloud;
    geometry::mesh::Mesh stamp;
    uint64_t used = 0;
  };
  boost::container::map<uint64_t, Stamping> m_stamped;
  uint64_t m_frame = 0;
  uint64_t m_stampings = 0;
  int64_t m_shadowMapsDrawn = 0;
  int64_t m_programsPending = 0;
  static uint64_t nextSerial() {
    static std::atomic<uint64_t> made{0};
    return ++made;
  }
  uint64_t m_serial = nextSerial();
  std::vector<DeviceStage> m_deviceStages;
  DeviceTiming m_deviceTiming = DeviceTiming::Off;
  int64_t m_deviceStagesFrame = -1;
  ImageSource m_source;
  DeviceSource m_deviceSource;
};

class Scene;

/** HOW A PICTURE REACHES A CANVAS. */
enum class Presentation : uint8_t {
  /** Read back into host memory and drawn as a raster image: the same
   *  bytes onto every canvas, which is what a capture that will be
   *  diffed draws. */
  Readback,
  /** Sampled where the device drew it, when the canvas draws on a queue
   *  that can reach that device — the order between the device's drawing
   *  and the canvas's sampling kept on the GPU, with no wait on this
   *  thread — and read back as above where it cannot. What a live window
   *  draws. */
  OnDevice,
};

/** Draw the picture @p scene's last `render()` formed on @p canvas: the
 *  resource its passes wrote and the frame presents, which a frame's
 *  passes have already drawn by the time this is called. The canvas is
 *  addressed in the pixels of the whole picture the frame's camera
 *  framed, so a frame formed over a part of it (`Frame::viewOffset`)
 *  lands where that part stands. A frame that declared no passes, or
 *  whose passes did not run, formed no picture and draws nothing.
 *  @p presentation says whether the picture may be sampled in place;
 *  the scene's stats count which way it went. */
void draw(Scene& scene, SkCanvas& canvas,
          Presentation presentation = Presentation::Readback);

/** A pixel size as Skia's. */
inline SkISize toSk(glm::ivec2 size) { return SkISize::Make(size.x, size.y); }
/** A blend as Skia's. */
SkBlendMode toSk(material::BlendMode mode);
/** A colour as Skia's, field for field. */
inline SkColor4f toSk(const material::Color& colour) {
  return {colour.r, colour.g, colour.b, colour.a};
}
/** A 2D affine matrix on column vectors as Skia's. */
inline SkMatrix toSk(const glm::mat3& matrix) {
  return SkMatrix::MakeAll(matrix[0][0], matrix[1][0], matrix[2][0],
                           matrix[0][1], matrix[1][1], matrix[2][1],
                           matrix[0][2], matrix[1][2], matrix[2][2]);
}
/** Skia's matrix as a 2D affine matrix on column vectors. */
inline glm::mat3 fromSk(const SkMatrix& matrix) {
  glm::mat3 out{1.0f};
  out[0][0] = matrix.getScaleX();
  out[1][0] = matrix.getSkewX();
  out[2][0] = matrix.getTranslateX();
  out[0][1] = matrix.getSkewY();
  out[1][1] = matrix.getScaleY();
  out[2][1] = matrix.getTranslateY();
  out[0][2] = matrix.getPerspX();
  out[1][2] = matrix.getPerspY();
  out[2][2] = matrix.get(SkMatrix::kMPersp2);
  return out;
}
/** How a sampling reads between texels, as Skia's filter. */
inline SkFilterMode toSk(material::Sampling sampling) {
  return sampling == material::Sampling::Nearest ? SkFilterMode::kNearest
                                                 : SkFilterMode::kLinear;
}

}  // namespace sigil::world
