/** @file
 * Region — the masking family's comparable region value: its four forms,
 * and the equality that lets a gated node prune. Its resolution against
 * a node's outline is the brush tier's.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

using detail::Kind;

Region Region::own() { return Region{}; }

Region Region::rect(const SkRect& r) {
  Region out;
  out.m_kind = Kind::Rect;
  out.m_rect = r;
  return out;
}

Region Region::oval(const SkRect& bounds) {
  Region out;
  out.m_kind = Kind::Oval;
  out.m_rect = bounds;
  return out;
}

Region Region::path(SkPath p) {
  Region out;
  out.m_kind = Kind::Path;
  out.m_path = std::move(p);
  return out;
}

bool Region::operator==(const Region& other) const {
  if (m_kind != other.m_kind) return false;
  switch (m_kind) {
    case Kind::Own:
      return true;
    case Kind::Rect:
    case Kind::Oval:
      return m_rect == other.m_rect;
    case Kind::Path:
      return m_path == other.m_path;
  }
  return false;
}

}  // namespace sigil::compose
