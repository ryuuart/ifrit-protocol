#pragma once

/** @file
 * UniformBlock — the caller-owned, revisioned float buffer behind a live
 * array uniform.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sigil::material {

/** A CALLER-OWNED UNIFORM BUFFER WITH A REVISION — the live form of an
 *  array uniform, for per-frame data no scalar `Output` can carry: a
 *  particle table, a per-bar spectrum, a set of rects a simulation moves.
 *
 *  Own it where you own your model, write `values()`, then `commit()` to
 *  publish. The binding (`Material::uniform` / `Effect::uniform` with a
 *  block) reads the CURRENT values at every paint and declares volatility
 *  the way a bound scalar `Output*` does, so the node paints live and no
 *  cache can freeze the table; the revision is what lets the resolve memo
 *  see that an uncommitted frame changed nothing and keep the built shader.
 *
 *  LIFETIME AND EQUALITY follow the bound-scalar rules exactly. The
 *  binding is a shared_ptr, so the buffer cannot dangle, but it compares
 *  by IDENTITY: a block recreated every describe reads as a new binding
 *  each time and re-patches its node — hold the block beside your model,
 *  not in the describe. The values belong to the system and never enter
 *  the prune comparison. Not thread-safe, deliberately: one owner, one
 *  writer, matching PixelBuffer. */
class UniformBlock {
 public:
  /** `floatCount` is the buffer's length in FLOATS, and it must equal the
   *  declared uniform's total float count — 3 float4s is 12. The size is
   *  fixed for the block's life, because the declared array's is. */
  explicit UniformBlock(size_t floatCount) : m_values(floatCount, 0.0f) {}
  /** The floats, yours to write. Publish with commit(). */
  std::span<float> values() { return m_values; }
  std::span<const float> values() const { return m_values; }
  size_t size() const { return m_values.size(); }
  /** PUBLISH the edit: the next paint resolves a fresh shader from the new
   *  values (an uncommitted frame reuses the previous one). */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }

 private:
  std::vector<float> m_values;
  uint64_t m_revision = 0;
};

}  // namespace sigil::material
