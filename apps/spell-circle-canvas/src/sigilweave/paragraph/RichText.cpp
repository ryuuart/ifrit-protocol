/** @file
 * The mixed-text value's builders: `rich()`, the four `add` forms, the
 * inline slot and the style sheet names resolve through.
 */

#include "sigilweave/paragraph/RichText.h"

#include <utility>

namespace sigil::weave {

RichText rich() { return RichText(); }

RichText rich(TextStyle base) { return RichText(std::move(base)); }

RichText& RichText::add(std::u8string_view utf8) {
  Run run;
  run.utf8 = std::u8string(utf8);
  run.style = m_base;
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, TextStyle style) {
  Run run;
  run.utf8 = std::u8string(utf8);
  run.style = std::move(style);
  // The one form that states a WHOLE style: nothing about this run is the
  // base's, so nothing about it changes when the base does.
  run.total = true;
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, Type partial) {
  Run run;
  run.utf8 = std::u8string(utf8);
  run.style = overlay(m_base, partial);
  run.over = std::move(partial);
  m_runs.push_back(std::move(run));
  return *this;
}

RichText& RichText::add(std::u8string_view utf8, std::string_view styleName) {
  Run run;
  run.utf8 = std::u8string(utf8);
  run.styleName = std::string(styleName);
  run.style = m_base;
  if (m_hasStyles) {
    // find(), not the always-answering lookup: an unregistered name resolves
    // to the base handed to rich(), which is this text's one default, and a
    // run that resolved to nothing must carry no partial either.
    if (const Type* named = m_styles.find(run.styleName)) {
      run.over = *named;
      run.style = overlay(m_base, *named);
    }
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

RichText& RichText::styles(StyleSheet sheet) {
  m_styles = std::move(sheet);
  m_hasStyles = true;
  for (Run& run : m_runs) {
    // Only a run written with a NAME is re-resolved: one written with its
    // own partial or its own whole style named no class to look up.
    if (run.styleName.empty()) continue;
    if (const Type* named = m_styles.find(run.styleName)) {
      run.over = *named;
      run.style = overlay(m_base, *named);
    } else {
      run.over.reset();
      run.style = m_base;
    }
  }
  return *this;
}

}  // namespace sigil::weave
