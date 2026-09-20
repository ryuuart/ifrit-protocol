#pragma once

/** @file
 * @ingroup material-core
 *
 * UniformBlock — the caller-owned, revisioned float buffer behind a live
 * array uniform.
 */

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sigil::material {

/** A CALLER-OWNED UNIFORM BUFFER WITH A REVISION — the live form of an
 *  array uniform, for per-frame data no scalar output can carry. Own it
 *  where you own your model, write `values()`, then `commit()` to
 *  publish; the binding reads the CURRENT values at every paint, and the
 *  revision is what lets a resolve memo keep its shader across an
 *  uncommitted frame. Not thread-safe: one owner, one writer.
 *  @trap It compares by IDENTITY, so a block recreated every describe
 *  re-patches its node; hold it beside your model. */
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
