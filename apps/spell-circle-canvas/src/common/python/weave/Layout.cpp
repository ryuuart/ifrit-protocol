#include <pybind11/stl.h>
#include <sigildraw/Pen.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/layout/Beside.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Query.h>

#include <memory>
#include <thread>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;
// The native placement borrows ordinary shaped words from its paragraph.
// Retain a value snapshot so editing or discarding the authoring paragraph
// cannot invalidate those glyph pointers.
struct OwnedLayout : weave::ParagraphLayout {
  OwnedLayout(const weave::Paragraph& paragraph, weave::ParagraphLayout value)
      : weave::ParagraphLayout(std::move(value)), source(paragraph) {}
  void check(const weave::Paragraph& paragraph) const {
    if (paragraph.identity() != source.identity() ||
        paragraph.revision() != source.revision() ||
        paragraph.wordRevision() != source.wordRevision())
      throw py::value_error("Rebuild the layout after changing its paragraph.");
  }
  weave::Paragraph source;
};
struct Fonts {
  std::thread::id thread = std::this_thread::get_id();
  weave::FontContext value;
  explicit Fonts(sk_sp<SkTypeface> face)
      : value(weave::ports::systemFontManager(), std::move(face)) {}
  weave::FontContext& get() {
    if (thread != std::this_thread::get_id())
      throw std::runtime_error("FontContext belongs to its creating thread.");
    return value;
  }
};
}  // namespace
void bindWeaveLayout(py::module_& root) {
  using namespace weave;
  auto module = root.attr("weave").cast<py::module_>();
  auto charRange =
      bindRecord<CharRange>(module, "CharRange", "Unknown CharRange field: ");
  charRange.def_readwrite("start", &CharRange::start)
      .def_readwrite("end", &CharRange::end);
  auto styleSpan =
      bindRecord<StyleSpan>(module, "StyleSpan", "Unknown StyleSpan field: ");
  styleSpan.def_readwrite("start", &StyleSpan::start)
      .def_readwrite("end", &StyleSpan::end)
      .def_readwrite("style", &StyleSpan::style);
  auto placeholder = bindRecord<Placeholder>(module, "Placeholder",
                                             "Unknown Placeholder field: ");
  placeholder.def_readwrite("width", &Placeholder::width)
      .def_readwrite("height", &Placeholder::height)
      .def_readwrite("baselineDrop", &Placeholder::baselineDrop);
  auto strut =
      bindRecord<Paragraph::Strut>(module, "Strut", "Unknown Strut field: ");
  strut.def_readwrite("ascent", &Paragraph::Strut::ascent)
      .def_readwrite("height", &Paragraph::Strut::height)
      .def_readwrite("capHeight", &Paragraph::Strut::capHeight)
      .def_readwrite("xHeight", &Paragraph::Strut::xHeight);
  auto textEdit = bindRecord<Paragraph::TextEdit>(module, "TextEdit",
                                                  "Unknown TextEdit field: ");
  textEdit.def_readwrite("start", &Paragraph::TextEdit::start)
      .def_readwrite("removed", &Paragraph::TextEdit::removed)
      .def_readwrite("inserted", &Paragraph::TextEdit::inserted);
  auto fontStats = bindRecord<FontContext::Stats>(module, "FontStats",
                                                  "Unknown FontStats field: ");
  fontStats.def_readwrite("shapeCalls", &FontContext::Stats::shapeCalls)
      .def_readwrite("shapeCacheHits", &FontContext::Stats::shapeCacheHits)
      .def_readwrite("fallbackQueries", &FontContext::Stats::fallbackQueries)
      .def_readwrite("coverageQueries", &FontContext::Stats::coverageQueries)
      .def_readwrite("opticalProfileQueries",
                     &FontContext::Stats::opticalProfileQueries)
      .def_readwrite("opticalReferenceQueries",
                     &FontContext::Stats::opticalReferenceQueries);
  auto lineMetricsOptions = bindRecord<LineMetricsOptions>(
      module, "LineMetricsOptions", "Unknown LineMetricsOptions field: ");
  lineMetricsOptions.def_readwrite("height", &LineMetricsOptions::height)
      .def_readwrite("ascent", &LineMetricsOptions::ascent);
  auto knuthPlassOptions = bindRecord<KnuthPlassOptions>(
      module, "KnuthPlassOptions", "Unknown KnuthPlassOptions field: ");
  knuthPlassOptions.def_readwrite("tolerance", &KnuthPlassOptions::tolerance)
      .def_readwrite("minimumIntervalWidth",
                     &KnuthPlassOptions::minimumIntervalWidth)
      .def_readwrite("candidates", &KnuthPlassOptions::candidates);
  auto overflowOptions = bindRecord<OverflowOptions>(
      module, "OverflowOptions", "Unknown OverflowOptions field: ");
  overflowOptions.def_readwrite("ellipsis", &OverflowOptions::ellipsis)
      .def_readwrite("maxLines", &OverflowOptions::maxLines);
  auto pathTextOptions = bindRecord<PathTextOptions>(
      module, "PathTextOptions", "Unknown PathTextOptions field: ");
  pathTextOptions.def_readwrite("tangentRotationSteps",
                                &PathTextOptions::tangentRotationSteps);
  auto paragraphLayoutOptions = bindRecord<ParagraphLayoutOptions>(
      module, "ParagraphLayoutOptions",
      "Unknown ParagraphLayoutOptions field: ");
  paragraphLayoutOptions.def_readwrite("live", &ParagraphLayoutOptions::live)
      .def_readwrite("alignment", &ParagraphLayoutOptions::alignment)
      .def_readwrite("lineBreakStrategy",
                     &ParagraphLayoutOptions::lineBreakStrategy)
      .def_readwrite("lineMetrics", &ParagraphLayoutOptions::lineMetrics)
      .def_readwrite("hyphenation", &ParagraphLayoutOptions::hyphenation)
      .def_readwrite("justification", &ParagraphLayoutOptions::justification)
      .def_readwrite("knuthPlass", &ParagraphLayoutOptions::knuthPlass)
      .def_readwrite("overflow", &ParagraphLayoutOptions::overflow)
      .def_readwrite("tabStops", &ParagraphLayoutOptions::tabStops)
      .def_readwrite("pathText", &ParagraphLayoutOptions::pathText)
      .def_readwrite("frame", &ParagraphLayoutOptions::frame)
      .def_readwrite("reserved", &ParagraphLayoutOptions::reserved)
      .def_readwrite("nextMeasure", &ParagraphLayoutOptions::nextMeasure)
      .def_readwrite("kinsoku", &ParagraphLayoutOptions::kinsoku)
      .def_readwrite("hanging", &ParagraphLayoutOptions::hanging)
      .def_readwrite("mojikumi", &ParagraphLayoutOptions::mojikumi)
      .def_readwrite("tsume", &ParagraphLayoutOptions::tsume)
      .def_readwrite("blocks", &ParagraphLayoutOptions::blocks)
      .def_readwrite("blockDefault", &ParagraphLayoutOptions::blockDefault);
  auto lineRequest = bindRecord<LineRequest>(module, "LineRequest",
                                             "Unknown LineRequest field: ");
  lineRequest.def_readwrite("index", &LineRequest::index)
      .def_readwrite("bandStart", &LineRequest::bandStart)
      .def_readwrite("lineHeight", &LineRequest::lineHeight)
      .def_readwrite("ascent", &LineRequest::ascent)
      .def_readwrite("blockIndex", &LineRequest::blockIndex)
      .def_readwrite("lineInBlock", &LineRequest::lineInBlock);
  auto lineInterval = bindRecord<LineInterval>(module, "LineInterval",
                                               "Unknown LineInterval field: ");
  lineInterval.def_readwrite("length", &LineInterval::length)
      .def_readwrite("contourStart", &LineInterval::contourStart)
      .def_readwrite("wrapContour", &LineInterval::wrapContour)
      .def_readwrite("advanceScale", &LineInterval::advanceScale);
  auto span = bindRecord<Span>(module, "Span", "Unknown Span field: ");
  span.def_readwrite("start", &Span::start).def_readwrite("end", &Span::end);
  auto band = bindRecord<Band>(module, "Band", "Unknown Band field: ");
  band.def_readwrite("start", &Band::start).def_readwrite("end", &Band::end);
  auto placedInitial = bindRecord<PlacedInitial>(
      module, "PlacedInitial", "Unknown PlacedInitial field: ");
  placedInitial.def_readwrite("placed", &PlacedInitial::placed)
      .def_readwrite("box", &PlacedInitial::box)
      .def_readwrite("baseline", &PlacedInitial::baseline)
      .def_readwrite("fontSize", &PlacedInitial::fontSize)
      .def_readwrite("bands", &PlacedInitial::bands)
      .def_readwrite("notch", &PlacedInitial::notch)
      .def_readwrite("textEnd", &PlacedInitial::textEnd);
  auto warichuSplit = bindRecord<WarichuSplit>(module, "WarichuSplit",
                                               "Unknown WarichuSplit field: ");
  warichuSplit.def_readwrite("advance", &WarichuSplit::advance)
      .def_readwrite("band", &WarichuSplit::band)
      .def_readwrite("cutWord", &WarichuSplit::cutWord);
  auto glyphFit =
      bindRecord<GlyphFit>(module, "GlyphFit", "Unknown GlyphFit field: ");
  glyphFit.def_readwrite("letterSpacing", &GlyphFit::letterSpacing)
      .def_readwrite("glyphScale", &GlyphFit::glyphScale);
  lineInterval.def_property(
      "origin", [](const LineInterval& x) { return x.origin; },
      [](LineInterval& x, py::handle v) { x.origin = point(v); });
  lineInterval.def_property(
      "direction", [](const LineInterval& x) { return x.direction; },
      [](LineInterval& x, py::handle v) { x.direction = point(v); });
  py::class_<LineMetrics>(module, "LineMetrics")
      .def_readonly("lineIndex", &LineMetrics::lineIndex)
      .def_readonly("baseline", &LineMetrics::baseline)
      .def_readonly("ascent", &LineMetrics::ascent)
      .def_readonly("descent", &LineMetrics::descent)
      .def_readonly("left", &LineMetrics::left)
      .def_readonly("right", &LineMetrics::right)
      .def_readonly("textBegin", &LineMetrics::textBegin)
      .def_readonly("textEnd", &LineMetrics::textEnd)
      .def("rect", &LineMetrics::rect);
  py::class_<ColumnMetrics>(module, "ColumnMetrics")
      .def_readonly("lineIndex", &ColumnMetrics::lineIndex)
      .def_readonly("axis", &ColumnMetrics::axis)
      .def_readonly("pitch", &ColumnMetrics::pitch)
      .def_readonly("top", &ColumnMetrics::top)
      .def_readonly("bottom", &ColumnMetrics::bottom)
      .def_readonly("textBegin", &ColumnMetrics::textBegin)
      .def_readonly("textEnd", &ColumnMetrics::textEnd)
      .def("rect", &ColumnMetrics::rect);
  py::class_<Fonts>(module, "FontContext")
      .def(py::init<sk_sp<SkTypeface>>(), py::arg("defaultTypeface") = nullptr)
      .def("defaultTypeface",
           [](Fonts& f) { return f.get().defaultTypeface(); })
      .def("stats", [](Fonts& f) { return f.get().stats(); })
      .def("resetStats", [](Fonts& f) { f.get().resetStats(); })
      .def("purgeShapeCache", [](Fonts& f) { f.get().purgeShapeCache(); })
      .def("purgeAllCaches", [](Fonts& f) { f.get().purgeAllCaches(); })
      .def("variedTypefaceCount",
           [](Fonts& f) { return f.get().variedTypefaceCount(); })
      .def(
          "variedTypeface",
          [](Fonts& f, sk_sp<SkTypeface> face,
             const std::vector<FontVariation>& variations) {
            return f.get().variedTypeface(face, variations);
          },
          py::arg("base"), py::arg("variations"))
      .def(
          "resolveTypeface",
          [](Fonts& f, sk_sp<SkTypeface> face, int32_t codePoint,
             const std::string& language) {
            return f.get().resolveTypeface(face, codePoint, language.c_str());
          },
          py::arg("primaryTypeface"), py::arg("codePoint"),
          py::arg("languageTag") = "")
      .def(
          "glyphAdvanceEm",
          [](Fonts& f, sk_sp<SkTypeface> face, SkGlyphID glyph, bool vertical) {
            return f.get().glyphAdvanceEm(face, glyph, vertical);
          },
          py::arg("base"), py::arg("glyph"), py::arg("vertical") = false);
  py::class_<Hyphenator, std::shared_ptr<Hyphenator>>(module, "Hyphenator")
      .def(
          "breakPoints",
          [](const Hyphenator& h, const std::u16string& word,
             const std::string& language) {
            std::vector<uint32_t> out;
            h.breakPoints(word, language, out);
            return out;
          },
          py::arg("word"), py::arg("languageTag"));
  auto kit = module.attr("kit").cast<py::module_>();
  py::class_<kit::PatternHyphenator, Hyphenator,
             std::shared_ptr<kit::PatternHyphenator>>(kit, "PatternHyphenator")
      .def(py::init<std::string, std::string_view>(), py::arg("languagePrefix"),
           py::arg("patternFile"))
      .def("load", &kit::PatternHyphenator::load, py::arg("languagePrefix"),
           py::arg("patternFile"))
      .def("language", &kit::PatternHyphenator::language)
      .def("patternCount", &kit::PatternHyphenator::patternCount);
  kit.def("englishHyphenationPatterns", &kit::englishHyphenationPatterns);
  auto paragraph = py::class_<Paragraph>(module, "Paragraph");
  paragraph.def(py::init<>())
      .def(py::init([](const std::u16string& text, const Type& type) {
             Paragraph p;
             p.appendText(text, textStyle(type));
             return p;
           }),
           py::arg("text"), py::arg("type"))
      .def(py::init([](const std::u16string& text, const TextStyle& style) {
             Paragraph p;
             p.appendText(text, style);
             return p;
           }),
           py::arg("text"), py::arg("style"))
      .def(
          "appendText",
          [](Paragraph& p, const std::u16string& text, const TextStyle& style) {
            p.appendText(text, style);
          },
          py::arg("text"), py::arg("style"))
      .def(
          "replaceText",
          [](Paragraph& p, uint32_t start, uint32_t end,
             const std::string& text) {
            p.replaceText(start, end,
                          std::u8string_view(
                              reinterpret_cast<const char8_t*>(text.data()),
                              text.size()));
          },
          py::arg("start"), py::arg("end"), py::arg("text"))
      .def("setStyle", &Paragraph::setStyle, py::arg("start"), py::arg("end"),
           py::arg("style"))
      .def("setPaint",
           py::overload_cast<uint32_t, uint32_t, const PaintStyle&>(
               &Paragraph::setPaint),
           py::arg("start"), py::arg("end"), py::arg("paint"))
      .def(
          "setPaint",
          [](Paragraph& p, const std::vector<CharRange>& ranges,
             const PaintStyle& paint) { p.setPaint(ranges, paint); },
          py::arg("ranges"), py::arg("paint"))
      .def("appendPlaceholder", &Paragraph::appendPlaceholder,
           py::arg("placeholder"), py::arg("style"))
      .def("setPlaceholder", &Paragraph::setPlaceholder, py::arg("index"),
           py::arg("placeholder"))
      .def("setHyphenator", &Paragraph::setHyphenator, py::arg("hyphenator"),
           py::arg("limits") = HyphenationLimits{}, py::keep_alive<1, 2>())
      .def("setWritingMode", &Paragraph::setWritingMode, py::arg("mode"))
      .def("setSoftHyphenBreaks", &Paragraph::setSoftHyphenBreaks,
           py::arg("enabled"))
      .def("setKinsoku", &Paragraph::setKinsoku, py::arg("table"))
      .def("setLineBreakLocale", &Paragraph::setLineBreakLocale,
           py::arg("locale"))
      .def(
          "editsSince",
          [](const Paragraph& p, uint64_t revision)
              -> std::optional<std::vector<Paragraph::TextEdit>> {
            std::vector<Paragraph::TextEdit> edits;
            if (!p.editsSince(revision, edits)) return {};
            return edits;
          },
          py::arg("revision"));
  paragraph.def("clear", &Paragraph::clear, py::return_value_policy::copy);
  paragraph.def("writingMode", &Paragraph::writingMode,
                py::return_value_policy::copy);
  paragraph.def("softHyphenBreaks", &Paragraph::softHyphenBreaks,
                py::return_value_policy::copy);
  paragraph.def("identity", &Paragraph::identity,
                py::return_value_policy::copy);
  paragraph.def("wordRevision", &Paragraph::wordRevision,
                py::return_value_policy::copy);
  paragraph.def("lineBreakLocale", &Paragraph::lineBreakLocale,
                py::return_value_policy::copy);
  paragraph.def("kinsoku", &Paragraph::kinsoku, py::return_value_policy::copy);
  paragraph.def("text", &Paragraph::text, py::return_value_policy::copy);
  paragraph.def("spans", &Paragraph::spans, py::return_value_policy::copy);
  paragraph.def("revision", &Paragraph::revision,
                py::return_value_policy::copy);
  paragraph.def("shapedWordCount", &Paragraph::shapedWordCount,
                py::return_value_policy::copy);
  paragraph.def("needsShaping", &Paragraph::needsShaping,
                py::return_value_policy::copy);
  paragraph.def("placeholders", &Paragraph::placeholders,
                py::return_value_policy::copy);
  paragraph.def(
      "ensureShaped",
      [](Paragraph& p, Fonts& fonts) { return p.ensureShaped(fonts.get()); },
      py::arg("fonts"));
  paragraph.def(
      "ensureAnalyzed",
      [](Paragraph& p, Fonts& fonts) { return p.ensureAnalyzed(fonts.get()); },
      py::arg("fonts"));
  paragraph.def(
      "strut", [](Paragraph& p, Fonts& fonts) { return p.strut(fonts.get()); },
      py::arg("fonts"));
  paragraph.def(
      "naturalWidth",
      [](Paragraph& p, Fonts& fonts) { return p.naturalWidth(fonts.get()); },
      py::arg("fonts"));
  paragraph.def(
      "strutAt",
      [](Paragraph& p, Fonts& fonts, uint32_t offset) {
        return p.strutAt(fonts.get(), offset);
      },
      py::arg("fonts"), py::arg("textOffset"));
  paragraph.def("sentenceStarts", [](const Paragraph& p) {
    auto values = p.sentenceStarts();
    return std::vector<uint32_t>(values.begin(), values.end());
  });
  py::class_<ParagraphBuilder>(module, "ParagraphBuilder")
      .def(py::init<const TextStyle&>(), py::arg("baseStyle"))
      .def("pushStyle", &ParagraphBuilder::pushStyle, py::arg("style"), fluent)
      .def("popStyle", &ParagraphBuilder::popStyle, fluent)
      .def("addText",
           py::overload_cast<std::string_view>(&ParagraphBuilder::addText),
           py::arg("text"), fluent)
      .def("addPlaceholder", &ParagraphBuilder::addPlaceholder,
           py::arg("placeholder"), fluent)
      .def("build", &ParagraphBuilder::build);
  py::enum_<FlowAxis>(module, "FlowAxis")
      .value("Lines", FlowAxis::kLines)
      .value("Columns", FlowAxis::kColumns);
  py::class_<FlowGeometry>(module, "FlowGeometry")
      .def(
          "lineIntervals",
          [](FlowGeometry& flow, const LineRequest& request)
              -> std::optional<std::vector<LineInterval>> {
            std::vector<LineInterval> intervals;
            if (!flow.lineIntervals(request, intervals)) return {};
            return intervals;
          },
          py::arg("request"))
      .def("uniformIntervals", &FlowGeometry::uniformIntervals);
  py::class_<BlockFlow, FlowGeometry>(module, "BlockFlow")
      .def(py::init([](py::handle bounds) { return BlockFlow(rect(bounds)); }),
           py::arg("bounds"));
  py::class_<VerticalBlockFlow, FlowGeometry>(module, "VerticalBlockFlow")
      .def(py::init([](py::handle bounds) {
             return VerticalBlockFlow(rect(bounds));
           }),
           py::arg("bounds"));
  py::class_<PathFlow, FlowGeometry>(module, "PathFlow")
      .def(py::init<const SkPath&>(), py::arg("path"))
      .def("addPath", &PathFlow::addPath, py::arg("path"));
  py::class_<LineSetFlow, FlowGeometry>(module, "LineSetFlow")
      .def(py::init<std::vector<std::vector<LineInterval>>>(),
           py::arg("lines") = std::vector<std::vector<LineInterval>>{})
      .def_property(
          "lines", [](LineSetFlow& x) { return x.lines(); },
          [](LineSetFlow& x, std::vector<std::vector<LineInterval>> lines) {
            x.lines() = std::move(lines);
          });
  py::class_<FlowShape, std::shared_ptr<FlowShape>>(module, "FlowShape")
      .def("bounds", &FlowShape::bounds)
      .def(
          "bandSpans",
          [](FlowShape& s, FlowAxis axis, Band band, float margin) {
            std::vector<Span> out;
            s.bandSpans(axis, band, margin, out);
            return out;
          },
          py::arg("axis"), py::arg("band"), py::arg("margin") = 0.0f);
  bindRecord<Exclusion>(module, "Exclusion", "Unknown exclusion field: ")
      .def_readwrite("shape", &Exclusion::shape)
      .def_readwrite("margin", &Exclusion::margin)
      .def_property(
          "offset", [](const Exclusion& x) { return x.offset; },
          [](Exclusion& x, py::handle v) { x.offset = point(v); });
  py::class_<ExclusionFlow, FlowGeometry>(module, "ExclusionFlow")
      .def(py::init([](py::handle bounds, FlowAxis axis) {
             return std::make_unique<ExclusionFlow>(rect(bounds), axis);
           }),
           py::arg("bounds"), py::arg("axis") = FlowAxis::kLines)
      .def_property(
          "exclusions", [](ExclusionFlow& f) { return f.exclusions(); },
          [](ExclusionFlow& f, std::vector<Exclusion> exclusions) {
            f.exclusions() = std::move(exclusions);
          })
      .def("bounds", &ExclusionFlow::bounds, py::return_value_policy::copy)
      .def("axis", &ExclusionFlow::axis)
      .def("setMinimumIntervalWidth", &ExclusionFlow::setMinimumIntervalWidth,
           py::arg("minimumWidth"));
  auto flowshapes = module.def_submodule("flowshape");
  flowshapes.def(
      "rectangle",
      [](py::handle bounds) { return flowshape::rectangle(rect(bounds)); },
      py::arg("bounds"));
  flowshapes.def(
      "circle",
      [](py::handle bounds) { return flowshape::circle(rect(bounds)); },
      py::arg("bounds"));
  flowshapes.def(
      "ellipse",
      [](py::handle bounds) { return flowshape::ellipse(rect(bounds)); },
      py::arg("bounds"));
  flowshapes.def("path", &flowshape::path, py::arg("path"));
  flowshapes.def(
      "coverage",
      [](sk_sp<SkImage> image, py::handle bounds, float threshold) {
        return flowshape::coverage(image, rect(bounds), threshold);
      },
      py::arg("image"), py::arg("bounds"), py::arg("threshold") = 0.5f);
  auto layout = py::class_<OwnedLayout>(module, "ParagraphLayout");
  layout
      .def(
          "draw",
          [](const OwnedLayout& value, BorrowedPen& pen,
             const Paragraph& paragraph,
             const std::optional<PaintStyle>& paint) {
            value.check(paragraph);
            value.draw(pen.get().canvas(), paragraph,
                       paint ? &*paint : nullptr);
          },
          py::arg("pen"), py::arg("paragraph"),
          py::arg("overridePaint") = py::none())
      .def(
          "drawBatched",
          [](const OwnedLayout& value, BorrowedPen& pen,
             const Paragraph& paragraph,
             const std::optional<PaintStyle>& paint) {
            value.check(paragraph);
            value.drawBatched(pen.get().canvas(), paragraph,
                              paint ? &*paint : nullptr);
          },
          py::arg("pen"), py::arg("paragraph"),
          py::arg("overridePaint") = py::none())
      .def(
          "lineMetrics",
          [](const OwnedLayout& l, const Paragraph& p) {
            l.check(p);
            return l.lineMetrics(p);
          },
          py::arg("paragraph"))
      .def(
          "columnMetrics",
          [](const OwnedLayout& l, const Paragraph& p) {
            l.check(p);
            return l.columnMetrics(p);
          },
          py::arg("paragraph"))
      .def("glyphOutline",
           [](const OwnedLayout& value) { return value.glyphOutline(); })
      .def("overflowed",
           [](const OwnedLayout& value) { return value.overflowed(); });
  layout.def_readonly("linePitch", &ParagraphLayout::linePitch);
  layout.def_readonly("lineCount", &ParagraphLayout::lineCount);
  layout.def_readonly("firstUnplacedWord", &ParagraphLayout::firstUnplacedWord);
  layout.def_readonly("ellipsized", &ParagraphLayout::ellipsized);
  layout.def_readonly("degradedBlocks", &ParagraphLayout::degradedBlocks);
  layout.def_readonly("reusedBlocks", &ParagraphLayout::reusedBlocks);
  layout.def_readonly("initial", &ParagraphLayout::initial);
  layout.def_readonly("intervals", &ParagraphLayout::intervals);
  layout.def_readonly("tangentRotationSteps",
                      &ParagraphLayout::tangentRotationSteps);
  module.def(
      "layoutParagraph",
      [](Fonts& fonts, Paragraph& p, FlowGeometry& flow,
         ParagraphLayoutOptions options, uint32_t firstWord,
         std::shared_ptr<const Hyphenator> hyphenator) {
        options.hyphenation.patterns = std::move(hyphenator);
        return OwnedLayout(
            p, layoutParagraph(fonts.get(), p, flow, options, firstWord));
      },
      py::arg("fonts"), py::arg("paragraph"), py::arg("geometry"),
      py::arg("options") = ParagraphLayoutOptions{}, py::arg("firstWord") = 0,
      py::arg("hyphenator") = nullptr, py::keep_alive<2, 6>());
  module.def(
      "layoutSingleLine",
      [](Fonts& fonts, Paragraph& p, py::handle origin,
         const PathTextOptions& path) {
        return OwnedLayout(
            p, layoutSingleLine(fonts.get(), p, point(origin), path));
      },
      py::arg("fonts"), py::arg("paragraph"), py::arg("baselineOrigin"),
      py::arg("pathText") = PathTextOptions{});
  module.def(
      "findAllOccurrences",
      [](const Paragraph& p, const std::u16string& needle,
         std::optional<CharRange> scope) {
        return scope ? findAllOccurrences(p, needle, *scope)
                     : findAllOccurrences(p, needle);
      },
      py::arg("paragraph"), py::arg("needle"), py::arg("scope") = py::none());
  module.def(
      "findRegexMatches",
      [](const Paragraph& p, const std::string& pattern,
         std::optional<CharRange> scope) {
        auto utf8 = std::u8string_view(
            reinterpret_cast<const char8_t*>(pattern.data()), pattern.size());
        auto result = scope ? findRegexMatches(p, utf8, *scope)
                            : findRegexMatches(p, utf8);
        if (!result) throw py::value_error("Invalid regular expression.");
        return *result;
      },
      py::arg("paragraph"), py::arg("pattern"), py::arg("scope") = py::none());
  module.def(
      "wordRanges",
      [](Paragraph& p, Fonts& fonts) { return wordRanges(p, fonts.get()); },
      py::arg("paragraph"), py::arg("fonts"));
  py::class_<MarkerSet>(module, "MarkerSet")
      .def(py::init<>())
      .def(py::init<const Paragraph&>(), py::arg("paragraph"))
      .def("setRanges", &MarkerSet::setRanges, py::arg("name"),
           py::arg("ranges"))
      .def(
          "rangesFor",
          [](const MarkerSet& m,
             const std::string& name) -> std::optional<std::vector<CharRange>> {
            if (auto p = m.rangesFor(name)) return *p;
            return {};
          },
          py::arg("name"))
      .def("remove", &MarkerSet::remove, py::arg("name"))
      .def("synchronize", &MarkerSet::synchronize, py::arg("paragraph"))
      .def("applyStyle", &MarkerSet::applyStyle, py::arg("paragraph"),
           py::arg("name"), py::arg("style"))
      .def("applyPaint", &MarkerSet::applyPaint, py::arg("paragraph"),
           py::arg("name"), py::arg("paint"));
  auto beside = bindRecord<Beside>(module, "Beside", "Unknown beside field: ");
  py::enum_<Beside::Side>(beside, "Side")
      .value("Before", Beside::Side::Before)
      .value("After", Beside::Side::After);
  beside
      .def_property(
          "base", [](const Beside& b) { return b.base; },
          [](Beside& b, py::handle value) { b.base = rect(value); })
      .def_readwrite("writingMode", &Beside::writingMode)
      .def_readwrite("side", &Beside::side)
      .def_readwrite("gap", &Beside::gap);
  module.def(
      "bandBeside",
      [](Fonts& fonts, const TextStyle& style, float gap) {
        return bandBeside(fonts.get(), style, gap);
      },
      py::arg("fonts"), py::arg("style"), py::arg("gap") = 0.0f);
  module.def(
      "layoutBeside",
      [](Fonts& fonts, Paragraph& reading, const Beside& beside) {
        return OwnedLayout(reading, layoutBeside(fonts.get(), reading, beside));
      },
      py::arg("fonts"), py::arg("reading"), py::arg("beside"));
  module.def(
      "shareOfReading",
      [](const std::u16string& reading, float here, float next) {
        return shareOfReading(reading, here, next);
      },
      py::arg("reading"), py::arg("here"), py::arg("next"));
  module.def(
      "warichuSplit",
      [](Fonts& fonts, Paragraph& note) {
        return weave::warichuSplit(fonts.get(), note);
      },
      py::arg("fonts"), py::arg("note"));
  module.def(
      "layoutWarichu",
      [](Fonts& fonts, Paragraph& note, py::handle slot, WritingMode mode) {
        return OwnedLayout(note,
                           layoutWarichu(fonts.get(), note, rect(slot), mode));
      },
      py::arg("fonts"), py::arg("note"), py::arg("slot"),
      py::arg("writingMode") = WritingMode::kHorizontal);
}
}  // namespace sigil::python
