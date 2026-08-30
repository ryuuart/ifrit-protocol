#pragma once

/** @file
 * SigilGeometry pop — POP-style point combinators as VALUES. A Chain is
 * a description (nondestructive: edit a field, re-describe) over the
 * Cloud vocabulary in Points.h. The LANGUAGE and its CPU reference
 * executor share one scope: pop::on() opens a Chain, pop::cook() is the
 * reference (a Cloud, ready for points::instance / panels /
 * drawBillboards on the Skia painter), and SigilWorld runs the same
 * Chain as compute dispatches over GPU lanes. The two must agree —
 * same hashes, same spline, same formulas.
 */

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "sigilgeometry/curves/Curves.h"
#include "sigilgeometry/pop/Points.h"

namespace sigil::geometry {

/** The dials of pop's swept sinks (tube, ribbon, sweep); spelled
 *  pop::SweepSinkOptions at the call site. */
struct PopSweepSinkOptions {
  bool closed = false;  ///< join the last cooked point to the first
  int segments = 160;   ///< resampled cross-sections along the path
};

/** The point-operator language: a scope holding the vocabulary rather
 *  than a type anyone instantiates. Inside it are the attribute
 *  references operators address, the operator descriptions themselves,
 *  and the Chain that sequences them. A Chain only DESCRIBES work —
 *  executing it is the job of a backend, and the CPU and GPU backends
 *  are required to produce the same result from the same Chain. */
struct pop {
  enum class Lane : int32_t { P = 0, Dir = 1, Color = 2, Scale = 3, T = 4 };
  /** TouchDesigner's real superpower, adopted: operators address
   *  attributes BY NAME. The conventional lanes ("P", "T", "Dir",
   *  "Scale", "Color", plus "Tex" for texture hinting) are just
   *  well-known names — any other name creates a custom float4
   *  attribute on first write, flows through every filter, and
   *  exports on the cooked Cloud. The Lane enum remains as sugar for
   *  the builtins. */
  struct AttrRef {
    std::string name = "P";
    AttrRef() = default;
    AttrRef(Lane lane)  // NOLINT: implicit by design
        : name(lane == Lane::P       ? "P"
               : lane == Lane::Dir   ? "Dir"
               : lane == Lane::Color ? "Color"
               : lane == Lane::Scale ? "Scale"
                                     : "T") {}
    AttrRef(const char* n) : name(n) {}             // NOLINT: implicit
    AttrRef(std::string n) : name(std::move(n)) {}  // NOLINT
    bool operator==(const AttrRef&) const = default;
  };
  /** The GPU executor's packed-lane index for a builtin — "Tex"
   *  included, slot 5; -1 for custom names, whose chains the GPU
   *  executor declines gracefully. */
  static int32_t builtinIndex(const AttrRef& attr) {
    if (attr.name == "P") return 0;
    if (attr.name == "T") return 1;
    if (attr.name == "Dir") return 2;
    if (attr.name == "Scale") return 3;
    if (attr.name == "Color") return 4;
    if (attr.name == "Tex") return 5;
    return -1;
  }
  static constexpr int32_t kBuiltinSlots = 6;
  /** Generator: scatter count points along a window of a closed
   *  loop — writes P, T, Dir (the tangent), Scale = 1. */
  struct SplineScatter {
    std::vector<glm::vec3> loop;
    int count = 10000;
    float head = 1, span = 1;
    float radius = 0;  ///< stable per-point offset in the normal plane
    uint32_t seed = 1;
    bool operator==(const SplineScatter&) const = default;
  };
  /** Filter: lane += a stable random cube offset per point. */
  struct Jitter {
    AttrRef lane = Lane::P;
    float amplitude = 10;
    uint32_t seed = 7;
    std::string mask;  ///< see "Masks" below; empty = every point
    bool operator==(const Jitter&) const = default;
  };
  /** Filter: lane += a smooth sin-field drift sampled at P. */
  struct Noise {
    AttrRef lane = Lane::P;
    float amplitude = 10;
    float frequency = 0.01f;
    float seed = 0;
    std::string mask;
    bool operator==(const Noise&) const = default;
  };
  /** Filter: lane = lerp(from, to) by the T attribute. */
  struct Ramp {
    AttrRef lane = Lane::Color;
    glm::vec4 from = {1, 1, 1, 1};
    glm::vec4 to = {1, 1, 1, 1};
    std::string mask;
    bool operator==(const Ramp&) const = default;
  };
  /** Filter: lane.x = base * (1 + spread * (hash * 2 - 1)). */
  struct Vary {
    AttrRef lane = Lane::Scale;
    float base = 1;
    float spread = 0.5f;
    uint32_t seed = 11;
    std::string mask;
    bool operator==(const Vary&) const = default;
  };
  /** Filter: Dir = normalize(target - P) — billboards, gazes. */
  struct LookAt {
    glm::vec3 target = {0, 0, 0};
    std::string mask;
    bool operator==(const LookAt&) const = default;
  };
  /** Filter: lane = lane * mul + add, per component. */
  struct Math {
    AttrRef lane = Lane::P;
    glm::vec4 mul = {1, 1, 1, 1};
    glm::vec4 add = {0, 0, 0, 0};
    std::string mask;
    bool operator==(const Math&) const = default;
  };
  /** Filter: neighborhood smoothing — each point eases toward its
   *  chain-order neighbors' midpoint (ends clamp). The op the ribbon
   *  example demanded: it heals Noise kinks before a swept sink so
   *  parallel-transport frames stop tearing. Double-buffered on both
   *  executors, so order can't leak. */
  struct Relax {
    AttrRef lane = Lane::P;
    float strength = 0.5f;  ///< 0 = off, 1 = full midpoint
    int iterations = 1;
    std::string mask;
    bool operator==(const Relax&) const = default;
  };
  /** Generator: scatter count points ON a formed model's surface.
   *  Seeds a chain from a Mesh — sweep a tube, scatter on it, form
   *  again: pops build on pops' results. CPU-cooked today (the GPU
   *  executor declines mesh-led chains). */
  struct MeshScatter {
    Mesh mesh;
    int count = 10000;
    uint32_t seed = 1;
    bool operator==(const MeshScatter&) const = default;
  };
  /** Creator (TD's Attribute Create): fill an attribute — customs
   *  spring into being on first write. */
  struct Fill {
    AttrRef attr = "Tex";
    glm::vec4 value = {0, 0, 1, 1};
    std::string mask;
    bool operator==(const Fill&) const = default;
  };
  /** Texture hint: pick a sprite-atlas cell per point (stable hash)
   *  and write "Tex" = {uOffset, vOffset, uScale, vScale}. The
   *  stamps sink applies it to each stamped point's uvs. */
  struct Atlas {
    int cols = 2, rows = 2;
    uint32_t seed = 17;
    std::string mask;
    bool operator==(const Atlas&) const = default;
  };
  /** Filter, PRIMITIVE class (TD/Houdini's Attribute Promote,
   *  point -> prim): bake a point attribute onto the PRIMITIVES the
   *  chain's forming sink builds — Mesh::prims[to], one float4 per
   *  triangle. The reserved source name "Id" writes the owning point's
   *  index instead of reading a lane.
   *
   *  Class boundaries, stated: inert on the point sink (a Cloud has no
   *  primitives) and on the swept sinks (their triangles ride
   *  RESAMPLED cross-sections, not points — there is no owning point
   *  to promote from); honoured by the stamping sink cookMesh(). The
   *  GPU executor cooks POINTS only and declines any chain holding
   *  this op outright rather than dropping it silently. */
  struct Promote {
    AttrRef from = Lane::Color;
    std::string to;  ///< primitive lane name; empty = the source's name
    bool operator==(const Promote&) const = default;
  };
  /** Filter (TouchDesigner's Lookup): DRIVE one attribute from another
   *  through a table of stops. The key is dot(from, weights); it is
   *  remapped from [low, high] onto the table's span and sampled with
   *  linear interpolation, so the table is a curve, not a palette.
   *
   *  This is `fade` generalized — Ramp is the two-stop case driven by
   *  T with no domain. Any source attribute, any number of stops, any
   *  range: "colour by height", "size by density", a non-linear
   *  falloff on a custom lane. Per-point and count-invariant, so BOTH
   *  executors run it; an empty table is a no-op on both. */
  struct Lookup {
    AttrRef from = Lane::T;
    glm::vec4 weights = {1, 0, 0, 0};  ///< key = dot(from, weights)
    AttrRef to = Lane::Color;
    std::vector<glm::vec4> stops = {{0, 0, 0, 1}, {1, 1, 1, 1}};
    float low = 0, high = 1;  ///< the source range the table spans
    std::string mask;
    bool operator==(const Lookup&) const = default;
  };
  /** Filter, PERMUTATION class (TouchDesigner's Sort): reorder the
   *  whole point set by dot(by, weights). Every lane travels with its
   *  point — a permutation, not a rewrite, so the count never moves.
   *
   *  Chain ORDER is meaning here: the point sink draws in it (painter
   *  order for transparent sprites, which is what the Skia sink has
   *  instead of a depth buffer), the swept sinks thread their path
   *  through it, and Relax smooths along it. Sorting is therefore an
   *  authoring verb, not a display trick.
   *
   *  CPU-only, and stated as a boundary rather than a gap: a
   *  permutation is not a per-point map, so it does not fit the GPU
   *  executor's one-kernel-per-op arena model (it would want a sorting
   *  NETWORK — log^2(n) dispatches and a ping-pong — which is a
   *  different dispatch shape, not a different formula). SigilWorld
   *  declines any chain holding one, the way it declines MeshScatter
   *  and Promote. */
  struct Sort {
    AttrRef by = Lane::P;
    glm::vec4 weights = {0, 0, 1, 0};  ///< key = dot(by, weights)
    bool descending = false;
    bool operator==(const Sort&) const = default;
  };
  /** MASKS — the selection every filter takes.
   *
   *  Every per-point filter carries a `mask`: the name of a lane whose
   *  .x, clamped to [0, 1], is how much of the operator's write each
   *  point receives — result = old + (new - old) * mask. An empty name
   *  means every point in full, which is why the default is empty
   *  rather than a lane. Fractional values feather; 0 and 1 select.
   *  Naming a lane nothing has written yet selects NOBODY (an untouched
   *  custom lane is all zeros), the same way an empty group is empty.
   *  Select below is the operator that writes such a lane from a shape;
   *  any operator that writes .x — Lookup, Math, Fill, a custom lane
   *  from an importer — writes a mask too. Both executors apply the
   *  mask with the same expression, so a masked chain cooks identically
   *  on the CPU and the GPU. */

  /** Selector: write a mask lane from a region of space — 1 inside,
   *  0 outside, a smooth band across the outer `feather` fraction of
   *  the extent (0 = hard edge). Sphere tests length((p - center) /
   *  size); Box tests the largest axis ratio, so `size` is a radius per
   *  axis in both. `combine` folds the result into whatever the lane
   *  already holds (Replace, Union = max, Intersect = min, Subtract =
   *  old * (1 - new)) so several regions build one selection. `invert`
   *  flips inside and outside before combining. Reads any lane's xyz —
   *  P by default, but "select by direction" is one field away. */
  struct Select {
    enum class Shape : int32_t { Sphere = 0, Box = 1 };
    enum class Combine : int32_t {
      Replace = 0,
      Union = 1,
      Intersect = 2,
      Subtract = 3
    };
    std::string to = "sel";
    Shape shape = Shape::Sphere;
    glm::vec3 center = {0, 0, 0};
    glm::vec3 size = {100, 100, 100};
    float feather = 0;
    bool invert = false;
    Combine combine = Combine::Replace;
    AttrRef from = Lane::P;
    bool operator==(const Select&) const = default;
  };
  /** Filter: lane = matrix * lane — the whole affine vocabulary in one
   *  op (Math is the diagonal case). As a POSITION (w = 1) the
   *  translation applies; as a DIRECTION (`direction`, w = 0) only the
   *  upper 3x3 acts and the result is renormalized, so an Affine on
   *  P and a second on Dir keep a stamp's basis honest under rotation.
   *  .w of the lane passes through untouched either way. */
  struct Affine {
    AttrRef lane = Lane::P;
    glm::mat4 matrix = glm::mat4(1.0f);
    bool direction = false;
    std::string mask;
    bool operator==(const Affine&) const = default;
  };
  /** Filter: lane.xyz += normalize(along.xyz) * distance — push every
   *  point out along its own direction (Dir by default: the tangent on
   *  a loop scatter, the surface normal on a mesh scatter). A
   *  zero-length `along` moves nothing. */
  struct Peak {
    float distance = 10;
    AttrRef along = Lane::Dir;
    AttrRef lane = Lane::P;
    std::string mask;
    bool operator==(const Peak&) const = default;
  };
  /** Filter: the classic space deformers over an axis. Each point's
   *  height h = dot(p - origin, axis) is remapped to u = (h - low) /
   *  (high - low), clamped to [0, 1] (low == high puts every point at
   *  u = 1 above low and 0 below), and the effect grows with u:
   *   - Twist  rotates the perpendicular part about the axis by
   *            amount * u degrees;
   *   - Taper  scales the perpendicular part by 1 + (amount - 1) * u,
   *            so amount is the scale reached at the top;
   *   - Bend   wraps the band [low, high] into an arc of amount degrees
   *            curving toward `direction` (projected perpendicular to
   *            the axis), points past either end riding rigidly on the
   *            tangent there — the arc's length is preserved. Amount 0
   *            is the identity.
   *  Only positions bend (Dir is untouched); re-derive a direction
   *  downstream with LookAt or an Affine when it matters. */
  struct Deform {
    enum class Kind : int32_t { Twist = 0, Taper = 1, Bend = 2 };
    Kind kind = Kind::Twist;
    float amount = 90;
    glm::vec3 axis = {0, 1, 0};
    glm::vec3 origin = {0, 0, 0};
    glm::vec3 direction = {1, 0, 0};  ///< Bend only
    float low = 0, high = 100;
    AttrRef lane = Lane::P;
    std::string mask;
    bool operator==(const Deform&) const = default;
  };
  /** Filter: to = a + (b - a) * factor, the factor a constant or a
   *  lane's .x (`factorLane`; when named it replaces the constant).
   *  Blends between two attributes, copies one to another
   *  (factor 0), or fades a lane toward another by a third — Houdini's
   *  attribute blend and Blender's mix in one per-point op. */
  struct Mix {
    AttrRef a = Lane::Color;
    AttrRef b = Lane::Color;
    AttrRef to = Lane::Color;
    float factor = 0.5f;
    std::string factorLane;
    std::string mask;
    bool operator==(const Mix&) const = default;
  };
  /** Generator: seed the chain from an EXISTING point set — an imported
   *  .geo or PLY poured through asCloud(), a cooked Cloud, anything with
   *  positions — every lane riding along as an attribute. The
   *  conventional lanes land on the builtins: positions → P, "t" → T,
   *  "dir" (or "normal") → Dir, "size" → Scale, "tint" → Color, "Tex"
   *  → Tex; every other lane becomes a custom attribute of the same
   *  name (scalars in .x, vectors with w = 0, colours as they are). A
   *  Houdini group therefore arrives as a mask lane under its own name.
   *  The count is the cloud's; the cloud is the data, so re-describing
   *  with a different cloud re-uploads it on the GPU executor. */
  struct PointSet {
    Cloud cloud;
    bool operator==(const PointSet&) const = default;
  };
  /** Variant ORDER IS ABI: SigilWorld maps each op's variant index to
   *  a compute PSO. New ops are APPENDED, never inserted. */
  using Op =
      std::variant<SplineScatter, Jitter, Noise, Ramp, Vary, LookAt, Math,
                   Relax, MeshScatter, Fill, Atlas, Promote, Lookup, Sort,
                   Select, Affine, Peak, Deform, Mix, PointSet>;
  using Chain = std::vector<Op>;

  /** The artist's spelling — TouchDesigner ergonomics over the same
   *  values: one entry verb, chained INTENT verbs with loud defaults
   *  (seeds auto-vary; every parameter is optional), and the result
   *  IS the Chain — nothing hides in the builder, so reach into any
   *  op afterwards and re-cook. Sinks form directly:
   *
   *    Mesh comet = pop::on(loop).count(9000).window(0.9f, 0.3f)
   *                     .spread(40).noise(18)
   *                     .fade(pink, cyan).tube(9);
   */
  class Builder {
   public:
    explicit Builder(std::vector<glm::vec3> loop) {
      SplineScatter scatter;
      scatter.loop = std::move(loop);
      m_chain.emplace_back(scatter);
    }
    /** Compose: build ON another chain — its cooked P becomes this
     *  chain's path. Pops feed pops; positions are the currency.
     *  (Cooked on the CPU reference at build time; a GPU-resident
     *  chain-to-chain feed is the queued next step.) */
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
    Builder& spread(float radius) {
      if (auto* s = std::get_if<SplineScatter>(&m_chain.front()))
        s->radius = radius;
      return *this;
    }
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
    Builder& rampBy(std::vector<glm::vec4> stops = {{0, 0, 0, 1},
                                                    {1, 1, 1, 1}}) {
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
    /** Heal kinks: neighborhood smoothing on P (the ribbon-saver). */
    Builder& smooth(float strength = 0.5f, int iterations = 2) {
      m_chain.emplace_back(Relax{Lane::P, strength, iterations});
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
     *  space::place or glm. */
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
    /** Escape hatch: any raw op joins the chain. */
    Builder& op(Op o) {
      m_chain.push_back(std::move(o));
      return *this;
    }

    operator Chain() const { return m_chain; }
    const Chain& chain() const { return m_chain; }

    // The sinks (the executor below runs them): pick the former.
    Cloud cloud() const;
    Mesh stamps(const Mesh& stamp) const;
    Mesh tube(float radius, int sides = 12, bool closed = false,
              int segments = 160) const;
    Mesh ribbon(float width, bool closed = false, int segments = 160) const;
    Mesh sweep(const SkPath& profile, bool closed = false,
               int segments = 160) const;

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
  static Builder on(std::vector<glm::vec3> loop) {
    return Builder(std::move(loop));
  }
  /** The composing entry: points on another chain's cooked result. */
  static Builder on(const Chain& upstream) { return Builder(upstream); }
  /** The surface entry: points on a formed model's faces. */
  static Builder on(Mesh surface, int count = 10000) {
    MeshScatter scatter;
    scatter.mesh = std::move(surface);
    scatter.count = count;
    return Builder(std::move(scatter));
  }
  /** The given entry: an existing point set, lanes and all. */
  static Builder on(Cloud given) { return Builder(PointSet{std::move(given)}); }

  /** THE CPU REFERENCE EXECUTOR and the helpers both executors share.
   *  The GPU executor in SigilWorld runs the same Chain over compute
   *  lanes; everything below is the definition it must agree with. */
  /** How a PointSet's cloud lays out as attributes: the conventional
   *  lanes onto the builtins, everything else under its own name — one
   *  function, so the CPU cook and the GPU executor's initial upload
   *  agree lane for lane. @p lanes gains or overwrites the seeded names,
   *  every lane sized to the cloud. */
  static void seedAttrs(
      const Cloud& cloud,
      std::map<std::string, std::vector<glm::vec4>, std::less<>>& lanes);
  /** The custom attribute names seedAttrs would create for @p cloud (the
   *  lanes that are not builtins), in a stable order. */
  static std::vector<std::string> seedCustomNames(const Cloud& cloud);

  /** PARAMETER ADDRESSING: an operator's numeric fields by name, the way
   *  a control surface or an animation lane reaches into a chain without
   *  knowing the operator's type. Names are the struct's own field names,
   *  vector components dotted (`"center.x"`, `"add.w"`, `"from.g"`) —
   *  every float, int, bool and enum field an operator has, and nothing a
   *  string, a lane name, a mesh, a cloud or a matrix (those are
   *  descriptions, not dials). Ints truncate, bools read non-zero, enums
   *  take their integer value. `setField` returns false and writes
   *  nothing for a name the operator does not have; `getField` returns
   *  nullopt for it. */
  static bool setField(Op& op, std::string_view field, float value);
  static std::optional<float> getField(const Op& op, std::string_view field);

  /** The frame a Deform runs in: its axis normalized, its bend direction
   *  made perpendicular to that axis and normalized (a direction parallel
   *  to the axis, or zero, falls back to a fixed perpendicular), and
   *  side = axis x direction. One function, so the CPU cook and the GPU
   *  executor's parameter upload deform in the identical frame. */
  static void deformFrame(const Deform& op, glm::vec3* axis,
                          glm::vec3* direction, glm::vec3* side);

  /** The CPU reference cook: evaluates @p chain into a Cloud with the
   *  conventional lanes — "t" (scalar), "dir" (vector), "tint" (color),
   *  "size" (scalar) — bit-faithful to the GPU executor's formulas. */
  static Cloud cook(const Chain& chain);

  /** The mesh-forming sink: cook @p chain and stamp @p stamp at every
   *  point into ONE Mesh (dir orients, size scales, tint colors) — a
   *  pop-DESCRIBED 3D model, drawable by space::drawMesh on the Skia
   *  painter and place in SigilWorld alike. */
  static Mesh cookMesh(const Chain& chain, const Mesh& stamp);

  /** Swept sinks: the chain's cooked points become the PATH — a
   *  Catmull-Rom through P in chain order, so Jitter/Noise/Math edits
   *  BEND the sweep — and the curve generators form the model. The
   *  same nondestructive description, a different former. */
  using SweepSinkOptions = PopSweepSinkOptions;
  static Mesh cookTube(const Chain& chain, float radius, int sides = 12,
                       const SweepSinkOptions& options = {});
  static Mesh cookRibbon(const Chain& chain, float width,
                         const SweepSinkOptions& options = {});
  /** The general former: ANY closed 2D outline (a star, a squircle, an
   *  Ops.h recipe's result) becomes the cross-section, swept along the
   *  chain's cooked path — tube generalized to the whole shape
   *  vocabulary. */
  static Mesh cookSweep(const Chain& chain, const SkPath& profile,
                        const SweepSinkOptions& options = {});
};

inline Cloud pop::Builder::cloud() const { return pop::cook(m_chain); }
inline Mesh pop::Builder::stamps(const Mesh& stamp) const {
  return pop::cookMesh(m_chain, stamp);
}
inline Mesh pop::Builder::tube(float radius, int sides, bool closed,
                               int segments) const {
  return pop::cookTube(m_chain, radius, sides,
                       {.closed = closed, .segments = segments});
}
inline Mesh pop::Builder::ribbon(float width, bool closed, int segments) const {
  return pop::cookRibbon(m_chain, width,
                         {.closed = closed, .segments = segments});
}
inline pop::Builder::Builder(const Chain& upstream)
    : Builder(pop::cook(upstream).positions) {}

inline Mesh pop::Builder::sweep(const SkPath& profile, bool closed,
                                int segments) const {
  return pop::cookSweep(m_chain, profile,
                        {.closed = closed, .segments = segments});
}

}  // namespace sigil::geometry
