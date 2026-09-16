#include "KitBindings.h"

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Board.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>

#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "Bindings.h"
#include "ComposeBindings.h"
#include "ValueBindings.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace composeKit = compose::kit;
namespace sketchKit = sigil::sketch::kit;

namespace {

template <class T>
struct Optional : std::false_type {};
template <class T>
struct Optional<std::optional<T>> : std::true_type {
  using Value = T;
};
template <class T>
struct Vector : std::false_type {};
template <class T, class Allocator>
struct Vector<std::vector<T, Allocator>> : std::true_type {};

template <class T>
T converted(py::handle value) {
  if constexpr (Optional<T>::value) {
    if (value.is_none()) return std::nullopt;
    return converted<typename Optional<T>::Value>(value);
  } else if constexpr (std::is_same_v<T, SkColor4f>) {
    return color(value);
  } else if constexpr (std::is_same_v<T, SkPoint>) {
    return point(value);
  } else if constexpr (std::is_same_v<T, SkSize>) {
    if (py::isinstance<SkSize>(value)) return py::cast<SkSize>(value);
    const auto pair = py::cast<std::array<float, 2>>(value);
    return {pair[0], pair[1]};
  } else if constexpr (std::is_same_v<T, compose::Dimension>) {
    return dimension(value);
  } else if constexpr (std::is_same_v<T, compose::Fill>) {
    return fill(value);
  } else if constexpr (std::is_same_v<T, compose::SurfacePaint>) {
    return surfacePaint(value);
  } else if constexpr (std::is_same_v<T, compose::Align>) {
    return alignment(value);
  } else if constexpr (std::is_same_v<T, compose::Justify>) {
    return justification(value);
  } else if constexpr (std::is_same_v<T, compose::Utf8>) {
    return py::cast<std::string>(value);
  } else {
    return py::cast<T>(value);
  }
}

template <class T>
py::class_<T> record(py::module_& module, const char* name) {
  return bindRecord<T>(module, name, "Unknown kit property: ");
}

template <class T, class M>
void field(py::class_<T>& type, const char* name, M T::* member) {
  if constexpr (std::is_same_v<M, compose::Utf8>) {
    type.def_property(
        name,
        [member](const T& value) {
          const auto& bytes = (value.*member).bytes();
          return std::string(bytes.begin(), bytes.end());
        },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        });
  } else if constexpr (Optional<M>::value || Vector<M>::value) {
    // Replaced payloads and resized vectors invalidate references into storage.
    // A saved Python child therefore belongs to an owned copy.
    type.def_property(
        name, [member](const T& value) -> M { return value.*member; },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        });
  } else {
    type.def_property(
        name, [member](T& value) -> M& { return value.*member; },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        },
        py::return_value_policy::reference_internal);
  }
}

template <class T>
void part(py::class_<T>& type, const char* name,
          composeKit::Part<compose::Utf8, T> T::* member) {
  type.def_property(
      name,
      [member](const T& value) {
        const auto draw = value.*member;
        return py::cpp_function([draw](const std::string& words,
                                       const T& props) {
          return draw ? draw(compose::Utf8(words), props) : compose::Element{};
        });
      },
      [member](T& value, py::object callable) {
        if (callable.is_none()) {
          value.*member = {};
          return;
        }
        if (!PyCallable_Check(callable.ptr()))
          throw py::type_error("A kit line part must be callable or None");
        const int arity = py::module_::import("sigil._loader")
                              .attr("arity")(callable, 2)
                              .template cast<int>();
        auto held =
            retainCallback(py::reinterpret_borrow<py::function>(callable));
        value.*
            member = [held, arity](const compose::Utf8& words, const T& props) {
          const py::gil_scoped_acquire lock;
          const KitScopeBoundary scopes;
          try {
            const auto& bytes = words.bytes();
            const std::string text(bytes.begin(), bytes.end());
            py::object result;
            if (arity == 0)
              result = held->get()();
            else if (arity == 1)
              result = held->get()(text);
            else
              result = held->get()(text, py::cast(T(props)));
            return result.template cast<compose::Element>();
          } catch (const py::error_already_set& error) {
            throw std::runtime_error(error.what());
          }
        };
      });
}

class ThemeProvider;
struct Providers {
  Providers() {
    // The native thread-local stack must outlive its provider owners at exit.
    (void)core::environment::capture();
  }
  std::vector<std::shared_ptr<ThemeProvider>> values;
  ~Providers();
};
thread_local Providers providers;

/** The active stack owns providers until they close on their original thread.
 *  A Python reference can move between threads without moving native scope. */
class ThemeProvider : public std::enable_shared_from_this<ThemeProvider> {
 public:
  explicit ThemeProvider(sketchKit::Theme theme) : m_theme(std::move(theme)) {}

  std::shared_ptr<ThemeProvider> enter() {
    if (m_used)
      throw std::runtime_error("A theme provider can only be entered once");
    m_used = true;
    m_thread = std::this_thread::get_id();
    m_scope = std::make_unique<sketchKit::Provide>(m_theme);
    const auto snapshot = core::environment::capture();
    m_depth = snapshot.size();
    m_identity = snapshot.back().value.get();
    providers.values.push_back(shared_from_this());
    return shared_from_this();
  }

  void close() {
    if (!m_scope) return;
    if (m_thread != std::this_thread::get_id())
      throw std::runtime_error(
          "A theme provider must close on its owning thread");
    const auto snapshot = core::environment::capture();
    if (providers.values.empty() || providers.values.back().get() != this ||
        snapshot.size() != m_depth || snapshot.back().value.get() != m_identity)
      throw std::runtime_error(
          "Theme providers must close in reverse nesting order");
    reset();
    providers.values.pop_back();
  }

  void reset() { m_scope.reset(); }
  bool active() const { return m_scope != nullptr; }

 private:
  sketchKit::Theme m_theme;
  bool m_used = false;
  std::thread::id m_thread;
  std::unique_ptr<sketchKit::Provide> m_scope;
  std::size_t m_depth = 0;
  const void* m_identity = nullptr;
};

void unwind(std::size_t depth) {
  while (providers.values.size() > depth) {
    providers.values.back()->reset();
    providers.values.pop_back();
  }
}
Providers::~Providers() { unwind(0); }

void bindTheme(py::module_& module) {
  auto palette = record<sketchKit::Palette>(module, "Palette");
  field(palette, "ground", &sketchKit::Palette::ground);
  field(palette, "cellGround", &sketchKit::Palette::cellGround);
  field(palette, "ink", &sketchKit::Palette::ink);
  field(palette, "ash", &sketchKit::Palette::ash);
  field(palette, "rule", &sketchKit::Palette::rule);
  field(palette, "figure", &sketchKit::Palette::figure);
  palette.def(py::self == py::self);

  auto line = record<sketchKit::Register>(module, "Register");
  field(line, "size", &sketchKit::Register::size);
  field(line, "track", &sketchKit::Register::track);
  field(line, "mono", &sketchKit::Register::mono);
  field(line, "face", &sketchKit::Register::face);
  line.def(py::self == py::self);

  auto type = record<sketchKit::TypeScale>(module, "TypeScale");
  field(type, "title", &sketchKit::TypeScale::title);
  field(type, "subtitle", &sketchKit::TypeScale::subtitle);
  field(type, "footer", &sketchKit::TypeScale::footer);
  field(type, "captionLabel", &sketchKit::TypeScale::captionLabel);
  field(type, "captionNote", &sketchKit::TypeScale::captionNote);
  field(type, "eyebrow", &sketchKit::TypeScale::eyebrow);
  field(type, "section", &sketchKit::TypeScale::section);
  field(type, "sans", &sketchKit::TypeScale::sans);
  field(type, "mono", &sketchKit::TypeScale::mono);
  type.def(py::self == py::self);

  auto spacing = record<sketchKit::Spacing>(module, "Spacing");
#define SPACING(name) field(spacing, #name, &sketchKit::Spacing::name)
  SPACING(marginX);
  SPACING(marginTop);
  SPACING(marginBottom);
  SPACING(subtitleGap);
  SPACING(contentGap);
  SPACING(captionGap);
  SPACING(captionNoteGap);
  SPACING(cellGap);
  SPACING(wellPadding);
  SPACING(rowGap);
  SPACING(labelGap);
  SPACING(chipPaddingX);
  SPACING(chipPaddingY);
  SPACING(panelPadding);
  SPACING(swatchSide);
  SPACING(barHeight);
  SPACING(panelCorners);
  SPACING(screenCorners);
  SPACING(bezel);
  SPACING(chipCorners);
  SPACING(tickReach);
#undef SPACING
  spacing.def(py::self == py::self);

  auto theme = record<sketchKit::Theme>(module, "Theme");
  field(theme, "palette", &sketchKit::Theme::palette);
  field(theme, "type", &sketchKit::Theme::type);
  field(theme, "spacing", &sketchKit::Theme::spacing);
  field(theme, "captionWhere", &sketchKit::Theme::captionWhere);
  theme.def(py::self == py::self)
      .def("font", py::overload_cast<const sketchKit::Register&>(
                       &sketchKit::Theme::font, py::const_))
      .def("font",
           [](const sketchKit::Theme& value, const sketchKit::Register& line,
              py::handle ink) { return value.font(line, color(ink)); })
      .def("styleSheet", &sketchKit::Theme::styleSheet)
      .def("voice", &sketchKit::Theme::voice)
      .def("style",
           [](const sketchKit::Theme& value, const sketchKit::Register& line,
              py::handle ink) { return value.style(line, fill(ink)); });
  py::enum_<sketchKit::Voice>(module, "Voice")
      .value("Book", sketchKit::Voice::Book)
      .value("Terminal", sketchKit::Voice::Terminal)
      .value("Interface", sketchKit::Voice::Interface);
  module.def(
      "houseFace",
      [](sketchKit::Voice voice, int weight, bool italic) {
        return sketchKit::houseFace(
            voice, weight,
            italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
      },
      py::arg("voice"), py::arg("weight") = 400, py::arg("italic") = false);
  module.def("houseTheme", [] { return sketchKit::houseTheme(); });
  module.def("theme", [] { return sketchKit::theme(); });
  py::class_<ThemeProvider, std::shared_ptr<ThemeProvider>>(module, "Provide")
      .def(py::init<sketchKit::Theme>(), py::arg("theme"))
      .def("__enter__", &ThemeProvider::enter)
      .def("__exit__",
           [](ThemeProvider& scope, py::object, py::object, py::object) {
             scope.close();
             return false;
           })
      .def("close", &ThemeProvider::close)
      .def_property_readonly("active", &ThemeProvider::active);
}

template <class Well>
void wellFields(py::class_<Well>& type) {
  field(type, "width", &Well::width);
  field(type, "height", &Well::height);
  field(type, "ground", &Well::ground);
  field(type, "padding", &Well::padding);
  field(type, "paddingY", &Well::paddingY);
  field(type, "clip", &Well::clip);
  field(type, "corners", &Well::corners);
  field(type, "keyline", &Well::keyline);
  field(type, "keylineWidth", &Well::keylineWidth);
  field(type, "placed", &Well::placed);
  field(type, "content", &Well::content);
}

template <class Content>
void contentType(py::module_& module, const char* name) {
  auto type = record<Content>(module, name);
  field(type, "across", &Content::across);
  field(type, "down", &Content::down);
}

void bindComposeKit(py::module_& module) {
  py::enum_<composeKit::Caption::Where>(module, "CaptionWhere")
      .value("Split", composeKit::Caption::Where::Split)
      .value("Above", composeKit::Caption::Where::Above)
      .value("Below", composeKit::Caption::Where::Below);
  contentType<composeKit::Well::Content>(module, "WellContent");
  auto well = record<composeKit::Well>(module, "Well");
  wellFields(well);
  auto caption = record<composeKit::Caption>(module, "Caption");
  field(caption, "where", &composeKit::Caption::where);
  field(caption, "gap", &composeKit::Caption::gap);
  field(caption, "noteGap", &composeKit::Caption::noteGap);
  field(caption, "labelMeasure", &composeKit::Caption::labelMeasure);
  field(caption, "noteMeasure", &composeKit::Caption::noteMeasure);
  field(caption, "align", &composeKit::Caption::align);
  field(caption, "justify", &composeKit::Caption::justify);
  field(caption, "body", &composeKit::Caption::body);
  field(caption, "reading", &composeKit::Caption::reading);
  part(caption, "label", &composeKit::Caption::label);
  part(caption, "note", &composeKit::Caption::note);
  part(caption, "readingLine", &composeKit::Caption::readingLine);
  auto cells = record<composeKit::Cells>(module, "Cells");
  field(cells, "cells", &composeKit::Cells::cells);
  field(cells, "column", &composeKit::Cells::column);
  field(cells, "gap", &composeKit::Cells::gap);
  field(cells, "divider", &composeKit::Cells::divider);
  field(cells, "dividerWidth", &composeKit::Cells::dividerWidth);
  field(cells, "align", &composeKit::Cells::align);
  auto grid = record<composeKit::PanelGrid>(module, "PanelGrid");
  field(grid, "cells", &composeKit::PanelGrid::cells);
  field(grid, "columns", &composeKit::PanelGrid::columns);
  field(grid, "gap", &composeKit::PanelGrid::gap);
  field(grid, "rowGap", &composeKit::PanelGrid::rowGap);
  field(grid, "divider", &composeKit::PanelGrid::divider);
  field(grid, "dividerWidth", &composeKit::PanelGrid::dividerWidth);
  field(grid, "align", &composeKit::PanelGrid::align);
  field(grid, "measure", &composeKit::PanelGrid::measure);
  auto sheet = record<composeKit::Sheet>(module, "Sheet");
  field(sheet, "title", &composeKit::Sheet::title);
  field(sheet, "subtitle", &composeKit::Sheet::subtitle);
  field(sheet, "footer", &composeKit::Sheet::footer);
  field(sheet, "marginX", &composeKit::Sheet::marginX);
  field(sheet, "marginTop", &composeKit::Sheet::marginTop);
  field(sheet, "marginBottom", &composeKit::Sheet::marginBottom);
  field(sheet, "subtitleGap", &composeKit::Sheet::subtitleGap);
  field(sheet, "contentGap", &composeKit::Sheet::contentGap);
  field(sheet, "ground", &composeKit::Sheet::ground);
  field(sheet, "rule", &composeKit::Sheet::rule);
  field(sheet, "ruleWidth", &composeKit::Sheet::ruleWidth);
  field(sheet, "key", &composeKit::Sheet::key);
  part(sheet, "titleLine", &composeKit::Sheet::titleLine);
  part(sheet, "subtitleLine", &composeKit::Sheet::subtitleLine);
  part(sheet, "footerLine", &composeKit::Sheet::footerLine);
  auto board = record<composeKit::Board>(module, "Board");
  field(board, "size", &composeKit::Board::size);
  field(board, "ground", &composeKit::Board::ground);
  auto panel = record<composeKit::Panel>(module, "Panel");
  field(panel, "eyebrow", &composeKit::Panel::eyebrow);
  field(panel, "title", &composeKit::Panel::title);
  field(panel, "note", &composeKit::Panel::note);
  field(panel, "rule", &composeKit::Panel::rule);
  field(panel, "ruleWidth", &composeKit::Panel::ruleWidth);
  field(panel, "gap", &composeKit::Panel::gap);
  field(panel, "titleGap", &composeKit::Panel::titleGap);
  field(panel, "body", &composeKit::Panel::body);
  part(panel, "eyebrowLine", &composeKit::Panel::eyebrowLine);
  part(panel, "titleLine", &composeKit::Panel::titleLine);
  part(panel, "noteLine", &composeKit::Panel::noteLine);
  module.def("well",
             py::overload_cast<const composeKit::Well&>(&composeKit::well));
  module.def("well",
             py::overload_cast<const composeKit::Well&, compose::Element>(
                 &composeKit::well));
  module.def("cell", [](const composeKit::Caption& voice, std::string label,
                        std::string note, compose::Element body) {
    return composeKit::cell(voice, label, note, std::move(body));
  });
  module.def("cells", &composeKit::cells);
  module.def("panelGrid", &composeKit::panelGrid);
  module.def("sheet", &composeKit::sheet);
  module.def("board", &composeKit::board);
  module.def("panel", &composeKit::panel);
  module.def("captionLabel",
             [](std::string text) { return composeKit::captionLabel(text); });
  module.def("captionNote",
             [](std::string text) { return composeKit::captionNote(text); });
  module.def("figure",
             [](std::string text) { return composeKit::figure(text); });

  auto companion = record<composeKit::Line::Companion>(module, "LineCompanion");
  field(companion, "thickness", &composeKit::Line::Companion::thickness);
  field(companion, "gap", &composeKit::Line::Companion::gap);
  field(companion, "fill", &composeKit::Line::Companion::fill);
  field(companion, "dash", &composeKit::Line::Companion::dash);
  auto line = record<composeKit::Line>(module, "Line");
  field(line, "length", &composeKit::Line::length);
  field(line, "thickness", &composeKit::Line::thickness);
  field(line, "column", &composeKit::Line::column);
  field(line, "fill", &composeKit::Line::fill);
  field(line, "inset", &composeKit::Line::inset);
  field(line, "pair", &composeKit::Line::pair);
  auto ladder = record<composeKit::Ladder>(module, "Ladder");
  field(ladder, "count", &composeKit::Ladder::count);
  field(ladder, "pitch", &composeKit::Ladder::pitch);
  field(ladder, "thickness", &composeKit::Ladder::thickness);
  field(ladder, "column", &composeKit::Ladder::column);
  field(ladder, "fill", &composeKit::Ladder::fill);
  module.def("line", &composeKit::line);
  module.def("ladder", &composeKit::ladder);
  module.def("centred", py::overload_cast<>(&composeKit::centred));
  module.def("centred",
             py::overload_cast<compose::Element>(&composeKit::centred));
  module.def("at", [](float x, float y, py::handle w, py::handle h) {
    return composeKit::at(x, y, dimension(w), dimension(h));
  });
  module.def("at", [](compose::Element element, float x, float y, py::handle w,
                      py::handle h) {
    return composeKit::at(std::move(element), x, y, dimension(w), dimension(h));
  });
  module.def("disc", [](py::handle centre, float radius) {
    return composeKit::disc(point(centre), radius);
  });
  module.def("dot", [](py::handle centre, float radius, py::handle ink) {
    return composeKit::dot(point(centre), radius, surfacePaint(ink));
  });
  module.def("ring",
             [](py::handle centre, float radius, compose::Decoration pen) {
               return composeKit::ring(point(centre), radius, std::move(pen));
             });
}

void bindSketchKit(py::module_& module) {
  bindTheme(module);
  contentType<sketchKit::Well::Content>(module, "WellContent");
  auto recess = record<sketchKit::Well::Recess>(module, "Recess");
  field(recess, "shade", &sketchKit::Well::Recess::shade);
  field(recess, "offset", &sketchKit::Well::Recess::offset);
  field(recess, "blur", &sketchKit::Well::Recess::blur);
  field(recess, "lipLight", &sketchKit::Well::Recess::lipLight);
  field(recess, "lipDark", &sketchKit::Well::Recess::lipDark);
  field(recess, "lipWidth", &sketchKit::Well::Recess::lipWidth);
  auto relief = record<sketchKit::Well::Relief>(module, "Relief");
  field(relief, "depth", &sketchKit::Well::Relief::depth);
  field(relief, "blur", &sketchKit::Well::Relief::blur);
  field(relief, "angleDeg", &sketchKit::Well::Relief::angleDeg);
  field(relief, "light", &sketchKit::Well::Relief::light);
  field(relief, "shade", &sketchKit::Well::Relief::shade);
  auto well = record<sketchKit::Well>(module, "Well");
  wellFields(well);
  field(well, "recess", &sketchKit::Well::recess);
  field(well, "relief", &sketchKit::Well::relief);
  auto stage = record<sketchKit::Stage>(module, "Stage");
  field(stage, "size", &sketchKit::Stage::size);
  field(stage, "captureAt", &sketchKit::Stage::captureAt);
  field(stage, "background", &sketchKit::Stage::background);
  field(stage, "oversample", &sketchKit::Stage::oversample);
  field(stage, "plateOnly", &sketchKit::Stage::plateOnly);
  field(stage, "nonlinearPicture", &sketchKit::Stage::nonlinearPicture);
  auto page = record<sketchKit::Page>(module, "Page");
  field(page, "title", &sketchKit::Page::title);
  field(page, "subtitle", &sketchKit::Page::subtitle);
  field(page, "footer", &sketchKit::Page::footer);
  field(page, "ruled", &sketchKit::Page::ruled);
  field(page, "ground", &sketchKit::Page::ground);
  field(page, "key", &sketchKit::Page::key);
  auto cell = record<sketchKit::Cell>(module, "Cell");
  field(cell, "plate", &sketchKit::Cell::plate);
  field(cell, "measure", &sketchKit::Cell::measure);
  auto run = record<sketchKit::Run>(module, "Run");
  field(run, "cells", &sketchKit::Run::cells);
  field(run, "column", &sketchKit::Run::column);
  field(run, "gap", &sketchKit::Run::gap);
  field(run, "ruled", &sketchKit::Run::ruled);
  field(run, "align", &sketchKit::Run::align);
  auto grid = record<sketchKit::PanelGrid>(module, "PanelGrid");
  field(grid, "cells", &sketchKit::PanelGrid::cells);
  field(grid, "columns", &sketchKit::PanelGrid::columns);
  field(grid, "gap", &sketchKit::PanelGrid::gap);
  field(grid, "rowGap", &sketchKit::PanelGrid::rowGap);
  field(grid, "ruled", &sketchKit::PanelGrid::ruled);
  field(grid, "align", &sketchKit::PanelGrid::align);
  field(grid, "measure", &sketchKit::PanelGrid::measure);
  module.def("stage", &stageContext);
  module.def("page", &sketchKit::page);
  module.def("well",
             py::overload_cast<const sketchKit::Well&>(&sketchKit::well));
  module.def("well",
             py::overload_cast<const sketchKit::Well&, compose::Element>(
                 &sketchKit::well));
  module.def("caption", [](float measure, std::string label, std::string note,
                           compose::Element body) {
    return sketchKit::caption(measure, label, note, std::move(body));
  });
  module.def("cell", [](const sketchKit::Cell& plate, std::string label,
                        std::string note, compose::Element picture) {
    return sketchKit::cell(plate, label, note, std::move(picture));
  });
  module.def("cells", &sketchKit::cells);
  module.def("panelGrid", &sketchKit::panelGrid);
}

template <class Scheme>
void layoutFunction(py::module_& compose) {
  compose.def("layout", [](Scheme scheme) {
    return sigil::compose::layout(std::move(scheme));
  });
}

void bindLayouts(py::module_& compose) {
  namespace layouts = sigil::compose::layouts;
  auto module = compose.def_submodule("layouts");
  auto track = record<layouts::Track>(module, "Track");
  py::enum_<layouts::Track::Kind>(module, "TrackKind")
      .value("Fixed", layouts::Track::Kind::Fixed)
      .value("Content", layouts::Track::Kind::Content)
      .value("Fraction", layouts::Track::Kind::Fraction);
  field(track, "minKind", &layouts::Track::minKind);
  field(track, "minValue", &layouts::Track::minValue);
  field(track, "maxKind", &layouts::Track::maxKind);
  field(track, "maxValue", &layouts::Track::maxValue);
  track.def(py::self == py::self);
  module.def("px", &layouts::px);
  module.def("content", &layouts::content);
  module.def("fr", &layouts::fr, py::arg("weight") = 1.0f);
  module.def("minmax", &layouts::minmax);
  module.def("repeatTrack", &layouts::repeatTrack);
  auto grid = record<layouts::Grid>(module, "Grid");
  field(grid, "columns", &layouts::Grid::columns);
  field(grid, "rows", &layouts::Grid::rows);
  field(grid, "areas", &layouts::Grid::areas);
  field(grid, "gap", &layouts::Grid::gap);
  field(grid, "dense", &layouts::Grid::dense);
  field(grid, "across", &layouts::Grid::across);
  field(grid, "down", &layouts::Grid::down);
  grid.def(py::self == py::self);
  auto radial = record<layouts::Radial>(module, "Radial");
  field(radial, "radiusFraction", &layouts::Radial::radiusFraction);
  field(radial, "startDeg", &layouts::Radial::startDeg);
  field(radial, "sweepDeg", &layouts::Radial::sweepDeg);
  field(radial, "radiusAt", &layouts::Radial::radiusAt);
  auto diagonal = record<layouts::Diagonal>(module, "Diagonal");
  py::enum_<layouts::Diagonal::Anchor>(module, "DiagonalAnchor")
      .value("Start", layouts::Diagonal::Anchor::Start)
      .value("End", layouts::Diagonal::Anchor::End);
  field(diagonal, "skewDeg", &layouts::Diagonal::skewDeg);
  field(diagonal, "gap", &layouts::Diagonal::gap);
  field(diagonal, "anchor", &layouts::Diagonal::anchor);
  auto baseline = record<layouts::BaselineGrid>(module, "BaselineGrid");
  field(baseline, "rhythm", &layouts::BaselineGrid::rhythm);
  field(baseline, "offset", &layouts::BaselineGrid::offset);
  field(baseline, "gap", &layouts::BaselineGrid::gap);
  auto jittered = record<layouts::Jittered>(module, "Jittered");
  field(jittered, "seed", &layouts::Jittered::seed);
  field(jittered, "jitter", &layouts::Jittered::jitter);
  auto along = record<layouts::AlongPath>(module, "AlongPath");
  along.def_property(
      "path",
      [](const layouts::AlongPath& value) {
        return py::cpp_function([path = value.path](SkSize size) {
          return path ? path(size) : SkPath{};
        });
      },
      [](layouts::AlongPath& value, py::handle path) {
        const compose::Shape outline = shape(path);
        value.path = [outline](SkSize size) { return outline(size); };
      });
  field(along, "startFraction", &layouts::AlongPath::startFraction);
  field(along, "endFraction", &layouts::AlongPath::endFraction);
  layoutFunction<layouts::Radial>(compose);
  layoutFunction<layouts::Grid>(compose);
  layoutFunction<layouts::Diagonal>(compose);
  layoutFunction<layouts::BaselineGrid>(compose);
  layoutFunction<layouts::Jittered>(compose);
  layoutFunction<layouts::AlongPath>(compose);
}

}  // namespace

KitScopeBoundary::KitScopeBoundary() : m_depth(providers.values.size()) {}
KitScopeBoundary::~KitScopeBoundary() { unwind(m_depth); }

void bindKits(py::module_& root) {
  auto compose = root.attr("compose").cast<py::module_>();
  auto components = compose.def_submodule("kit");
  bindComposeKit(components);
  bindLayouts(compose);
  auto sketch = root.def_submodule("sketch");
  auto specimens = sketch.def_submodule("kit");
  bindSketchKit(specimens);
}

}  // namespace sigil::sketch::python
