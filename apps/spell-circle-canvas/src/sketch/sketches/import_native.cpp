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

// TAGS: Materials/Compositing, Media/Images

#include <sigilcompose/core/Core.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilscry/engine/WebEngine.h>
#include <sigilscry/engine/WebView.h>
#include <sigilscry/platform/Runtime.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scry/Settling.h>
#include <sigilsketch/scry/SharedEngine.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/style/Type.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/light/Light.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;
namespace material = sigil::material;
namespace scry = sigil::scry;
namespace gm = sigil::geometry::mesh;

using namespace sigil::compose;

namespace {

constexpr float kPanel = 200.0f;       // how large each screen stands
constexpr SkISize kBake = {512, 512};  // the compose scene's pixel size
constexpr int kPageW = 512, kPageH = 512;

constexpr glm::vec3 kEye{0.0f, 90.0f, 470.0f};

/** WHAT THE COMPOSE SCREEN CARRIES: an element tree like any other, sized
 *  in pixels because it is painted to a picture of a stated size rather
 *  than laid out in a window. */
Element dial(float edge) {
  // One bar per step of the read-out, the height a reading of the step
  // itself — the picture the compose producer paints.
  const auto tick = [](int i) {
    const float t = (float)i / 17.0f;
    return box()
        .width(10)
        .height(14 + 74.0f * (0.5f + 0.5f * std::sin(t * 8.4f)))
        .borderRadius({3})
        .fill(Fill::color({0.30f, 0.86f, 1.0f, 0.9f}));
  };
  return stack()
      .width(edge)
      .height(edge)
      .fill(Fill::color({0.043f, 0.055f, 0.094f, 1}))
      .children({box().inset(34).column().gap(22).children(
          {text("COMPOSE")
               .font({.size = 30, .track = 6})
               .ink(material::Color{1, 1, 1, 0.92f}),
           text("a composer painting into a surface of "
                "its own; texture() is the value a slot "
                "holds")
               .font({.size = 17, .track = 0.4f})
               .ink(material::Color{1, 1, 1, 0.45f})
               .width(edge - 68),
           box()
               .row()
               .gap(7)
               .alignItems(Align::End)
               .children({each(18, tick)})})});
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

/** WHAT THE PAGE IS PUT THROUGH before a body may wear it: the document,
 *  its own word that it is complete, and the view going still.
 *
 *  The page's own answer and the engine's own repaints either arrive or
 *  do not, so the picture is a function of the declaration and not of
 *  the load: the same set photographed twice on one machine wears one
 *  page. The one clock in it is the quiet window, which says the view
 *  has stopped publishing — a stretch with no event in it, which no
 *  event can announce — and a page that never stops changing is one this
 *  set cannot photograph. */
sketch::scry::Sequence arriving() {
  return {.html = page(),
          .question = "String(document.readyState)",
          .expected = "complete",
          .quiet = true};
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

struct ImportNative {
  /** WHAT THIS MACHINE MUST HAVE: the web engine, since one of the two
   *  screens is a page. */
  static bool available(std::string* why) { return scry::available(why); }

  std::shared_ptr<TextureScene> composed;
  std::shared_ptr<scry::WebView> view;
  /** After the view it settles, so the events it latched are released
   *  while that view is still standing. */
  std::unique_ptr<sketch::scry::Settling> settling;
  sk_sp<SkImage> pageFrame;

  /** WHAT THE PAGE SCREEN WEARS. Once the page has arrived, the still —
   *  the frame the settle stopped on, not whatever the view holds later.
   *  Until then, in a window, the view's own latest frame — but only
   *  once a repaint carries the loaded document: a view paints its empty
   *  page the moment it exists, and that blank is the engine's, not the
   *  page's. A capture never wears the latest: its settle is behind it
   *  before its first frame, so its every frame is a function of the
   *  scene time. The material compares the frame it wears by identity,
   *  so a frame re-read every frame is uploaded once. */
  void wearPage() {
    if (!settling) return;
    if (settling->arrived())
      pageFrame = settling->still().image;
    else if (view && settling->painted())
      pageFrame = view->frame().image;
  }

  void setup(sketch::SetContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = {880, 480},
              .captureAt = 0.4,
              .background = material::Color{0.02f, 0.024f, 0.036f, 1.0f}});
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
      // A settle standing from an earlier declaration goes before the
      // view it latched, which the new view replaces.
      settling.reset();
      view = engine->createView(kPageW, kPageH);
      settling = sketch::scry::settle(*view, arriving(), ctx.deterministic);
      // A CAPTURE HAS THE PAGE BY NOW, and an unsettled one is not a
      // picture it may wear: the frame it would photograph is one the
      // engine is still laying out. A window comes straight back and
      // wears the page as it comes, from describe().
      if (ctx.deterministic && !settling->arrived())
        throw std::runtime_error(
            "the page never stopped changing, so there is no frame to wear");
      wearPage();
    }
  }

  world::Frame describe(float seconds) {
    // The page arrives on the engine's thread, and a set describes itself
    // every frame, so this is where it is asked for rather than waited on.
    if (settling) settling->advance();
    wearPage();
    if (composed) composed->render(dial((float)kBake.width()), (double)seconds);

    material::Material screenSurface =
        material::kit::unlit({.baseColor = {1, 1, 1, 1}});

    material::Material fromCompose = screenSurface;
    if (composed)
      fromCompose.slot(material::kit::kBaseColorSlot, composed->texture());

    // THE PAGE SCREEN'S GROUND until a frame carries the page: the page's
    // own, so a page arriving in a window is a picture appearing on a
    // dark screen and not a screen changing colour — a slot with no map
    // in it is shaded as the base colour alone, and white is a flash. A
    // capture never shows the ground: its settle is behind it before its
    // first frame.
    material::Material fromPage =
        pageFrame ? screenSurface
                  : material::kit::unlit(
                        {.baseColor = {0.043f, 0.063f, 0.094f, 1.0f}});
    if (pageFrame)
      fromPage.slot(material::kit::kBaseColorSlot,
                    material::Texture::of(pageFrame));

    world::Element root;
    root.key("set").children(
        {world::Element().key("sun").light(world::light::sun(
             {-0.4f, -0.8f, -0.4f}, {0.95f, 0.96f, 1.0f, 1.0f}, 0.9f)),
         world::Element()
             .key("plate")
             .at({0, -118, 0})
             .rotateX(-90.0f)
             .mesh(gm::quad(900, 700))
             .fill(material::kit::surface(
                 {.baseColor = {0.05f, 0.06f, 0.09f, 1.0f}})),
         screen("compose", -125.0f, 17.0f, std::move(fromCompose)),
         screen("page", 125.0f, -17.0f, std::move(fromPage))});
    return world::Frame(std::move(root));
  }
};

}  // namespace

SIGIL_SKETCH(ImportNative, "Kit · API",
             "two pictures made outside the material system — a "
             "compose scene and a web page — filling the same "
             "surface slot on two bodies")
