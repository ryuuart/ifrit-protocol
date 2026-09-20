#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/unicode/Unicode.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace unicode = weave::unicode;

namespace {

/** The UTF-8 bytes of @p text as the needle slot holds them. A Python
 *  string arrives already encoded as UTF-8, so the two spellings are the
 *  same bytes and nothing is transcoded. */
std::u8string utf8Bytes(std::string_view text) {
  return std::u8string(text.begin(), text.end());
}

/** UTF-8 bytes as the string Python reads them back through. */
std::string readUtf8(std::u8string_view bytes) {
  return std::string(bytes.begin(), bytes.end());
}

/** Mixes @p value into @p seed so a whole record hashes as one number.
 *  Every field a comparison reads is mixed in, which is what keeps equal
 *  values hashing alike. */
template <class T>
void mixHash(size_t& seed, const T& value) {
  seed ^= std::hash<T>{}(value) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
}

size_t hashSelector(const weave::Selector& selector);

size_t hashState(const weave::Selector::State& state) {
  size_t seed = 0;
  mixHash(seed, static_cast<int>(state.kind));
  mixHash(seed, state.lo);
  mixHash(seed, state.hi);
  mixHash(seed, state.pattern);
  mixHash(seed, static_cast<int>(state.each));
  mixHash(seed, state.take);
  mixHash(seed, state.drop);
  for (const auto& operand : state.operands)
    mixHash(seed, hashSelector(operand));
  return seed;
}

/** A selector's hash, over the same state its comparison reads. A
 *  selector that addresses everything carries no state and hashes as
 *  zero, which is the one value no state of its own can produce. */
size_t hashSelector(const weave::Selector& selector) {
  const auto* state = selector.state();
  return state ? hashState(*state) : 0;
}

/** The code points of @p values as the numbers Python counts them in.
 *  A code point crossing the boundary as a character would arrive as a
 *  one-character string instead of the number `ord` answers. */
std::vector<uint32_t> codePointNumbers(const std::vector<char32_t>& values) {
  return std::vector<uint32_t>(values.begin(), values.end());
}

/** SELECTOR STATE: the form a selector took, and the way back from one.
 *  The class itself is registered with the rest of the type vocabulary;
 *  what a caller needs to RESOLVE a selection — which kind it is, the
 *  bounds or needle it carries, and the operands a combination holds —
 *  is added here, together with the constructor that spells a form this
 *  library ships no builder for. */
void bindSelectorState(py::module_& module) {
  using weave::Selector;
  auto selector = extend<Selector>(module, "weave.Selector");
  py::enum_<Selector::Kind>(selector, "Kind")
      .value("All", Selector::Kind::All)
      .value("Word", Selector::Kind::Word)
      .value("Line", Selector::Kind::Line)
      .value("Sentence", Selector::Kind::Sentence)
      .value("Range", Selector::Kind::Range)
      .value("Regex", Selector::Kind::Regex)
      .value("Text", Selector::Kind::Text)
      .value("Each", Selector::Kind::Each)
      .value("Named", Selector::Kind::Named)
      .value("Scope", Selector::Kind::Scope)
      .value("Union", Selector::Kind::Union)
      .value("Intersect", Selector::Kind::Intersect)
      .value("Complement", Selector::Kind::Complement);
  py::class_<Selector::State> state(selector, "State");
  state
      .def(py::init([](py::kwargs fields) {
        return keywordValue<Selector::State>(fields,
                                             "Unknown selector state field: ");
      }))
      .def("copy", [](const Selector::State& self) { return self; })
      .def_readwrite("kind", &Selector::State::kind)
      .def_readwrite("lo", &Selector::State::lo)
      .def_readwrite("hi", &Selector::State::hi)
      .def_property(
          "pattern",
          [](const Selector::State& self) { return readUtf8(self.pattern); },
          [](Selector::State& self, std::string_view value) {
            self.pattern = utf8Bytes(value);
          })
      .def_readwrite("each", &Selector::State::each)
      .def_readwrite("take", &Selector::State::take)
      .def_readwrite("drop", &Selector::State::drop)
      // The operands are handed out as their own list, so editing what
      // was read cannot reach into the state it came from.
      .def_property(
          "operands",
          [](const Selector::State& self) { return self.operands; },
          [](Selector::State& self, std::vector<Selector> value) {
            self.operands = std::move(value);
          })
      .def(py::self == py::self);
  copyProtocol(state);
  selector.def_static("of", &Selector::of, py::arg("state"))
      .def("state",
           [](const Selector& self) -> std::optional<Selector::State> {
             if (const auto* held = self.state()) return *held;
             return std::nullopt;
           })
      // A selector is a value that rides in larger ones and is resolved
      // against the text it was resolved against last, so it is a key: a
      // resolution cached under one is found again by an equal selector.
      .def("__hash__",
           [](const Selector& self) { return hashSelector(self); });
}

/** THE TEXT ANALYSIS LEAF: what the library can say about a string on its
 *  own — transcoding, character properties, script runs, case mapping,
 *  segmentation and the bidirectional reorder.
 *
 *  Every offset here counts UTF-16 code units, which is what the layout
 *  engine counts and is not what a Python index counts: a character
 *  outside the basic plane occupies two of them and one Python index.
 *  A code point is a number, the one `ord` answers, and never a
 *  one-character string. */
void bindUnicodeLeaf(py::module_& module) {
  auto leaf = submodule(module, "weave.unicode");
  py::enum_<unicode::VerticalOrientation>(leaf, "VerticalOrientation")
      .value("Upright", unicode::VerticalOrientation::kUpright)
      .value("Rotated", unicode::VerticalOrientation::kRotated)
      .value("TransformedUpright",
             unicode::VerticalOrientation::kTransformedUpright)
      .value("TransformedRotated",
             unicode::VerticalOrientation::kTransformedRotated);
  py::enum_<unicode::Case>(leaf, "Case")
      .value("Upper", unicode::Case::kUpper)
      .value("Lower", unicode::Case::kLower)
      .value("Capitalize", unicode::Case::kCapitalize);
  py::enum_<unicode::BaseDirection>(leaf, "BaseDirection")
      .value("LeftToRight", unicode::BaseDirection::kLeftToRight)
      .value("RightToLeft", unicode::BaseDirection::kRightToLeft)
      .value("AutoLeftToRight", unicode::BaseDirection::kAutoLeftToRight)
      .value("AutoRightToLeft", unicode::BaseDirection::kAutoRightToLeft);
  bindRecord<unicode::ScriptRun>(leaf, "ScriptRun", "Unknown script run field: ")
      .def_readwrite("end", &unicode::ScriptRun::end)
      .def_readwrite("script", &unicode::ScriptRun::script);
  bindRecord<unicode::LineBreak>(leaf, "LineBreak", "Unknown line break field: ")
      .def_readwrite("offset", &unicode::LineBreak::offset)
      .def_readwrite("mandatory", &unicode::LineBreak::mandatory)
      .def(py::self == py::self);
  bindRecord<unicode::BidiRun>(leaf, "BidiRun", "Unknown bidi run field: ")
      .def_readwrite("start", &unicode::BidiRun::start)
      .def_readwrite("end", &unicode::BidiRun::end)
      .def_readwrite("level", &unicode::BidiRun::level);

  leaf.def(
          "toUtf16",
          [](std::string_view utf8) {
            return unicode::toUtf16(utf8Bytes(utf8));
          },
          py::arg("utf8"))
      .def(
          "toUtf8",
          [](std::u16string_view utf16) {
            return readUtf8(unicode::toUtf8(utf16));
          },
          py::arg("utf16"))
      // The native decode advances the offset it was given; Python is
      // answered the code point and the offset the next one starts at.
      .def(
          "decodeAt",
          [](std::u16string_view text, size_t offset) {
            if (offset >= text.size())
              throw py::index_error(
                  "A UTF-16 offset to decode from is inside the text.");
            const char32_t codePoint = unicode::decodeAt(text, offset);
            return std::pair<uint32_t, size_t>(static_cast<uint32_t>(codePoint),
                                               offset);
          },
          py::arg("text"), py::arg("offset"));

  leaf.def(
          "isWhitespace",
          [](uint32_t codePoint) {
            return unicode::isWhitespace(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "isHardLineBreak",
          [](uint16_t unit) {
            return unicode::isHardLineBreak(static_cast<char16_t>(unit));
          },
          py::arg("unit"))
      .def(
          "inheritsTypeface",
          [](uint32_t codePoint) {
            return unicode::inheritsTypeface(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "mayRequireBidi",
          [](uint32_t codePoint) {
            return unicode::mayRequireBidi(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "isLetter",
          [](uint32_t codePoint) {
            return unicode::isLetter(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "isUpperCase",
          [](uint32_t codePoint) {
            return unicode::isUpperCase(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "isFullWidth",
          [](uint32_t codePoint) {
            return unicode::isFullWidth(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "verticalOrientation",
          [](uint32_t codePoint) {
            return unicode::verticalOrientation(
                static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"));

  leaf.def(
          "scriptOf",
          [](uint32_t codePoint) {
            return unicode::scriptOf(static_cast<char32_t>(codePoint));
          },
          py::arg("codePoint"))
      .def(
          "isSpecificScript",
          [](unicode::Script script) {
            return unicode::isSpecificScript(script);
          },
          py::arg("script"))
      .def("scriptLimit", [] { return unicode::scriptLimit(); })
      .def(
          "scriptShortName",
          [](unicode::Script script) -> std::optional<std::string> {
            if (const char* name = unicode::scriptShortName(script))
              return std::string(name);
            return std::nullopt;
          },
          py::arg("script"))
      .def(
          "shaperScript",
          [](unicode::Script script) { return unicode::shaperScript(script); },
          py::arg("script"))
      .def(
          "itemize",
          [](std::u16string_view text) { return unicode::itemize(text); },
          py::arg("text"));

  leaf.def("lineStartProhibited",
           [] { return codePointNumbers(unicode::lineStartProhibited()); })
      .def("lineEndProhibited",
           [] { return codePointNumbers(unicode::lineEndProhibited()); });

  leaf.def(
          "caseMapped",
          [](std::u16string_view text, unicode::Case mapping,
             std::string_view locale) {
            return unicode::caseMapped(text, mapping, locale);
          },
          py::arg("text"), py::arg("mapping"), py::arg("locale") = "")
      .def(
          "lowerCased",
          [](uint32_t codePoint) {
            return static_cast<uint32_t>(
                unicode::lowerCased(static_cast<char32_t>(codePoint)));
          },
          py::arg("codePoint"));

  leaf.def(
          "lineBreaks",
          [](std::u16string_view text, std::string_view locale) {
            return unicode::lineBreaks(text, locale);
          },
          py::arg("text"), py::arg("locale") = "")
      .def(
          "graphemeBoundaries",
          [](std::u16string_view text) {
            return unicode::graphemeBoundaries(text);
          },
          py::arg("text"))
      .def(
          "wordBoundaries",
          [](std::u16string_view text) {
            return unicode::wordBoundaries(text);
          },
          py::arg("text"))
      .def(
          "sentenceStarts",
          [](std::u16string_view text) {
            return unicode::sentenceStarts(text);
          },
          py::arg("text"))
      .def(
          "bidi",
          [](std::u16string_view text, unicode::BaseDirection base) {
            return unicode::bidi(text, base);
          },
          py::arg("text"),
          py::arg("base") = unicode::BaseDirection::kAutoLeftToRight);
}

}  // namespace

void bindWeaveSelectorUnicode(py::module_& module) {
  bindSelectorState(module);
  bindUnicodeLeaf(module);
}

}  // namespace sigil::python
