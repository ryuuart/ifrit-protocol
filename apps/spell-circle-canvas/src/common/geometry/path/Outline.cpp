/** @file
 * The outline value over the Skia path that executes it.
 */

#include "sigilgeometry/path/Outline.h"

#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/utils/SkParsePath.h>

#include <string>

#include "OutlineInternal.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::geometry::path {

namespace {
const std::shared_ptr<const detail::OutlineBody>& emptyBody() {
  static const auto body = std::make_shared<const detail::OutlineBody>();
  return body;
}
}  // namespace

Outline::Outline() : m_body(emptyBody()) {}

Outline::Outline(std::shared_ptr<const detail::OutlineBody> body)
    : m_body(body ? std::move(body) : emptyBody()) {}

Outline Outline::svg(std::string_view data) {
  const std::string text(data);
  if (auto parsed = SkParsePath::FromSVGString(text.c_str()))
    return fromSk(std::move(*parsed));
  return {};
}

Outline Outline::rectangle(const Rect& rect) {
  return fromSk(SkPath::Rect(toSk(rect)));
}

bool Outline::empty() const { return m_body->path.isEmpty(); }

FillRule Outline::fillRule() const {
  return OutlineAccess::ruleOf(m_body->path.getFillType());
}

Outline Outline::withFillRule(FillRule rule) const {
  SkPath path = m_body->path;
  path.setFillType(rule == FillRule::EvenOdd ? SkPathFillType::kEvenOdd
                                             : SkPathFillType::kWinding);
  return fromSk(std::move(path));
}

Rect Outline::bounds() const {
  if (m_body->path.isEmpty()) return {};
  return fromSk(m_body->path.computeTightBounds());
}

bool Outline::operator==(const Outline& other) const {
  return m_body == other.m_body || m_body->path == other.m_body->path;
}

SkPath toSk(const Outline& outline) { return OutlineAccess::path(outline); }

Outline fromSk(SkPath path) { return OutlineAccess::make(std::move(path)); }

}  // namespace sigil::geometry::path
