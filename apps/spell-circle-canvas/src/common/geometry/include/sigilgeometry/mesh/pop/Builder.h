#pragma once

/** @file
 * THE ARTIST'S SPELLING over the operator vocabulary: one entry verb
 * per kind of source, chained intent verbs with loud defaults, and a
 * result that IS the Chain — nothing hides in the builder, so reach
 * into any operator afterwards and re-cook. The sinks are reachable as
 * verbs too, so a description forms in one expression.
 */

#include <cstdint>
#include <utility>
#include <vector>

#include "sigilgeometry/mesh/pop/Sinks.h"

namespace sigil::geometry::mesh {
namespace pop {

/** The artist's spelling — TouchDesigner ergonomics over the same
 *  values: one entry verb, chained INTENT verbs with loud defaults
 *  (seeds auto-vary; every parameter is optional), and the result
 *  IS the Chain — nothing hides in the builder, so reach into any
 *  op afterwards and re-cook. Sinks form directly:
 *
 *    Mesh comet = pop::on(loop).count(9000).window(0.9f, 0.3f)
 *                     .spread(40).noise(18).fade(pink, cyan)
 *                     .sweep(sections::circle(), false,
 *                            {.segments = 160, .scale = 9});
 */
class Builder {
 public:
  explicit Builder(std::vector<glm::vec3> loop) {
    SplineScatter scatter;
    scatter.loop = std::move(loop);
    m_chain.emplace_back(scatter);
  }
  /** Compose: build ON another chain — its cooked P becomes this
   *  chain's path. Pops feed pops; positions are the currency. The
   *  upstream chain is cooked on the CPU reference as this one is
   *  built. */
  explicit Builder(const Chain& upstream);
  explicit Builder(MeshScatter scatter) {
    m_chain.emplace_back(std::move(scatter));
  }
  explicit Builder(PointSet given) { m_chain.emplace_back(std::move(given)); }
  /** Loop and surface entries only: a point set's count is its own. */
  Builder& count(int n) {
    if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
      s->count = n;
    else if (auto* m = std::get_if<MeshScatter>(&m_chain.front()))
      m->count = n;
    return *this;
  }
  /** Loop entries only; a surface entry has no window. */
  Builder& window(float head, float span) {
    if (auto* s = std::get_if<SplineScatter>(&m_chain.front())) {
      s->head = head;
      s->span = span;
    }
    return *this;
  }
  /** Loop entries only; a surface or point-set entry has no radius to
   *  spread and the call is a no-op. */
  Builder& spread(float radius) {
    if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
      s->radius = radius;
    return *this;
  }
  /** Pins the entry scatter's seed. Loop and surface entries only —
   *  a point set was handed its points and randomizes nothing. The
   *  chained verbs keep their own auto-varied seeds either way. */
  Builder& seed(uint32_t v) {
    if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
      s->seed = v;
    else if (auto* m = std::get_if<MeshScatter>(&m_chain.front()))
      m->seed = v;
    return *this;
  }
  Builder& jitter(float amplitude, AttrRef attr = Lane::P) {
    m_chain.emplace_back(Jitter{std::move(attr), amplitude, nextSeed()});
    return *this;
  }
  Builder& noise(float amplitude, float frequency = 0.01f,
                 AttrRef attr = Lane::P) {
    m_chain.emplace_back(
        Noise{std::move(attr), amplitude, frequency, (float)nextSeed()});
    return *this;
  }
  Builder& vary(float spread, float base = 1, AttrRef attr = Lane::Scale) {
    m_chain.emplace_back(Vary{std::move(attr), base, spread, nextSeed()});
    return *this;
  }
  Builder& fade(glm::vec4 from, glm::vec4 to) {
    m_chain.emplace_back(Ramp{Lane::Color, from, to});
    return *this;
  }
  Builder& tint(glm::vec4 color) { return fade(color, color); }
  Builder& lookAt(glm::vec3 target) {
    m_chain.emplace_back(LookAt{target});
    return *this;
  }
  Builder& move(glm::vec3 offset) {
    m_chain.emplace_back(
        Math{Lane::P, {1, 1, 1, 1}, {offset.x, offset.y, offset.z, 0}});
    return *this;
  }
  /** Create/fill any attribute — customs included. */
  Builder& fill(AttrRef attr, glm::vec4 value) {
    m_chain.emplace_back(Fill{std::move(attr), value});
    return *this;
  }
  /** Texture hint: a stable per-point sprite-atlas cell in "Tex". */
  Builder& atlas(int cols, int rows) {
    m_chain.emplace_back(Atlas{cols, rows, nextSeed()});
    return *this;
  }
  /** Drive one attribute from another through a table of stops —
   *  `fade` grown up: pick the source, pick which of its components
   *  reads, give the range it spans, hand over as many stops as the
   *  curve needs. `.rampBy(Lane::P, 1, {deep, shallow}, 0, 200)` is
   *  "colour by height". */
  Builder& rampBy(AttrRef from, int component, std::vector<glm::vec4> stops,
                  float low = 0, float high = 1, AttrRef to = Lane::Color) {
    m_chain.emplace_back(Lookup{std::move(from), componentWeight(component),
                                std::move(to), std::move(stops), low, high});
    return *this;
  }
  /** The loud-default spelling: a multi-stop gradient down T. */
  Builder& rampBy(std::vector<glm::vec4> stops = {{0, 0, 0, 1}, {1, 1, 1, 1}}) {
    return rampBy(Lane::T, 0, std::move(stops));
  }
  /** Put the points in order along an axis — farthest-first painter
   *  order for transparent sprites, or a re-threading of the path
   *  the swept sinks follow. Pass the camera's forward vector and
   *  `descending` for back-to-front. */
  Builder& order(glm::vec3 axis = {0, 0, 1}, bool descending = false) {
    m_chain.emplace_back(
        Sort{Lane::P, {axis.x, axis.y, axis.z, 0}, descending});
    return *this;
  }
  /** ...or by any attribute's component: `.orderBy("energy")`. */
  Builder& orderBy(AttrRef by, int component = 0, bool descending = false) {
    m_chain.emplace_back(
        Sort{std::move(by), componentWeight(component), descending});
    return *this;
  }
  /** Carry a point attribute onto the PRIMITIVES the sink forms —
   *  the prim class, addressed by the same names. "Id" promotes the
   *  owning point's index. An empty @p to keeps the source's name. */
  Builder& promote(AttrRef from, std::string to = {}) {
    if (to.empty()) to = from.name;
    m_chain.emplace_back(Promote{std::move(from), std::move(to)});
    return *this;
  }
  /** Heal kinks: chain-order smoothing on P (the sweep-saver). */
  Builder& smooth(float strength = 0.5f, int iterations = 2) {
    m_chain.emplace_back(Smooth{Lane::P, strength, iterations});
    return *this;
  }
  /** Push the points apart until nothing is nearer than @p radius. */
  Builder& relax(float radius, int iterations = 4, float strength = 0.5f) {
    m_chain.emplace_back(Relax{radius, iterations, strength});
    return *this;
  }
  /** Group the points into @p count clusters and write which one each
   *  landed in to a lane. */
  Builder& cluster(int count, std::string to = "cluster") {
    Cluster op;
    op.count = count;
    op.to = std::move(to);
    m_chain.emplace_back(std::move(op));
    return *this;
  }
  /** Carry a lane over from another cloud, gathered within @p radius. */
  Builder& transfer(Cloud source, std::string lane, float radius) {
    Transfer op;
    op.source = std::move(source);
    op.lane = std::move(lane);
    op.radius = radius;
    m_chain.emplace_back(std::move(op));
    return *this;
  }
  /** SELECT: write a mask lane from a region. `.select("top",
   *  Select::Shape::Box, {0, 200, 0}, {400, 60, 400})` names the
   *  points inside a slab; feather softens its edge. Later filters
   *  take it with `.masked("top")`. */
  Builder& select(std::string to, Select::Shape shape, glm::vec3 center,
                  glm::vec3 size, float feather = 0,
                  Select::Combine combine = Select::Combine::Replace,
                  bool invert = false) {
    Select g;
    g.to = std::move(to);
    g.shape = shape;
    g.center = center;
    g.size = size;
    g.feather = feather;
    g.combine = combine;
    g.invert = invert;
    m_chain.emplace_back(std::move(g));
    return *this;
  }
  /** ...the loud-default sphere. */
  Builder& select(std::string to, glm::vec3 center, float radius,
                  float feather = 0) {
    return select(std::move(to), Select::Shape::Sphere, center,
                  {radius, radius, radius}, feather);
  }
  /** MASK the operator just added: it writes each point only as far
   *  as `lane`.x says (0 none, 1 whole, between = a blend). Applies
   *  to the last filter on the chain; a no-op after a generator, a
   *  Sort, a Promote or a Select. */
  Builder& masked(std::string lane) {
    if (m_chain.empty()) return *this;
    std::visit(
        [&](auto& o) {
          if constexpr (requires { o.mask; }) o.mask = std::move(lane);
        },
        m_chain.back());
    return *this;
  }
  /** The affine vocabulary on P (or any lane): pass a matrix from
   *  camera::place or glm. */
  Builder& affine(const glm::mat4& matrix, AttrRef lane = Lane::P) {
    m_chain.emplace_back(Affine{std::move(lane), matrix, false});
    return *this;
  }
  /** ...and its direction twin: rotate Dir (or any direction lane)
   *  by the same matrix's upper 3x3, renormalized. */
  Builder& orient(const glm::mat4& matrix, AttrRef lane = Lane::Dir) {
    m_chain.emplace_back(Affine{std::move(lane), matrix, true});
    return *this;
  }
  /** Push every point along its own Dir. */
  Builder& peak(float distance, AttrRef along = Lane::Dir) {
    m_chain.emplace_back(Peak{distance, std::move(along)});
    return *this;
  }
  /** Twist about an axis: `degrees` reached at height `high`. */
  Builder& twist(float degrees, glm::vec3 axis = {0, 1, 0}, float low = 0,
                 float high = 100, glm::vec3 origin = {0, 0, 0}) {
    Deform d;
    d.kind = Deform::Kind::Twist;
    d.amount = degrees;
    d.axis = axis;
    d.origin = origin;
    d.low = low;
    d.high = high;
    m_chain.emplace_back(std::move(d));
    return *this;
  }
  /** Taper toward `scale` at height `high` (0 = a point, 2 = flare). */
  Builder& taper(float scale, glm::vec3 axis = {0, 1, 0}, float low = 0,
                 float high = 100, glm::vec3 origin = {0, 0, 0}) {
    Deform d;
    d.kind = Deform::Kind::Taper;
    d.amount = scale;
    d.axis = axis;
    d.origin = origin;
    d.low = low;
    d.high = high;
    m_chain.emplace_back(std::move(d));
    return *this;
  }
  /** Bend the band [low, high] along `axis` into an arc of `degrees`
   *  toward `direction`. */
  Builder& bend(float degrees, glm::vec3 axis = {0, 1, 0},
                glm::vec3 direction = {1, 0, 0}, float low = 0,
                float high = 100, glm::vec3 origin = {0, 0, 0}) {
    Deform d;
    d.kind = Deform::Kind::Bend;
    d.amount = degrees;
    d.axis = axis;
    d.origin = origin;
    d.direction = direction;
    d.low = low;
    d.high = high;
    m_chain.emplace_back(std::move(d));
    return *this;
  }
  /** Blend two attributes into a third by a constant... */
  Builder& mix(AttrRef a, AttrRef b, AttrRef to, float factor = 0.5f) {
    m_chain.emplace_back(
        Mix{std::move(a), std::move(b), std::move(to), factor, {}});
    return *this;
  }
  /** ...or by a lane's .x — "fade toward white by heat". */
  Builder& mixBy(AttrRef a, AttrRef b, AttrRef to, std::string factorLane) {
    m_chain.emplace_back(Mix{std::move(a), std::move(b), std::move(to), 0,
                             std::move(factorLane)});
    return *this;
  }
  /** Duplicate an attribute under another name. */
  Builder& copy(const AttrRef& from, AttrRef to) {
    m_chain.emplace_back(Mix{from, from, std::move(to), 0, {}});
    return *this;
  }
  /** DELETE: drop the points @p mask names — `select` names them,
   *  this removes them. A point's mask .x at or above @p threshold
   *  counts as named. */
  Builder& drop(std::string mask, float threshold = 0.5f) {
    m_chain.emplace_back(Delete{std::move(mask), threshold, false});
    return *this;
  }
  /** ...and its complement: keep only the points @p mask names. */
  Builder& keep(std::string mask, float threshold = 0.5f) {
    m_chain.emplace_back(Delete{std::move(mask), threshold, true});
    return *this;
  }
  /** NORMAL: make a direction lane unit-length, and — with a nonzero
   *  @p sense — turn every one of them away from @p center (+1) or
   *  toward it (-1). */
  Builder& normal(float sense = 0, glm::vec3 center = {0, 0, 0},
                  AttrRef lane = Lane::Dir) {
    Normal n;
    n.lane = std::move(lane);
    n.center = center;
    n.sense = sense;
    m_chain.emplace_back(std::move(n));
    return *this;
  }
  /** Escape hatch: any raw op joins the chain. */
  Builder& op(Op o) {
    m_chain.push_back(std::move(o));
    return *this;
  }

  operator Chain() const { return m_chain; }
  const Chain& chain() const { return m_chain; }

  // The sinks (the runtime below cooks for them): pick the former.
  /** The chain cooked to its points — pop::cook on this builder's
   *  chain. */
  Cloud cloud(const Runtime& runtime = Runtime::cpu()) const;
  /** The chain cooked and @p stamp placed at every point — pop::cookMesh
   *  on this builder's chain. */
  Mesh stamps(const Mesh& stamp, const Runtime& runtime = Runtime::cpu()) const;
  /** The chain cooked into a spine and @p profile carried along it —
   *  pop::cookSweep on this builder's chain. */
  Mesh sweep(const path::Polyline& profile, bool closed = false,
             const SweepOptions& options = {.segments = 160},
             const Runtime& runtime = Runtime::cpu()) const;
  /** The chain cooked and splatted onto @p canvas as camera-facing
   *  sprites — pop::cookBillboards on this builder's chain. */
  void billboards(SkCanvas& canvas, const camera::Camera& camera,
                  SkSize viewport, const points::BillboardStyle& style = {},
                  const Runtime& runtime = Runtime::cpu()) const;

 private:
  static glm::vec4 componentWeight(int component) {
    glm::vec4 w{0, 0, 0, 0};
    w[component < 0 ? 0 : (component > 3 ? 3 : component)] = 1;
    return w;
  }
  uint32_t nextSeed() { return m_seed++; }
  Chain m_chain;
  uint32_t m_seed = 101;
};
/** The entry verb: points on (a window of) this closed loop. */
inline Builder on(std::vector<glm::vec3> loop) {
  return Builder(std::move(loop));
}
/** The composing entry: points on another chain's cooked result. */
inline Builder on(const Chain& upstream) { return Builder(upstream); }
/** The surface entry: points on a formed model's faces. */
inline Builder on(Mesh surface, int count = 10000) {
  MeshScatter scatter;
  scatter.mesh = std::move(surface);
  scatter.count = count;
  return Builder(std::move(scatter));
}
/** The given entry: an existing point set, lanes and all. */
inline Builder on(Cloud given) { return Builder(PointSet{std::move(given)}); }

}  // namespace pop

inline Cloud pop::Builder::cloud(const Runtime& runtime) const {
  return pop::cook(m_chain, runtime);
}
inline Mesh pop::Builder::stamps(const Mesh& stamp,
                                 const Runtime& runtime) const {
  return pop::cookMesh(m_chain, stamp, runtime);
}
inline void pop::Builder::billboards(SkCanvas& canvas,
                                     const camera::Camera& camera,
                                     SkSize viewport,
                                     const points::BillboardStyle& style,
                                     const Runtime& runtime) const {
  pop::cookBillboards(m_chain, canvas, camera, viewport, style, runtime);
}
inline pop::Builder::Builder(const Chain& upstream)
    : Builder(pop::cook(upstream).positions) {}

inline Mesh pop::Builder::sweep(const path::Polyline& profile, bool closed,
                                const pop::SweepOptions& options,
                                const Runtime& runtime) const {
  return pop::cookSweep(m_chain, profile, closed, options, runtime);
}

}  // namespace sigil::geometry::mesh
