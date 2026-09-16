#include "KitBindings.h"

#include <pybind11/operators.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilpython/KitBindings.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>

#include <memory>
#include <stdexcept>
#include <thread>

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
namespace {
using sigil::python::kit::contentType;
using sigil::python::kit::field;
using sigil::python::kit::record;
using sigil::python::kit::wellFields;

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
    m_restoreIdentity = core::environment::restoreIdentity();
    sigil::python::retainScope(shared_from_this(), [](void* scope) noexcept {
      static_cast<ThemeProvider*>(scope)->reset();
    });
    return shared_from_this();
  }

  void close() {
    if (!m_scope) return;
    if (m_thread != std::this_thread::get_id())
      throw std::runtime_error(
          "A theme provider must close on its owning thread");
    if (core::environment::restoreIdentity() != m_restoreIdentity)
      throw std::runtime_error(
          "A theme provider must close in its owning environment");
    const auto snapshot = core::environment::capture();
    if (!sigil::python::isCurrentScope(this) || snapshot.size() != m_depth ||
        snapshot.back().value.get() != m_identity)
      throw std::runtime_error(
          "Theme providers must close in reverse nesting order");
    sigil::python::closeScope(this);
  }

  void reset() noexcept { m_scope.reset(); }
  bool active() const { return m_scope != nullptr; }

 private:
  sketchKit::Theme m_theme;
  bool m_used = false;
  std::thread::id m_thread;
  std::unique_ptr<sketchKit::Provide> m_scope;
  std::size_t m_depth = 0;
  const void* m_identity = nullptr;
  std::uint64_t m_restoreIdentity = 0;
};

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
              py::handle ink) {
             return value.font(line, sigil::python::color(ink));
           })
      .def("styleSheet", &sketchKit::Theme::styleSheet)
      .def("voice", &sketchKit::Theme::voice)
      .def("style", [](const sketchKit::Theme& value,
                       const sketchKit::Register& line, py::handle ink) {
        return value.style(line, sigil::python::fill(ink));
      });
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
  module.def("studyTheme", [] { return sketchKit::studyTheme(); });
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

void bindSpecimens(py::module_& module) {
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

}  // namespace

void bindSketchKit(py::module_& root) {
  auto sketch = root.attr("sketch").cast<py::module_>();
  auto specimens = sketch.def_submodule("kit");
  bindSpecimens(specimens);
}

}  // namespace sigil::sketch::python
