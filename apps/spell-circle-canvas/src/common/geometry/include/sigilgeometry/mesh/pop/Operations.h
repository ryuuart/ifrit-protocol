#pragma once

/** @file
 * The POP OPERATOR VOCABULARY: the attribute a filter addresses, the
 * twenty-five operator descriptions themselves, and the Chain that
 * sequences them. Every one is a VALUE — a description, nondestructive:
 * edit a field and re-describe — and nothing here evaluates anything.
 *
 * The variant order is what a device runtime rests on, so an operator
 * is appended and never inserted.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "sigilgeometry/mesh/pop/Points.h"

namespace sigil::geometry::mesh {

/** The point-operator language: a scope holding the vocabulary rather
 *  than a type anyone instantiates. Inside it are the attribute
 *  references operators address, the operator descriptions themselves,
 *  and the Chain that sequences them. A Chain only DESCRIBES work —
 *  executing it is the job of a backend, and the CPU and GPU backends
 *  are required to produce the same result from the same Chain. */
namespace pop {
/** Sugar for the conventional attribute names: `Lane::P` and the
 *  string "P" address the same attribute. Anything outside this set is
 *  reached by its name. */
enum class Lane : int32_t { P = 0, T = 1, Dir = 2, Scale = 3, Color = 4 };
/** TouchDesigner's real superpower, adopted: operators address
 *  attributes BY NAME. The conventional lanes ("P", "T", "Dir",
 *  "Scale", "Color", plus "Tex" for texture hinting) are just
 *  well-known names — any other name creates a custom float4
 *  attribute on first write, flows through every filter, and
 *  exports on the cooked Cloud. The Lane enum remains as sugar for
 *  the builtins. */
struct AttributeReference {
  std::string name = "P";
  AttributeReference() = default;
  AttributeReference(Lane lane)  // NOLINT: implicit by design
      : name(lane == Lane::P       ? "P"
             : lane == Lane::Dir   ? "Dir"
             : lane == Lane::Color ? "Color"
             : lane == Lane::Scale ? "Scale"
                                   : "T") {}
  AttributeReference(const char* n) : name(n) {}             // NOLINT: implicit
  AttributeReference(std::string n) : name(std::move(n)) {}  // NOLINT
  bool operator==(const AttributeReference&) const = default;
};
/** Which builtin @p attribute is — "Tex" included, slot 5 — and -1 for a
 *  custom name. It is how a reader tells the conventional lanes apart
 *  from the ones a chain invented, which is what the export needs. The
 *  five it shares with `Lane` are numbered the same there: one set of
 *  names has one numbering. */
inline int32_t builtinIndex(const AttributeReference& attribute) {
  if (attribute.name == "P") return 0;
  if (attribute.name == "T") return 1;
  if (attribute.name == "Dir") return 2;
  if (attribute.name == "Scale") return 3;
  if (attribute.name == "Color") return 4;
  if (attribute.name == "Tex") return 5;
  return -1;
}
inline constexpr int32_t kBuiltinSlots = 6;
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
  AttributeReference lane = Lane::P;
  float amplitude = 10;
  uint32_t seed = 7;
  std::string mask;  ///< see "Masks" below; empty = every point
  bool operator==(const Jitter&) const = default;
};
/** Filter: lane += a smooth sin-field drift sampled at P. Its field
 *  is a sum of LIBRARY sines, which is why it has no portable kernel:
 *  a polynomial sine is a different function, not a rounding of one,
 *  so a kernel would change what this operator means rather than where
 *  it runs. */
struct Noise {
  AttributeReference lane = Lane::P;
  float amplitude = 10;
  float frequency = 0.01f;
  float seed = 0;
  std::string mask;
  bool operator==(const Noise&) const = default;
};
/** Filter: lane = lerp(from, to) by the T attribute. */
struct Ramp {
  AttributeReference lane = Lane::Color;
  glm::vec4 from = {1, 1, 1, 1};
  glm::vec4 to = {1, 1, 1, 1};
  std::string mask;
  bool operator==(const Ramp&) const = default;
};
/** Filter: EVERY COMPONENT of the lane = base * (1 + spread * (hash * 2
 *  - 1)). The one value goes into all four, so a `Vary` on a colour
 *  varies its alpha with its channels. */
struct Vary {
  AttributeReference lane = Lane::Scale;
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
/** Filter: lane = lane * multiplier + add, per component. */
struct Math {
  AttributeReference lane = Lane::P;
  glm::vec4 multiplier = {1, 1, 1, 1};
  glm::vec4 add = {0, 0, 0, 0};
  std::string mask;
  bool operator==(const Math&) const = default;
};
/** Filter: CHAIN-ORDER SMOOTHING — each point eases toward the midpoint
 *  of the two beside it IN THE CHAIN (ends clamp). It heals Noise kinks
 *  before a swept sink so parallel-transport frames stop tearing.
 *  Double-buffered, so order can't leak — which is also why it has no
 *  per-point kernel: a point reads two it does not own, so one lane
 *  cannot be both what is read and what is written.
 *
 *  It is not `Relax`, and the two are not variants of one operator: this
 *  one reads the neighbours a SORT decides and works on any lane, and
 *  that one reads the neighbours SPACE decides and works on positions.
 *  A chain of points along a rail wants this; a scatter wants that. */
struct Smooth {
  AttributeReference lane = Lane::P;
  float strength = 0.5f;  ///< 0 = off, 1 = full midpoint
  int iterations = 1;
  std::string mask;
  bool operator==(const Smooth&) const = default;
};
/** Generator: scatter count points ON a formed model's surface.
 *  Seeds a chain from a Mesh — sweep a cable, scatter on it, form
 *  again: pops build on pops' results. Like every generator it is
 *  evaluated on the host wherever the chain is cooked, because it
 *  MAKES the points rather than mapping over them. */
struct MeshScatter {
  Mesh mesh;
  int count = 10000;
  uint32_t seed = 1;
  bool operator==(const MeshScatter&) const = default;
};
/** Creator (TD's Attribute Create): fill an attribute — customs
 *  spring into being on first write. */
struct Fill {
  AttributeReference attribute = "Tex";
  glm::vec4 value = {0, 0, 1, 1};
  std::string mask;
  bool operator==(const Fill&) const = default;
};
/** Texture hint: pick a sprite-atlas cell per point (stable hash)
 *  and write "Tex" = {uOffset, vOffset, uScale, vScale}. The
 *  stamps sink applies it to each stamped point's uvs. */
struct Atlas {
  int columns = 2, rows = 2;
  uint32_t seed = 17;
  std::string mask;
  bool operator==(const Atlas&) const = default;
};
/** Filter, PRIMITIVE class (TD/Houdini's Attribute Promote,
 *  point -> primitive): bake a point attribute onto the PRIMITIVES the
 *  chain's forming sink builds — Mesh::primitives[to], one float4 per
 *  triangle. The reserved source name "Id" writes the owning point's
 *  index instead of reading a lane.
 *
 *  Class boundaries, stated: inert on the point sink (a Cloud has no
 *  primitives) and on the swept sinks (their triangles ride
 *  RESAMPLED cross-sections, not points — there is no owning point
 *  to promote from); honoured by the stamping sink cookMesh(). A
 *  device executor cooks POINTS only and declines any chain holding
 *  this operation outright rather than dropping it silently. */
struct Promote {
  AttributeReference from = Lane::Color;
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
  AttributeReference from = Lane::T;
  glm::vec4 weights = {1, 0, 0, 0};  ///< key = dot(from, weights)
  AttributeReference to = Lane::Color;
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
 *  through it, and Smooth smooths along it. Sorting is therefore an
 *  authoring verb, not a display trick.
 *
 *  Host-only, and stated as a boundary rather than a gap: a
 *  permutation is not a per-point map, so it has no kernel (it would
 *  want a sorting NETWORK — log^2(n) dispatches and a ping-pong —
 *  which is a different dispatch shape, not a different formula). A
 *  device executor declines any chain holding one, the way it
 *  declines Smooth and Promote. */
struct Sort {
  AttributeReference by = Lane::P;
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
  AttributeReference from = Lane::P;
  bool operator==(const Select&) const = default;
};
/** Filter: lane = matrix * lane — the whole affine vocabulary in one
 *  operation (Math is the diagonal case). As a POSITION (w = 1) the
 *  translation applies; as a DIRECTION (`direction`, w = 0) only the
 *  upper 3x3 acts and the result is renormalized, so an Affine on
 *  P and a second on Dir keep a stamp's basis honest under rotation.
 *  .w of the lane passes through untouched either way. */
struct Affine {
  AttributeReference lane = Lane::P;
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
  AttributeReference along = Lane::Dir;
  AttributeReference lane = Lane::P;
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
 *  downstream with LookAt or an Affine when it matters. Twist and Bend
 *  turn on library trigonometry, so like Noise this operator has no
 *  portable kernel and a device executor declines it. */
struct Deform {
  enum class Kind : int32_t { Twist = 0, Taper = 1, Bend = 2 };
  Kind kind = Kind::Twist;
  float amount = 90;
  glm::vec3 axis = {0, 1, 0};
  glm::vec3 origin = {0, 0, 0};
  glm::vec3 direction = {1, 0, 0};  ///< Bend only
  float low = 0, high = 100;
  AttributeReference lane = Lane::P;
  std::string mask;
  bool operator==(const Deform&) const = default;
};
/** Filter: to = a + (b - a) * factor, the factor a constant or a
 *  lane's .x (`factorLane`; when named it replaces the constant).
 *  Blends between two attributes, copies one to another
 *  (factor 0), or fades a lane toward another by a third — Houdini's
 *  attribute blend and Blender's mix in one per-point operation. */
struct Mix {
  AttributeReference a = Lane::Color;
  AttributeReference b = Lane::Color;
  AttributeReference to = Lane::Color;
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
/** Filter, SET class (TouchDesigner's Delete): drop the points a mask
 *  lane names — the other half of `Select`, which can feather a
 *  selection but not remove it. A point whose `mask` .x is at or above
 *  `threshold` is SELECTED; the selected go, and `keep` inverts that
 *  so only they remain. Every lane is compacted through one
 *  permutation, so the store stays coherent and only the count moves.
 *
 *  An unnamed mask deletes nothing. That is the one place the mask
 *  convention is read the other way round — elsewhere an empty name
 *  means every point in full, and here it would mean the whole set,
 *  which is not something an operator should do by omission.
 *
 *  Host-only, and a boundary rather than a gap: the count is what this
 *  operator changes, and a per-point map cannot change it. A device
 *  executor declines a chain holding one, the way it declines Sort. */
struct Delete {
  std::string mask;
  float threshold = 0.5f;
  bool keep = false;
  bool operator==(const Delete&) const = default;
};
/** Filter (TouchDesigner's Normal): make a direction lane a UNIT
 *  direction, and give every one of them the same sense. `lane` is
 *  normalized in place, a direction too short to have one taking
 *  `fallback` instead; then, where `sense` is not zero, each is turned
 *  to face away from `center` (+1) or toward it (-1), measured from
 *  the point's own position in `from`. That is what makes a scattered
 *  surface's directions agree across a seam, and what stands a
 *  closed shape's stamps up the same way all over. */
struct Normal {
  AttributeReference lane = Lane::Dir;
  AttributeReference from = Lane::P;
  glm::vec3 center = {0, 0, 0};
  float sense = 0;
  glm::vec3 fallback = {0, 0, 1};
  std::string mask;
  bool operator==(const Normal&) const = default;
};
/** Filter: SPATIAL RELAXATION — every point pushed out of the way of the
 *  points within `radius` of it, all of them at once, the pass repeated.
 *  What turns a scatter that clumped into one that is evenly spread
 *  without being a lattice, and the operator a packing, a stipple and a
 *  settled particle set all want.
 *
 *  It is POSITIONAL by definition: the neighbours are the ones space
 *  decides and the thing moved is where the point is, so there is no
 *  lane to name. Two coincident points have no direction to separate
 *  along and are left where they are — inventing a bearing for them
 *  would make the answer depend on the order the points are walked in.
 *
 *  Host-only, and a boundary rather than a gap: a point reads points it
 *  does not own, which is the same reason `Smooth` has no kernel. A
 *  device executor declines a chain holding one by name. */
struct Relax {
  float radius = 1;
  int iterations = 4;
  float strength = 0.5f;  ///< how much of each pass' push is taken, 0 to 1
  std::string mask;
  bool operator==(const Relax&) const = default;
};
/** Filter: K-MEANS CLUSTERING — the points grouped into `count` groups by
 *  proximity in whatever metric `weights` names, with the group each
 *  point landed in written to the `to` lane's .x as a whole number.
 *
 *  `weights` is what makes one operator serve every grouping a caller
 *  wants: {1, 1, 1, 0} over P groups by POSITION, {0, 0, 0, 1} over a
 *  colour lane groups by ALPHA, and an uneven set groups by a squashed
 *  metric — near in x, anywhere in y. The centres start at `count` points
 *  drawn from the set itself, seeded, so one seed is one grouping.
 *
 *  Host-only for the reason every neighbourhood operator is. */
struct Cluster {
  AttributeReference from = Lane::P;
  std::string to = "cluster";
  glm::vec4 weights = {1, 1, 1, 0};
  int count = 8;
  uint32_t seed = 1;
  int iterations = 8;
  bool operator==(const Cluster&) const = default;
};
/** Filter: ATTRIBUTE TRANSFER — one lane carried over from ANOTHER cloud,
 *  each destination point taking a distance-weighted average of the
 *  nearest `maxSamples` source points within `radius`.
 *
 *  `maxSamples` of one is a plain nearest-neighbour lookup, which is what
 *  "give every scattered point the colour of the nearest sample" is.
 *  Above one the gather smooths across the sources, weighted by one over
 *  the distance, so a sparse source does not band. `blendWidth` is the
 *  outer fraction of the radius over which the answer tapers back to
 *  what the destination lane already held, so a transfer that only
 *  covers part of a cloud does not leave a hard edge where its reach
 *  ends. A point with no source in range keeps its own value whatever
 *  the blend width is.
 *
 *  Host-only for the reason every neighbourhood operator is, and doubly
 *  so: the points read are not even in the cloud being cooked. */
struct Transfer {
  Cloud source;
  /** Which lane travels. The lane is read on the source and written on
   *  the destination under the same name; an empty name transfers
   *  nothing. */
  std::string lane;
  /** How far a destination point looks. Zero looks nowhere. */
  float radius = 0;
  int maxSamples = 1;
  /** The outer fraction of the radius the answer fades back over, 0 to
   *  1. Zero is a hard edge at the radius. */
  float blendWidth = 0;
  std::string mask;
  bool operator==(const Transfer&) const = default;
};
/** Variant ORDER IS ABI: a renderer that dispatches these on a device
 *  maps each operation's variant index to a compute pipeline. New operations
 * are APPENDED, never inserted. */
using Operation =
    std::variant<SplineScatter, Jitter, Noise, Ramp, Vary, LookAt, Math, Smooth,
                 MeshScatter, Fill, Atlas, Promote, Lookup, Sort, Select,
                 Affine, Peak, Deform, Mix, PointSet, Delete, Normal, Relax,
                 Cluster, Transfer>;
using Chain = std::vector<Operation>;

/** The operator's own name — "Jitter", "Select", "PointSet" — for a
 *  chain listed on a control surface and for the message a runtime
 *  that cannot run one produces. */
std::string_view operationName(const Operation& operation);

}  // namespace pop
}  // namespace sigil::geometry::mesh
