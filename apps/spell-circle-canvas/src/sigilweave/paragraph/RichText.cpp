/** @file
 * The mixed-text value's builders: `rich()`, the three `add` forms, the
 * inline slot and the style set names resolve through.
 */

#include "sigilweave/paragraph/RichText.h"

#include <utility>

namespace sigil::weave {

RichText rich(TextStyle base) {
  return RichText(std::move(base));
}

RichText& RichText::add(std::u8string_view utf8) {
  m_runs.push_back(Run{std::u8string(utf8), m_base, {}});
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, TextStyle style) {
  m_runs.push_back(Run{std::u8string(utf8), std::move(style), {}});
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, std::string_view styleName) {
  Run run{std::u8string(utf8), m_base, std::string(styleName)};
  if (m_hasStyles) {
    // find(), not the always-answering lookup: an unregistered name resolves
    // to the base handed to rich(), which is this text's one default.
    if (const TextStyle* named = m_styles.find(run.styleName))
      run.style = *named;
  }
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::slot(std::string name, SkSize size, float baselineDrop) {
  Run run;
  // U+FFFC OBJECT REPLACEMENT CHARACTER. The slot is CONTENT: it occupies
  // one code point, so it counts as a cluster, falls inside the ranges a
  // selection names, and takes its turn in anything that steps over units
  // exactly as a letter does. A layout matches its reserved box to this
  // occurrence by order.
  run.utf8 = u8"￼";
  run.style = m_base;
  run.slotName = std::move(name);
  run.slotSize = size;
  run.slotBaselineDrop = baselineDrop;
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::styles(StyleSet set) {
  m_styles = std::move(set);
  m_hasStyles = true;
  for (Run& run : m_runs) {
    if (run.styleName.empty()) continue;
    const TextStyle* named = m_styles.find(run.styleName);
    run.style = named ? *named : m_base;
  }
  return *this;
}

}  // namespace sigil::weave
