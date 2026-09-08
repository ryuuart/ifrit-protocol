#include "PaintProfile.h"

#include <cstdio>  // std::snprintf, on a keyless node's label

namespace sigil::compose {

std::string profileLabel(const detail::Instance& inst, const SkRect& rect) {
  const detail::ElementNode& node = *inst.description;
  const char* kind = "box";
  switch (node.kind) {
    case detail::Kind::Box:
      kind = "box";
      break;
    case detail::Kind::Text:
      kind = "text";
      break;
    case detail::Kind::Image:
      kind = "image";
      break;
    case detail::Kind::Custom:
      kind = "custom";
      break;
    default:
      break;
  }
  char buf[96];
  std::snprintf(buf, sizeof buf, "%s %.0fx%.0f", kind, rect.width(),
                rect.height());
  if (!node.key.empty()) return node.key + " (" + buf + ")";
  return buf;
}

}  // namespace sigil::compose
