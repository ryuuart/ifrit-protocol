#pragma once
/** @file
 * What a mark keeps between paints, on the instance that paints it, by
 * the mark's place among the node's marks.
 */

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace sigil::compose::detail {

struct MaterialStrokeCache;
struct BevelFaceCache;
struct CoveragePart;
class HeldCoverageMasks;

/** WHAT ONE MARK KEEPS BETWEEN PAINTS: a lit keyline's prepared surface
 *  and the maps of the band it covers, a bevel's shaded shoulder, the
 *  masks a shadow casts from a silhouette's held parts. A
 *  description is immutable once described and may be painted by several
 *  composers at once, so what a paint prepares stands here, on the
 *  instance that paints it, and never in the mark's value. Each entry is
 *  checked against what it was made from before it is read, so a mark
 *  that moves to another place finds a state it rebuilds rather than a
 *  wrong one. */
struct MarkState {
  std::shared_ptr<MaterialStrokeCache> stroke;
  std::shared_ptr<BevelFaceCache> face;
  std::shared_ptr<HeldCoverageMasks> coverage;
  /** The held parts the outline handed to the mark is composed from, for
   *  this paint: null wherever that outline is not a silhouette composed
   *  from held parts. Owned by the instance, valid for the paint call. */
  const std::vector<CoveragePart>* coverageParts = nullptr;
};

/** One instance's mark states, by the mark's slot (beneath, over or in
 *  front of the fill) and its index within that slot. */
class MarkStates {
 public:
  MarkState& at(uint8_t slot, size_t index) { return m_states[{slot, index}]; }

 private:
  std::map<std::pair<uint8_t, size_t>, MarkState> m_states;
};

}  // namespace sigil::compose::detail
