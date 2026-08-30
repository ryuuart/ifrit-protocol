/** @file
 * The operator dials by name: one field table per operator, walked by
 * both the setter and the getter so the two can never list different
 * fields.
 */

#include <string>

#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh {

namespace {

/** One field table per operator: name -> a float view onto the member.
 *  A visitor over the operator gives each visitor a `f(name, ref)`
 *  call per numeric field, and both setField and getField walk it, so
 *  the two can never list different fields. */
template <class OpRef, class F>
void eachField(OpRef& op, F&& f) {
  const auto vec3 = [&](const char* base, auto& v) {
    f(std::string(base) + ".x", v.x);
    f(std::string(base) + ".y", v.y);
    f(std::string(base) + ".z", v.z);
  };
  const auto vec4 = [&](const char* base, auto& v) {
    f(std::string(base) + ".x", v.x);
    f(std::string(base) + ".y", v.y);
    f(std::string(base) + ".z", v.z);
    f(std::string(base) + ".w", v.w);
    // Colour spellings for the same components.
    f(std::string(base) + ".r", v.x);
    f(std::string(base) + ".g", v.y);
    f(std::string(base) + ".b", v.z);
    f(std::string(base) + ".a", v.w);
  };
  std::visit(
      [&](auto& o) {
        using T = std::decay_t<decltype(o)>;
        if constexpr (std::is_same_v<T, pop::SplineScatter>) {
          f("count", o.count);
          f("head", o.head);
          f("span", o.span);
          f("radius", o.radius);
          f("seed", o.seed);
        } else if constexpr (std::is_same_v<T, pop::Jitter>) {
          f("amplitude", o.amplitude);
          f("seed", o.seed);
        } else if constexpr (std::is_same_v<T, pop::Noise>) {
          f("amplitude", o.amplitude);
          f("frequency", o.frequency);
          f("seed", o.seed);
        } else if constexpr (std::is_same_v<T, pop::Ramp>) {
          vec4("from", o.from);
          vec4("to", o.to);
        } else if constexpr (std::is_same_v<T, pop::Vary>) {
          f("base", o.base);
          f("spread", o.spread);
          f("seed", o.seed);
        } else if constexpr (std::is_same_v<T, pop::LookAt>) {
          vec3("target", o.target);
        } else if constexpr (std::is_same_v<T, pop::Math>) {
          vec4("mul", o.mul);
          vec4("add", o.add);
        } else if constexpr (std::is_same_v<T, pop::Relax>) {
          f("strength", o.strength);
          f("iterations", o.iterations);
        } else if constexpr (std::is_same_v<T, pop::MeshScatter>) {
          f("count", o.count);
          f("seed", o.seed);
        } else if constexpr (std::is_same_v<T, pop::Fill>) {
          vec4("value", o.value);
        } else if constexpr (std::is_same_v<T, pop::Atlas>) {
          f("cols", o.cols);
          f("rows", o.rows);
          f("seed", o.seed);
        } else if constexpr (std::is_same_v<T, pop::Lookup>) {
          vec4("weights", o.weights);
          f("low", o.low);
          f("high", o.high);
        } else if constexpr (std::is_same_v<T, pop::Sort>) {
          vec4("weights", o.weights);
          f("descending", o.descending);
        } else if constexpr (std::is_same_v<T, pop::Select>) {
          f("shape", o.shape);
          vec3("center", o.center);
          vec3("size", o.size);
          f("feather", o.feather);
          f("invert", o.invert);
          f("combine", o.combine);
        } else if constexpr (std::is_same_v<T, pop::Affine>) {
          f("direction", o.direction);
        } else if constexpr (std::is_same_v<T, pop::Peak>) {
          f("distance", o.distance);
        } else if constexpr (std::is_same_v<T, pop::Deform>) {
          f("kind", o.kind);
          f("amount", o.amount);
          vec3("axis", o.axis);
          vec3("origin", o.origin);
          vec3("direction", o.direction);
          f("low", o.low);
          f("high", o.high);
        } else if constexpr (std::is_same_v<T, pop::Mix>) {
          f("factor", o.factor);
        }
        // Promote and PointSet have no numeric fields.
      },
      op);
}

template <class T>
void assign(T& member, float value) {
  if constexpr (std::is_same_v<T, bool>)
    member = value != 0.0f;
  else if constexpr (std::is_enum_v<T>)
    member = (T)(int)value;
  else
    member = (T)value;
}

template <class T>
float readBack(const T& member) {
  if constexpr (std::is_same_v<T, bool>)
    return member ? 1.0f : 0.0f;
  else if constexpr (std::is_enum_v<T>)
    return (float)(int)member;
  else
    return (float)member;
}

}  // namespace

bool pop::setField(pop::Op& op, std::string_view field, float value) {
  bool hit = false;
  eachField(op, [&](const std::string& name, auto& member) {
    if (!hit && name == field) {
      assign(member, value);
      hit = true;
    }
  });
  return hit;
}

std::optional<float> pop::getField(const pop::Op& op, std::string_view field) {
  std::optional<float> out;
  eachField(op, [&](const std::string& name, const auto& member) {
    if (!out && name == field) out = readBack(member);
  });
  return out;
}
}  // namespace sigil::geometry::mesh
