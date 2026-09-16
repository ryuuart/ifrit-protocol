/** @file
 * Native motion descriptions and shared sources for retained Python trees.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Time.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/ComposeBindings.h>
#include <sigilpython/MotionBindings.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>

namespace sigil::python {
namespace py = pybind11;
namespace {

struct Easing {
  choreograph::EaseFn value;
};

std::chrono::milliseconds milliseconds(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0 || seconds > 1e12)
    throw py::value_error(
        "Animation time must be finite, nonnegative seconds.");
  return std::chrono::milliseconds{
      static_cast<std::chrono::milliseconds::rep>(seconds * 1000)};
}

double seconds(std::chrono::milliseconds value) {
  return static_cast<double>(value.count()) / 1000;
}

motion::Transition transition(double duration, py::handle easing,
                              double delay) {
  return {milliseconds(duration), motionEase(easing), milliseconds(delay)};
}

bool numeric(py::handle value) {
  return py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value);
}

template <class T, class Convert>
void outputType(py::module_& module, const char* name, Convert convert) {
  using Output = choreograph::Output<T>;
  py::class_<Output, std::shared_ptr<Output>>(module, name)
      .def(py::init([convert](py::handle value) {
             return std::make_shared<Output>(value.is_none() ? T{}
                                                             : convert(value));
           }),
           py::arg("value") = py::none())
      .def_property(
          "value", [](const Output& value) -> T { return value.value(); },
          [convert](Output& output, py::handle value) {
            output = convert(value);
          })
      .def(
          "set",
          [convert](Output& output, py::handle value) {
            output = convert(value);
          },
          py::arg("value"))
      .def("get", [](const Output& value) -> T { return value.value(); })
      .def("__call__", [](const Output& value) -> T { return value.value(); })
      .def("isConnected", &Output::isConnected)
      .def("endValue", &Output::endValue)
      .def("disconnect", &Output::disconnect);
}

template <class T, class Convert>
void frameTypes(py::module_& module, const char* prefix, Convert convert) {
  const std::string p(prefix);
  py::class_<motion::From<T>>(module, (p + "From").c_str())
      .def(
          "to",
          [convert](const motion::From<T>& start, py::handle value) {
            return motion::FromTo<T>{start.value, convert(value)};
          },
          py::arg("value"));
  py::class_<motion::FromTo<T>>(module, (p + "FromTo").c_str());
  py::class_<motion::To<T>>(module, (p + "To").c_str());
  py::class_<motion::Waypoints<T>>(module, (p + "Waypoints").c_str());
  py::class_<motion::Transitioned<T>>(module, (p + "Transitioned").c_str())
      .def_property_readonly(
          "value",
          [](const motion::Transitioned<T>& value) { return value.value; })
      .def_property_readonly(
          "from_value",
          [](const motion::Transitioned<T>& value) { return value.from; })
      .def_readwrite("spec", &motion::Transitioned<T>::spec)
      .def_property_readonly(
          "waypoints",
          [](const motion::Transitioned<T>& value) {
            py::list result;
            for (const auto& [time, point] : value.waypoints)
              result.append(py::make_tuple(seconds(time), point));
            return result;
          })
      .def("copy", [](const motion::Transitioned<T>& value) { return value; });
}

template <class T, class Convert>
motion::Waypoints<T> waypoints(const py::list& frames, Convert convert) {
  std::vector<std::pair<std::chrono::milliseconds, T>> values;
  double previous = 0;
  for (auto item : frames) {
    const auto frame = py::cast<py::sequence>(item);
    if (frame.size() != 2)
      throw py::value_error("A waypoint needs a time and a value.");
    const auto time = py::cast<double>(frame[0]);
    const auto when = milliseconds(time);
    if (time < previous)
      throw py::value_error("Waypoint times must be nondecreasing.");
    values.emplace_back(when, convert(frame[1]));
    previous = time;
  }
  return motion::through<T>(std::move(values));
}

template <class T>
py::object animatePath(py::handle path, const motion::Transition& spec) {
  if (py::isinstance<motion::FromTo<T>>(path))
    return py::cast(motion::animate(py::cast<motion::FromTo<T>>(path), spec));
  if (py::isinstance<motion::To<T>>(path))
    return py::cast(motion::animate(py::cast<motion::To<T>>(path), spec));
  if (py::isinstance<motion::Waypoints<T>>(path)) {
    auto value =
        motion::animate(py::cast<motion::Waypoints<T>>(path), spec.easing());
    value.spec.delay = spec.delay;
    return py::cast(std::move(value));
  }
  return {};
}

motion::Transitioned<compose::Fill> fillTransition(
    const motion::Transitioned<SkColor4f>& value) {
  motion::Transitioned<compose::Fill> result;
  result.value = compose::Fill::color(value.value);
  result.spec = value.spec;
  if (value.from) result.from = compose::Fill::color(*value.from);
  for (const auto& [when, color] : value.waypoints)
    result.waypoints.emplace_back(when, compose::Fill::color(color));
  return result;
}

}  // namespace

choreograph::EaseFn motionEase(py::handle value) {
  if (value.is_none()) return &choreograph::easeOutQuad;
  if (py::isinstance<Easing>(value)) return py::cast<Easing>(value).value;
  if (py::isinstance<motion::ease::Curve>(value))
    return py::cast<motion::ease::Curve>(value);
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error("An easing must be a native curve or a callable.");
  auto held = retainCallback(py::reinterpret_borrow<py::function>(value));
  return [held](float progress) {
    const py::gil_scoped_acquire lock;
    const CallbackBoundary boundary;
    try {
      return held->get()(progress).cast<float>();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

motion::Transition motionTransition(py::handle value) {
  return value.is_none() ? motion::Transition{}
                         : py::cast<motion::Transition>(value);
}

motion::Animatable<float> motionAnimatable(py::handle value) {
  if (py::isinstance<motion::Animatable<float>>(value))
    return py::cast<motion::Animatable<float>>(value);
  if (py::isinstance<motion::Transitioned<float>>(value))
    return py::cast<motion::Transitioned<float>>(value);
  if (py::isinstance<choreograph::Output<float>>(value))
    return motion::Animatable<float>(
        py::cast<std::shared_ptr<choreograph::Output<float>>>(value));
  if (py::isinstance<motion::Bound>(value))
    return py::cast<motion::Bound>(value);
  return py::cast<float>(value);
}

motion::Animatable<SkColor4f> motionInk(py::handle value) {
  if (py::isinstance<motion::Transitioned<SkColor4f>>(value))
    return py::cast<motion::Transitioned<SkColor4f>>(value);
  if (py::isinstance<choreograph::Output<SkColor4f>>(value))
    return motion::Animatable<SkColor4f>(
        py::cast<std::shared_ptr<choreograph::Output<SkColor4f>>>(value));
  return color(value);
}

motion::Animatable<compose::Fill> motionFill(py::handle value) {
  if (py::isinstance<motion::Transitioned<compose::Fill>>(value))
    return py::cast<motion::Transitioned<compose::Fill>>(value);
  if (py::isinstance<motion::Transitioned<SkColor4f>>(value))
    return fillTransition(py::cast<motion::Transitioned<SkColor4f>>(value));
  if (py::isinstance<choreograph::Output<compose::Fill>>(value))
    return motion::Animatable<compose::Fill>(
        py::cast<std::shared_ptr<choreograph::Output<compose::Fill>>>(value));
  return fill(value);
}

void bindMotion(py::module_& root) {
  auto module = root.def_submodule("motion");
  py::class_<motion::Ticker::FixedStatus,
             std::shared_ptr<motion::Ticker::FixedStatus>>(module,
                                                           "FixedStatus")
      .def(py::init<>())
      .def_readonly("stepsRun", &motion::Ticker::FixedStatus::stepsRun)
      .def_readonly("clamped", &motion::Ticker::FixedStatus::clamped);

  py::class_<motion::Animatable<float>>(module, "Animatable")
      .def(py::init([](py::handle value) { return motionAnimatable(value); }),
           py::arg("value"))
      .def("copy", [](const motion::Animatable<float>& value) { return value; })
      .def_property_readonly("index", &motion::Animatable<float>::index)
      .def_property_readonly(
          "value",
          [](const motion::Animatable<float>& value) {
            if (const auto* plain = value.plain()) return *plain;
            if (const auto* keyed = value.transitioned()) return keyed->value;
            const float current = value.binding()->value();
            return value.boundMap() ? value.boundMap()->apply(current)
                                    : current;
          },
          "The constant, transition target, or current bound source value.")
      .def(
          "__eq__",
          [](const motion::Animatable<float>& a,
             const motion::Animatable<float>& b) {
            return motion::propertyEqual(a, b);
          },
          py::arg("other"));
  py::class_<Easing>(module, "Easing")
      .def(
          "__call__",
          [](const Easing& ease, float progress) {
            return ease.value(progress);
          },
          py::arg("progress"))
      .def(
          "__eq__",
          [](const Easing& a, const Easing& b) {
            return motion::easeEqual(a.value, b.value);
          },
          py::arg("other"));
  py::class_<motion::ease::Curve>(module, "Curve")
      .def(py::init<>())
      .def("at", &motion::ease::Curve::at, py::arg("progress"))
      .def("__call__", &motion::ease::Curve::at, py::arg("progress"))
      .def(
          "__eq__",
          [](const motion::ease::Curve& a, const motion::ease::Curve& b) {
            return a == b;
          },
          py::arg("other"));
  auto ease = module.def_submodule("ease");
  const auto named = [&](const char* name, float (*function)(float)) {
    ease.attr(name) = py::cast(Easing{function});
  };
  named("linear", &choreograph::easeNone);
  named("inQuad", &choreograph::easeInQuad);
  named("outQuad", &choreograph::easeOutQuad);
  named("inOutQuad", &choreograph::easeInOutQuad);
  named("inCubic", &choreograph::easeInCubic);
  named("outCubic", &choreograph::easeOutCubic);
  named("inOutCubic", &choreograph::easeInOutCubic);
  named("inQuart", &choreograph::easeInQuart);
  named("outQuart", &choreograph::easeOutQuart);
  named("inOutQuart", &choreograph::easeInOutQuart);
  named("inQuint", &choreograph::easeInQuint);
  named("outQuint", &choreograph::easeOutQuint);
  named("inOutQuint", &choreograph::easeInOutQuint);
  named("inSine", &choreograph::easeInSine);
  named("outSine", &choreograph::easeOutSine);
  named("inOutSine", &choreograph::easeInOutSine);
  named("inExpo", &choreograph::easeInExpo);
  named("outExpo", &choreograph::easeOutExpo);
  named("inOutExpo", &choreograph::easeInOutExpo);
  named("inCirc", &choreograph::easeInCirc);
  named("outCirc", &choreograph::easeOutCirc);
  named("inOutCirc", &choreograph::easeInOutCirc);
  named("smoothstep", &motion::ease::smoothstep);
  ease.def("outBack", &motion::ease::outBack, py::arg("overshoot") = 1.70158f);
  ease.def("inBack", &motion::ease::inBack, py::arg("overshoot") = 1.70158f);
  ease.def("inOutBack", &motion::ease::inOutBack,
           py::arg("overshoot") = 1.70158f);
  ease.def("outElastic", &motion::ease::outElastic, py::arg("amplitude") = 1.0f,
           py::arg("period") = 0.3f);
  ease.def("inElastic", &motion::ease::inElastic, py::arg("amplitude") = 1.0f,
           py::arg("period") = 0.3f);
  ease.def("outBounce", &motion::ease::outBounce,
           py::arg("overshoot") = 1.70158f);
  ease.def("cubicBezier", &motion::ease::cubicBezier, py::arg("x1"),
           py::arg("y1"), py::arg("x2"), py::arg("y2"));

  py::class_<motion::Transition>(module, "Transition")
      .def(py::init(&transition), py::arg("duration") = 0.25,
           py::arg("ease") = py::none(), py::arg("delay") = 0.0)
      .def_property(
          "duration",
          [](const motion::Transition& spec) { return seconds(spec.duration); },
          [](motion::Transition& spec, double value) {
            spec.duration = milliseconds(value);
          })
      .def_property(
          "delay",
          [](const motion::Transition& spec) { return seconds(spec.delay); },
          [](motion::Transition& spec, double value) {
            spec.delay = milliseconds(value);
          })
      .def_property(
          "ease",
          [](const motion::Transition& spec) { return Easing{spec.easing()}; },
          [](motion::Transition& spec, py::handle value) {
            spec.ease = motionEase(value);
          })
      .def("copy", [](const motion::Transition& spec) { return spec; })
      .def("__eq__", &motion::transitionEqual, py::arg("other"));
  outputType<float>(module, "Output",
                    [](py::handle value) { return py::cast<float>(value); });
  outputType<SkColor4f>(module, "ColorOutput", &color);
  outputType<compose::Fill>(module, "FillOutput", &fill);
  frameTypes<float>(module, "",
                    [](py::handle value) { return py::cast<float>(value); });
  frameTypes<SkColor4f>(module, "Color", &color);
  frameTypes<compose::Fill>(module, "Fill", &fill);

  module.def(
      "from_",
      [](py::handle value) -> py::object {
        if (numeric(value))
          return py::cast(motion::from(py::cast<float>(value)));
        if (py::isinstance<compose::Fill>(value))
          return py::cast(motion::from(fill(value)));
        return py::cast(motion::from(color(value)));
      },
      py::arg("value"));
  module.def(
      "to",
      [](py::handle value) -> py::object {
        if (numeric(value)) return py::cast(motion::to(py::cast<float>(value)));
        if (py::isinstance<compose::Fill>(value))
          return py::cast(motion::to(fill(value)));
        return py::cast(motion::to(color(value)));
      },
      py::arg("value"));
  module.def(
      "through",
      [](py::iterable input) -> py::object {
        py::list frames(input);
        if (frames.empty()) return py::cast(motion::Waypoints<float>{});
        const auto first = py::cast<py::sequence>(frames[0]);
        if (first.size() != 2)
          throw py::value_error("A waypoint needs a time and a value.");
        if (numeric(first[1]))
          return py::cast(waypoints<float>(
              frames, [](py::handle v) { return py::cast<float>(v); }));
        if (py::isinstance<compose::Fill>(first[1]))
          return py::cast(waypoints<compose::Fill>(frames, &fill));
        return py::cast(waypoints<SkColor4f>(frames, &color));
      },
      py::arg("frames"));
  module.def(
      "animate",
      [](py::handle path, py::handle options, py::handle easing) {
        auto spec = motionTransition(options);
        if (!easing.is_none()) spec.ease = motionEase(easing);
        if (auto result = animatePath<float>(path, spec)) return result;
        if (auto result = animatePath<SkColor4f>(path, spec)) return result;
        if (auto result = animatePath<compose::Fill>(path, spec)) return result;
        throw py::type_error(
            "animate() needs from_(a).to(b), to(value), or through(frames).");
      },
      py::arg("path"), py::arg("spec") = py::none(), py::kw_only(),
      py::arg("ease") = py::none());
  module.def(
      "entrance",
      [](py::handle start, py::handle stop, double duration, double delay,
         py::handle easing) -> py::object {
        const auto spec = transition(duration, easing, delay);
        if (numeric(start))
          return py::cast(motion::animate(
              motion::from(py::cast<float>(start)).to(py::cast<float>(stop)),
              spec));
        if (py::isinstance<compose::Fill>(start))
          return py::cast(
              motion::animate(motion::from(fill(start)).to(fill(stop)), spec));
        return py::cast(
            motion::animate(motion::from(color(start)).to(color(stop)), spec));
      },
      py::arg("start"), py::arg("stop"), py::arg("duration") = 0.25,
      py::arg("delay") = 0.0, py::arg("ease") = py::none());
  module.def(
      "transition",
      [](py::handle target, double duration, double delay,
         py::handle easing) -> py::object {
        const auto spec = transition(duration, easing, delay);
        if (numeric(target))
          return py::cast(
              motion::animate(motion::to(py::cast<float>(target)), spec));
        if (py::isinstance<compose::Fill>(target))
          return py::cast(motion::animate(motion::to(fill(target)), spec));
        return py::cast(motion::animate(motion::to(color(target)), spec));
      },
      py::arg("target"), py::arg("duration") = 0.25, py::arg("delay") = 0.0,
      py::arg("ease") = py::none());

  constexpr auto fluent = py::return_value_policy::reference_internal;
  py::class_<motion::Bound>(module, "Bound")
      .def(py::init([](std::shared_ptr<choreograph::Output<float>> output) {
             if (!output) throw py::type_error("A binding needs an Output.");
             return motion::bind(std::move(output));
           }),
           py::arg("output"))
      .def("copy", [](const motion::Bound& value) { return value; })
      .def("source", &motion::Bound::source, py::arg("low"), py::arg("high"),
           fluent)
      .def("window", &motion::Bound::window, py::arg("low"), py::arg("high"),
           fluent)
      .def("pingPong", &motion::Bound::pingPong, fluent)
      .def("cosine", &motion::Bound::cosine, fluent)
      .def("trapezoid", &motion::Bound::trapezoid, py::arg("riseStart"),
           py::arg("holdStart"), py::arg("holdEnd"), py::arg("fallEnd"), fluent)
      .def("square", &motion::Bound::square, py::arg("duty") = 0.5f, fluent)
      .def(
          "wave",
          [](motion::Bound& chain, py::handle easing) -> motion::Bound& {
            return chain.wave(motionEase(easing));
          },
          py::arg("easing"), fluent)
      .def(
          "map",
          [](motion::Bound& chain, py::handle easing) -> motion::Bound& {
            return chain.map(motionEase(easing));
          },
          py::arg("function"), fluent)
      .def("scale", &motion::Bound::scale, py::arg("factor"), fluent)
      .def("offset", &motion::Bound::offset, py::arg("amount"), fluent)
      .def("target", &motion::Bound::target, py::arg("low"), py::arg("high"),
           fluent)
      .def("invert", &motion::Bound::invert, fluent)
      .def("quantize", &motion::Bound::quantize, py::arg("steps"), fluent)
      .def("wrap", &motion::Bound::wrap, py::arg("period"), fluent)
      .def("wiggle", &motion::Bound::wiggle, py::arg("amount") = 1.0f,
           py::arg("frequency") = 2.0f, py::arg("seed") = 0,
           py::arg("octaves") = 1, py::arg("falloff") = 0.5f, fluent)
      .def("clamp", &motion::Bound::clamp, py::arg("low"), py::arg("high"),
           fluent)
      .def("sample",
           [](const motion::Bound& chain) {
             return chain.value().apply(
                 chain.value().source ? chain.value().source->value() : 0.0f);
           })
      .def(
          "apply",
          [](const motion::Bound& chain, float input) {
            return chain.value().apply(input);
          },
          py::arg("input"))
      .def(
          "__eq__",
          [](const motion::Bound& a, const motion::Bound& b) {
            return motion::boundMapEqual(a.value(), b.value());
          },
          py::arg("other"));
  module.def(
      "bind",
      [](std::shared_ptr<choreograph::Output<float>> output) {
        if (!output) throw py::type_error("A binding needs an Output.");
        return motion::bind(std::move(output));
      },
      py::arg("output"));
  module.def(
      "wiggle",
      [](std::shared_ptr<choreograph::Output<float>> output, float amount,
         float frequency, uint32_t seed, int octaves, float falloff) {
        if (!output) throw py::type_error("A binding needs an Output.");
        return motion::bind(std::move(output))
            .scale(0)
            .wiggle(amount, frequency, seed, octaves, falloff);
      },
      py::arg("output"), py::arg("amount") = 1.0f, py::arg("frequency") = 2.0f,
      py::arg("seed") = 0, py::arg("octaves") = 1, py::arg("falloff") = 0.5f);
  module.def("phase", &motion::phase, py::arg("seconds"), py::arg("period"));
  module.def("quantizeTime", &motion::quantizeTime<double>, py::arg("seconds"),
             py::arg("hz"));
  module.def("stepIndex", &motion::stepIndex, py::arg("seconds"),
             py::arg("hz"));
  module.def("decay", &motion::decay, py::arg("age"), py::arg("tau"));
  module.def("flash", &motion::flash, py::arg("age"), py::arg("attack"),
             py::arg("tau"), py::arg("rest") = 0.0f);
  module.def("clamp01", &motion::clamp01, py::arg("value"));
  module.def(
      "ramp",
      [](double delay, double duration, py::handle easing) {
        return transition(duration, easing, delay);
      },
      py::arg("delay"), py::arg("duration"), py::arg("ease") = py::none());
}

}  // namespace sigil::python
