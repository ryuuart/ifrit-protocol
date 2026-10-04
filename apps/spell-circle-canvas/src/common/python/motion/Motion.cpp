/** @file
 * The value that can change over time, in Python: the easings, the
 * transition, the tween and its keyframes, `animate()` and
 * `animatable()`, the animatable number, colour and fill, and the
 * arithmetic over a clock reading.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/values/Tween.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {

/** A curve as Python holds it: callable, and comparable under the rule
 *  two held curves compare by. */
struct Easing {
  motion::Easing value;
};

bool numeric(py::handle value) {
  return !PyBool_Check(value.ptr()) &&
         (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value));
}

/** @p value as a length of animation time, which is finite and never
 *  negative: a motion only goes forward. @p what opens the message. */
motion::Duration animationTime(motion::Duration value, const char* what) {
  if (!std::isfinite(value.count()) || value.count() < 0)
    throw py::value_error(std::string(what) +
                          " must be finite, nonnegative seconds.");
  return value;
}

motion::Staggered<motion::Duration> animationTimes(py::handle value,
                                                   const char* what) {
  auto times = staggeredDuration(value);
  if (!times.isStaggered()) animationTime(times.value(), what);
  return times;
}

// ── the three value families a tween moves ────────────────────────────

/** What differs between a tween of a number, of a colour and of a fill:
 *  how a Python value is read as one, as a value that may differ per
 *  child, and how one is read back. */
template <class T>
struct Family;

template <>
struct Family<float> {
  static float read(py::handle value) { return py::cast<float>(value); }
  static motion::Staggered<float> readStaggered(py::handle value) {
    return staggeredNumber(value);
  }
  static py::object reading(const motion::Staggered<float>& value) {
    return staggeredReading(value);
  }
  static py::object reading(float value) { return py::float_(value); }
};

template <>
struct Family<material::Color> {
  static material::Color read(py::handle value) { return materialColor(value); }
  static motion::Staggered<material::Color> readStaggered(py::handle value) {
    return materialColor(value);
  }
  static py::object reading(const motion::Staggered<material::Color>& value) {
    return py::cast(value.value());
  }
  static py::object reading(const material::Color& value) {
    return py::cast(value);
  }
};

template <>
struct Family<compose::Fill> {
  static compose::Fill read(py::handle value) { return fill(value); }
  static motion::Staggered<compose::Fill> readStaggered(py::handle value) {
    return fill(value);
  }
  static py::object reading(const motion::Staggered<compose::Fill>& value) {
    return py::cast(value.value());
  }
  static py::object reading(const compose::Fill& value) {
    return py::cast(value);
  }
};

template <class T>
motion::Keyframe<T> keyframe(py::handle value) {
  if (py::isinstance<motion::Keyframe<T>>(value))
    return py::cast<motion::Keyframe<T>>(value);
  return {Family<T>::read(value), std::nullopt, {}};
}

/** A tween of @p T written out of the fields Python names, each None
 *  where the author named nothing and the default stands. */
template <class T>
motion::Tween<T> tweenOf(py::handle from, py::handle to, py::handle keyframes,
                         py::handle duration, py::handle delay, py::handle ease,
                         int loop, motion::Duration loopDelay, bool alternate,
                         motion::Composition composition) {
  motion::Tween<T> tween;
  if (!from.is_none()) tween.from = Family<T>::readStaggered(from);
  if (!to.is_none()) tween.to = Family<T>::readStaggered(to);
  if (!keyframes.is_none())
    for (auto item : py::iter(keyframes))
      tween.keyframes.push_back(keyframe<T>(item));
  if (!duration.is_none())
    tween.duration = animationTimes(duration, "A tween's duration");
  if (!delay.is_none()) tween.delay = animationTimes(delay, "A tween's delay");
  if (!ease.is_none()) tween.ease = motionEase(ease);
  tween.loop = loop;
  tween.loopDelay = animationTime(loopDelay, "A tween's loop delay");
  tween.alternate = alternate;
  tween.composition = composition;
  return tween;
}

template <class T>
py::class_<motion::Keyframe<T>> bindKeyframe(py::module_& module,
                                             const char* name) {
  using Keyframe = motion::Keyframe<T>;
  py::class_<Keyframe> type(module, name);
  type.def(py::init([](py::handle to, std::optional<motion::Duration> duration,
                       py::handle ease) {
             Keyframe step{Family<T>::read(to), std::nullopt, {}};
             if (duration)
               step.duration =
                   animationTime(*duration, "A keyframe's duration");
             if (!ease.is_none()) step.ease = motionEase(ease);
             return step;
           }),
           py::arg("to"), py::arg("duration") = py::none(),
           py::arg("ease") = py::none())
      .def_property(
          "to",
          [](const Keyframe& step) { return Family<T>::reading(step.to); },
          [](Keyframe& step, py::handle value) {
            step.to = Family<T>::read(value);
          })
      .def_property(
          "duration", [](const Keyframe& step) { return step.duration; },
          [](Keyframe& step, std::optional<motion::Duration> value) {
            if (value) animationTime(*value, "A keyframe's duration");
            step.duration = value;
          })
      .def_property(
          "ease",
          [](const Keyframe& step) -> py::object {
            if (!step.ease) return py::none();
            return py::cast(Easing{step.ease});
          },
          [](Keyframe& step, py::handle value) {
            step.ease = value.is_none() ? motion::Easing{} : motionEase(value);
          })
      .def("copy", [](const Keyframe& step) { return step; })
      .def(
          "__eq__",
          [](const Keyframe& left, const Keyframe& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(type);
  return type;
}

template <class T>
py::class_<motion::Tween<T>> bindTween(py::module_& module, const char* name,
                                       const char* keyframeName) {
  using Tween = motion::Tween<T>;
  bindKeyframe<T>(module, keyframeName);
  py::class_<Tween> type(module, name);
  type.def(py::init([](py::handle from, py::handle to, py::handle keyframes,
                       py::handle duration, py::handle delay, py::handle ease,
                       int loop, motion::Duration loopDelay, bool alternate,
                       motion::Composition composition) {
             return tweenOf<T>(from, to, keyframes, duration, delay, ease, loop,
                               loopDelay, alternate, composition);
           }),
           py::kw_only(), py::arg("from_") = py::none(),
           py::arg("to") = py::none(), py::arg("keyframes") = py::none(),
           py::arg("duration") = 0.25, py::arg("delay") = 0.0,
           py::arg("ease") = py::none(), py::arg("loop") = 0,
           py::arg("loopDelay") = 0.0, py::arg("alternate") = false,
           py::arg("composition") = motion::Composition::Replace)
      .def_property(
          "from_",
          [](const Tween& tween) -> py::object {
            if (!tween.from) return py::none();
            return Family<T>::reading(*tween.from);
          },
          [](Tween& tween, py::handle value) {
            if (value.is_none())
              tween.from.reset();
            else
              tween.from = Family<T>::readStaggered(value);
          })
      .def_property(
          "to",
          [](const Tween& tween) -> py::object {
            if (!tween.to) return py::none();
            return Family<T>::reading(*tween.to);
          },
          [](Tween& tween, py::handle value) {
            if (value.is_none())
              tween.to.reset();
            else
              tween.to = Family<T>::readStaggered(value);
          })
      .def_property(
          "keyframes",
          [](const Tween& tween) {
            py::list steps;
            for (const auto& step : tween.keyframes)
              steps.append(py::cast(step));
            return steps;
          },
          [](Tween& tween, py::handle steps) {
            tween.keyframes.clear();
            for (auto item : py::iter(steps))
              tween.keyframes.push_back(keyframe<T>(item));
          })
      .def_property(
          "duration",
          [](const Tween& tween) { return staggeredReading(tween.duration); },
          [](Tween& tween, py::handle value) {
            tween.duration = animationTimes(value, "A tween's duration");
          })
      .def_property(
          "delay",
          [](const Tween& tween) { return staggeredReading(tween.delay); },
          [](Tween& tween, py::handle value) {
            tween.delay = animationTimes(value, "A tween's delay");
          })
      .def_property(
          "ease", [](const Tween& tween) { return Easing{tween.easing()}; },
          [](Tween& tween, py::handle value) {
            tween.ease = motionEase(value);
          })
      .def_readwrite("loop", &Tween::loop)
      .def_property(
          "loopDelay", [](const Tween& tween) { return tween.loopDelay; },
          [](Tween& tween, motion::Duration value) {
            tween.loopDelay = animationTime(value, "A tween's loop delay");
          })
      .def_readwrite("alternate", &Tween::alternate)
      .def_readwrite("composition", &Tween::composition)
      .def("rest",
           [](const Tween& tween) { return Family<T>::reading(tween.rest()); })
      .def("resolved", &Tween::resolved, py::arg("place"))
      .def("isStaggered", &Tween::isStaggered)
      .def("isEntrance", &Tween::isEntrance)
      .def("copy", [](const Tween& tween) { return tween; })
      .def(
          "__eq__",
          [](const Tween& left, const Tween& right) {
            return motion::tweenEqual(left, right);
          },
          py::arg("other"));
  copyProtocol(type);
  return type;
}

// ── the animatable number, colour and fill ─────────────────────────────

template <class T>
py::class_<motion::Animatable<T>> bindAnimatable(
    py::module_& module, const char* name,
    motion::Animatable<T> (*read)(py::handle), const char* doc) {
  using Animatable = motion::Animatable<T>;
  using FloatForm = motion::Animatable<float>::Form;
  py::class_<Animatable> type(module, name, doc);
  type.def(py::init([read](py::handle value) { return read(value); }),
           py::arg("value"))
      .def_property(
          "value",
          [](const Animatable& value) {
            return Family<T>::reading(value.value());
          },
          [](Animatable& value, py::handle next) {
            value = Family<T>::read(next);
          },
          "The value now. Setting it writes a live value, which every "
          "copy reads; any other form becomes that constant.")
      .def(
          "set",
          [](Animatable& value, py::handle next) {
            value = Family<T>::read(next);
          },
          py::arg("value"))
      .def("__call__",
           [](const Animatable& value) {
             return Family<T>::reading(value.value());
           })
      .def("isRunning", &Animatable::isRunning)
      .def_property_readonly("form",
                             [](const Animatable& value) {
                               return static_cast<FloatForm>(value.form());
                             })
      .def_property_readonly("described",
                             [](const Animatable& value) -> py::object {
                               const auto* tween = value.described();
                               if (!tween) return py::none();
                               return py::cast(*tween);
                             })
      .def("copy", [](const Animatable& value) { return value; })
      .def(
          "__eq__",
          [](const Animatable& left, const Animatable& right) {
            return motion::propertyEqual(left, right);
          },
          py::arg("other"));
  copyProtocol(type);
  return type;
}

motion::Tween<compose::Fill> fillTween(
    const motion::Tween<material::Color>& tween) {
  motion::Tween<compose::Fill> result;
  if (tween.from) result.from = compose::Fill::color(tween.from->value());
  if (tween.to) result.to = compose::Fill::color(tween.to->value());
  for (const auto& step : tween.keyframes)
    result.keyframes.push_back(
        {compose::Fill::color(step.to), step.duration, step.ease});
  result.duration = tween.duration;
  result.delay = tween.delay;
  result.ease = tween.ease;
  result.loop = tween.loop;
  result.alternate = tween.alternate;
  result.composition = tween.composition;
  return result;
}

/** Which family a tween's values belong to, read off the first value an
 *  author named. */
enum class Kind { Number, Colour, Fill };

Kind kindOf(py::handle value) {
  if (numeric(value) || py::isinstance<motion::Staggered<double>>(value) ||
      py::isinstance<motion::Keyframe<float>>(value))
    return Kind::Number;
  if (py::isinstance<compose::Fill>(value) ||
      py::isinstance<motion::Keyframe<compose::Fill>>(value))
    return Kind::Fill;
  return Kind::Colour;
}

}  // namespace

motion::Easing motionEase(py::handle value) {
  if (value.is_none()) return motion::ease::outQuad;
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

py::object easingReading(const motion::Easing& curve) {
  return py::cast(Easing{curve});
}

motion::Tween<float> motionTransition(py::handle value) {
  if (value.is_none()) return {};
  if (py::isinstance<motion::Tween<float>>(value))
    return py::cast<motion::Tween<float>>(value);
  motion::Tween<float> transition;
  transition.duration = animationTime(py::cast<motion::Duration>(value),
                                      "A transition's duration");
  return transition;
}

motion::Tween<float> motionTween(py::handle value) {
  if (!py::isinstance<motion::Tween<float>>(value))
    throw py::type_error("A motion on a number needs a Tween.");
  return py::cast<motion::Tween<float>>(value);
}

motion::Animatable<float> motionAnimatable(py::handle value) {
  if (py::isinstance<motion::Animatable<float>>(value))
    return py::cast<motion::Animatable<float>>(value);
  if (py::isinstance<motion::Tween<float>>(value))
    return motion::animate(py::cast<motion::Tween<float>>(value));
  if (!PyNumber_Check(value.ptr()))
    throw py::type_error(
        "An animatable number is a number, an Animatable or a Tween.");
  return py::cast<float>(value);
}

motion::Animatable<material::Color> motionInk(py::handle value) {
  if (py::isinstance<motion::Animatable<material::Color>>(value))
    return py::cast<motion::Animatable<material::Color>>(value);
  if (py::isinstance<motion::Tween<material::Color>>(value))
    return motion::animate(py::cast<motion::Tween<material::Color>>(value));
  return materialColor(value);
}

motion::Animatable<compose::Fill> motionFill(py::handle value) {
  if (py::isinstance<motion::Animatable<compose::Fill>>(value))
    return py::cast<motion::Animatable<compose::Fill>>(value);
  if (py::isinstance<motion::Tween<compose::Fill>>(value))
    return motion::animate(py::cast<motion::Tween<compose::Fill>>(value));
  if (py::isinstance<motion::Tween<material::Color>>(value))
    return motion::animate(
        fillTween(py::cast<motion::Tween<material::Color>>(value)));
  if (py::isinstance<motion::Animatable<material::Color>>(value)) {
    const auto ink = py::cast<motion::Animatable<material::Color>>(value);
    if (const auto* tween = ink.described())
      return motion::animate(fillTween(*tween));
    if (const auto* constant = ink.constant())
      return compose::Fill::color(*constant);
    throw py::type_error(
        "A live colour cannot fill a surface; make the live value a fill: "
        "animatable(Fill.color(...)).");
  }
  return fill(value);
}

void bindMotion(py::module_& root) {
  auto module = root.def_submodule("motion");

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
  named("linear", &motion::ease::linear);
  named("inQuad", &motion::ease::inQuad);
  named("outQuad", &motion::ease::outQuad);
  named("inOutQuad", &motion::ease::inOutQuad);
  named("inCubic", &motion::ease::inCubic);
  named("outCubic", &motion::ease::outCubic);
  named("inOutCubic", &motion::ease::inOutCubic);
  named("inQuart", &motion::ease::inQuart);
  named("outQuart", &motion::ease::outQuart);
  named("inOutQuart", &motion::ease::inOutQuart);
  named("inQuint", &motion::ease::inQuint);
  named("outQuint", &motion::ease::outQuint);
  named("inOutQuint", &motion::ease::inOutQuint);
  named("inSine", &motion::ease::inSine);
  named("outSine", &motion::ease::outSine);
  named("inOutSine", &motion::ease::inOutSine);
  named("inExpo", &motion::ease::inExpo);
  named("outExpo", &motion::ease::outExpo);
  named("inOutExpo", &motion::ease::inOutExpo);
  named("inCirc", &motion::ease::inCirc);
  named("outCirc", &motion::ease::outCirc);
  named("inOutCirc", &motion::ease::inOutCirc);
  named("smoothstep", &motion::ease::smoothstep);
  ease.def("inBack", &motion::ease::inBack, py::arg("overshoot") = 1.70158f);
  ease.def("outBack", &motion::ease::outBack, py::arg("overshoot") = 1.70158f);
  ease.def("inOutBack", &motion::ease::inOutBack,
           py::arg("overshoot") = 1.70158f);
  ease.def("inElastic", &motion::ease::inElastic, py::arg("amplitude") = 1.0f,
           py::arg("period") = 0.3f);
  ease.def("outElastic", &motion::ease::outElastic, py::arg("amplitude") = 1.0f,
           py::arg("period") = 0.3f);
  ease.def("inOutElastic", &motion::ease::inOutElastic,
           py::arg("amplitude") = 1.0f, py::arg("period") = 0.3f);
  ease.def("inBounce", &motion::ease::inBounce,
           py::arg("overshoot") = 1.70158f);
  ease.def("outBounce", &motion::ease::outBounce,
           py::arg("overshoot") = 1.70158f);
  ease.def("inOutBounce", &motion::ease::inOutBounce,
           py::arg("overshoot") = 1.70158f);
  // `in` is a Python keyword, so the power family's first member takes
  // the trailing underscore Python spells a reserved word with.
  ease.def("in_", &motion::ease::in, py::arg("power") = 1.68f);
  ease.def("out", &motion::ease::out, py::arg("power") = 1.68f);
  ease.def("inOut", &motion::ease::inOut, py::arg("power") = 1.68f);
  ease.def("steps", &motion::ease::steps, py::arg("count"),
           py::arg("jumpAtStart") = false);
  ease.def("cubicBezier", &motion::ease::cubicBezier, py::arg("x1"),
           py::arg("y1"), py::arg("x2"), py::arg("y2"));

  py::enum_<motion::Composition>(
      module, "Composition",
      "How a change mid-flight composes with the motion already running: "
      "Replace starts again from the value on screen, Blend adds the change "
      "on top so the velocity carries through.")
      .value("Replace", motion::Composition::Replace)
      .value("Blend", motion::Composition::Blend);

  auto tween = bindTween<float>(module, "Tween", "Keyframe");
  tween.doc() =
      "A motion, described: from_ named is an entrance, to alone eases on "
      "change. from_, to, duration and delay each take a plain value or a "
      "stagger().";
  tween.def(
      "at",
      [](const motion::Tween<float>& value, motion::Duration time) {
        return value.at(time);
      },
      py::arg("time"),
      "The value this long after the motion starts, read with no engine.");
  bindTween<material::Color>(module, "ColorTween", "ColorKeyframe");
  bindTween<compose::Fill>(module, "FillTween", "FillKeyframe");

  py::enum_<motion::Animatable<float>::Form>(module, "AnimatableForm")
      .value("Constant", motion::Animatable<float>::Form::Constant)
      .value("Described", motion::Animatable<float>::Form::Described)
      .value("Live", motion::Animatable<float>::Form::Live)
      .value("Bound", motion::Animatable<float>::Form::Bound);
  auto number = bindAnimatable<float>(
      module, "Animatable", &motionAnimatable,
      "A number that can change over time: a constant, a described motion, "
      "a live value somebody writes, or a live value followed through a "
      "Binding. Every copy of a live value reads and writes one cell.");
  number
      .def_property_readonly(
          "binding",
          [](const motion::Animatable<float>& value) -> py::object {
            const auto* stages = value.binding();
            if (!stages) return py::none();
            return py::cast(*stages);
          })
      .def_property_readonly(
          "source", [](const motion::Animatable<float>& value) -> py::object {
            const auto* source = value.source();
            if (!source) return py::none();
            return py::cast(*source);
          });
  bindAnimatable<material::Color>(module, "ColorAnimatable", &motionInk,
                                  "A colour that can change over time.");
  bindAnimatable<compose::Fill>(module, "FillAnimatable", &motionFill,
                                "A fill that can change over time.");

  module.def(
      "animatable",
      [](py::handle value) -> py::object {
        if (numeric(value))
          return py::cast(motion::animatable(py::cast<float>(value)));
        if (py::isinstance<compose::Fill>(value))
          return py::cast(motion::animatable(fill(value)));
        return py::cast(motion::animatable(materialColor(value)));
      },
      py::arg("value"),
      "A live value starting at the value given: write it, hand it to a "
      "property, and the property follows.");
  module.def(
      "animate",
      [](py::handle described, py::handle from, py::handle to,
         py::handle keyframes, py::handle duration, py::handle delay,
         py::handle easing, int loop, motion::Duration loopDelay,
         bool alternate, motion::Composition composition) -> py::object {
        if (py::isinstance<motion::Tween<float>>(described))
          return py::cast(
              motion::animate(py::cast<motion::Tween<float>>(described)));
        if (py::isinstance<motion::Tween<material::Color>>(described))
          return py::cast(motion::animate(
              py::cast<motion::Tween<material::Color>>(described)));
        if (py::isinstance<motion::Tween<compose::Fill>>(described))
          return py::cast(motion::animate(
              py::cast<motion::Tween<compose::Fill>>(described)));
        if (!described.is_none())
          throw py::type_error("animate() takes a Tween, or a tween's fields.");
        py::object frames = py::none();
        py::handle first = !from.is_none() ? from : to;
        if (!keyframes.is_none()) {
          py::list steps(py::reinterpret_borrow<py::object>(keyframes));
          if (first.is_none() && !steps.empty()) first = steps[0];
          frames = std::move(steps);
        }
        if (first.is_none())
          throw py::value_error("animate() needs from_, to or keyframes.");
        const py::object length =
            duration.is_none() ? py::object(py::float_(0.25))
                               : py::reinterpret_borrow<py::object>(duration);
        switch (kindOf(first)) {
          case Kind::Number:
            return py::cast(motion::animate(
                tweenOf<float>(from, to, frames, length, delay, easing, loop,
                               loopDelay, alternate, composition)));
          case Kind::Fill:
            return py::cast(motion::animate(tweenOf<compose::Fill>(
                from, to, frames, length, delay, easing, loop, loopDelay,
                alternate, composition)));
          case Kind::Colour:
            break;
        }
        return py::cast(motion::animate(
            tweenOf<material::Color>(from, to, frames, length, delay, easing,
                                     loop, loopDelay, alternate, composition)));
      },
      py::arg("tween") = py::none(), py::kw_only(),
      py::arg("from_") = py::none(), py::arg("to") = py::none(),
      py::arg("keyframes") = py::none(), py::arg("duration") = py::none(),
      py::arg("delay") = py::none(), py::arg("ease") = py::none(),
      py::arg("loop") = 0, py::arg("loopDelay") = 0.0,
      py::arg("alternate") = false,
      py::arg("composition") = motion::Composition::Replace,
      "A property value that moves: the tween given, or one written out of "
      "the fields named. A number, a colour and a fill each make their own "
      "animatable.");

  module.def("phase", &motion::phase, py::arg("time"), py::arg("period"));
  module.def(
      "quantizeTime",
      [](motion::Duration time, double rate) {
        return motion::quantizeTime(time, rate);
      },
      py::arg("time"), py::arg("rate"));
  module.def("stepIndex", &motion::stepIndex, py::arg("time"), py::arg("rate"));
  module.def("decay", &motion::decay, py::arg("age"), py::arg("timeConstant"));
  module.def("flash", &motion::flash, py::arg("age"), py::arg("attack"),
             py::arg("timeConstant"), py::arg("rest") = 0.0f);
  module.def("clamp01", &motion::clamp01, py::arg("value"));
}

}  // namespace sigil::python
