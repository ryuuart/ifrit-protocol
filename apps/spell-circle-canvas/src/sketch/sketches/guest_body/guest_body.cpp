/** @file
 * guest_body — ANOTHER APPLICATION'S PICTURE, WORN BY A BODY IN A LIT
 * SET.
 *
 * `guest_picture` puts a publication on a page; this one puts the same
 * publication on a screen standing in space, turning slowly in front of
 * a wall, with a bezel around it that catches the light as it comes
 * round. The picture reaches the body the way every other picture does —
 * a `material::Texture` in the base-colour slot of a surface — so
 * nothing about the set knows where the pixels came from.
 *
 * THE SCREEN IS UNLIT ON PURPOSE. A screen is its own light, and a
 * shading term across it would read as a smear over somebody else's
 * drawing. The bezel and the wall behind it are lit, which is what makes
 * the turn legible.
 *
 * THE PICTURE LEAVES THE GPU HERE, and this page says so rather than
 * implying otherwise. A frame arrives as a texture on the device the
 * publication is offered over, and the renderer that shades a body does
 * not stand there — a frame is Metal and the world draws through Vulkan
 * — so `sigil::sketch::Guest::texture` reads the pixels back into host
 * memory and the renderer uploads them to its own device like any other
 * image. `guest_picture` is the case that copies nothing: a canvas draws
 * on the device the frame arrived on, so it wraps the texture where it
 * stands.
 *
 * NOTHING ARRIVES IN A CAPTURE, and the stand-in is the honest plate:
 * what another application happens to be publishing while a still is
 * taken is not a function of this file, so a guest opened for a capture
 * subscribes to nothing at all and the screen wears the waiting card
 * this sketch paints itself. The picture is the window's.
 *
 * SEE IT MOVE. From the build directory, one Sketchbook publishing and
 * one wearing it:
 *
 *     build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
 *         --sketch feed_vitals --publish Guest
 *     build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
 *         --sketch guest_body
 *
 * `Receiver --list` says what is being offered under what name, which is
 * what to check when the screen keeps waiting.
 *
 * EDIT THESE FIRST
 *   kPublication  the name this screen waits on
 *   kApplication  the one application it will take it from; empty is any
 *   kScreen       how wide the screen stands in the set
 *   kTurnSeconds  how long one whole turn of the screen takes
 */

// TAGS: Media/Video, Data/Sources, Materials/Compositing

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/light/Light.h>

#include <memory>
#include <utility>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

using namespace sigil::compose;

namespace {

/** The name a second Sketchbook publishes under with `--publish Guest`,
 *  and the application it may be taken from — empty takes whichever
 *  application answers to the name. */
const char* kPublication = "Guest";
const char* kApplication = "";

constexpr SkSize kCanvas = {1000, 620};
constexpr SkColor4f kGround = {0.016f, 0.02f, 0.031f, 1.0f};

/** How large the screen stands, and how much bezel shows around it. The
 *  screen is 16:9 because that is the shape a canvas publishing to it
 *  most often has; a publication of any other shape is stretched over
 *  the face, since a slot's map is sampled across the whole of it. */
constexpr float kScreen = 250.0f;
constexpr float kScreenHeight = kScreen * 9.0f / 16.0f;
constexpr float kBezel = 12.0f;
/** How far the bezel stands behind the screen, so the two faces do not
 *  fight for one depth. */
constexpr float kRelief = 1.5f;

constexpr float kTurnSeconds = 14.0f;  // one whole turn of the screen
constexpr double kCaptureAt = 1.2;     // the moment a still is taken at

/** The pixel size the waiting card is painted at — the screen's own
 *  shape, so the card is not stretched over it. */
constexpr SkISize kCard = {768, 432};

constexpr glm::vec3 kEye{0.0f, 54.0f, 400.0f};

/** WHAT THE SCREEN WEARS WHILE NOBODY IS PUBLISHING: the name nothing is
 *  publishing under, and the one line that would start somebody
 *  publishing under it. A card this sketch paints rather than a flat
 *  colour, because a plate of this set should say what it is waiting
 *  for. */
Element waiting() {
  const sketch::kit::Theme& look = sketch::kit::theme();
  return stack()
      .width((float)kCard.width())
      .height((float)kCard.height())
      .fill(Fill::color(look.palette.ground))
      .children(
          {kit::centred()
               .cover()
               .column()
               .gap(look.spacing.contentGap)
               .children(
                   {text(kit::formatted("WAITING FOR “%s”", kPublication))
                        .font(look.font({.size = 32, .track = 7}))
                        .ink(look.palette.ink),
                    text(
                        "nothing on this machine is publishing under that name")
                        .font(look.font({.size = 17}))
                        .ink(look.palette.ash),
                    text(kit::formatted(
                             "Sketchbook --sketch feed_vitals --publish %s",
                             kPublication))
                        .font(look.font({.size = 16, .mono = true}))
                        .ink(look.palette.ash)})});
}

struct GuestBody {
  /** The publication, held by its name: made once per reload and kept,
   *  because making one per frame would open a subscription per frame. */
  std::shared_ptr<sketch::Guest> guest;
  /** The stand-in, painted into a texture of its own. The session keeps
   *  the scene standing for as long as a body wears it. */
  std::shared_ptr<TextureScene> card;

  void setup(sketch::SetContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    gm::camera::Camera lens;
    lens.eye = kEye;
    lens.target = {0.0f, 6.0f, 0.0f};
    lens.fovYDeg = 40.0f;
    ctx.camera(lens);

    guest = std::make_shared<sketch::Guest>(ctx, kPublication, kApplication);
    card = ctx.textureScene(kCard);
  }

  world::Frame describe(float seconds) {
    if (card) card->render(waiting(), (double)seconds);

    // THE ASK IS EVERY FRAME, which is also what opens onto a publisher
    // that appeared after this set was declared, or came back after one
    // stopped. What it answers decides which picture is on the screen
    // and nothing else about the set.
    material::Texture picture = guest ? guest->texture() : material::Texture{};
    if (!picture.valid() && card) picture = card->texture();

    material::Material screen =
        material::kit::unlit({.baseColor = {1, 1, 1, 1}});
    if (picture.valid())
      screen.slot(material::kit::kBaseColorSlot, std::move(picture));

    world::Element root;
    root.key("set").children(
        {world::Element().key("sun").light(world::light::sun(
             {-0.45f, -0.55f, -0.7f}, {0.98f, 0.97f, 0.94f, 1.0f}, 1.45f)),
         world::Element().key("fill").light(world::light::sun(
             {0.7f, -0.2f, 0.4f}, {0.42f, 0.55f, 0.78f, 1.0f}, 0.35f)),
         // The wall the set stands in front of, so the screen's turn is
         // read against something that is not turning.
         world::Element()
             .key("wall")
             .at({0, 40, -240})
             .mesh(gm::quad(1100, 620))
             .fill(material::kit::surface(
                 {.baseColor = {0.055f, 0.066f, 0.094f, 1.0f},
                  .roughness = 0.85f})),
         world::Element()
             .key("floor")
             .at({0, -108, -40})
             .rotateX(-90.0f)
             .mesh(gm::quad(1100, 520))
             .fill(material::kit::surface(
                 {.baseColor = {0.07f, 0.078f, 0.10f, 1.0f},
                  .roughness = 0.6f})),
         // THE BODY: one turning thing, with the lit bezel and the unlit
         // screen riding it, so the two faces turn together.
         world::Element()
             .key("body")
             .at({0, 6, 0})
             .rotateY(seconds * 360.0f / kTurnSeconds)
             .children({world::Element()
                            .key("bezel")
                            .at({0, 0, -kRelief})
                            .mesh(gm::quad(kScreen + kBezel * 2.0f,
                                           kScreenHeight + kBezel * 2.0f))
                            .backface(world::Backface::Visible)
                            .fill(material::kit::surface(
                                {.baseColor = {0.60f, 0.63f, 0.67f, 1.0f},
                                 .metallic = 0.9f,
                                 .roughness = 0.34f})),
                        world::Element()
                            .key("screen")
                            .mesh(gm::quad(kScreen, kScreenHeight))
                            .backface(world::Backface::Visible)
                            .fill(std::move(screen))})});
    return world::Frame(std::move(root));
  }
};

}  // namespace

SIGIL_SKETCH(GuestBody, "Media",
             "Another application's published frames worn by a body in a lit "
             "set: the frame read into a texture, dropped into a surface's "
             "base-colour slot, and turned in front of a wall — with the "
             "waiting card on the screen while nobody is offering one.")
