#pragma once

/** @file
 * ONE SEEDED STREAM, AND THE SHAPES DRAWN FROM IT.
 *
 * `Noise.h` holds the mixers: stateless words, and two hand-carried
 * counters. What is here is the stream a caller HOLDS — a value, copyable
 * and assignable, that a component can keep in a member and a describe
 * can take by reference — and the distributions read out of it as values
 * rather than as a function per name.
 *
 * A shape is any value with an `Answer` type and a `draw(Stream&)`, so
 * `stream.sample(Gaussian{0, 1})` and `stream.sample(myOwnShape)` are the
 * same call. That is the whole of the extension point: a new
 * distribution is a struct with two members, not a new method here.
 *
 * WHAT IS THE SAME NUMBER EVERYWHERE. `bits()` is the word the named
 * mixer produced, so a stream is bit-exact on every platform and every
 * draw built out of `bits()` — `unit`, `below`, `Uniform`, `shuffle`,
 * `Weighted`, `Reservoir`, and every sequence source — is bit-exact too.
 * `Gaussian` and `Exponential` are exact functions of those words through
 * one `std::log` or `std::sqrt` call, and inherit whatever that one call
 * rounds to on the machine running it.
 *
 * ONE SQUEEZE. Every source here is read to a float through the 24
 * mantissa bits a float holds exactly, which is the squeeze `noise::`
 * uses too, so `[0, 1)` is the range drawn and 1 is never answered. A
 * caller replacing a hand-carried `pcgNext` loop keeps the same WORDS
 * through `bits()` and the same unit floats.
 */

#include <sigilcore/compute/Hash.h>
#include <sigilcore/compute/Noise.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::core::chance {

/** WHERE A STREAM'S NEXT WORD COMES FROM.
 *
 *  The first three are the mixers of `Noise.h` walked as counters: their
 *  successive draws are uncorrelated and clumped exactly as independent
 *  draws are. The last four are LOW-DISCREPANCY SEQUENCES — they are not
 *  random at all, they are the numbers that fill an interval most evenly
 *  for the count drawn so far, which is what a scatter that must not
 *  clump wants. Because the source sits under `bits()`, every shape below
 *  draws low-discrepancy for free when the stream is one of these; there
 *  is no second vocabulary for it. */
enum class Source : uint8_t {
  /** The PCG word — `noise::pcgNext`. What new work takes. */
  Pcg,
  /** The 64-bit avalanche walked by its gamma — `noise::Mix64Stream`.
   *  Take it where two integers have to be folded into one seed. */
  Mix64,
  /** The xorshift32 step — `noise::xorshiftNext`. Kept because work
   *  already stored as bytes was seeded through it. */
  Xorshift,
  /** The radical inverse in `parameter`'s base: the Halton sequence.
   *  Base 2 and 3 on two streams are the classic pair of axes. */
  Halton,
  /** The radical inverse in base 2, read as bits reversed: the first
   *  Sobol dimension, and the same numbers as `Halton` base 2. */
  Sobol,
  /** The golden-ratio additive recurrence: a counter stepped by the
   *  64-bit golden word. The most even one-dimensional sequence there
   *  is, and the one a phyllotaxis angle already spells by hand. */
  Golden,
  /** `parameter` strata across [0, 1), one jittered draw in each, in
   *  order, the ladder repeating once it is walked: even coverage with
   *  the clumping of a jitter inside a cell rather than across the whole
   *  interval. */
  Stratified,
};

/** THE SEEDED STREAM A CALLER HOLDS.
 *
 *  A value: copy it and the copy draws the same continuation; assign it
 *  and a run is rewound. `bits()` is the one primitive — everything else
 *  on the class and every shape below is defined in terms of it, so a
 *  source is added in one switch and every distribution follows.
 *
 *  Not a cryptographic generator and not a substitute for one. */
class Stream {
 public:
  /** The PCG stream from seed 0. */
  Stream() = default;

  /** The PCG word, seeded so that `pcg(s).bits()` is the word
   *  `noise::pcgNext` answers for a state of @p seed. A seed wider than
   *  the 32-bit state has its high half folded onto its low one, so
   *  every bit of it moves the run. */
  [[nodiscard]] static Stream pcg(uint64_t seed) {
    return Stream(Source::Pcg, seed, 0);
  }
  /** The 64-bit avalanche walked by its gamma, as `noise::Mix64Stream`
   *  walks it — the same words for the same seed. */
  [[nodiscard]] static Stream mix64(uint64_t seed) {
    return Stream(Source::Mix64, seed, 0);
  }
  /** The xorshift32 step. Zero is the state all three shifts fix, so a
   *  seed of zero starts the stream at one instead. */
  [[nodiscard]] static Stream xorshift(uint64_t seed) {
    return Stream(Source::Xorshift, seed, 0);
  }
  /** The Halton sequence in @p base, @p skip terms in. Bases below two
   *  are not a radical inverse and read as two. */
  [[nodiscard]] static Stream halton(uint32_t base, uint64_t skip = 0) {
    return Stream(Source::Halton, skip, base);
  }
  /** The first Sobol dimension, @p skip terms in. */
  [[nodiscard]] static Stream sobol(uint64_t skip = 0) {
    return Stream(Source::Sobol, skip, 0);
  }
  /** The golden-ratio recurrence, offset by @p seed. */
  [[nodiscard]] static Stream golden(uint64_t seed = 0) {
    return Stream(Source::Golden, seed, 0);
  }
  /** @p strata equal cells across [0, 1), one draw in each in order,
   *  jittered inside its cell by a mixer keyed on @p seed, the ladder
   *  starting over once every cell has been drawn from. A count of zero
   *  or one is one cell, which is an unjittered zero. */
  [[nodiscard]] static Stream stratified(uint32_t strata, uint64_t seed = 0) {
    return Stream(Source::Stratified, seed, strata);
  }

  /** THE ONE FACTORY A CARRIED CHOICE GOES THROUGH: the source, the
   *  seed, and the one word the sequence sources need — a Halton base,
   *  a stratum count, and nothing for the rest. This is what lets a
   *  token carry a source without carrying a constructor. */
  [[nodiscard]] static Stream of(Source source, uint64_t seed,
                                 uint32_t parameter = 0) {
    return Stream(source, seed, parameter);
  }

  [[nodiscard]] Source source() const { return m_source; }
  [[nodiscard]] uint32_t parameter() const { return m_parameter; }
  /** How many words have been drawn. A sequence source's term index; a
   *  mixer's step count. */
  [[nodiscard]] uint64_t drawn() const { return m_drawn; }

  /** THE NEXT WORD. Every other draw is this one squeezed. */
  uint32_t bits() {
    ++m_drawn;
    switch (m_source) {
      case Source::Pcg: {
        auto state = (uint32_t)m_state;
        const uint32_t word = noise::pcgNext(state);
        m_state = state;
        return word;
      }
      case Source::Mix64:
        m_state += noise::kMix64Gamma;
        return (uint32_t)(noise::mix64(m_state) >> 32u);
      case Source::Xorshift: {
        auto state = (uint32_t)m_state;
        const uint32_t word = noise::xorshiftNext(state);
        m_state = state;
        return word;
      }
      case Source::Halton:
        return fixed(
            radicalInverse(m_state++, m_parameter < 2u ? 2u : m_parameter));
      case Source::Sobol:
        return reversed((uint32_t)m_state++);
      case Source::Golden:
        m_state += kGoldenStep;
        return (uint32_t)(m_state >> 32u);
      case Source::Stratified: {
        const uint32_t strata = m_parameter < 1u ? 1u : m_parameter;
        // The ladder is walked by the draw count, not by the state: the
        // seed here names the jitter, not a place in the sequence.
        const uint64_t index = m_drawn - 1;
        const double jitter =
            strata > 1u ? (double)noise::pcgUnit(
                              (uint32_t)(index ^ (m_seed * 2654435761ull)))
                        : 0.0;
        const double cell = (double)(index % strata);
        return fixed((cell + jitter) / (double)strata);
      }
    }
    return 0;
  }

  /** The next value in [0, 1), through the 24 mantissa bits a float
   *  holds exactly — so 1 is never drawn and every draw is a number a
   *  float represents without rounding. */
  float unit() { return (float)(bits() >> 8u) * (1.0f / 16777216.0f); }
  /** The next value in [-1, 1). */
  float signedUnit() { return unit() * 2.0f - 1.0f; }
  /** The next value in [lo, hi). */
  float range(float lo, float hi) { return lo + unit() * (hi - lo); }

  /** An index in [0, @p count), 0 when the count is empty.
   *
   *  The unit float scaled, not a rejection loop: the bias is one part
   *  in 2^24, and rejection would throw terms out of a low-discrepancy
   *  sequence, which is the one thing that source exists to keep. */
  size_t below(size_t count) {
    if (count <= 1) return 0;
    const auto index = (size_t)(unit() * (float)count);
    return index < count ? index : count - 1;
  }

  /** A STANDARD NORMAL DRAW: Marsaglia's polar method, which spends a
   *  pair of unit draws on a pair of normals and holds the second for
   *  the next call. That held value is why this is a member and not a
   *  free function — the spare belongs to the stream that paid for it.
   *
   *  The polar method REJECTS the pairs that land outside the unit
   *  disc, and a source whose pairs do not scatter has nothing else to
   *  offer: a stratified ladder of one cell answers the same number for
   *  ever, and that pair is rejected for ever. So the rejections are
   *  counted, and the pair that exhausts the count is turned by the
   *  sine-and-cosine form instead, which rejects nothing. Every source
   *  named by `Source` therefore terminates, and a scattering one takes
   *  the polar path on all but about one draw in 10^21. */
  float normal() {
    if (m_spareHeld) {
      m_spareHeld = false;
      return m_spare;
    }
    for (unsigned attempt = 0; attempt < kNormalRejections; ++attempt) {
      const float u = signedUnit();
      const float v = signedUnit();
      const float s = u * u + v * v;
      if (s < 1.0f && s > 0.0f) {
        const float f = std::sqrt(-2.0f * std::log(s) / s);
        m_spare = v * f;
        m_spareHeld = true;
        return u * f;
      }
    }
    // The radius from one draw and the angle from the next. The unit
    // draw reaches zero, whose logarithm is not a number, so it is
    // lifted onto the smallest step the 24-bit unit grid has.
    constexpr float kUnitStep = 1.0f / 16777216.0f;
    const float drawn = unit();
    const float radius =
        std::sqrt(-2.0f * std::log(drawn > kUnitStep ? drawn : kUnitStep));
    const float angle = 2.0f * 3.14159265358979323846f * unit();
    m_spare = radius * std::sin(angle);
    m_spareHeld = true;
    return radius * std::cos(angle);
  }

  /** THE DRAW: @p shape's own answer, read out of this stream. Any value
   *  with an `Answer` type and a `draw(Stream&) const` is a shape. */
  template <typename Shape>
  typename Shape::Answer sample(const Shape& shape) {
    return shape.draw(*this);
  }

  /** Back to the beginning of @p seed's run, the held normal dropped. */
  void reseed(uint64_t seed) {
    m_seed = seed;
    m_state = start(m_source, seed);
    m_drawn = 0;
    m_spareHeld = false;
  }
  /** The seed the run started from. */
  [[nodiscard]] uint64_t seed() const { return m_seed; }

 private:
  Stream(Source source, uint64_t seed, uint32_t parameter)
      : m_source(source),
        m_parameter(parameter),
        m_seed(seed),
        m_state(start(source, seed)) {}

  /** The 64-bit golden word: the step whose fractional orbit fills the
   *  unit interval more evenly than any other single step. */
  static constexpr uint64_t kGoldenStep = 0x9e3779b97f4a7c15ull;

  /** How many pairs `normal()` may reject before it turns the next pair
   *  by the form that rejects nothing. About a fifth of the pairs a
   *  scattering source offers fall outside the disc, so this many in a
   *  row is not a run of bad luck; it is a source that cannot offer a
   *  second point. */
  static constexpr unsigned kNormalRejections = 32u;

  static uint64_t start(Source source, uint64_t seed) {
    // The two 32-bit mixers step a 32-bit word, so a wider seed would
    // lose its high half on the way in and two seeds an even 2^32 apart
    // would walk one run. The half is folded down instead, which leaves
    // a seed that fits in 32 bits exactly where it was.
    if (source == Source::Pcg || source == Source::Xorshift) {
      auto word = (uint32_t)(seed ^ (seed >> 32u));
      // The xorshift shifts all fix zero, so a stream that begins there
      // stays there for ever; one is the nearest state that does not.
      if (source == Source::Xorshift && word == 0u) word = 1u;
      return word;
    }
    return seed;
  }

  /** A unit double as the top 32 bits of a fixed-point word, so a
   *  sequence and a mixer answer `bits()` in the same currency. */
  static uint32_t fixed(double unit) {
    const double scaled = unit * 4294967296.0;
    if (!(scaled > 0.0)) return 0u;
    if (scaled >= 4294967295.0) return 0xFFFFFFFFu;
    return (uint32_t)scaled;
  }

  /** The digits of @p index in @p base, mirrored about the point. */
  static double radicalInverse(uint64_t index, uint32_t base) {
    double fraction = 1.0, result = 0.0;
    const auto b = (double)base;
    while (index > 0) {
      fraction /= b;
      result += fraction * (double)(index % base);
      index /= base;
    }
    return result;
  }

  /** @p index's bits mirrored — the radical inverse in base two, read
   *  as a fixed-point word with no arithmetic at all. */
  static uint32_t reversed(uint32_t index) {
    index = (index >> 16u) | (index << 16u);
    index = ((index & 0x00ff00ffu) << 8u) | ((index & 0xff00ff00u) >> 8u);
    index = ((index & 0x0f0f0f0fu) << 4u) | ((index & 0xf0f0f0f0u) >> 4u);
    index = ((index & 0x33333333u) << 2u) | ((index & 0xccccccccu) >> 2u);
    index = ((index & 0x55555555u) << 1u) | ((index & 0xaaaaaaaau) >> 1u);
    return index;
  }

  Source m_source = Source::Pcg;
  uint32_t m_parameter = 0;
  uint64_t m_seed = 0;
  uint64_t m_state = 0;
  uint64_t m_drawn = 0;
  bool m_spareHeld = false;
  float m_spare = 0.0f;
};

// ---- the shapes -----------------------------------------------------------

/** [lo, hi). The default is the unit interval, so `Uniform{}` is
 *  `unit()` under the name the other shapes are spelled by. */
struct Uniform {
  float lo = 0.0f;
  float hi = 1.0f;
  using Answer = float;
  [[nodiscard]] Answer draw(Stream& stream) const {
    return stream.range(lo, hi);
  }
  friend bool operator==(const Uniform&, const Uniform&) = default;
};

/** THE NORMAL DISTRIBUTION: most draws within one @p sd of @p mean, a
 *  twentieth of them beyond two, and no bound at all. What a jitter that
 *  should look natural rather than boxed takes. */
struct Gaussian {
  float mean = 0.0f;
  float sd = 1.0f;
  using Answer = float;
  [[nodiscard]] Answer draw(Stream& stream) const {
    return mean + stream.normal() * sd;
  }
  friend bool operator==(const Gaussian&, const Gaussian&) = default;
};

/** THE WAIT BETWEEN EVENTS THAT ARRIVE AT @p rate PER UNIT: never
 *  negative, mean `1/rate`, and memoryless — the wait already spent says
 *  nothing about the wait remaining. What a stream of arrivals is spaced
 *  by. A rate of zero or less has no distribution and answers 0. */
struct Exponential {
  float rate = 1.0f;
  using Answer = float;
  [[nodiscard]] Answer draw(Stream& stream) const {
    if (!(rate > 0.0f)) return 0.0f;
    // 1 - unit() rather than unit(): the unit draw reaches 0, whose
    // logarithm is not a number, and never reaches 1, whose logarithm is
    // the zero this wants.
    return -std::log(1.0f - stream.unit()) / rate;
  }
  friend bool operator==(const Exponential&, const Exponential&) = default;
};

/** A CHOICE IN PROPORTION TO ITS WEIGHT: the index of one of @p weights,
 *  each drawn as often as its share of the positive total. Weights that
 *  are zero, negative or not a number are never chosen. A distribution
 *  with no positive weight in it has no answer.
 *
 *  It answers an INDEX rather than a value, so one shape serves every
 *  element type and the caller indexes its own container. */
struct Weighted {
  /** BORROWED, not held: the span names weights the caller keeps alive
   *  for as long as it draws from this shape. It is why this is the one
   *  shape here without an `operator==` — two spans over equal numbers
   *  in different memory are not the same distribution to compare, and
   *  comparing the addresses would answer a question nobody asked. */
  std::span<const float> weights;
  using Answer = std::optional<size_t>;
  [[nodiscard]] Answer draw(Stream& stream) const {
    float total = 0.0f;
    for (float weight : weights)
      if (weight > 0.0f) total += weight;
    if (!(total > 0.0f)) return std::nullopt;

    const float choice = stream.unit() * total;
    float cumulative = 0.0f;
    std::optional<size_t> last;
    for (size_t i = 0; i < weights.size(); ++i) {
      if (!(weights[i] > 0.0f)) continue;
      last = i;
      cumulative += weights[i];
      if (choice < cumulative) return i;
    }
    // Rounding in the running sum can leave the last positive weight
    // just short of the draw; it is the one the draw fell in.
    return last;
  }
};

/** @p items in a uniformly random order, in place — Fisher-Yates walked
 *  from the end, so every permutation is equally likely and no element
 *  is examined twice. */
template <typename T>
void shuffle(Stream& stream, std::span<T> items) {
  for (size_t remaining = items.size(); remaining > 1; --remaining) {
    const size_t chosen = stream.below(remaining);
    if (chosen != remaining - 1) std::swap(items[chosen], items[remaining - 1]);
  }
}

/** K OF A RUN WHOSE LENGTH IS NOT KNOWN YET, each item equally likely to
 *  be among them, in one pass and in `keep` words of memory.
 *
 *  Offer every item once, in order; `kept()` is then the indices chosen,
 *  and they are correct at EVERY point of the run, not only at its end —
 *  which is the whole reason to reach for this rather than shuffling a
 *  vector. It holds indices rather than items so one class serves every
 *  element type. */
class Reservoir {
 public:
  explicit Reservoir(size_t keep) : m_keep(keep) { m_kept.reserve(keep); }

  /** Offers the next item. Answers the slot it took, or nothing when the
   *  item was passed over. */
  std::optional<size_t> offer(Stream& stream) {
    const size_t index = m_seen++;
    if (m_kept.size() < m_keep) {
      m_kept.push_back(index);
      return m_kept.size() - 1;
    }
    if (m_keep == 0) return std::nullopt;
    const size_t slot = stream.below(m_seen);
    if (slot >= m_keep) return std::nullopt;
    m_kept[slot] = index;
    return slot;
  }

  /** The indices held, in slot order — not in the order they arrived. */
  [[nodiscard]] std::span<const size_t> kept() const { return m_kept; }
  /** How many items have been offered. */
  [[nodiscard]] size_t seen() const { return m_seen; }
  /** How many the reservoir was asked to hold. */
  [[nodiscard]] size_t keep() const { return m_keep; }

 private:
  size_t m_keep;
  size_t m_seen = 0;
  std::vector<size_t> m_kept;
};

/** THE SEED A WHOLE SHEET RE-ROLLS FROM, as one carried value.
 *
 *  Chance is chosen once for a drawing and read in many places, which is
 *  what makes it a token rather than an argument: every element asks for
 *  its own stream by a NAME, the name is folded into the one seed, and
 *  the streams are independent of each other and stable under any other
 *  element being added or removed. Changing `seed` re-rolls every one of
 *  them together; changing nothing re-draws the same sheet.
 *
 *  `density` and `jitter` ride along because they are the two numbers a
 *  scatter is set by and are chosen for a sheet in the same breath as
 *  the seed. Every member is a plain number, so the value compares
 *  exactly and a memo below one of these can be skipped. */
struct Chance {
  uint64_t seed = 0;
  Source source = Source::Pcg;
  /** The one word a sequence source needs: a Halton base, a stratum
   *  count, nothing for the mixers. */
  uint32_t parameter = 0;
  /** How much of what could be drawn is drawn, 0 to 1. */
  float density = 1.0f;
  /** How far a placed thing may wander off its place, in the caller's
   *  own units. */
  float jitter = 0.0f;

  /** The stream for one named use — `"stars"`, `"grain"`, `"row 3"`.
   *  The name is folded into the seed, so two uses never walk each
   *  other's draws and a use added later leaves the others alone. */
  [[nodiscard]] Stream stream(std::string_view use = {}) const {
    return Stream::of(source, hash::fnv1a(seed, use), parameter);
  }

  friend bool operator==(const Chance&, const Chance&) = default;
};

}  // namespace sigil::core::chance
