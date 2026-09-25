/** @file
 * The shaped form of an animatable number: the `Binding` a followed
 * value runs through, its ranges, envelope and wiggle, and `bind()`,
 * which follows a live number through one.
 */

#include <pybind11/stl.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {

/** A range read from @p value: a `Range`, or a low and a high end. */
motion::Range range(py::handle value) {
  if (py::isinstance<motion::Range>(value)) return py::cast<motion::Range>(value);
  const auto ends = py::cast<py::sequence>(value);
  if (py::len(ends) != 2)
    throw py::value_error("A range is a low end and a high end.");
  return {py::cast<float>(ends[0]), py::cast<float>(ends[1])};
}

/** Writes onto @p stages every field Python named, leaving the rest. */
void applyStages(motion::Binding& stages, py::handle from, py::handle clampFrom,
                 py::handle alternate, py::handle envelope, py::handle ease,
                 py::handle quantize, py::handle reverse, py::handle to,
                 py::handle wrap, py::handle wiggle, py::handle clamp) {
  if (!from.is_none()) stages.from = range(from);
  if (!clampFrom.is_none()) stages.clampFrom = py::cast<bool>(clampFrom);
  if (!alternate.is_none()) stages.alternate = py::cast<bool>(alternate);
  if (!envelope.is_none()) stages.envelope = py::cast<motion::Envelope>(envelope);
  if (!ease.is_none()) stages.ease = motionEase(ease);
  if (!quantize.is_none()) stages.quantize = py::cast<int>(quantize);
  if (!reverse.is_none()) stages.reverse = py::cast<bool>(reverse);
  if (!to.is_none()) stages.to = range(to);
  if (!wrap.is_none()) stages.wrap = py::cast<float>(wrap);
  if (!wiggle.is_none()) stages.wiggle = py::cast<motion::Wiggle>(wiggle);
  if (!clamp.is_none()) stages.clamp = range(clamp);
}

/** The eleven stage keywords, each None where the stage keeps what it
 *  had. */
template <class Function>
void defStages(Function&& define) {
  define(py::arg("from_") = py::none(), py::arg("clampFrom") = py::none(),
         py::arg("alternate") = py::none(), py::arg("envelope") = py::none(),
         py::arg("ease") = py::none(), py::arg("quantize") = py::none(),
         py::arg("reverse") = py::none(), py::arg("to") = py::none(),
         py::arg("wrap") = py::none(), py::arg("wiggle") = py::none(),
         py::arg("clamp") = py::none());
}

}  // namespace

void bindMotionAnimatableForms(py::module_& root) {
  auto module = root.def_submodule("motion");

  auto rangeType = py::class_<motion::Range>(
      module, "Range", "A range of numbers, low end first.");
  rangeType
      .def(py::init([](float low, float high) {
             return motion::Range{low, high};
           }),
           py::arg("low") = 0.0f, py::arg("high") = 1.0f)
      .def_readwrite("low", &motion::Range::low)
      .def_readwrite("high", &motion::Range::high)
      .def("copy", [](const motion::Range& value) { return value; })
      .def(
          "__eq__",
          [](const motion::Range& left, const motion::Range& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(rangeType);

  auto envelopeType = py::class_<motion::Envelope>(
      module, "Envelope",
      "The shape a normalised phase takes across its span, made by the "
      "envelope factories. The default shapes nothing.");
  py::enum_<motion::Envelope::Shape>(envelopeType, "Shape")
      .value("None_", motion::Envelope::Shape::None)
      .value("Cosine", motion::Envelope::Shape::Cosine)
      .value("Trapezoid", motion::Envelope::Shape::Trapezoid)
      .value("Square", motion::Envelope::Shape::Square)
      .value("Shaped", motion::Envelope::Shape::Shaped);
  envelopeType.def(py::init<>())
      .def_readonly("shape", &motion::Envelope::shape)
      .def_readonly("riseStart", &motion::Envelope::riseStart)
      .def_readonly("holdStart", &motion::Envelope::holdStart)
      .def_readonly("holdEnd", &motion::Envelope::holdEnd)
      .def_readonly("fallEnd", &motion::Envelope::fallEnd)
      .def_readonly("duty", &motion::Envelope::duty)
      .def("copy", [](const motion::Envelope& value) { return value; })
      .def(
          "__eq__",
          [](const motion::Envelope& left, const motion::Envelope& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(envelopeType);
  auto envelopes = module.def_submodule("envelope");
  envelopes.def("cosine", &motion::envelope::cosine);
  envelopes.def("square", &motion::envelope::square, py::arg("duty") = 0.5f);
  envelopes.def("trapezoid", &motion::envelope::trapezoid, py::arg("riseStart"),
                py::arg("holdStart"), py::arg("holdEnd"), py::arg("fallEnd"));
  envelopes.def(
      "shaped",
      [](py::handle curve) { return motion::envelope::shaped(motionEase(curve)); },
      py::arg("curve"));

  auto wiggle = bindRecord<motion::Wiggle>(module, "Wiggle",
                                           "Unknown Wiggle field: ");
  wiggle.doc() =
      "Smooth procedural noise added in the property's own units, sampled "
      "at the bound value's normalised phase. An amount of zero is none.";
  wiggle.def_readwrite("amount", &motion::Wiggle::amount)
      .def_readwrite("frequency", &motion::Wiggle::frequency)
      .def_readwrite("seed", &motion::Wiggle::seed)
      .def_readwrite("octaves", &motion::Wiggle::octaves)
      .def_readwrite("falloff", &motion::Wiggle::falloff)
      .def(
          "__eq__",
          [](const motion::Wiggle& left, const motion::Wiggle& right) {
            return left == right;
          },
          py::arg("other"));

  auto binding = py::class_<motion::Binding>(
      module, "Binding",
      "The stages a followed number is shaped through, applied in the order "
      "the fields are declared: from_, clampFrom, alternate, envelope, ease, "
      "quantize, reverse, to, wrap, wiggle, clamp.");
  defStages([&](auto... arguments) {
    binding.def(py::init([](py::handle from, py::handle clampFrom,
                            py::handle alternate, py::handle envelope,
                            py::handle ease, py::handle quantize,
                            py::handle reverse, py::handle to, py::handle wrap,
                            py::handle wiggle, py::handle clamp) {
                  motion::Binding stages;
                  applyStages(stages, from, clampFrom, alternate, envelope,
                              ease, quantize, reverse, to, wrap, wiggle, clamp);
                  return stages;
                }),
                py::kw_only(), arguments...);
  });
  binding
      .def_property(
          "from_", [](const motion::Binding& stages) { return stages.from; },
          [](motion::Binding& stages, py::handle value) {
            stages.from = range(value);
          })
      .def_readwrite("clampFrom", &motion::Binding::clampFrom)
      .def_readwrite("alternate", &motion::Binding::alternate)
      .def_readwrite("envelope", &motion::Binding::envelope)
      .def_property(
          "ease",
          [](const motion::Binding& stages) -> py::object {
            if (!stages.ease) return py::none();
            return easingReading(stages.ease);
          },
          [](motion::Binding& stages, py::handle value) {
            stages.ease = value.is_none() ? motion::Easing{} : motionEase(value);
          })
      .def_readwrite("quantize", &motion::Binding::quantize)
      .def_readwrite("reverse", &motion::Binding::reverse)
      .def_property(
          "to", [](const motion::Binding& stages) { return stages.to; },
          [](motion::Binding& stages, py::handle value) {
            stages.to = range(value);
          })
      .def_readwrite("wrap", &motion::Binding::wrap)
      .def_readwrite("wiggle", &motion::Binding::wiggle)
      .def_property(
          "clamp", [](const motion::Binding& stages) { return stages.clamp; },
          [](motion::Binding& stages, py::handle value) {
            stages.clamp = range(value);
          })
      .def("apply", &motion::Binding::apply, py::arg("value"),
           "Runs the stages on one sample of the followed value.")
      .def("copy", [](const motion::Binding& stages) { return stages; })
      .def(
          "__eq__",
          [](const motion::Binding& left, const motion::Binding& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(binding);

  defStages([&](auto... arguments) {
    module.def(
        "bind",
        [](py::handle source, std::optional<motion::Binding> base,
           py::handle from, py::handle clampFrom, py::handle alternate,
           py::handle envelope, py::handle ease, py::handle quantize,
           py::handle reverse, py::handle to, py::handle wrap,
           py::handle wiggle, py::handle clamp) {
          motion::Binding stages = base.value_or(motion::Binding{});
          applyStages(stages, from, clampFrom, alternate, envelope, ease,
                      quantize, reverse, to, wrap, wiggle, clamp);
          return motion::bind(motionAnimatable(source), std::move(stages));
        },
        py::arg("source"), py::arg("stages") = py::none(), py::kw_only(),
        arguments...,
        "Follows a live number, shaped on its way: the result reads the "
        "source through the stages whenever it is read. The keywords "
        "override the stages given.");
  });
}

}  // namespace sigil::python
