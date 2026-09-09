/** @file
 * The attributes kept the same length through a birth and a death, the drift
 * an attribute's own rate is, and the draws an emitter takes off a stream to
 * make one particle.
 */

#include "sigilmotion/physics/Particles.h"

namespace sigil::motion::physics {

namespace {

/** A whole turn. */
constexpr float kTurn = 6.28318530717958647692f;

}  // namespace

size_t Particles::add(Vec2 at, Vec2 startingVelocity, float lifetime,
                      float startingMass) {
  const size_t index = points.add(at, startingVelocity, startingMass);
  age.push_back(0.0f);
  life.push_back(lifetime);
  for (Attribute& attribute : attributes) attribute.values.push_back(0.0f);
  return index;
}

void Particles::remove(size_t index) {
  if (index >= size()) return;
  const size_t last = size() - 1;
  age[index] = age[last];
  life[index] = life[last];
  age.pop_back();
  life.pop_back();
  for (Attribute& attribute : attributes) {
    attribute.values[index] = attribute.values[last];
    attribute.values.pop_back();
  }
  points.remove(index);
}

void Particles::clear() {
  points.clear();
  age.clear();
  life.clear();
  for (Attribute& attribute : attributes) attribute.values.clear();
}

Attribute& Particles::attribute(std::string_view name) {
  for (Attribute& attribute : attributes)
    if (attribute.name == name) return attribute;
  attributes.push_back(Attribute{.name = std::string(name)});
  attributes.back().values.assign(size(), 0.0f);
  return attributes.back();
}

const Attribute* Particles::attribute(std::string_view name) const {
  for (const Attribute& attribute : attributes)
    if (attribute.name == name) return &attribute;
  return nullptr;
}

void Particles::live(float elapsed) {
  const size_t count = size();
  for (size_t i = 0; i < count; ++i) age[i] += elapsed;
  for (Attribute& attribute : attributes) {
    // An attribute that neither drifts nor is bounded is left alone: adding
    // nothing to every value of it and holding it inside no bounds is
    // the same numbers back, and this is the attribute most consumers have
    // most of.
    const bool bounded =
        attribute.least > -std::numeric_limits<float>::infinity() ||
        attribute.most < std::numeric_limits<float>::infinity();
    if (attribute.rate == 0.0f && !bounded) continue;
    const float step = attribute.rate * elapsed;
    for (size_t i = 0; i < count; ++i)
      attribute.values[i] = std::clamp(attribute.values[i] + step,
                                       attribute.least, attribute.most);
  }
}

size_t Particles::reap() {
  return reap([this](size_t index) { return expired(index); });
}

size_t Emitter::burst(Particles& particles, core::chance::Stream& stream,
                      size_t count) const {
  if (count == 0) return 0;

  // Every attribute is asked for BEFORE any address into one is taken:
  // adding an attribute moves the vector the attributes are held in, and
  // an address taken first would be into the old one.
  const auto vectorFor = [&particles](const std::string& name) {
    return name == kLife  ? &particles.life
           : name == kAge ? &particles.age
                          : &particles.attribute(name).values;
  };
  for (const BirthAttribute& birth : attributes) vectorFor(birth.name);
  for (const FixedAttribute& held : fixed) vectorFor(held.name);

  std::vector<std::vector<float>*> drawnInto, heldInto;
  drawnInto.reserve(attributes.size());
  heldInto.reserve(fixed.size());
  for (const BirthAttribute& birth : attributes)
    drawnInto.push_back(vectorFor(birth.name));
  for (const FixedAttribute& held : fixed)
    heldInto.push_back(vectorFor(held.name));

  const Vec2 across{-along.y, along.x};
  const Vec2 sideways{-aim.y, aim.x};

  for (size_t born = 0; born < count; ++born) {
    Vec2 place = at;
    switch (from) {
      case EmitFrom::Point:
        break;
      case EmitFrom::Segment:
        place += along * (size.x * stream.signedUnit());
        break;
      case EmitFrom::Box:
        place += along * (size.x * stream.signedUnit());
        place += across * (size.y * stream.signedUnit());
        break;
      case EmitFrom::Disc: {
        const float turn = stream.unit() * kTurn;
        // The square root is what spreads the draws evenly over the AREA
        // instead of over the radius, where half of them would land in
        // the inner quarter of the disc.
        const float reach = size.x * std::sqrt(stream.unit());
        place += Vec2{std::cos(turn), std::sin(turn)} * reach;
        break;
      }
      case EmitFrom::Ring: {
        const float turn = stream.unit() * kTurn;
        place += Vec2{std::cos(turn), std::sin(turn)} * size.x;
        break;
      }
    }

    // How far off the aim, and which side of it. The two are drawn apart
    // because they are different questions — how wide the cone is opened
    // and which way this one went — and only the first is the cone's own
    // shape.
    const float off = cone * stream.unit();
    const float side = stream.unit() < 0.5f ? -1.0f : 1.0f;
    const Vec2 heading =
        aim * std::cos(off) + sideways * (std::sin(off) * side);

    // Sequenced, not two draws inside one call: which argument of a call
    // is evaluated first is nobody's promise, and a cloud a seed replays
    // would then depend on which compiler built the library.
    const Vec2 thrown = heading * speed.draw(stream);
    // A WEIGHT STATED AS ONE NUMBER IS STAMPED RATHER THAN DRAWN, and
    // costs no word — the answer is the same either way, and spending
    // one would put a range nobody uses between a cloud and the seed
    // that replays it.
    const float weight = mass.varies() ? mass.draw(stream) : mass.constant();
    const size_t index = particles.add(place, thrown, 0.0f, weight);
    for (size_t i = 0; i < attributes.size(); ++i)
      (*drawnInto[i])[index] = attributes[i].drawn.draw(stream);
    for (size_t i = 0; i < fixed.size(); ++i)
      (*heldInto[i])[index] = fixed[i].value;
  }
  return count;
}

size_t Emitter::emit(Particles& particles, core::chance::Stream& stream,
                     float elapsed) {
  if (!(rate > 0.0f) || !(elapsed > 0.0f)) return 0;
  carry += rate * elapsed;
  if (!(carry >= 1.0f)) return 0;
  const float whole = std::floor(carry);
  carry -= whole;
  // A rate and a span that between them ask for more births than a count
  // can name is the caller's arithmetic; what this may not do is convert
  // a float no `size_t` holds.
  constexpr float kMostAtOnce = 1.0e9f;
  return burst(particles, stream, (size_t)std::min(whole, kMostAtOnce));
}

}  // namespace sigil::motion::physics
