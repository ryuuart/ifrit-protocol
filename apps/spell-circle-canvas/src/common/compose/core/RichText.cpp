/** @file
 * Mixed text as a comparable value: the runs a RichText holds, the
 * placeholder slots it reserves, and the style set its names resolve in.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

// ---------------------------------------------------------------------------
// rich() — mixed text as a value

RichText rich(sigil::weave::TextStyle base) {
  return RichText(std::move(base));
}

RichText& RichText::add(std::u8string_view utf8) {
  m_runs.push_back(Run{std::u8string(utf8), m_base, {}});
  return *this;
}

RichText& RichText::add(std::u8string_view utf8,
                        sigil::weave::TextStyle style) {
  m_runs.push_back(Run{std::u8string(utf8), std::move(style), {}});
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, std::string_view styleName) {
  // The inherited set is captured on the FIRST named run rather than at
  // rich(), so an unnamed value costs nothing and the capture still happens
  // inside the author's describe scope. styles() overrides it whichever way
  // round the two are written.
  if (!m_hasStyles && !m_stylesExplicit) {
    if (const sigil::weave::StyleSet* ambient =
            env::inherited<sigil::weave::StyleSet>()) {
      m_styles = *ambient;
      m_hasStyles = true;
    }
  }
  Run run{std::u8string(utf8), m_base, std::string(styleName)};
  if (m_hasStyles) {
    // find(), not operator[]: an unregistered name resolves to the base
    // handed to rich(), which is this text's one default.
    if (const sigil::weave::TextStyle* named = m_styles.find(run.styleName))
      run.style = *named;
  }
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::slot(std::string key, SkSize size, float baselineDrop) {
  Run run;
  // U+FFFC OBJECT REPLACEMENT CHARACTER. The slot is CONTENT: it occupies
  // one code point, so it counts as a cluster, falls inside the ranges a
  // selector names, and takes its beat in a cascade exactly as a letter
  // does. The engine matches its reserved box to this occurrence by order.
  run.utf8 = u8"￼";
  run.style = m_base;
  run.slotKey = std::move(key);
  run.slotSize = size;
  run.slotBaselineDrop = baselineDrop;
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::styles(sigil::weave::StyleSet set) {
  m_styles = std::move(set);
  m_hasStyles = true;
  m_stylesExplicit = true;
  for (Run& run : m_runs) {
    if (run.styleName.empty()) continue;
    const sigil::weave::TextStyle* named = m_styles.find(run.styleName);
    run.style = named ? *named : m_base;
  }
  return *this;
}

}  // namespace sigil::compose
