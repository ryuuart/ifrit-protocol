#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/ValueBindings.h>
#include <sigilpython/WeaveBindings.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/query/Selector.h>

#include <algorithm>
#include <array>
#include <string>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

template <class Owner, class Value>
void optionalField(py::class_<Owner>& binding, const char* name,
                   std::optional<Value> Owner::* member) {
  binding.def_property(
      name, [member](const Owner& self) { return self.*member; },
      [member](Owner& self, std::optional<Value> value) {
        self.*member = std::move(value);
      });
}

template <class T>
py::class_<T> record(py::module_& module, const char* name) {
  return bindRecord<T>(module, name, "Unknown typography field: ")
      .def(py::self == py::self);
}

}  // namespace
void bindWeave(py::module_& module) {
  auto text = module.def_submodule("weave");
  using namespace weave;
  py::enum_<TextTransform>(text, "TextTransform")
      .value("None_", TextTransform::kNone)
      .value("Uppercase", TextTransform::kUppercase)
      .value("Lowercase", TextTransform::kLowercase)
      .value("Capitalize", TextTransform::kCapitalize);
  py::enum_<VerticalForm>(text, "VerticalForm")
      .value("Auto", VerticalForm::kAuto)
      .value("Upright", VerticalForm::kUpright)
      .value("Rotated", VerticalForm::kRotated)
      .value("TateChuYoko", VerticalForm::kTateChuYoko);
  py::class_<FontFeature>(text, "FontFeature")
      .def(py::init([](const std::string& tag, uint32_t value) {
             if (tag.size() != 4)
               throw py::value_error("An OpenType tag needs four bytes.");
             FontFeature result;
             std::copy_n(tag.begin(), 4, result.tag);
             result.value = value;
             return result;
           }),
           py::arg("tag"), py::arg("value") = 1)
      .def_property_readonly(
          "tag",
          [](const FontFeature& self) { return std::string(self.tag, 4); })
      .def_readwrite("value", &FontFeature::value)
      .def(py::self == py::self);
  py::class_<FontVariation>(text, "FontVariation")
      .def(py::init([](const std::string& tag, float value) {
             if (tag.size() != 4)
               throw py::value_error("An OpenType tag needs four bytes.");
             FontVariation result;
             std::copy_n(tag.begin(), 4, result.tag);
             result.value = value;
             return result;
           }),
           py::arg("tag"), py::arg("value"))
      .def_property_readonly(
          "tag",
          [](const FontVariation& self) { return std::string(self.tag, 4); })
      .def_readwrite("value", &FontVariation::value)
      .def(py::self == py::self);
  py::class_<ShapingStyle>(text, "ShapingStyle")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<ShapingStyle>(fields); }))
      .def_readwrite("typeface", &ShapingStyle::typeface)
      .def_readwrite("fontSize", &ShapingStyle::fontSize)
      .def_readwrite("letterSpacing", &ShapingStyle::letterSpacing)
      .def_readwrite("scaleX", &ShapingStyle::scaleX)
      .def_readwrite("wordSpacing", &ShapingStyle::wordSpacing)
      .def_readwrite("languageTag", &ShapingStyle::languageTag)
      .def_property(
          "fontFeatures",
          [](const ShapingStyle& self) { return self.fontFeatures; },
          [](ShapingStyle& self, std::vector<FontFeature> value) {
            self.fontFeatures = std::move(value);
          })
      .def_property(
          "variations",
          [](const ShapingStyle& self) { return self.variations; },
          [](ShapingStyle& self, std::vector<FontVariation> value) {
            self.variations = std::move(value);
          })
      .def_readwrite("textTransform", &ShapingStyle::textTransform)
      .def_readwrite("verticalForm", &ShapingStyle::verticalForm)
      .def_readwrite("aliased", &ShapingStyle::aliased)
      .def_readwrite("opticalKerning", &ShapingStyle::opticalKerning)
      .def(py::self == py::self);
  auto decoration = record<Decoration>(text, "Decoration");
  py::enum_<Decoration::Kind>(decoration, "Kind")
      .value("Underline", Decoration::Kind::kUnderline)
      .value("Strikethrough", Decoration::Kind::kStrikethrough)
      .value("Overline", Decoration::Kind::kOverline)
      .value("Highlight", Decoration::Kind::kHighlight);
  py::enum_<Decoration::Span>(decoration, "Span")
      .value("DecoratedRange", Decoration::Span::kDecoratedRange)
      .value("PerWord", Decoration::Span::kPerWord);
  py::enum_<Decoration::Side>(decoration, "Side")
      .value("Default", Decoration::Side::kDefault)
      .value("Opposite", Decoration::Side::kOpposite);
  decoration.def_readwrite("kind", &Decoration::kind)
      .def_readwrite("span", &Decoration::span)
      .def_readwrite("side", &Decoration::side)
      .def_readwrite("thickness", &Decoration::thickness)
      .def_readwrite("offset", &Decoration::offset)
      .def_readwrite("skipInk", &Decoration::skipInk)
      .def_property(
          "color",
          [](const Decoration& value) {
            return SkColor4f::FromColor(value.color);
          },
          [](Decoration& value, py::object ink) {
            value.color = color(ink).toSkColor();
          });
  optionalField(decoration, "paint", &Decoration::paint);
  record<PaintLayer>(text, "PaintLayer")
      .def_readwrite("paint", &PaintLayer::paint)
      .def_property(
          "offset", [](const PaintLayer& value) { return value.offset; },
          [](PaintLayer& value, py::object offset) {
            value.offset = point(offset);
          })
      .def_static(
          "blurred",
          [](SkPaint paint, float sigma, py::object offset) {
            return PaintLayer::blurred(std::move(paint), sigma, point(offset));
          },
          py::arg("paint"), py::arg("sigma"),
          py::arg("offset") = py::make_tuple(0, 0));
  py::class_<PaintStyle>(text, "PaintStyle")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<PaintStyle>(fields); }))
      .def_readwrite("foreground", &PaintStyle::foreground)
      .def_readwrite("baselineShift", &PaintStyle::baselineShift)
      .def_readwrite("underlays", &PaintStyle::underlays)
      .def_readwrite("overlays", &PaintStyle::overlays)
      .def_readwrite("decorations", &PaintStyle::decorations)
      .def("addUnderlay", &PaintStyle::addUnderlay, py::arg("layer"), fluent)
      .def("addOverlay", &PaintStyle::addOverlay, py::arg("layer"), fluent)
      .def("addDecoration", &PaintStyle::addDecoration, py::arg("decoration"),
           fluent)
      .def("copy", [](const PaintStyle& value) { return value; })
      .def(py::self == py::self);
  py::class_<TextStyle>(text, "TextStyle")
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<TextStyle>(fields); }))
      .def_readwrite("shaping", &TextStyle::shaping)
      .def_readwrite("paint", &TextStyle::paint)
      .def("weight", &TextStyle::weight, py::arg("weight"), fluent)
      .def("opticalSize", &TextStyle::opticalSize, py::arg("size"), fluent)
      .def("condense", &TextStyle::condense, py::arg("width"), fluent)
      .def(
          "variation",
          [](TextStyle& self, const std::string& tag,
             float value) -> TextStyle& {
            if (tag.size() != 4)
              throw py::value_error("An OpenType tag needs four bytes.");
            char bytes[5] = {};
            std::copy_n(tag.begin(), 4, bytes);
            return self.variation(bytes, value);
          },
          py::arg("tag"), py::arg("value"), fluent)
      .def(py::self == py::self);
  text.def("textStyle", &weave::textStyle, py::arg("type"))
      .def("initialType", &weave::initialType);
  py::enum_<TextAlignment>(text, "TextAlignment")
      .value("Start", TextAlignment::kStart)
      .value("Center", TextAlignment::kCenter)
      .value("End", TextAlignment::kEnd)
      .value("Justify", TextAlignment::kJustify);
  py::enum_<LineBreakStrategy>(text, "LineBreakStrategy")
      .value("Greedy", LineBreakStrategy::kGreedy)
      .value("KnuthPlass", LineBreakStrategy::kKnuthPlass);
  py::enum_<WritingMode>(text, "WritingMode")
      .value("Horizontal", WritingMode::kHorizontal)
      .value("VerticalRL", WritingMode::kVerticalRL);
  auto leading = py::class_<Leading>(text, "Leading");
  py::enum_<Leading::Kind>(leading, "Kind")
      .value("Face", Leading::Kind::kFace)
      .value("Multiple", Leading::Kind::kMultiple)
      .value("Absolute", Leading::Kind::kAbsolute)
      .value("Grid", Leading::Kind::kGrid);
  leading.def(py::init<>())
      .def_static("face", &Leading::face)
      .def_static("multiple", &Leading::multiple, py::arg("factor"))
      .def_static("absolute", &Leading::absolute, py::arg("pixels"))
      .def_static("grid", &Leading::grid, py::arg("step"))
      .def_readwrite("kind", &Leading::kind)
      .def_readwrite("value", &Leading::value)
      .def(py::self == py::self);
  auto justificationOptions =
      record<JustificationOptions>(text, "JustificationOptions");
  py::enum_<JustificationOptions::SingleWord>(justificationOptions,
                                              "SingleWord")
      .value("Align", JustificationOptions::SingleWord::kAlign)
      .value("Justify", JustificationOptions::SingleWord::kJustify);
  justificationOptions
      .def_readwrite("lastLineAlignment",
                     &JustificationOptions::lastLineAlignment)
      .def_readwrite("justifyLastLine", &JustificationOptions::justifyLastLine)
      .def_readwrite("expandIdeographicGaps",
                     &JustificationOptions::expandIdeographicGaps)
      .def_readwrite("maxIdeographicExpansion",
                     &JustificationOptions::maxIdeographicExpansion)
      .def_readwrite("wordSpacing", &JustificationOptions::wordSpacing)
      .def_readwrite("spaceStretch", &JustificationOptions::spaceStretch)
      .def_readwrite("spaceShrink", &JustificationOptions::spaceShrink)
      .def_readwrite("letterSpacing", &JustificationOptions::letterSpacing)
      .def_readwrite("letterSpacingMinimum",
                     &JustificationOptions::letterSpacingMinimum)
      .def_readwrite("letterSpacingMaximum",
                     &JustificationOptions::letterSpacingMaximum)
      .def_readwrite("glyphScale", &JustificationOptions::glyphScale)
      .def_readwrite("glyphScaleMinimum",
                     &JustificationOptions::glyphScaleMinimum)
      .def_readwrite("glyphScaleMaximum",
                     &JustificationOptions::glyphScaleMaximum)
      .def_readwrite("singleWord", &JustificationOptions::singleWord);
  auto hyphenationLimits = record<HyphenationLimits>(text, "HyphenationLimits");
  hyphenationLimits
      .def_readwrite("minimumWordLength", &HyphenationLimits::minimumWordLength)
      .def_readwrite("minimumLettersBefore",
                     &HyphenationLimits::minimumLettersBefore)
      .def_readwrite("minimumLettersAfter",
                     &HyphenationLimits::minimumLettersAfter)
      .def_readwrite("capitalizedWords", &HyphenationLimits::capitalizedWords);
  auto hyphenationOptions =
      record<HyphenationOptions>(text, "HyphenationOptions");
  hyphenationOptions.def_readwrite("enabled", &HyphenationOptions::enabled)
      .def_readwrite("penalty", &HyphenationOptions::penalty)
      .def_readwrite("limits", &HyphenationOptions::limits)
      .def_readwrite("consecutiveLimit", &HyphenationOptions::consecutiveLimit)
      .def_readwrite("zone", &HyphenationOptions::zone)
      .def_readwrite("lastWordOfBlock", &HyphenationOptions::lastWordOfBlock);
  auto tabStop = record<TabStop>(text, "TabStop");
  py::enum_<TabStop::Align>(tabStop, "Align")
      .value("Start", TabStop::Align::kStart)
      .value("Center", TabStop::Align::kCenter)
      .value("End", TabStop::Align::kEnd)
      .value("Character", TabStop::Align::kCharacter);
  tabStop.def_readwrite("position", &TabStop::position)
      .def_readwrite("align", &TabStop::align)
      .def_readwrite("alignOn", &TabStop::alignOn)
      .def_readwrite("leader", &TabStop::leader);
  auto tabStopOptions = record<TabStopOptions>(text, "TabStopOptions");
  tabStopOptions.def_readwrite("stops", &TabStopOptions::stops)
      .def_readwrite("interval", &TabStopOptions::interval);
  auto kinsokuTable = record<KinsokuTable>(text, "KinsokuTable");
  kinsokuTable.def_readwrite("notLineStart", &KinsokuTable::notLineStart)
      .def_readwrite("notLineEnd", &KinsokuTable::notLineEnd);
  kinsokuTable.def("empty", &KinsokuTable::empty);
  auto hangingEdge = record<HangingEdge>(text, "HangingEdge");
  hangingEdge.def_readwrite("character", &HangingEdge::character)
      .def_readwrite("atStart", &HangingEdge::atStart)
      .def_readwrite("atEnd", &HangingEdge::atEnd);
  auto hangingTable = record<HangingTable>(text, "HangingTable");
  hangingTable.def_readwrite("entries", &HangingTable::entries);
  hangingTable.def("empty", &HangingTable::empty);
  auto reservedBand = record<ReservedBand>(text, "ReservedBand");
  reservedBand.def_readwrite("before", &ReservedBand::before)
      .def_readwrite("after", &ReservedBand::after);
  auto indentOptions = record<IndentOptions>(text, "IndentOptions");
  indentOptions.def_readwrite("start", &IndentOptions::start)
      .def_readwrite("end", &IndentOptions::end)
      .def_readwrite("firstLine", &IndentOptions::firstLine)
      .def_readwrite("lastLine", &IndentOptions::lastLine);
  auto keepOptions = record<KeepOptions>(text, "KeepOptions");
  keepOptions.def_readwrite("widowLines", &KeepOptions::widowLines)
      .def_readwrite("orphanLines", &KeepOptions::orphanLines)
      .def_readwrite("withNext", &KeepOptions::withNext)
      .def_readwrite("allLinesTogether", &KeepOptions::allLinesTogether)
      .def_readwrite("startInNextFrame", &KeepOptions::startInNextFrame);
  auto initialLetter = record<InitialLetter>(text, "InitialLetter");
  py::enum_<InitialLetter::Align>(initialLetter, "Align")
      .value("Alphabetic", InitialLetter::Align::kAlphabetic)
      .value("Ideographic", InitialLetter::Align::kIdeographic)
      .value("Hanging", InitialLetter::Align::kHanging);
  py::enum_<InitialLetter::Wrap>(initialLetter, "Wrap")
      .value("Box", InitialLetter::Wrap::kBox)
      .value("Glyph", InitialLetter::Wrap::kGlyph);
  initialLetter.def_readwrite("lines", &InitialLetter::lines)
      .def_readwrite("sink", &InitialLetter::sink)
      .def_readwrite("graphemes", &InitialLetter::graphemes)
      .def_readwrite("align", &InitialLetter::align)
      .def_readwrite("wrap", &InitialLetter::wrap)
      .def_readwrite("margin", &InitialLetter::margin)
      .def_readwrite("style", &InitialLetter::style);
  auto frameOptions = record<FrameOptions>(text, "FrameOptions");
  py::enum_<FrameOptions::FirstBaseline>(frameOptions, "FirstBaseline")
      .value("Ascent", FrameOptions::FirstBaseline::kAscent)
      .value("CapHeight", FrameOptions::FirstBaseline::kCapHeight)
      .value("XHeight", FrameOptions::FirstBaseline::kXHeight)
      .value("Leading", FrameOptions::FirstBaseline::kLeading)
      .value("Fixed", FrameOptions::FirstBaseline::kFixed);
  py::enum_<FrameOptions::Distribute>(frameOptions, "Distribute")
      .value("Start", FrameOptions::Distribute::kStart)
      .value("Center", FrameOptions::Distribute::kCenter)
      .value("End", FrameOptions::Distribute::kEnd)
      .value("Justify", FrameOptions::Distribute::kJustify);
  frameOptions.def_readwrite("firstBaseline", &FrameOptions::firstBaseline)
      .def_readwrite("firstBaselineOffset", &FrameOptions::firstBaselineOffset)
      .def_readwrite("distribute", &FrameOptions::distribute)
      .def_readwrite("maximumInterlineSpacing",
                     &FrameOptions::maximumInterlineSpacing)
      .def_readwrite("extent", &FrameOptions::extent);
  auto paragraphStyle = record<ParagraphStyle>(text, "ParagraphStyle");
  paragraphStyle.def_readwrite("leading", &ParagraphStyle::leading)
      .def_readwrite("halfLeading", &ParagraphStyle::halfLeading)
      .def_readwrite("spaceBefore", &ParagraphStyle::spaceBefore)
      .def_readwrite("spaceAfter", &ParagraphStyle::spaceAfter)
      .def_readwrite("reserved", &ParagraphStyle::reserved)
      .def_readwrite("indent", &ParagraphStyle::indent)
      .def_readwrite("keep", &ParagraphStyle::keep)
      .def_readwrite("balanceRaggedLines", &ParagraphStyle::balanceRaggedLines)
      .def_readwrite("initial", &ParagraphStyle::initial);
  optionalField(paragraphStyle, "alignment", &ParagraphStyle::alignment);
  optionalField(paragraphStyle, "justification",
                &ParagraphStyle::justification);
  optionalField(paragraphStyle, "hyphenation", &ParagraphStyle::hyphenation);
  optionalField(paragraphStyle, "tabStops", &ParagraphStyle::tabStops);
  py::enum_<MojikumiClass>(text, "MojikumiClass")
      .value("Other", MojikumiClass::kOther)
      .value("Ideograph", MojikumiClass::kIdeograph)
      .value("Opening", MojikumiClass::kOpening)
      .value("Closing", MojikumiClass::kClosing)
      .value("FullStop", MojikumiClass::kFullStop)
      .value("Comma", MojikumiClass::kComma)
      .value("MiddleDot", MojikumiClass::kMiddleDot);
  using Members = std::array<std::u16string, MojikumiTable::kClasses>;
  using Room = std::array<std::array<float, MojikumiTable::kClasses>,
                          MojikumiTable::kClasses>;
  record<MojikumiTable>(text, "MojikumiTable")
      .def_property(
          "members",
          [](const MojikumiTable& value) {
            Members result;
            std::copy_n(value.members, result.size(), result.begin());
            return result;
          },
          [](MojikumiTable& value, const Members& members) {
            std::copy(members.begin(), members.end(), value.members);
          })
      .def_property(
          "room",
          [](const MojikumiTable& value) {
            Room result;
            for (size_t row = 0; row < result.size(); ++row)
              std::copy_n(value.room[row], result[row].size(),
                          result[row].begin());
            return result;
          },
          [](MojikumiTable& value, const Room& room) {
            for (size_t row = 0; row < room.size(); ++row)
              std::copy(room[row].begin(), room[row].end(), value.room[row]);
          })
      .def("classOf", &MojikumiTable::classOf, py::arg("character"))
      .def("empty", &MojikumiTable::empty);
  auto block = py::class_<Block>(text, "Block");
  block
      .def(py::init(
          [](py::kwargs fields) { return keywordValue<Block>(fields); }))
      .def_property(
          "leading", [](const Block& self) { return self.leading; },
          [](Block& self, std::optional<Leading> value) {
            self.leading = std::move(value);
          })
      .def_readwrite("halfLeading", &Block::halfLeading)
      .def_readwrite("alignment", &Block::alignment)
      .def_readwrite("firstLineIndent", &Block::firstLineIndent)
      .def_readwrite("lastLineIndent", &Block::lastLineIndent)
      .def_readwrite("widowLines", &Block::widowLines)
      .def_readwrite("orphanLines", &Block::orphanLines)
      .def_readwrite("balanceRaggedLines", &Block::balanceRaggedLines)
      .def_readwrite("writingMode", &Block::writingMode)
      .def_readwrite("lineBreakLocale", &Block::lineBreakLocale)
      .def_readwrite("lineBreak", &Block::lineBreak)
      .def_readwrite("lastLineAlignment", &Block::lastLineAlignment)
      .def_readwrite("justifyLastLine", &Block::justifyLastLine)
      .def_readwrite("tsume", &Block::tsume)
      .def("empty", &Block::empty)
      .def(py::self == py::self);
  optionalField(block, "justification", &Block::justification);
  optionalField(block, "hyphenation", &Block::hyphenation);
  optionalField(block, "tabStops", &Block::tabStops);
  optionalField(block, "kinsoku", &Block::kinsoku);
  optionalField(block, "hanging", &Block::hanging);
  optionalField(block, "mojikumi", &Block::mojikumi);
  text.def("toParagraphStyle", &toParagraphStyle, py::arg("block"));
  py::class_<Rule>(text, "Rule")
      .def(py::init<std::string>(), py::arg("name"))
      .def(py::init<std::string, Type>(), py::arg("name"), py::arg("type"))
      .def(py::init<std::string, Block>(), py::arg("name"), py::arg("block"))
      .def("font", &Rule::font, py::arg("type"), fluent)
      .def("block", py::overload_cast<Block>(&Rule::block), py::arg("block"),
           fluent)
      .def("name", &Rule::name)
      .def("type", &Rule::type, py::return_value_policy::copy)
      .def("block", py::overload_cast<>(&Rule::block, py::const_),
           py::return_value_policy::copy)
      .def(py::self == py::self);
  text.def("rule", &weave::rule, py::arg("name"));
  py::class_<StyleSheet>(text, "StyleSheet")
      .def(py::init([](const std::vector<Rule>& rules) {
             StyleSheet value;
             for (const auto& rule : rules) value.set(rule);
             return value;
           }),
           py::arg("rules") = std::vector<Rule>{})
      .def(py::init<TextStyle>(), py::arg("style"))
      .def("base", py::overload_cast<TextStyle>(&StyleSheet::base),
           py::arg("style"), fluent)
      .def("base", py::overload_cast<>(&StyleSheet::base, py::const_),
           py::return_value_policy::copy)
      .def("set", py::overload_cast<const Rule&>(&StyleSheet::set),
           py::arg("rule"), fluent)
      .def("set", py::overload_cast<std::string, Type>(&StyleSheet::set),
           py::arg("name"), py::arg("type"), fluent)
      .def("set", py::overload_cast<std::string, Block>(&StyleSheet::set),
           py::arg("name"), py::arg("block"), fluent)
      .def("__getitem__", &StyleSheet::operator[], py::arg("name"))
      .def(
          "find",
          [](const StyleSheet& self,
             const std::string& name) -> std::optional<Rule> {
            if (auto rule = self.find(name)) return *rule;
            return {};
          },
          py::arg("name"))
      .def("contains", &StyleSheet::contains, py::arg("name"))
      .def("__contains__", &StyleSheet::contains, py::arg("name"))
      .def("types", &StyleSheet::types, py::return_value_policy::copy)
      .def("rules", &StyleSheet::rules, py::return_value_policy::copy)
      .def("__len__", &StyleSheet::size)
      .def("empty", &StyleSheet::empty)
      .def(py::self == py::self);
  py::class_<TypeSheet>(text, "TypeSheet")
      .def(py::init<>())
      .def(py::init<TextStyle>(), py::arg("style"))
      .def("base", py::overload_cast<TextStyle>(&TypeSheet::base),
           py::arg("style"), fluent)
      .def("base", py::overload_cast<>(&TypeSheet::base, py::const_),
           py::return_value_policy::copy)
      .def("set", &TypeSheet::set, py::arg("name"), py::arg("type"), fluent)
      .def("__getitem__", &TypeSheet::operator[], py::arg("name"))
      .def(
          "find",
          [](const TypeSheet& self,
             const std::string& name) -> std::optional<Type> {
            if (auto value = self.find(name)) return *value;
            return {};
          },
          py::arg("name"))
      .def("contains", &TypeSheet::contains, py::arg("name"))
      .def("__contains__", &TypeSheet::contains, py::arg("name"))
      .def("entries", &TypeSheet::entries, py::return_value_policy::copy)
      .def("__len__", &TypeSheet::size)
      .def("empty", &TypeSheet::empty)
      .def("copy", [](const TypeSheet& self) { return self; })
      .def(py::self == py::self);
  auto richText = py::class_<RichText>(text, "RichText");
  py::class_<RichText::Run>(richText, "Run")
      .def_property_readonly("utf8",
                             [](const RichText::Run& run) {
                               return std::string(run.utf8.begin(),
                                                  run.utf8.end());
                             })
      .def_property_readonly("style",
                             [](const RichText::Run& run) { return run.style; })
      .def_readonly("styleName", &RichText::Run::styleName)
      .def_property_readonly("over",
                             [](const RichText::Run& run) { return run.over; })
      .def_readonly("total", &RichText::Run::total)
      .def_readonly("slotName", &RichText::Run::slotName)
      .def_property_readonly("slotSize",
                             [](const RichText::Run& run) {
                               return std::make_pair(run.slotSize.width(),
                                                     run.slotSize.height());
                             })
      .def_readonly("slotBaselineDrop", &RichText::Run::slotBaselineDrop)
      .def(py::self == py::self);
  richText.def(py::init<>())
      .def(py::init<TextStyle>(), py::arg("base"))
      .def("add", py::overload_cast<std::string_view>(&RichText::add),
           py::arg("text"), fluent)
      .def("add",
           py::overload_cast<std::string_view, TextStyle>(&RichText::add),
           py::arg("text"), py::arg("style"), fluent)
      .def("add", py::overload_cast<std::string_view, Type>(&RichText::add),
           py::arg("text"), py::arg("type"), fluent)
      .def(
          "add",
          py::overload_cast<std::string_view, std::string_view>(&RichText::add),
          py::arg("text"), py::arg("name"), fluent)
      .def(
          "slot",
          [](RichText& self, const std::string& name,
             std::pair<float, float> size, float baselineDrop) -> RichText& {
            return self.slot(name, SkSize::Make(size.first, size.second),
                             baselineDrop);
          },
          py::arg("name"), py::arg("size"), py::arg("baselineDrop") = 0.0f,
          fluent)
      .def("styles", &RichText::styles, py::arg("sheet"), fluent)
      .def("base", &RichText::base, py::return_value_policy::copy)
      .def("hasBase", &RichText::hasBase)
      .def("hasStyles", &RichText::hasStyles)
      .def("empty", &RichText::empty)
      .def("runs",
           [](const RichText& self) {
             return std::vector<RichText::Run>(self.runs().begin(),
                                               self.runs().end());
           })
      .def("copy", [](const RichText& self) { return self; })
      .def(py::self == py::self);
  text.def("rich", py::overload_cast<>(&rich))
      .def("rich", py::overload_cast<TextStyle>(&rich), py::arg("base"));
  py::class_<Story>(text, "Story")
      .def(py::init<>())
      .def(py::init<RichText>(), py::arg("content"))
      .def(py::init<std::string_view, TextStyle>(), py::arg("text"),
           py::arg("style"))
      .def("paragraphs", &Story::paragraphs, py::arg("blocks"), fluent)
      .def("content", &Story::content, py::return_value_policy::copy)
      .def("blocks",
           [](const Story& self) {
             return std::vector<ParagraphStyle>(self.blocks().begin(),
                                                self.blocks().end());
           })
      .def("empty", &Story::empty)
      .def("copy", [](const Story& self) { return self; })
      .def(py::self == py::self);
  py::enum_<Unit>(text, "Unit")
      .value("Glyph", Unit::Glyph)
      .value("Cluster", Unit::Cluster)
      .value("Word", Unit::Word)
      .value("Line", Unit::Line)
      .value("Sentence", Unit::Sentence);
  py::class_<Selector>(text, "Selector")
      .def(py::init<>())
      .def("take", &Selector::take, py::arg("count"))
      .def("drop", &Selector::drop, py::arg("count"))
      .def("__or__", &Selector::operator|, py::arg("other"), py::is_operator())
      .def("__and__", &Selector::operator&, py::arg("other"), py::is_operator())
      .def("__invert__", &Selector::operator!)
      .def(py::self == py::self);
  auto select = text.def_submodule("selectors");
  select.def("word", &selectors::word, py::arg("index"))
      .def("words", &selectors::words, py::arg("start"), py::arg("end"))
      .def("line", &selectors::line, py::arg("index"))
      .def("lines", &selectors::lines, py::arg("start"), py::arg("end"))
      .def("sentence", &selectors::sentence, py::arg("index"))
      .def("each", &selectors::each, py::arg("unit"))
      .def(
          "range",
          [](uint32_t start, uint32_t end) {
            return selectors::range({start, end});
          },
          py::arg("start"), py::arg("end"))
      .def(
          "text",
          [](const std::string& value) {
            return selectors::text(std::u8string(value.begin(), value.end()));
          },
          py::arg("text"))
      .def(
          "regex",
          [](const std::string& value) {
            return selectors::regex(std::u8string(value.begin(), value.end()));
          },
          py::arg("pattern"));
}

}  // namespace sigil::python
