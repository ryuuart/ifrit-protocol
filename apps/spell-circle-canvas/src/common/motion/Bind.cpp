#include "sigilmotion/Bind.h"

#include <cmath>

namespace sigil::motion {

namespace detail {

inline uint32_t wiggleHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

inline float wiggleLattice(int32_t cell, uint32_t seed) {
  const uint32_t h =
      wiggleHash((uint32_t)cell * 0x9e3779b9u ^ wiggleHash(seed + 0x85ebca6bu));
  return (float)(h >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

inline float wiggleOctave(float x, uint32_t seed) {
  const float base = std::floor(x);
  if (!(base > -2.0e9f && base < 2.0e9f)) return 0.0f;
  const float t = x - base;
  const int32_t cell = (int32_t)base;
  const float u = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
  const float a = wiggleLattice(cell, seed);
  const float b = wiggleLattice(cell + 1, seed);
  return a + (b - a) * u;
}

inline float wiggleNoise(float x, uint32_t seed, int octaves, float falloff) {
  const int n = octaves < 1 ? 1 : (octaves > 8 ? 8 : octaves);
  float sum = 0.0f, weight = 0.0f, amp = 1.0f, freq = 1.0f;
  for (int i = 0; i < n; ++i) {
    sum += amp * wiggleOctave(x * freq, seed + (uint32_t)i * 0x9e3779b9u);
    weight += amp;
    amp *= falloff;
    freq *= 2.0f;
  }
  return weight > 0.0f ? sum / weight : 0.0f;
}

}  // namespace detail

float BoundFloat::apply(float v) const {
  v = v * inScale + inOffset;
  if (clampInput) v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
  // The noise PHASE is read here — off the normalised input, before the
  // curve shapes it and before the affine chain puts it in output
  // units. Phase from the schedule, amplitude in output units; see
  // Bound::wiggle for why that pairing is the useful one.
  const float phase = v;
  // THE ENVELOPE, on the phase and before the curve. It sits after the
  // noise phase is read for the same reason `wrap` does: the shake reads
  // the SCHEDULE, so a ping-ponged phase does not retrace the identical
  // shake on the way back and a trapezoid's dark stretch does not freeze
  // it. It sits before `map` so the curve shapes what the envelope
  // PRODUCED — any curve through (0,0) and (1,1) rounds a trapezoid's
  // shoulders while leaving its hold at exactly 1 and its dark at exactly
  // 0 — and before the affine chain, which is what puts the shape into
  // the property's own units.
  switch (envelope) {
    case Envelope::kNone:
      break;
    case Envelope::kPingPong: {
      // A triangle of period 1: there and back across the span, and then
      // again, so a phase that keeps climbing keeps bouncing.
      const float u = v - std::floor(v);
      v = u < 0.5f ? u + u : 2.0f - (u + u);
      break;
    }
    case Envelope::kCosine:
      // Periodic by construction — the same repeat, from the cosine
      // itself rather than from a fold.
      v = 0.5f - 0.5f * std::cos(6.28318530717958648f * v);
      break;
    case Envelope::kTrapezoid:
      // Dark outside [riseStart, fallEnd] — NOT folded into one span,
      // because `window()` clamps its input to exactly 1 and a fold would
      // read that as the START of the next pass, turning a settled beat
      // dark. A repeating trapezoid rides a phase that already wraps.
      if (v <= riseStart || v >= fallEnd)
        v = 0.0f;
      else if (v < holdStart)
        // Reachable only when the corners differ, because the test above
        // already answered every v at or below riseStart — which is what
        // makes a zero-length shoulder an instant cut rather than a
        // division by zero.
        v = (v - riseStart) / (holdStart - riseStart);
      else if (v <= holdEnd)
        v = 1.0f;
      else
        v = (fallEnd - v) / (fallEnd - holdEnd);
      break;
    case Envelope::kSquare: {
      // The pulse, folded on the same period pingPong folds on: 1 across
      // the first `duty` of each period, 0 across the rest. PHASE 0 IS
      // ON — `u < duty` answers 1 at exactly 0 — because a pulse's owner
      // is born at the start of its cycle and must be born visible; a
      // shape answering 0 at exactly 0 would blank that one instant at
      // every seam of a wrapping phase.
      const float u = v - std::floor(v);
      v = u < duty ? 1.0f : 0.0f;
      break;
    }
    case Envelope::kWave: {
      // The caller's own shape, read on the folded phase u ∈ [0,1) — so
      // whatever the function draws across one period, the signal
      // repeats it. An empty function passes the folded phase through.
      const float u = v - std::floor(v);
      v = waveFn ? waveFn(u) : u;
      break;
    }
  }
  if (curve) v = curve(v);
  if (steps > 1) v = std::round(v * (float)(steps - 1)) / (float)(steps - 1);
  v = v * scale + offset;
  // AFTER the affine chain, BEFORE wiggle: the noise phase above reads
  // the unwrapped schedule, so a wrapped phase wiggles continuously
  // across the seam instead of repeating its shake every lap.
  if (wrapPeriod > 0.0f) {
    v = std::fmod(v, wrapPeriod);
    if (v < 0.0f) v += wrapPeriod;
  }
  if (wiggleAmount != 0.0f)
    v += wiggleAmount * detail::wiggleNoise(phase * wiggleFrequency, wiggleSeed,
                                            wiggleOctaves, wiggleFalloff);
  if (clamped) v = v < lo ? lo : (v > hi ? hi : v);
  return v;
}

Bound& Bound::source(float lo, float hi) {
  const float span = hi - lo;
  m_b.inScale = span != 0.0f ? 1.0f / span : 0.0f;
  m_b.inOffset = span != 0.0f ? -lo / span : 0.0f;
  return *this;
}

Bound& Bound::window(float lo, float hi) {
  source(lo, hi);
  m_b.clampInput = true;
  return *this;
}

Bound& Bound::pingPong() {
  m_b.envelope = Envelope::kPingPong;
  return *this;
}

Bound& Bound::cosine() {
  m_b.envelope = Envelope::kCosine;
  return *this;
}

Bound& Bound::trapezoid(float riseStart, float holdStart, float holdEnd,
                        float fallEnd) {
  m_b.envelope = Envelope::kTrapezoid;
  m_b.riseStart = riseStart;
  m_b.holdStart = holdStart < riseStart ? riseStart : holdStart;
  m_b.holdEnd = holdEnd < m_b.holdStart ? m_b.holdStart : holdEnd;
  m_b.fallEnd = fallEnd < m_b.holdEnd ? m_b.holdEnd : fallEnd;
  return *this;
}

Bound& Bound::square(float duty) {
  m_b.envelope = Envelope::kSquare;
  m_b.duty = duty < 0.0f ? 0.0f : (duty > 1.0f ? 1.0f : duty);
  return *this;
}

Bound& Bound::wave(choreograph::EaseFn shape) {
  m_b.envelope = Envelope::kWave;
  m_b.waveFn = std::move(shape);
  return *this;
}

Bound& Bound::map(choreograph::EaseFn curve) {
  m_b.curve = std::move(curve);
  return *this;
}

Bound& Bound::scale(float s) {
  m_b.scale *= s;
  m_b.offset *= s;
  return *this;
}

Bound& Bound::offset(float o) {
  m_b.offset += o;
  return *this;
}

Bound& Bound::target(float lo, float hi) { return scale(hi - lo).offset(lo); }

Bound& Bound::invert() {
  m_b.scale = -m_b.scale;
  m_b.offset = 1.0f - m_b.offset;
  return *this;
}

Bound& Bound::quantize(int steps) {
  m_b.steps = steps > 1 ? steps : 0;
  return *this;
}

Bound& Bound::wrap(float period) {
  m_b.wrapPeriod = period > 0.0f ? period : 0.0f;
  return *this;
}

Bound& Bound::wiggle(float amount, float frequency, uint32_t seed, int octaves,
                     float falloff) {
  m_b.wiggleAmount = amount;
  m_b.wiggleFrequency = frequency;
  m_b.wiggleSeed = seed;
  m_b.wiggleOctaves = octaves < 1 ? 1 : (octaves > 8 ? 8 : octaves);
  m_b.wiggleFalloff = falloff < 0.0f ? 0.0f : (falloff > 1.0f ? 1.0f : falloff);
  return *this;
}

Bound& Bound::clamp(float lo, float hi) {
  m_b.clamped = true;
  m_b.lo = lo;
  m_b.hi = hi;
  return *this;
}

Bound bind(const choreograph::Output<float>* source) { return Bound{source}; }

Bound wiggle(const choreograph::Output<float>* source, float amount,
             float frequency, uint32_t seed, int octaves, float falloff) {
  Bound b{source};
  b.scale(0.0f).wiggle(amount, frequency, seed, octaves, falloff);
  return b;
}

}  // namespace sigil::motion
