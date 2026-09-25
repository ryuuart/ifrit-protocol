/** @file
 * Signals that are a function of time and nothing else: the oscillator
 * and its waves, and the closed-form spring with the two numbers it is
 * stated in.
 */

#include <sigilmotion/values/Oscillator.h>
#include <sigilmotion/values/Spring.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/motion/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindMotionSignals(py::module_& root) {
  auto module = root.def_submodule("motion");

  py::enum_<motion::Wave>(module, "Wave",
                          "The classic shapes an oscillator swings through.")
      .value("Sine", motion::Wave::Sine)
      .value("Triangle", motion::Wave::Triangle)
      .value("Sawtooth", motion::Wave::Sawtooth)
      .value("Square", motion::Wave::Square);
  auto oscillator = bindRecord<motion::Oscillator>(
      module, "Oscillator", "Unknown Oscillator field: ");
  oscillator.doc() =
      "One repeating signal, stated as a wave, a rate in cycles per second, "
      "a phase in cycles, an amplitude and a centre. Callable with a number "
      "of seconds, so it serves as a curve too.";
  oscillator.def_readwrite("wave", &motion::Oscillator::wave)
      .def_readwrite("hertz", &motion::Oscillator::hertz)
      .def_readwrite("phase", &motion::Oscillator::phase)
      .def_readwrite("amplitude", &motion::Oscillator::amplitude)
      .def_readwrite("centre", &motion::Oscillator::centre)
      .def_readwrite("duty", &motion::Oscillator::duty)
      .def("at", &motion::Oscillator::at, py::arg("time"),
           "The signal at this time.")
      .def("fold", &motion::Oscillator::fold, py::arg("time"),
           "Where in the cycle this time is, in [0, 1).")
      .def("shape", &motion::Oscillator::shape, py::arg("unitPhase"),
           "The waveform on a phase already folded into [0, 1), in [-1, 1].")
      .def("__call__", &motion::Oscillator::operator(), py::arg("input"))
      .def(
          "__eq__",
          [](const motion::Oscillator& left, const motion::Oscillator& right) {
            return left == right;
          },
          py::arg("other"));

  auto parameters = bindRecord<motion::SpringParameters>(
      module, "SpringParameters", "Unknown SpringParameters field: ");
  parameters.doc() =
      "How a spring settles: the period it would oscillate at undamped, and "
      "the damping ratio — below 1 it rings, at 1 it arrives without "
      "crossing, above 1 it crawls in.";
  parameters.def_readwrite("period", &motion::SpringParameters::period)
      .def_readwrite("damping", &motion::SpringParameters::damping)
      .def(
          "__eq__",
          [](const motion::SpringParameters& left,
             const motion::SpringParameters& right) { return left == right; },
          py::arg("other"));

  auto spring = bindRecord<motion::Spring>(module, "Spring",
                                           "Unknown Spring field: ");
  spring.doc() =
      "Where a spring is: the value it holds and the velocity it holds it "
      "with. Stepped in closed form, so one step of any size is exact.";
  spring.def_readwrite("value", &motion::Spring::value)
      .def_readwrite("velocity", &motion::Spring::velocity)
      .def("step", &motion::Spring::step, py::arg("target"),
           py::arg("elapsed"),
           py::arg("parameters") = motion::SpringParameters{},
           "The spring this long later, flying at the target.")
      .def("isSettled", &motion::Spring::isSettled, py::arg("target"),
           py::arg("slack") = 0.5f,
           "Within the slack of the target and slower than the slack per "
           "second.")
      .def(
          "__eq__",
          [](const motion::Spring& left, const motion::Spring& right) {
            return left == right;
          },
          py::arg("other"));
}

}  // namespace sigil::python
