/** @file
 * scattered_model — an imported model, and the same model as dust.
 *
 * A model is brought in through the mesh codec, fitted to the stage, and
 * two things are made of it: the body itself, standing still and dim,
 * and a cloud scattered over its surface with a small STAMP at every
 * point of it, turning above it. What the codec hands over is the same
 * `Mesh` currency a generated body is, so nothing past the import knows
 * which of the two it is holding — which is the whole of what this
 * study says.
 *
 * WHERE THE MODEL COMES FROM is `res://models/`, the sketch assets
 * mount. Nothing is mounted there in this repository, so what the
 * declaration resolves to — and therefore what a plate is taken from —
 * is the generated body below: a plate is a function of the declaration,
 * and what a machine happens to have on disk is not. Point a host at a
 * directory of models (`--assets <dir>`) and the same declaration
 * scatters those instead, one file name at a time.
 *
 * The scatter is SEEDED and resolved once, at setup: it is a property of
 * the subject and not of the moment, so every frame describes the same
 * cloud and it cooks once however long the study runs.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/codec/Decode.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilloader/Loader.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
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
namespace loader = sigil::loader;

using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

/** How far across the subject stands, whatever it turned out to be.
 *  Every distance below is written against it, so an imported model and
 *  the generated one are framed the same way. */
constexpr float kExtent = 150.0f;
constexpr int kMotes = 17000;

/** The names tried under `res://models/`, in order. They are the ones
 *  the repository's asset manifest fetches, so a host pointed at that
 *  directory finds one without being told which. */
constexpr std::string_view kCandidates[] = {"Avocado.glb", "Duck.glb",
                                            "bunny.obj"};

/** THE MODEL, decoded through the mesh codec and fitted to the stage —
 *  or nothing, when the mount holds no such file. The bytes come from
 *  the resource hub rather than from a path, so the same call reads a
 *  mounted directory, an archive or a URL without the study knowing
 *  which. */
std::optional<gm::Mesh> imported(sketch::Assets& assets) {
  for (std::string_view name : kCandidates) {
    const std::string uri = "res://models/" + std::string(name);
    const std::shared_ptr<const loader::Bytes> bytes = assets.hub().blob(uri);
    if (!bytes || bytes->bytes.empty()) continue;
    const std::optional<gm::codec::decode::Model> model =
        gm::codec::decode::model(bytes->bytes.data(), bytes->bytes.size(), uri);
    if (!model || model->parts.empty()) continue;
    gm::Mesh merged = model->merged();
    merged.transform(model->fitTransform(kExtent * 2.0f));
    return merged;
  }
  return std::nullopt;
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

/** THE DUST: points on the subject's surface, each carrying the size and
 *  the tint its stamp is drawn at. Both lanes are written from the point
 *  itself — its height decides its colour, its index its size — so the
 *  cloud describes the whole look and the stamp is one small body
 *  repeated. */
gm::Cloud dustOver(const gm::Mesh& subject) {
  gm::Cloud dust = gm::points::onMesh(subject, kMotes, 3);
  float low = 0.0f, high = 0.0f;
  for (const glm::vec3& point : dust.positions) {
    low = std::min(low, point.y);
    high = std::max(high, point.y);
  }
  const float span = std::max(high - low, 1e-3f);
  std::vector<glm::vec4>& tint = dust.color("tint");
  std::vector<float>& size = dust.scalar("size", 1.0f);
  for (size_t i = 0; i < dust.size(); ++i) {
    const float up = (dust.positions[i].y - low) / span;
    tint[i] = {0.35f + 0.62f * up, 0.52f + 0.18f * up, 0.95f - 0.55f * up,
               1.0f};
    // A varied size read off the index rather than off a generator, so
    // the cloud is the same cloud on every run and on every tier.
    size[i] = 0.55f + 0.85f * (0.5f + 0.5f * std::sin((float)i * 2.399963f));
  }
  return dust;
}

}  // namespace

namespace {

struct ScatteredModel final : sketch::Set {
  gm::Mesh subject;
  gm::Cloud dust;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(860, 580);
    ctx.background({0.028f, 0.031f, 0.042f, 1.0f});
    ctx.captureAt(1.7);
    std::optional<gm::Mesh> model = imported(ctx.assets);
    subject = model ? std::move(*model) : generated();
    dust = dustOver(subject);
  }

  world::Frame describe(float seconds) override {
    kit::Set set;
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
    Element core = Element()
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
    Element shell = Element()
                        .key("dust")
                        .rotateY(seconds * 9.0f)
                        .scale(1.05f)
                        .cloud(dust)
                        .stamp(gm::quad(2.9f, 2.9f))
                        .fill(material::kit::surface(
                            {.baseColor = {1, 1, 1, 1}, .roughness = 0.65f}))
                        .tag("dust");

    return Frame(kit::litSet(
        Element().key("subject").child(std::move(core)).child(std::move(shell)),
        set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(ScatteredModel, "Set",
             "An imported model scattered into stamps \xe2\x80\x94 the mesh "
             "codec's output is the same currency a generated body is, and "
             "a cloud over its surface stands a facet at every point")
