#include <pybind11/operators.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Board.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilpython/KitBindings.h>

namespace sigil::python {
namespace py = pybind11;
namespace composeKit = compose::kit;
namespace {
using kit::contentType;
using kit::field;
using kit::record;
using kit::wellFields;

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
        const int arity = py::module_::import("sigil._callbacks")
                              .attr("arity")(callable, 2)
                              .template cast<int>();
        auto held =
            retainCallback(py::reinterpret_borrow<py::function>(callable));
        value.*
            member = [held, arity](const compose::Utf8& words, const T& props) {
          const py::gil_scoped_acquire lock;
          const CallbackBoundary scopes;
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

void bindComponents(py::module_& module) {
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
             py::overload_cast<const composeKit::Well&>(&composeKit::well),
             py::arg("specification"));
  module.def("well",
             py::overload_cast<const composeKit::Well&, compose::Element>(
                 &composeKit::well),
             py::arg("specification"), py::arg("surface"));
  module.def(
      "cell",
      [](const composeKit::Caption& voice, std::string label, std::string note,
         compose::Element body) {
        return composeKit::cell(voice, label, note, std::move(body));
      },
      py::arg("voice"), py::arg("label"), py::arg("note"), py::arg("body"));
  module.def("cells", &composeKit::cells, py::arg("run"));
  module.def("panelGrid", &composeKit::panelGrid, py::arg("grid"));
  module.def("sheet", &composeKit::sheet, py::arg("page"), py::arg("content"));
  module.def("board", &composeKit::board, py::arg("board"));
  module.def("panel", &composeKit::panel, py::arg("region"),
             py::arg("content"));
  module.def(
      "captionLabel",
      [](std::string text) { return composeKit::captionLabel(text); },
      py::arg("text"));
  module.def(
      "captionNote",
      [](std::string text) { return composeKit::captionNote(text); },
      py::arg("text"));
  module.def(
      "figure", [](std::string text) { return composeKit::figure(text); },
      py::arg("text"));

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
  module.def("line", &composeKit::line, py::arg("mark"));
  module.def("ladder", &composeKit::ladder, py::arg("rungs"));
  module.def("centred", py::overload_cast<>(&composeKit::centred));
  module.def("centred",
             py::overload_cast<compose::Element>(&composeKit::centred),
             py::arg("child"));
  module.def(
      "at",
      [](float x, float y, py::handle w, py::handle h) {
        return composeKit::at(x, y, dimension(w), dimension(h));
      },
      py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"));
  module.def(
      "at",
      [](compose::Element element, float x, float y, py::handle w,
         py::handle h) {
        return composeKit::at(std::move(element), x, y, dimension(w),
                              dimension(h));
      },
      py::arg("element"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"));
  module.def(
      "disc",
      [](py::handle centre, float radius) {
        return composeKit::disc(point(centre), radius);
      },
      py::arg("centre"), py::arg("radius"));
  module.def(
      "dot",
      [](py::handle centre, float radius, py::handle ink) {
        return composeKit::dot(point(centre), radius, surfacePaint(ink));
      },
      py::arg("centre"), py::arg("radius"), py::arg("ink"));
  module.def(
      "ring",
      [](py::handle centre, float radius, compose::Decoration pen) {
        return composeKit::ring(point(centre), radius, std::move(pen));
      },
      py::arg("centre"), py::arg("radius"), py::arg("pen"));
}

template <class Scheme>
void layoutFunction(py::module_& compose) {
  compose.def(
      "layout",
      [](Scheme scheme, py::args children) {
        return sigil::compose::layout(std::move(scheme))
            .children(elements(children));
      },
      py::arg("scheme"));
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
  module.def("px", &layouts::px, py::arg("pixels"));
  module.def("content", &layouts::content);
  module.def("fr", &layouts::fr, py::arg("weight") = 1.0f);
  module.def("minmax", &layouts::minmax, py::arg("minimum"),
             py::arg("maximum"));
  module.def("repeatTrack", &layouts::repeatTrack, py::arg("count"),
             py::arg("track"));
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

void bindComposeKit(py::module_& root) {
  auto compose = root.attr("compose").cast<py::module_>();
  auto components = compose.def_submodule("kit");
  bindComponents(components);
  bindLayouts(compose);
}

}  // namespace sigil::python
