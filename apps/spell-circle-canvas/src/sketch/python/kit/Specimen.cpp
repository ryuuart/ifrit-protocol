

#include <pybind11/operators.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>

#include <utility>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
namespace {
using sigil::python::kit::contentType;
using sigil::python::kit::field;
using sigil::python::kit::record;
using sigil::python::kit::wellFields;

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
  field(type, "control", &sketchKit::TypeScale::control);
  field(type, "sans", &sketchKit::TypeScale::sans);
  field(type, "mono", &sketchKit::TypeScale::mono);
  type.def(py::self == py::self);

  auto spacing = record<sketchKit::Spacing>(module, "Spacing");
#define SPACING(name) field(spacing, #name, &sketchKit::Spacing::name)
  SPACING(marginHorizontal);
  SPACING(marginTop);
  SPACING(marginBottom);
  SPACING(subtitleGap);
  SPACING(contentGap);
  SPACING(captionGap);
  SPACING(captionNoteGap);
  SPACING(cellGap);
  SPACING(comparisonGap);
  SPACING(sectionGap);
  SPACING(wellPadding);
  SPACING(rowGap);
  SPACING(labelGap);
  SPACING(chipPaddingX);
  SPACING(chipPaddingY);
  SPACING(panelPadding);
  SPACING(swatchSide);
  SPACING(barHeight);
  SPACING(tickerGap);
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
      .def("font",
           py::overload_cast<const sketchKit::Register&>(
               &sketchKit::Theme::font, py::const_),
           py::arg("line"))
      .def(
          "font",
          [](const sketchKit::Theme& value, const sketchKit::Register& line,
             py::handle ink) {
            return value.font(line, sigil::python::color(ink));
          },
          py::arg("line"), py::arg("ink"))
      .def("styleSheet", &sketchKit::Theme::styleSheet)
      .def("properties", &sketchKit::Theme::properties,
           "The spacing and the palette alone, as custom properties on the "
           "node the sheet is applied on, with none of the role rules.")
      .def(
          "sans",
          [](const sketchKit::Theme& value, float size, py::handle color,
             float track) {
            return value.sans(size, sigil::python::color(color), track);
          },
          py::arg("size"), py::arg("color"), py::arg("track") = 0.0f,
          "A line in the sheet's sans face at an arbitrary size, for the "
          "text none of the registers covers.")
      .def(
          "mono",
          [](const sketchKit::Theme& value, float size, py::handle color,
             float track) {
            return value.mono(size, sigil::python::color(color), track);
          },
          py::arg("size"), py::arg("color"), py::arg("track") = 0.0f,
          "The same in the sheet's mono face.")
      .def("voice", &sketchKit::Theme::voice, py::arg("noteMeasure"))
      .def(
          "style",
          [](const sketchKit::Theme& value, const sketchKit::Register& line,
             py::handle ink) {
            return value.style(line, sigil::python::fill(ink));
          },
          py::arg("line"), py::arg("ink"));
  py::enum_<sketchKit::Voice>(module, "Voice")
      .value("Book", sketchKit::Voice::Book)
      .value("Terminal", sketchKit::Voice::Terminal)
      .value("Interface", sketchKit::Voice::Interface);
  module.def("houseFace", &sketchKit::houseFace, py::arg("voice"),
             py::arg("weight") = 400,
             py::arg("slant") = weave::FaceSlant::Upright);
  module.def("houseTheme", [] { return sketchKit::houseTheme(); });
  module.def("studyTheme", [] { return sketchKit::studyTheme(); });
  module.def("specimenTheme", [] { return sketchKit::specimenTheme(); });
  py::enum_<sketchKit::Density>(module, "Density")
      .value("Compact", sketchKit::Density::Compact)
      .value("Spacious", sketchKit::Density::Spacious);
  module.def(
      "featureTheme",
      [](sketchKit::Density density) {
        return sketchKit::featureTheme(density);
      },
      py::arg("density") = sketchKit::Density::Compact);
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
  field(relief, "angleDegrees", &sketchKit::Well::Relief::angleDegrees);
  field(relief, "light", &sketchKit::Well::Relief::light);
  field(relief, "shade", &sketchKit::Well::Relief::shade);
  auto well = record<sketchKit::Well>(module, "Well");
  wellFields(well);
  field(well, "recess", &sketchKit::Well::recess);
  field(well, "relief", &sketchKit::Well::relief);
  auto stage = record<sketchKit::Stage>(module, "Stage");
  field(stage, "theme", &sketchKit::Stage::theme);
  field(stage, "sheet", &sketchKit::Stage::sheet);
  field(stage, "size", &sketchKit::Stage::size);
  field(stage, "captureAt", &sketchKit::Stage::captureAt);
  field(stage, "background", &sketchKit::Stage::background);
  field(stage, "oversample", &sketchKit::Stage::oversample);
  field(stage, "plateOnly", &sketchKit::Stage::plateOnly);
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
  field(cell, "where", &sketchKit::Cell::where);
  auto comparisonCase =
      record<sketchKit::ComparisonCase>(module, "ComparisonCase");
  field(comparisonCase, "title", &sketchKit::ComparisonCase::title);
  field(comparisonCase, "control", &sketchKit::ComparisonCase::control);
  field(comparisonCase, "figure", &sketchKit::ComparisonCase::figure);
  field(comparisonCase, "note", &sketchKit::ComparisonCase::note);
  auto comparison = record<sketchKit::Comparison>(module, "Comparison");
  field(comparison, "cases", &sketchKit::Comparison::cases);
  field(comparison, "measure", &sketchKit::Comparison::measure);
  field(comparison, "gap", &sketchKit::Comparison::gap);
  field(comparison, "trackGap", &sketchKit::Comparison::trackGap);
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
  module.def("stage", &stageContext, py::arg("context"), py::arg("stage"));
  module.def("page", &sketchKit::page, py::arg("sheet"), py::arg("content"));
  module.def("well",
             py::overload_cast<const sketchKit::Well&>(&sketchKit::well),
             py::arg("specification"));
  module.def("well",
             py::overload_cast<const sketchKit::Well&, compose::Element>(
                 &sketchKit::well),
             py::arg("specification"), py::arg("surface"));
  module.def(
      "caption",
      [](float measure, std::string label, std::string note,
         compose::Element body, compose::kit::Caption::Where where) {
        return sketchKit::caption(measure, label, note, std::move(body), where);
      },
      py::arg("measure"), py::arg("label"), py::arg("note"), py::arg("body"),
      py::arg("where") = compose::kit::Caption::Where::Split);
  module.def(
      "cell",
      [](const sketchKit::Cell& plate, std::string label, std::string note,
         compose::Element picture) {
        return sketchKit::cell(plate, label, note, std::move(picture));
      },
      py::arg("plate"), py::arg("label"), py::arg("note"), py::arg("picture"));
  module.def("comparison", &sketchKit::comparison, py::arg("comparison"));
  module.def("cells", &sketchKit::cells, py::arg("run"));
  module.def("panelGrid", &sketchKit::panelGrid, py::arg("grid"));
}

}  // namespace

void bindSketchKit(py::module_& root) {
  auto sketch = root.attr("sketch").cast<py::module_>();
  auto specimens = sketch.def_submodule("kit");
  bindSpecimens(specimens);
}

}  // namespace sigil::sketch::python
