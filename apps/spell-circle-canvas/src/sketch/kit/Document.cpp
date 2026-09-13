#include <sigilcompose/core/Factories.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>

#include <algorithm>
#include <string>
#include <utility>

namespace sigil::sketch::kit {

namespace {

/** The null value every reader falls back to, so a lookup through a
 *  document that did not arrive answers the same thing a lookup for a key
 *  it does not carry answers. */
const data::Json& nothing() {
  static const data::Json none;
  return none;
}

/** @p words with every `{name}` replaced by the figure of that name, left
 *  exactly as written where no figure carries the name. */
std::string written(
    std::string_view words,
    const std::vector<std::pair<std::string, std::string>>& figures) {
  std::string out;
  out.reserve(words.size());
  for (std::size_t i = 0; i < words.size();) {
    const std::size_t open = words.find('{', i);
    const std::size_t close =
        open == std::string_view::npos ? open : words.find('}', open);
    if (close == std::string_view::npos) {
      out += words.substr(i);
      break;
    }
    out += words.substr(i, open - i);
    const std::string_view name = words.substr(open + 1, close - open - 1);
    const auto found = std::ranges::find_if(
        figures, [name](const std::pair<std::string, std::string>& one) {
          return one.first == name;
        });
    out += found != figures.end() ? std::string_view(found->second)
                                  : words.substr(open, close - open + 1);
    i = close + 1;
  }
  return out;
}

/** One member of a run: a bare string is the sentence in no class of its
 *  own, a record names its own under `words` and `class`. */
std::string_view sentence(const data::Json& item) {
  return item.kind() == data::Json::Kind::Text ? item.text()
                                               : item["words"].text();
}

}  // namespace

Document::Document(SketchContext& ctx, std::string_view name)
    : m_held(ctx.assets.json(ctx.local(name))) {}

Document& Document::figures(Figures named) {
  for (std::pair<std::string, compose::Utf8>& one : named) {
    const std::string reading(
        reinterpret_cast<const char*>(one.second.bytes().data()),
        one.second.bytes().size());
    const auto found = std::ranges::find_if(
        m_figures, [&one](const std::pair<std::string, std::string>& held) {
          return held.first == one.first;
        });
    if (found != m_figures.end())
      found->second = reading;
    else
      m_figures.emplace_back(std::move(one.first), reading);
  }
  return *this;
}

const data::Json& Document::operator[](std::string_view key) const {
  return m_held ? (*m_held)[key] : nothing();
}

compose::Utf8 Document::phrase(const data::Json& node) const {
  return compose::Utf8(written(node.text(), m_figures));
}

std::vector<Document::Line> Document::run(const data::Json& node) const {
  std::vector<Line> out;
  out.reserve(node.size());
  for (const data::Json& item : node.items())
    out.push_back({compose::Utf8(written(sentence(item), m_figures)),
                   std::string(item["class"].text())});
  return out;
}

compose::Element lineOf(const Document::Line& one) {
  return compose::text(one.words).styleClass(one.styleClass);
}

weave::RichText Document::passage(const data::Json& node) const {
  weave::RichText woven = weave::rich();
  for (const Line& one : run(node)) {
    if (one.styleClass.empty())
      woven.add(one.words.bytes());
    else
      woven.add(one.words.bytes(), one.styleClass);
  }
  return woven;
}

}  // namespace sigil::sketch::kit
