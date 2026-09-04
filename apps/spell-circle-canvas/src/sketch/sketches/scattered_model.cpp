/** @file
 * scattered_model — an imported model, and the same model as dust.
 *
 * A model is brought in through the mesh codec, fitted to the stage, and
 * two things are made of it: the body itself, standing still and dim,
 * and a cloud scattered over its surface with a small STAMP at every
 * point of it, turning above it. What the codec hands over is the same
 * `Mesh` currency a generated body is, so nothing past the import knows
 * which of the two it is holding — which is the whole of what this
 * study says. What a round trip out through the encoder and back costs
 * is `codec_roundtrip`'s subject; this one is the door alone.
 *
 * WHERE THE MODEL COMES FROM is one named file under `res://models/`,
 * the sketch assets mount — the file the repository's asset manifest
 * fetches, and no other. ONE name rather than a list of candidates is
 * what makes the picture a function of the declaration: a search over
 * whatever a machine happens to hold would photograph a different body
 * on every machine, and two pictures under one name is what a plate
 * cannot be. With nothing mounted the subject is the body generated
 * below, which is the second and last picture this file can produce and
 * is stated here rather than discovered.
 *
 * THE DUST IS A CHAIN, not a written-out cloud. `pop::on(mesh, count)`
 * scatters over the faces, one ramp reads each point's height into its
 * colour, and one vary spreads the stamp sizes about a base — so the
 * whole look is three verbs the runtime cooks, on the host executor or
 * on the device's kernels, from the same description. The height the
 * ramp spans is the subject's own bounds, so an imported model that
 * stands anywhere in space is coloured top to bottom exactly as the
 * generated one is.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilio/IO.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace io = sigil::io;
namespace gm = sigil::geometry::mesh;

namespace {

/** How far across the subject stands, whatever it turned out to be.
 *  Every distance below is written against it, so an imported model and
 *  the generated one are framed the same way. */
constexpr float kExtent = 150.0f;
constexpr int kMotes = 17000;

/** THE ONE FILE this study imports: the glTF sample asset the
 *  repository's manifest fetches into the assets directory. */
constexpr std::string_view kModel = "res://models/Avocado.glb";

/** THE MODEL, decoded through the mesh codec and fitted to the stage —
 *  or nothing, when the mount holds no such file. The bytes come from
 *  the resource hub rather than from a path, so the same call reads a
 *  mounted directory, an archive or a URL without the study knowing
 *  which. */
std::optional<gm::Mesh> imported(sketch::Assets& assets) {
  const std::string uri(kModel);
  const std::shared_ptr<const io::Bytes> bytes = assets.hub().blob(uri);
  if (!bytes || bytes->bytes.empty()) return std::nullopt;
  const std::optional<gm::codec::decode::Model> model =
      gm::codec::decode::model(bytes->bytes.data(), bytes->bytes.size(), uri);
  if (!model || model->parts.empty()) return std::nullopt;
  gm::Mesh merged = model->merged();
  merged.transform(model->fitTransform(kExtent * 2.0f));
  return merged;
}

/** …and what stands in for it: a body made here rather than read,
 *  so the study has a subject on a machine that mounted nothing. Its
 *  exponent is under one, which pulls the surface in between its axes:
 *  a scatter reads where a surface TURNS, and a sphere has no such
 *  place anywhere on it. */
gm::Mesh generated() {
  return gm::superellipsoid({kExtent, kExtent * 0.78f, kExtent * 0.9f}, 0.6f,
                            72, 48);
}

/** THE DUST: a chain over the subject's faces — the scatter, a ramp
 *  that reads each point's height into its colour over the subject's own
 *  bounds, and a vary that spreads the stamp sizes about a base. The
 *  chain is the description; what cooks it is whichever runtime the
 *  frame is performed on. */
gm::pop::Chain dustOver(const gm::Mesh& subject) {
  glm::vec3 low{0.0f}, high{0.0f};
  subject.bounds(&low, &high);
  return gm::pop::on(subject, kMotes)
      .rampBy(gm::pop::Lane::P, 1,
              {{0.35f, 0.52f, 0.95f, 1.0f}, {0.97f, 0.70f, 0.40f, 1.0f}},
              low.y, high.y)
      .vary(0.85f, 0.55f);
}

}  // namespace

namespace {

struct ScatteredModel final : sketch::Set {
  gm::Mesh subject;
  gm::pop::Chain dust;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(860, 580);
    ctx.background({0.028f, 0.031f, 0.042f, 1.0f});
    ctx.captureAt(1.7);
    std::optional<gm::Mesh> model = imported(ctx.assets);
    subject = model ? std::move(*model) : generated();
    dust = dustOver(subject);
  }

  world::Frame describe(float seconds) override {
    world::kit::Set set;
    set.rig.extent = kExtent;
    set.rig.bearing = -34.0f;
    set.rig.elevation = 30.0f;
    set.ground = 3.4f;
    set.drop = 0.92f;
    set.table.radius = 470.0f;
    set.table.height = 175.0f;
    set.table.period = 16.0f;
    set.table.fovYDeg = 44.0f;

    // The body under its own dust: dim and unlit, so what reads is the
    // silhouette the scatter was taken from rather than a second lit
    // surface competing with it.
    world::Element core = world::Element()
                       .key("body")
                       .mesh(subject)
                       .fill(material::kit::unlit(
                           {.baseColor = {0.10f, 0.11f, 0.15f, 1.0f}}))
                       .tag("core");

    // …and the dust over it: one FLAKE at every point, laid flat against
    // the surface it was scattered from — a stamp is turned by the
    // cloud's own normal lane, which `onMesh` already wrote, so a flat
    // body is the cheapest stamp that still reads as a shell. It turns
    // slowly, so the scatter reads as a skin over the silhouette rather
    // than as a texture on it.
    world::Element shell = world::Element()
                        .key("dust")
                        .rotateY(seconds * 9.0f)
                        .scale(1.05f)
                        .chain(dust)
                        .stamp(gm::quad(2.9f, 2.9f))
                        .fill(material::kit::surface(
                            {.baseColor = {1, 1, 1, 1}, .roughness = 0.65f}))
                        .tag("dust");

    return world::Frame(world::kit::litSet(world::Element()
                                              .key("subject")
                                              .child(std::move(core))
                                              .child(std::move(shell)),
                                          set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(ScatteredModel, "Set",
             "An imported model scattered into stamps \xe2\x80\x94 the mesh "
             "codec's output is the same currency a generated body is, and "
             "a cloud over its surface stands a facet at every point")
