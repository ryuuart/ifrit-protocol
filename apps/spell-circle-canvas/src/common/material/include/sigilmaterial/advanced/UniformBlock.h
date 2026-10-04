#pragma once

/** @file
 * @ingroup material-advanced
 *
 * UniformBlock — the caller-owned, revisioned float buffer behind a live
 * array uniform.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sigil::material {

/** A CALLER-OWNED UNIFORM BUFFER WITH A REVISION — the live form of an
 *  array uniform, for per-frame data no scalar output can carry. Own it
 *  where you own your model, write `values()`, then `commit()` to
 *  publish. Bindings read only the last committed values, even when other
 *  inputs change. The revision lets a resolve memo reuse the shader while
 *  the published array is unchanged. Not thread-safe: edits, commits and
 *  reads must not overlap.
 *  @trap It compares by IDENTITY, so a block recreated every describe
 *  re-patches its node; hold it beside your model. */
class UniformBlock {
 public:
  /** `floatCount` is the buffer's length in FLOATS, and it must equal the
   *  declared uniform's total float count — 3 float4s is 12. The size is
   *  fixed for the block's life, because the declared array's is. */
  explicit UniformBlock(size_t floatCount)
      : m_values(floatCount, 0.0f), m_committedValues(floatCount, 0.0f) {}
  /** The draft floats, yours to write. Their storage stays fixed across
   *  commits, so a held span remains writable. Publish with commit(). */
  std::span<float> values() { return m_values; }
  std::span<const float> values() const { return m_values; }
  /** The published floats, initially zero. Contents change only at commit;
   *  the returned span borrows storage from this block. */
  std::span<const float> committedValues() const { return m_committedValues; }
  size_t size() const { return m_values.size(); }
  /** Publish the whole draft array, then advance the revision. Later draft
   *  writes cannot change what a binding reads until another commit. */
  void commit() {
    std::copy(m_values.begin(), m_values.end(), m_committedValues.begin());
    ++m_revision;
  }
  uint64_t revision() const { return m_revision; }

 private:
  std::vector<float> m_values;
  std::vector<float> m_committedValues;
  uint64_t m_revision = 0;
};

}  // namespace sigil::material
