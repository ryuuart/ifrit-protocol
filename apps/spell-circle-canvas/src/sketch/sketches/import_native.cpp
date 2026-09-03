/** @file
 * import_native — pictures made somewhere else, worn by a body.
 *
 * A surface reads its colour from whatever fills its `baseColorMap` slot,
 * and a `material::Texture` is that filler however the pixels were made.
 * Two producers stand here, and neither of them is the material system:
 *
 *   COMPOSE — `ctx.textureScene(size)` is a composer painting into a
 *     surface of its own. Its `texture()` is an ordinary value with every
 *     sampling dial on it, and the session keeps the scene alive for as
 *     long as a body wears it, because a scene that went would take the
 *     texture its image names with it.
 *   SCRY — a `WebView` publishes an immutable frame per repaint. The
 *     frame's `image` is a picture like any other, so `Texture::of` puts
 *     the page on a body with no adapter in between.
 *
 * BOTH ARRIVE AS HOST IMAGES, and this page says so rather than implying
 * otherwise: a renderer standing on a device uploads a copy of them.
 *
 * THE ZERO-COPY DOOR IS `world::diligent::importNative(device, native)`.
 * It gives the device a handle over a texture the graphics API already
 * holds — a decoder's, another engine's, a capture's — and hands back a
 * `material::Texture` whose `image()` is NULL, which is not an omission:
 * it is what says the pixels were never read back, so a renderer holding
 * another device draws the body undressed rather than something it
 * invented. A sketch reaches the device through `sketch::device()`, which
 * is null on the CPU tier, and the host force-loads every archive it
 * links, so a set that walks through the door does so under `--gpu`. This
 * sheet stops at the two producers: what it shows is the host image each
 * one hands over, on either tier.
 *
 * EDIT THESE FIRST
 *   kPanel — how large each screen stands in the set.
 *   kBake  — the pixel size the compose scene is painted at.
 *   the page, below — what the second screen wears.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilscry/platform/Runtime.h>
#include <sigilsketch/scry/SharedEngine.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/light/Light.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <thread>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;
namespace material = sigil::material;
namespace scry = sigil::scry;
namespace gm = sigil::geometry::mesh;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr float kPanel = 200.0f;       // how large each screen stands
constexpr SkISize kBake = {512, 512};  // the compose scene's pixel size
constexpr int kPageW = 512, kPageH = 512;

constexpr glm::vec3 kEye{0.0f, 90.0f, 470.0f};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

/** WHAT THE COMPOSE SCREEN CARRIES: an element tree like any other, sized
 *  in pixels because it is painted to a picture of a stated size rather
 *  than laid out in a window. */
Element dial(float edge) {
  auto ticks = box().row().gap(7).alignItems(Align::End);
  for (int i = 0; i < 18; ++i) {
    const float t = (float)i / 17.0f;
    ticks.child(box()
                    .width(10)
                    .height(14 + 74.0f * (0.5f + 0.5f * std::sin(t * 8.4f)))
                    .corners({3})
                    .fill(Fill::color({0.30f, 0.86f, 1.0f, 0.9f})));
  }
  return stack()
      .width(edge)
      .height(edge)
      .fill(Fill::color({0.043f, 0.055f, 0.094f, 1}))
      .child(box()
                 .absolute()
                 .inset(34, 34, 34, 34)
                 .column()
                 .gap(22)
                 .child(text(toU8("COMPOSE"), label(30, {1, 1, 1, 0.92f}, 6)))
                 .child(text(toU8("a composer painting into a surface of "
                                  "its own; texture() is the value a slot "
                                  "holds"),
                             label(17, {1, 1, 1, 0.45f}, 0.4f))
                            .width(Dim(edge - 68)))
                 .child(std::move(ticks)));
}

/** WHAT THE PAGE SCREEN CARRIES. Laid out by the web engine, published as
 *  a frame, worn as an image. */
const char* page() {
  return R"HTML(<!doctype html><meta charset="utf-8"><style>
  html,body{margin:0;height:100%;background:#0b1018;color:#e8f2ff;
    font:16px/1.5 -apple-system,Helvetica,Arial,sans-serif}
  .pad{padding:34px}
  h1{margin:0 0 18px;font-size:30px;letter-spacing:6px;font-weight:600}
  p{margin:0 0 22px;color:#7fa0c4;font-size:17px}
  .grid{display:grid;grid-template-columns:repeat(3,1fr);gap:12px}
  .box{height:88px;border-radius:10px;
    background:linear-gradient(140deg,#1d3d63,#0f2036)}
  .box:nth-child(2n){background:linear-gradient(140deg,#2a6f6a,#0f2b2a)}
</style><div class="pad">
<h1>SCRY</h1>
<p>a page laid out by the web engine, published as one immutable frame
and worn by a body</p>
<div class="grid"><div class="box"></div><div class="box"></div>
<div class="box"></div><div class="box"></div><div class="box"></div>
<div class="box"></div></div></div>)HTML";
}

/** Waits until the view has published a frame and stopped changing it. */
bool settled(scry::WebView& view) {
  using namespace std::chrono_literals;
  const auto deadline = std::chrono::steady_clock::now() + 15s;
  uint64_t published = 0;
  int stableTicks = 0;
  while (std::chrono::steady_clock::now() < deadline && stableTicks < 8) {
    const uint64_t version = view.frameVersion();
    stableTicks = (version > 0 && version == published) ? stableTicks + 1 : 0;
    published = version;
    std::this_thread::sleep_for(16ms);
  }
  return published > 0;
}

/** A screen: a quad wearing @p dressed, tilted @p yawDeg about the
 *  vertical and standing at @p x. Unlit on purpose — a screen emits, and
 *  a lighting term across it would read as a smear. */
world::Element screen(const char* key, float x, float yawDeg,
                      material::Material dressed) {
  return world::Element()
      .key(key)
      .at({x, 10.0f, 0.0f})
      .rotateY(yawDeg)
      .mesh(gm::quad(kPanel, kPanel))
      .fill(std::move(dressed));
}

}  // namespace

namespace {

struct ImportNative final : sketch::Set {
  /** WHAT THIS MACHINE MUST HAVE: the web engine, since one of the two
   *  screens is a page. */
  static bool available(std::string* why) { return scry::available(why); }

  std::shared_ptr<TextureScene> composed;
  std::shared_ptr<scry::WebView> view;
  sk_sp<SkImage> pageFrame;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(880, 480);
    ctx.background({0.02f, 0.024f, 0.036f, 1.0f});
    ctx.captureAt(0.4);
    gm::camera::Camera lens;
    lens.eye = kEye;
    lens.target = {0.0f, 0.0f, 0.0f};
    lens.fovYDeg = 40.0f;
    ctx.camera(lens);

    // The compose producer: the session keeps the scene, so the texture a
    // body wears stays standing.
    composed = ctx.textureScene(kBake);

    // The scry producer: one engine, one view, one settled frame. The
    // frame is held rather than re-read, because a set's every frame is a
    // function of the scene time and a live page is not.
    const std::shared_ptr<scry::WebEngine> engine =
        sketch::scry::sharedEngine();
    if (engine) {
      view = engine->createView(kPageW, kPageH);
      view->loadHTML(page());
      if (settled(*view)) pageFrame = view->frame().image;
    }
  }

  world::Frame describe(float seconds) override {
    if (composed) composed->render(dial((float)kBake.width()), (double)seconds);

    material::Material screenSurface =
        material::kit::unlit({.baseColor = {1, 1, 1, 1}});

    material::Material fromCompose = screenSurface;
    if (composed)
      fromCompose.child(material::kit::kBaseColorSlot, composed->texture());

    material::Material fromPage = screenSurface;
    if (pageFrame)
      fromPage.child(material::kit::kBaseColorSlot,
                     material::Texture::of(pageFrame));

    world::Element root;
    root.key("set")
        .child(world::Element().key("sun").light(world::light::sun(
            {-0.4f, -0.8f, -0.4f}, {0.95f, 0.96f, 1.0f, 1.0f}, 0.9f)))
        .child(world::Element()
                   .key("plate")
                   .at({0, -118, 0})
                   .rotateX(-90.0f)
                   .mesh(gm::quad(900, 700))
                   .fill(material::kit::surface(
                       {.baseColor = {0.05f, 0.06f, 0.09f, 1.0f}})))
        .child(screen("compose", -125.0f, 17.0f, std::move(fromCompose)))
        .child(screen("page", 125.0f, -17.0f, std::move(fromPage)));
    return world::Frame(std::move(root));
  }
};

}  // namespace

SIGIL_SKETCH(ImportNative, "Kit \xc2\xb7 API",
             "two pictures made outside the material system \xe2\x80\x94 a "
             "compose scene and a web page \xe2\x80\x94 filling the same "
             "surface slot on two bodies")
