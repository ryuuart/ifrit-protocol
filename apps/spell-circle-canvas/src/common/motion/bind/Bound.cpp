/** @file
 * The chain builder's stage verbs, each writing its field of the
 * BoundFloat it carries, and the two free spellings `bind()` and
 * `wiggle()`.
 */

#include "sigilmotion/bind/Bound.h"

namespace sigil::motion {

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
