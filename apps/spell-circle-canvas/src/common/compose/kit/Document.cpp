#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>

namespace sigil::compose::document {
namespace {

const VarTable& layoutDefaults() {
  static const VarTable defaults = [] {
    VarTable values;
    values.set(var(measure), Dimension(weave::em(38)));
    values.set(var(gap), Dimension(weave::em(1)));
    values.set(var(listGap), Dimension(weave::em(0.4f)));
    values.set(var(quoteInset), Dimension(weave::em(1)));
    return values;
  }();
  return defaults;
}

Element flow(std::string role, std::initializer_list<Children> children,
             std::string_view spacing = gap) {
  return box()
      .column()
      .role(std::move(role))
      .varDefaults(layoutDefaults())
      .gap(var(spacing))
      .children(children);
}

Text line(Utf8 words, std::string role, float scale) {
  return text(std::move(words))
      .role(std::move(role), {.size = weave::em(scale)});
}

}  // namespace

Element article(std::initializer_list<Children> children) {
  return flow("article", children)
      .role("article", {.leading = weave::Leading::multiple(1.45f)})
      .width(pct(100))
      .maxWidth(var(measure));
}

Element section(std::initializer_list<Children> children) {
  return flow("section", children);
}

Text heading(int level, Utf8 words) {
  static constexpr std::array sizes{2.0f, 1.5f, 1.25f, 1.1f, 1.0f, 0.875f};
  if (level < 1 || level > static_cast<int>(sizes.size()))
    throw std::out_of_range("Document heading level must be between 1 and 6");
  return line(std::move(words), "h" + std::to_string(level), sizes[level - 1]);
}

Text h1(Utf8 words) { return heading(1, std::move(words)); }
Text h2(Utf8 words) { return heading(2, std::move(words)); }
Text h3(Utf8 words) { return heading(3, std::move(words)); }
Text h4(Utf8 words) { return heading(4, std::move(words)); }
Text h5(Utf8 words) { return heading(5, std::move(words)); }
Text h6(Utf8 words) { return heading(6, std::move(words)); }

Text paragraph(Utf8 words) { return text(std::move(words)).role("paragraph"); }
Text paragraph(const weave::RichText& words) {
  return text(words).role("paragraph");
}
Text lead(Utf8 words) { return line(std::move(words), "lead", 1.125f); }
Text caption(Utf8 words) { return line(std::move(words), "caption", 0.875f); }
Text label(Utf8 words) { return line(std::move(words), "label", 0.875f); }
Text eyebrow(Utf8 words) {
  return text(std::move(words))
      .role("eyebrow", {.size = weave::em(0.75f), .track = weave::em(0.08f)});
}
Text footer(Utf8 words) { return line(std::move(words), "footer", 0.875f); }
Text code(Utf8 words) {
  static const auto mono =
      weave::ports::face({"Menlo", "Consolas", "monospace"});
  return text(std::move(words)).role("code", {.face = mono});
}

Element quote(std::initializer_list<Children> children) {
  return flow("quote", children).padding(0, var(quoteInset));
}
Element quote(Utf8 words) { return quote({paragraph(std::move(words))}); }
Element list(std::initializer_list<Children> children) {
  return flow("list", children, listGap);
}
Element item(Utf8 words, Utf8 marker) {
  return item(paragraph(std::move(words)), std::move(marker));
}
Element item(Element body, Utf8 marker) {
  return box()
      .row()
      .role("item")
      .alignItems(Align::Baseline)
      .gap(weave::em(0.25f))
      .children({text(std::move(marker))
                     .role("marker")
                     .minWidth(weave::em(1.25f))
                     .flexShrink(0),
                 std::move(body.minWidth(0).flexGrow(1))});
}
Element figure(Element body, Utf8 note) {
  Element result = flow("figure", {std::move(body)}, listGap);
  if (!note.empty()) result.children({caption(std::move(note))});
  return result;
}
Element rule() {
  return box().role("rule").height(1).width(pct(100)).fill(Fill::currentInk());
}

}  // namespace sigil::compose::document
