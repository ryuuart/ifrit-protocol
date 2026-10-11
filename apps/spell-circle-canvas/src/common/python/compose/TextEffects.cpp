#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/typography/Entrance.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilcompose/typography/Track.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace sigil::python {

namespace {
namespace py = pybind11;
using compose::Track;
using compose::textFx::Displaced;

/** The track a text leaf's glyphs run on: which of them, by what unit,
 *  on what schedule, and the master progress the whole track is read
 *  at. Its effect is the entrance it was made by. */
void bindTrack(py::module_& composition) {
  py::enum_<compose::Beats>(
      composition, "Beats",
      "What a track's stagger counts over: the glyphs it selects, or every "
      "glyph of the text.")
      .value("Selection", compose::Beats::Selection)
      .value("Text", compose::Beats::Text);
  auto track =
      bindRecord<Track>(composition, "Track", "A track has no field named ");
  track.doc() =
      "Which glyphs of a text leaf move, by what unit, on what schedule, "
      "and how far along they are: `progress` is the master reading, one "
      "for every glyph at rest, and binds a motion value.";
  track.def_readwrite("where", &Track::where)
      .def_readwrite("tween", &Track::tween)
      .def_readwrite("unit", &Track::unit)
      .def_readwrite("innerUnit", &Track::innerUnit)
      .def_readwrite("beatsOver", &Track::beatsOver)
      .def_property(
          "progress", [](const Track& self) { return self.progress; },
          [](Track& self, py::handle value) {
            self.progress = motionAnimatable(value);
          })
      .def_readwrite("reach", &Track::reach)
      .def_readwrite("continuous", &Track::continuous)
      .def(
          "__eq__",
          [](const Track& left, const Track& right) { return left == right; },
          py::arg("other"));
}

void bindDisplaced(py::module_& effects) {
  auto displaced = bindRecord<Displaced>(effects, "Displaced",
                                         "A displacement has no field named ");
  displaced.doc() =
      "Where a glyph stands away from home: offset by dx and dy pixels, "
      "leaning rotateDeg degrees, at scale, with a seeded scatter of "
      "scatterPx and scatterLeanDeg per glyph, fading in over fadeOver of "
      "its way and at opacity, and on a variable axis.";
  displaced.def_readwrite("dx", &Displaced::dx)
      .def_readwrite("dy", &Displaced::dy)
      .def_readwrite("rotateDeg", &Displaced::rotateDeg)
      .def_readwrite("scale", &Displaced::scale)
      .def_readwrite("scatterPx", &Displaced::scatterPx)
      .def_readwrite("scatterLeanDeg", &Displaced::scatterLeanDeg)
      .def_readwrite("fadeOver", &Displaced::fadeOver)
      .def_readwrite("opacity", &Displaced::opacity)
      .def_readwrite("axis", &Displaced::axis)
      .def(py::self == py::self);
}

}  // namespace

void bindComposeTextEffects(pybind11::module_& module) {
  namespace textFx = compose::textFx;
  auto composition = submodule(module, "compose");
  auto effects = submodule(module, "compose.textFx");
  bindTrack(composition);
  bindDisplaced(effects);
  bindMotionDisplacedTween(effects);

  effects.def(
      "entrance",
      [](const motion::Tween<Displaced>& tween, std::optional<Track> track) {
        return textFx::entrance(tween, track.value_or(Track{}));
      },
      py::arg("tween"), py::arg("track") = py::none(),
      "THE ENTRANCE, as a track: the tween says what every glyph does — "
      "from_ where it starts — and when, its duration, delay, loop and "
      "alternate the track's schedule. `track` supplies which glyphs, the "
      "unit and the master progress.");
  effects.def("rise", &textFx::rise, py::arg("distancePx") = 26.0f,
              "Each glyph rises from below.");
  effects.def("slide", &textFx::slide, py::arg("distancePx") = -32.0f,
              "Each glyph slides in from the side.");
  effects.def("pop", &textFx::pop, py::arg("fromScale") = 0.35f,
              py::arg("overshoot") = 1.70158f,
              "Each glyph grows from small, past its size and back.");
  effects.def("spinIn", &textFx::spinIn, py::arg("degrees") = 70.0f,
              py::arg("risePx") = 14.0f,
              "Each glyph straightens out of a lean as it rises.");
  effects.def("scatter", &textFx::scatter, py::arg("radiusPx") = 40.0f,
              py::arg("leanDeg") = 24.0f,
              "Each glyph gathers from a seeded scatter.");
  effects.def("typeOn", &textFx::typeOn,
              "Each glyph appears whole at its turn, as typed.");
  effects.def(
      "variableAxisSweep",
      [](const std::string& tag, float from, float to) {
        if (tag.size() != 4)
          throw py::value_error("A variable axis tag is four characters.");
        const char axis[5] = {tag[0], tag[1], tag[2], tag[3], '\0'};
        return textFx::variableAxisSweep(axis, from, to);
      },
      py::arg("tag"), py::arg("from_"), py::arg("to"),
      "Each glyph sweeps a variable axis from one value to another.");

  // The track a text leaf runs is this feature's, so the verb that hands
  // it one is registered here, after the track's own class.
  py::reinterpret_borrow<py::class_<compose::Text>>(
      py::type::of<compose::Text>())
      .def(
          "textFx",
          [](compose::Text& self, Track track) -> compose::Text& {
            return self.textFx(std::move(track));
          },
          py::arg("track"), py::return_value_policy::reference_internal,
          "Runs this track over the leaf's glyphs: an entrance from "
          "textFx.entrance, its progress the master reading.");
  submodule(module, "core.noise");
}

}  // namespace sigil::python
