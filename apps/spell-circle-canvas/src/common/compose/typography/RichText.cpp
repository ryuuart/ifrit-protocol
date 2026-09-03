/** @file
 * The mixed-text value's builders: `rich()`, the three `add` forms, the
 * inline slot and the style set names resolve through. Resolution happens
 * as a run is added, inside the author's describe scope, so the finished
 * value holds real styles and depends on no scope that has since ended.
 */

#include <sigilcompose/typography/RichText.h>
#include <sigilcore/reconcile/Env.h>

#include <utility>

namespace sigil::compose {

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
            core::env::inherited<sigil::weave::StyleSet>()) {
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
