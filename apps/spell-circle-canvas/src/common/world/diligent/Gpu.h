#pragma once

/** @file
 * What the GPU executor holds between one pass and the next: the frame's
 * resources as device textures, the pipelines built from compiled
 * programs, and the device's own shared resources and residencies — the
 * uniform buffer every draw is written into, and the meshes and maps a
 * view names, which the device feature puts there and this one only asks
 * for.
 *
 * Diligent's types appear here and in this feature's sources alone; the
 * public header names none of them.
 */

#include <Graphics/GraphicsEngine/interface/Buffer.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/PipelineState.h>
#include <Graphics/GraphicsEngine/interface/Query.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/ShaderResourceBinding.h>
#include <Graphics/GraphicsEngine/interface/Texture.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSize.h>
#include <sigilcore/hardware/Handle.h>
#include <sigilgeometry/device/Device.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/advanced/View.h>
#include <sigilworld/frame/Pass.h>

#include <Common/interface/RefCntAutoPtr.hpp>
#include <atomic>
#include <boost/container/map.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <glm/mat4x4.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Meshes.h"
#include "Pipelines.h"
#include "Programs.h"
#include "Resources.h"
#include "Textures.h"

namespace sigil::world::diligent {

/** How many lights one draw carries: the array every program here
 *  declares. A description naming more is drawn with the first sixteen. */
constexpr size_t kLights = 16;

// The device every executor here stands on is SigilGeometry's — it is
// the one point in the tree that can create a Diligent device — and this
// is the name it is spelled by in this feature's own sources.
using ::sigil::geometry::device::Device;

namespace dg = Diligent;

// …and the device feature's own scope, whose residencies, pipelines and
// shared resources this executor stands on. Spelled short here because
// every draw below reaches into it; nothing is re-exported.
namespace device = ::sigil::geometry::device;

/** THE FORMATS. Every resource a frame's passes write holds LINEAR
 *  RADIANCE, premultiplied, in half floats — one format for every
 *  resource is what lets two resources whose lives do not overlap be
 *  handed one texture, exactly as the ordering hands two names one
 *  surface. What a display reads is eight bits a channel, written by the
 *  final stage when a resource leaves the frame. */
using ::sigil::geometry::device::kColorFormat;
using ::sigil::geometry::device::kDepthFormat;
using ::sigil::geometry::device::kRadianceFormat;

/** ONE NAMED IMAGE on the device: what this frame wrote, and what the
 *  frame before it wrote. `previous()` is answered from the second, and
 *  the two are exchanged when the frame closes. */
struct DeviceImage {
  dg::RefCntAutoPtr<dg::ITexture> current;
  dg::RefCntAutoPtr<dg::ITexture> previous;
  /** Something has painted `current` since the frame opened. */
  bool written = false;
};

/** What a mesh is once it stands on the device. The residency that puts
 *  it there is the device feature's, and this is the name a draw here
 *  reads it by. */
using ::sigil::geometry::device::MeshBuffers;

/** What a draw on this device is made of, once its program is compiled.
 *  The cache, the key and the binding are the device feature's; these
 *  are the names this feature reads them by. */
using ::sigil::geometry::device::Pipeline;
using ::sigil::geometry::device::PipelineKey;

/** ONE LIGHT'S SHADOW MAP, as the shadow stage drew it and the shading
 *  reads it: the light, the projection that carried the world into the
 *  map, the square of the atlas it occupies, and what a stored depth
 *  means. */
struct ShadowMap {
  const PlacedLight* light = nullptr;
  /** World to the light's clip space, depth zero at its near plane. */
  glm::mat4 viewProjection{1.0f};
  /** The tile's top-left corner in the atlas, and its side, in texels. */
  int x = 0;
  int y = 0;
  int size = 0;
  float near = 0.0f;
  float far = 1.0f;
  bool perspective = false;
  /** One of the six faces of a point light's map, which stand in the
   *  atlas one after another in the order +x, -x, +y, -y, +z, -z. */
  bool cube = false;
  /** The width one texel covers: everywhere in an orthographic map, at a
   *  distance of one unit from the light in a perspective one. */
  float texel = 0.0f;
  /** The share of the map over which a shadow fades out at its edge. */
  float fade = 0.0f;
  /** ON THE FIRST OF A DIRECTIONAL LIGHT'S CASCADES, which follow it in
   *  the atlas: how far in front of the camera each one reaches, zero past
   *  the last. Zero throughout on every other map. */
  glm::vec4 ends{0.0f};
};

/** ONE PICTURE HANDED TO A QUEUE THAT IS NOT THIS DEVICE'S: the final
 *  stage's eight-bit picture in a texture of its own, the same texture as
 *  the other queue's API names it, and whether a draw there is still
 *  sampling it — set when it is handed over and cleared once the image
 *  that sampled it is gone and its submission has finished, which is the
 *  only moment the texture may be drawn into again. */
struct PresentedImage {
  dg::RefCntAutoPtr<dg::ITexture> texture;
  void* metal = nullptr;
  std::shared_ptr<std::atomic<bool>> sampling;
};

/** How many pictures may be out with another queue at once. A window
 *  keeps two or three frames in flight and lets go of what it sampled a
 *  frame after that; a picture asked for while every one of these is
 *  still out is read back instead. */
constexpr size_t kPresentedInFlight = 4;

/** THE DEVICE'S CLOCK OVER A FRAME'S STAGES. While a frame asks for it,
 *  every stage boundary writes a timestamp on the device, and a stage's
 *  time is the span from the boundary before it to its own: what the
 *  device spent finishing that stage once the one before it was done. A
 *  boundary with no name only starts the next span — what stands before a
 *  stage the device may have waited on the host for, such as the final
 *  stage drawn after the frame was submitted.
 *
 *  THE DEVICE ANSWERS LATE. A timestamp can be read only once the frame
 *  that wrote it has finished on the device, so a frame's marks are kept
 *  until they can be read — at most `kFramesKept` frames of them, the
 *  oldest dropped unread past that — and `latest` holds the newest frame
 *  read. A device that cannot write timestamps writes none and says so. */
struct StageClock {
  static constexpr size_t kFramesKept = 8;
  struct Mark {
    std::string stage;
    dg::RefCntAutoPtr<dg::IQuery> query;
  };
  /** Whether the frame under way is timed. */
  bool on = false;
  /** Whether the device was asked and has no timestamps. */
  bool unavailable = false;
  bool probed = false;
  /** The marks of the frame under way, and of frames not yet read, each
   *  with the targets of the scene that drew it: one runtime may serve
   *  several scenes, and a scene is told only of its own frames. */
  struct Marked {
    std::vector<Mark> marks;
    uint64_t scene = 0;
  };
  Marked open;
  std::deque<Marked> pending;
  /** Queries read and free to be written again. */
  std::vector<dg::RefCntAutoPtr<dg::IQuery>> spare;
  /** The newest frame read, its stages summed by name in the order they
   *  first ran, and its number among the frames timed. */
  std::vector<DeviceStage> latest;
  int64_t latestFrame = -1;
  uint64_t latestScene = 0;
  int64_t framesTimed = 0;
};

/** THE EXECUTOR'S STATE, shared by every copy of the runtime value one
 *  call made. */
struct Gpu {
  explicit Gpu(Device& d)
      : device(&d), shared(d), meshes(d), maps(d), pipelines(d) {}
  ~Gpu();

  Device* device = nullptr;
  /** What every executor on this device stands on — the uniform buffer,
   *  the samplers, the white texel and the readback — which this one
   *  holds rather than owns a second copy of. */
  ::sigil::geometry::device::Resources shared;
  /** …and, on the same terms, the meshes and the maps standing on that
   *  device. A frame names what it wants drawn; putting it there is the
   *  device's own business and is not spelled again here. */
  ::sigil::geometry::device::MeshResidency meshes;
  ::sigil::geometry::device::TextureResidency maps;
  SkISize extent{0, 0};

  boost::container::map<std::string, DeviceImage, std::less<>> images;
  dg::RefCntAutoPtr<dg::ITexture> depth;
  /** THE DEPTH OF WHAT IS OPAQUE, copied once the opaque bodies of a pass
   *  are drawn, so a blended body can read how far behind it the scene
   *  stands while the depth buffer itself is still being tested against.
   *  Made on the first ask. */
  dg::RefCntAutoPtr<dg::ITexture> opaqueDepth;
  /** …read through a view of the depth format itself, which every device
   *  samples, rather than the colour format the engine would choose. */
  dg::RefCntAutoPtr<dg::ITextureView> opaqueDepthView;
  /** THE SCENE BEHIND A SURFACE THAT SEES THROUGH ITSELF: the frame's lit
   *  colour as it stood once every body that does not was drawn, with the
   *  full chain of smaller levels below it, each the average of the one
   *  above, so a rough surface reads a blurrier picture. Made on the first
   *  frame that holds such a surface. */
  dg::RefCntAutoPtr<dg::ITexture> sceneColor;
  /** What a display reads: the final stage's eight-bit picture of the
   *  resource last read, made on the first ask. */
  dg::RefCntAutoPtr<dg::ITexture> display;
  /** THE PICTURES HANDED TO ANOTHER QUEUE, made on the first ask and
   *  reused once that queue is done with them. */
  std::vector<PresentedImage> presented;
  /** The fence a presented picture is signalled on once the device has
   *  drawn it, and the same fence as the other queue's API names it:
   *  what that queue's sampling waits for on the GPU. */
  ::sigil::core::hardware::FenceHandle presentedFence;
  void* presentedEvent = nullptr;
  /** The device cannot hand its pictures to a canvas's queue at this
   *  extent; asked again once the extent changes. */
  bool presentRefused = false;
  /** HOW THE FRAME'S PICTURE IS READ: the camera's exposure and tone
   *  curve, which the final stage applies when a resource leaves the
   *  frame, and which a surface drawn as authored is written through in
   *  reverse so it leaves exactly as it was authored. */
  float exposure = 1.0f;
  ToneCurve toneCurve = ToneCurve::Reinhard;
  /** The frame's time, the clock a material reading time is shown at. */
  double seconds = 0.0;
  /** The pass under way draws a body whose program is still compiling as
   *  a stand-in, and how many it has drawn so. */
  bool standIns = false;
  int64_t programsPending = 0;
  /** TARGETS NO RESOURCE NAMES, made on the first ask and kept for the
   *  extent's life. A device cannot sample an image it is drawing into,
   *  so every stage that reads and writes at once — one direction of a
   *  blur, a masked operation, a pass that writes what it reads — takes one of
   *  these, and they are addressed by index so that two such stages in
   *  one pass cannot be handed the same one. */
  std::vector<dg::RefCntAutoPtr<dg::ITexture>> scratch;
  /** TARGETS SMALLER THAN THE FRAME, which a blur and a bloom take their
   *  spreading in: level @c n is the frame's size halved @c n times,
   *  rounded up, and each level holds working targets addressed by index
   *  as `scratch` is, one format apiece. Made on the first ask and kept
   *  for the extent's life. */
  struct Reduced {
    int level = 0;
    size_t index = 0;
    dg::TEXTURE_FORMAT format = dg::TEX_FORMAT_UNKNOWN;
    dg::RefCntAutoPtr<dg::ITexture> texture;
  };
  std::vector<Reduced> reduced;
  /** Whether the device draws into the packed three-channel float format
   *  a bloom's spreads are held in; asked once. */
  int packedLight = -1;
  ::sigil::geometry::device::PipelineCache pipelines;
  /** THE SHADOW ATLAS: one depth texture holding every casting light's
   *  map, each in a square tile of its own, made on the first frame with
   *  a light that casts and kept while it is large enough. */
  dg::RefCntAutoPtr<dg::ITexture> shadowAtlas;
  /** …read through a view of the depth format itself. */
  dg::RefCntAutoPtr<dg::ITextureView> shadowAtlasView;
  int shadowAtlasSide = 0;
  /** The maps the geometry pass under way drew, empty when no light in it
   *  casts. */
  std::vector<ShadowMap> shadows;
  /** WHAT EACH TILE'S DEPTH WAS DRAWN FROM, one fold a map in the order
   *  of `shadows`, and the atlas they were drawn into: a map whose fold
   *  is unchanged keeps its tile. */
  std::vector<uint64_t> shadowFolds;
  const dg::ITexture* shadowFoldsAtlas = nullptr;
  /** A tile of cleared depth a redrawn map's tile is reset from. */
  dg::RefCntAutoPtr<dg::ITexture> shadowBlank;
  int shadowBlankSide = 0;
  /** The clock over the frame's stages, and the pass the stages being
   *  marked belong to. */
  StageClock clock;
  std::string clockPass;
  bool clockStarted = false;

  // ---- the clock (Clock.cpp) ----
  /** Opens the frame's marks with a boundary when @p timed, and reads
   *  every earlier frame the device has finished. */
  void startClock(bool timed, const Targets& scene);
  /** A stage boundary: the stage that ends here is @p stage of the pass
   *  under way, or only a boundary when @p stage is empty. Nothing when
   *  the frame is not timed. */
  void mark(std::string_view stage);
  /** …the same, with the stage named whole rather than by its pass. */
  void markWhole(std::string_view stage);
  /** States what the clock has read on @p targets. */
  void reportClock(Targets& targets) const;

  // ---- what the whole of it is made of (Gpu.cpp) ----
  /** Sizes the frame's targets to @p size, dropping everything made at
   *  another one. */
  void resize(SkISize size);
  /** The texture @p name is written into this frame, made on the first
   *  ask. Null when there is no extent to make one at. */
  dg::ITexture* target(std::string_view name);
  /** Working target @p index, made on the first ask. */
  dg::ITexture* working(size_t index);
  /** Working target @p index at level @p level — the frame's size halved
   *  that many times, rounded up — in @p format, made on the first ask. */
  dg::ITexture* workingAt(int level, size_t index,
                          dg::TEXTURE_FORMAT format = kRadianceFormat);
  /** THE FORMAT A BLOOM'S SPREADS ARE HELD IN: light with no alpha, so
   *  three packed floats where the device draws into them, and the
   *  frame's own format where it does not. */
  dg::TEXTURE_FORMAT spreadFormat();
  /** @p name as it stands, or as it stood at the end of the frame
   *  before; null when nothing has written it. */
  dg::ITexture* current(std::string_view name);
  dg::ITexture* previous(std::string_view name);
  /** Opens a frame: what the frame before wrote becomes what this one's
   *  `previous()` names, and the texture that held the frame before THAT
   *  is what this one writes into — so a resource costs two textures for
   *  its whole life, no copy, and no allocation per frame. */
  void beginFrame();
  /** Closes it: the device's frame is finished and the residencies let
   *  go of what no view has named lately. */
  void endFrame();
  /** @p name's pixels, read back through a staging texture. Null when
   *  nothing has written it. */
  sk_sp<SkImage> read(std::string_view name);
  /** @p name's picture as an image @p canvas samples where it stands,
   *  with nothing read back: the final stage drawn into a texture handed
   *  to the canvas's queue, signalled on the GPU once drawn, and the
   *  canvas's next submission made to wait for that signal. Null when
   *  the canvas's queue cannot reach this device — another API, another
   *  device, a driver that cannot name its Metal objects — or when every
   *  picture is still out, which the caller answers with `read`. */
  sk_sp<SkImage> present(std::string_view name, SkCanvas& canvas);

  /** A texture of this frame's size in @p format. */
  dg::RefCntAutoPtr<dg::ITexture> makeColor(
      const char* label, dg::TEXTURE_FORMAT format = kRadianceFormat);
  /** …and one of @p size. */
  dg::RefCntAutoPtr<dg::ITexture> makeColorAt(SkISize size, const char* label,
                                              dg::TEXTURE_FORMAT format);
  /** The copy of the depth buffer a blended body reads, made on the
   *  first ask; null where the device refused it. */
  dg::ITexture* depthForReading();
  /** The copy of the lit colour a surface that sees through itself
   *  reads, made on the first ask; null where the device refused it. */
  dg::ITexture* sceneColorForReading();
  /** The shadow atlas at @p side texels or more a side, made on the first
   *  ask and again when a larger one is asked for; null where the device
   *  refused it. */
  dg::ITexture* shadowAtlasAt(int side);
};

/** The number the programs read a tone curve by. */
float curveNumber(ToneCurve curve);

/** THE FINAL STAGE: @p radiance, exposed, compressed by the frame's tone
 *  curve and encoded for a display, written into @p display. */
void finalStage(Gpu& gpu, dg::ITexture* radiance, dg::ITexture* display);

/** Fills the whole of @p into with @p colour, an authored colour, as the
 *  light the final stage turns back into it. */
void fillTarget(Gpu& gpu, dg::ITexture* into, const material::Color& colour);

/** Binds @p colour as this frame's target again, with the frame's depth
 *  buffer when one is wanted, keeping what both hold. */
void resumeTarget(Gpu& gpu, dg::ITexture* colour, bool withDepth);

/** THE FRAME STATE every runtime here stands on, over the device's own
 *  resources. Every runtime makes one of these and shares it among the
 *  copies of the value it hands back. */
std::shared_ptr<Gpu> makeGpu(Device& device);

/** Binds @p colour as this frame's target, with the frame's depth buffer
 *  when one is wanted, and clears both. */
void openTarget(Gpu& gpu, dg::ITexture* colour, const float* clear,
                bool withDepth);

/** A texture's placement as a shader reads it: the same matrix a host
 *  tier puts on its style, in the four-by-four the uniform is. */
glm::mat4 mapMatrix(const SkMatrix& uv);

/** The two stages of a frame, one file each. */
void paintGeometry(Gpu& gpu, const PassWork& work, const View& view,
                   Targets& targets);
void applyPost(Gpu& gpu, const PassWork& work);

}  // namespace sigil::world::diligent
