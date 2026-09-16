#include "Bindings.h"

#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Color.h>
#include <sigildraw/Pen.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilweave/style/Type.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::sketch::python {

namespace py = pybind11;

SkColor4f color(py::handle value) {
  if (py::isinstance<py::str>(value))
    return draw::parseColor(py::cast<std::string>(value));
  if (!py::isinstance<py::tuple>(value) && !py::isinstance<py::list>(value))
    throw py::type_error("A color is a string or an RGB or RGBA sequence.");
  auto channels = py::reinterpret_borrow<py::sequence>(value);
  if (channels.size() != 3 && channels.size() != 4)
    throw py::value_error("A color needs three or four channels.");
  float rgba[4] = {0, 0, 0, 1};
  for (py::ssize_t i = 0; i < channels.size(); ++i) {
    rgba[i] = py::cast<float>(channels[i]);
    if (!std::isfinite(rgba[i]) || rgba[i] < 0 || rgba[i] > 1)
      throw py::value_error(
          "Color sequence channels must be between zero and one.");
  }
  return {rgba[0], rgba[1], rgba[2], rgba[3]};
}

namespace {

using compose::Element;
using draw::Pen;
using Animated = motion::Transitioned<float>;
constexpr auto kFluent = py::return_value_policy::reference_internal;
thread_local CallbackLifetime* currentLifetime = nullptr;

compose::Dimension dimension(py::handle value) {
  if (!py::isinstance<py::str>(value))
    return compose::Dimension{py::cast<float>(value)};
  const auto text = py::cast<std::string>(value);
  if (text == "auto") return compose::autoDimension();
  if (text.size() > 1 && text.back() == '%') {
    std::size_t end = 0;
    const float amount = std::stof(text, &end);
    if (end == text.size() - 1 && std::isfinite(amount))
      return compose::pct(amount);
  }
  throw py::value_error(
      "A dimension is a number, a percentage string, or 'auto'.");
}

motion::Animatable<float> animatable(py::handle value) {
  if (py::isinstance<Animated>(value)) return py::cast<Animated>(value);
  return py::cast<float>(value);
}

std::chrono::milliseconds milliseconds(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0 || seconds > 1e12)
    throw py::value_error(
        "Animation time must be finite, nonnegative seconds.");
  return std::chrono::milliseconds{
      static_cast<std::chrono::milliseconds::rep>(seconds * 1000)};
}

/** A Python object may keep this wrapper, but only the callback that
 *  borrowed the native pen may use it. The thread check precedes the
 *  pointer read, so Python worker threads never access render state. */
class BorrowedPen {
 public:
  explicit BorrowedPen(Pen& pen)
      : m_thread(std::this_thread::get_id()), m_pen(&pen) {}

  Pen& get() const {
    if (std::this_thread::get_id() != m_thread)
      throw std::runtime_error("A pen can only be used on its drawing thread.");
    if (!m_pen)
      throw std::runtime_error(
          "This pen is no longer inside its drawing callback.");
    return *m_pen;
  }
  void invalidate() { m_pen = nullptr; }

 private:
  const std::thread::id m_thread;
  Pen* m_pen;
};

/** Shared native descriptions copy this holder without touching Python's
 *  reference count. Python reference changes, calls and traceback formatting
 *  happen while the interpreter lock is held. */
class Callback {
 public:
  explicit Callback(py::function callback)
      : m_callable(callback.release().ptr()) {}
  Callback(const Callback&) = delete;
  Callback& operator=(const Callback&) = delete;
  ~Callback() { clear(); }

  void clear() {
    if (!Py_IsInitialized()) {
      m_callable = nullptr;
      return;
    }
    py::gil_scoped_acquire lock;
    Py_XDECREF(std::exchange(m_callable, nullptr));
  }

  void draw(Pen& pen) const {
    py::gil_scoped_acquire lock;
    if (!m_callable)
      throw std::runtime_error(
          "This drawing callback's sketch session has ended.");
    auto borrowed = std::make_shared<BorrowedPen>(pen);
    struct Invalidate {
      BorrowedPen& pen;
      ~Invalidate() { pen.invalidate(); }
    } invalidate{*borrowed};
    try {
      py::reinterpret_borrow<py::function>(m_callable)(borrowed);
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  }

 private:
  PyObject* m_callable;
};

using PenClass = py::class_<BorrowedPen, std::shared_ptr<BorrowedPen>>;

template <typename R, typename... Args, typename... Extra>
void penMethod(PenClass& cls, const char* name, R (Pen::*method)(Args...),
               Extra&&... extra) {
  cls.def(
      name,
      [method](BorrowedPen& pen, Args... args) -> R {
        return (pen.get().*method)(std::forward<Args>(args)...);
      },
      std::forward<Extra>(extra)...);
}

template <typename R, typename... Args, typename... Extra>
void penMethod(PenClass& cls, const char* name, R (Pen::*method)(Args...) const,
               Extra&&... extra) {
  cls.def(
      name,
      [method](BorrowedPen& pen, Args... args) -> R {
        return (pen.get().*method)(std::forward<Args>(args)...);
      },
      std::forward<Extra>(extra)...);
}

template <typename T>
void penProperty(PenClass& cls, const char* name, T Pen::* member) {
  cls.def_property_readonly(
      name, [member](BorrowedPen& pen) { return pen.get().*member; });
}

SkColor4f penColor(Pen& pen, const py::args& args) {
  if (args.size() == 1 && !py::isinstance<py::float_>(args[0]) &&
      !py::isinstance<py::int_>(args[0]))
    return color(args[0]);
  switch (args.size()) {
    case 1:
      return pen.color(py::cast<float>(args[0]));
    case 2:
      return pen.color(py::cast<float>(args[0]), py::cast<float>(args[1]));
    case 3:
      return pen.color(py::cast<float>(args[0]), py::cast<float>(args[1]),
                       py::cast<float>(args[2]));
    case 4:
      return pen.color(py::cast<float>(args[0]), py::cast<float>(args[1]),
                       py::cast<float>(args[2]), py::cast<float>(args[3]));
    default:
      throw py::type_error(
          "A color takes a string, sequence, or one to four numbers.");
  }
}

void bindPen(py::module_& module) {
  PenClass cls(module, "Pen");
  penProperty(cls, "width", &Pen::width);
  penProperty(cls, "height", &Pen::height);
  penProperty(cls, "frameCount", &Pen::frameCount);
  penProperty(cls, "deltaTime", &Pen::deltaTime);
  penProperty(cls, "mouseX", &Pen::mouseX);
  penProperty(cls, "mouseY", &Pen::mouseY);
  penProperty(cls, "mouseIsPressed", &Pen::mouseIsPressed);
  penProperty(cls, "keyIsPressed", &Pen::keyIsPressed);
  penProperty(cls, "key", &Pen::key);
  penProperty(cls, "keyCode", &Pen::keyCode);
  cls.def("fill", [](BorrowedPen& borrowed, py::args args) {
    auto& pen = borrowed.get();
    pen.fill(penColor(pen, args));
  });
  cls.def("stroke", [](BorrowedPen& borrowed, py::args args) {
    auto& pen = borrowed.get();
    pen.stroke(penColor(pen, args));
  });
  cls.def("background", [](BorrowedPen& borrowed, py::args args) {
    auto& pen = borrowed.get();
    pen.background(penColor(pen, args));
  });
  penMethod(cls, "clear", &Pen::clear);
  penMethod(cls, "noFill", &Pen::noFill);
  penMethod(cls, "noStroke", &Pen::noStroke);
  penMethod(cls, "strokeWeight", &Pen::strokeWeight);
  penMethod(cls, "strokeCap", &Pen::strokeCap);
  penMethod(cls, "strokeJoin", &Pen::strokeJoin);
  penMethod(cls, "smooth", &Pen::smooth);
  penMethod(cls, "noSmooth", &Pen::noSmooth);
  penMethod(cls, "blendMode", &Pen::blendMode);
  penMethod(cls, "rectMode", &Pen::rectMode);
  penMethod(cls, "ellipseMode", &Pen::ellipseMode);
  penMethod(cls, "angleMode",
            py::overload_cast<draw::Constant>(&Pen::angleMode));
  penMethod(cls, "colorMode",
            py::overload_cast<draw::Constant>(&Pen::colorMode));
  penMethod(cls, "colorMode",
            py::overload_cast<draw::Constant, float>(&Pen::colorMode));
  penMethod(cls, "point", py::overload_cast<float, float>(&Pen::point));
  penMethod(cls, "line",
            py::overload_cast<float, float, float, float>(&Pen::line));
  penMethod(cls, "rect",
            py::overload_cast<float, float, float, float>(&Pen::rect));
  penMethod(cls, "rect",
            py::overload_cast<float, float, float, float, float>(&Pen::rect));
  penMethod(cls, "square",
            py::overload_cast<float, float, float>(&Pen::square));
  penMethod(cls, "ellipse",
            py::overload_cast<float, float, float, float>(&Pen::ellipse));
  penMethod(cls, "ellipse",
            py::overload_cast<float, float, float>(&Pen::ellipse));
  penMethod(cls, "circle",
            py::overload_cast<float, float, float>(&Pen::circle));
  penMethod(cls, "arc", &Pen::arc, py::arg("x"), py::arg("y"), py::arg("width"),
            py::arg("height"), py::arg("start"), py::arg("stop"),
            py::arg("mode") = draw::OPEN);
  penMethod(cls, "triangle", &Pen::triangle);
  penMethod(cls, "quad", &Pen::quad);
  penMethod(cls, "bezier", &Pen::bezier);
  penMethod(cls, "beginShape", &Pen::beginShape,
            py::arg("kind") = draw::POLYGON);
  penMethod(cls, "vertex", &Pen::vertex);
  penMethod(cls, "curveVertex", &Pen::curveVertex);
  penMethod(cls, "bezierVertex", &Pen::bezierVertex);
  penMethod(cls, "quadraticVertex", &Pen::quadraticVertex);
  penMethod(cls, "beginContour", &Pen::beginContour);
  penMethod(cls, "endContour", &Pen::endContour);
  penMethod(cls, "endShape", &Pen::endShape, py::arg("mode") = draw::OPEN);
  penMethod(cls, "textSize", &Pen::textSize);
  penMethod(cls, "textFont",
            py::overload_cast<std::string_view>(&Pen::textFont));
  penMethod(cls, "textFont",
            py::overload_cast<std::string_view, float>(&Pen::textFont));
  penMethod(cls, "textAlign",
            py::overload_cast<draw::Constant>(&Pen::textAlign));
  penMethod(cls, "textAlign",
            py::overload_cast<draw::Constant, draw::Constant>(&Pen::textAlign));
  penMethod(cls, "textLeading", py::overload_cast<float>(&Pen::textLeading));
  penMethod(cls, "textStyle", &Pen::textStyle);
  penMethod(cls, "text",
            py::overload_cast<std::string_view, float, float>(&Pen::text));
  penMethod(cls, "text",
            py::overload_cast<std::string_view, float, float, float, float>(
                &Pen::text));
  penMethod(cls, "textWidth", &Pen::textWidth);
  penMethod(cls, "textAscent", &Pen::textAscent);
  penMethod(cls, "textDescent", &Pen::textDescent);
  penMethod(cls, "translate", &Pen::translate);
  penMethod(cls, "rotate", &Pen::rotate);
  penMethod(cls, "scale", py::overload_cast<float>(&Pen::scale));
  penMethod(cls, "scale", py::overload_cast<float, float>(&Pen::scale));
  penMethod(cls, "shearX", &Pen::shearX);
  penMethod(cls, "shearY", &Pen::shearY);
  penMethod(cls, "push", &Pen::push);
  penMethod(cls, "pop", &Pen::pop);
  penMethod(cls, "resetMatrix", &Pen::resetMatrix);
  penMethod(cls, "random", py::overload_cast<>(&Pen::random));
  penMethod(cls, "random", py::overload_cast<float>(&Pen::random));
  penMethod(cls, "random", py::overload_cast<float, float>(&Pen::random));
  penMethod(cls, "randomSeed", &Pen::randomSeed);
  penMethod(cls, "randomGaussian", &Pen::randomGaussian, py::arg("mean") = 0.0f,
            py::arg("sd") = 1.0f);
  penMethod(cls, "noise", &Pen::noise, py::arg("x"), py::arg("y") = 0.0f,
            py::arg("z") = 0.0f);
  penMethod(cls, "noiseSeed", &Pen::noiseSeed);
  penMethod(cls, "noiseDetail", &Pen::noiseDetail);
  penMethod(cls, "millis", &Pen::millis);
  penMethod(cls, "frameRate", py::overload_cast<>(&Pen::frameRate, py::const_));
  penMethod(cls, "frameRate", py::overload_cast<double>(&Pen::frameRate));
  penMethod(cls, "noLoop", &Pen::noLoop);
  penMethod(cls, "loop", &Pen::loop);
  penMethod(cls, "redraw", &Pen::redraw);
  penMethod(cls, "keyIsDown", &Pen::keyIsDown);
}

void bindConstants(py::module_& module) {
  auto constant = py::enum_<draw::Constant>(module, "Constant");
#define SIGIL_PY_CONSTANT(name) constant.value(#name, draw::name)
  SIGIL_PY_CONSTANT(CORNER);
  SIGIL_PY_CONSTANT(CORNERS);
  SIGIL_PY_CONSTANT(CENTER);
  SIGIL_PY_CONSTANT(RADIUS);
  SIGIL_PY_CONSTANT(RADIANS);
  SIGIL_PY_CONSTANT(DEGREES);
  SIGIL_PY_CONSTANT(RGB);
  SIGIL_PY_CONSTANT(HSB);
  SIGIL_PY_CONSTANT(HSL);
  SIGIL_PY_CONSTANT(OPEN);
  SIGIL_PY_CONSTANT(CHORD);
  SIGIL_PY_CONSTANT(PIE);
  SIGIL_PY_CONSTANT(CLOSE);
  SIGIL_PY_CONSTANT(ROUND);
  SIGIL_PY_CONSTANT(SQUARE);
  SIGIL_PY_CONSTANT(PROJECT);
  SIGIL_PY_CONSTANT(MITER);
  SIGIL_PY_CONSTANT(BEVEL);
  SIGIL_PY_CONSTANT(LEFT);
  SIGIL_PY_CONSTANT(RIGHT);
  SIGIL_PY_CONSTANT(TOP);
  SIGIL_PY_CONSTANT(BOTTOM);
  SIGIL_PY_CONSTANT(BASELINE);
  SIGIL_PY_CONSTANT(NORMAL);
  SIGIL_PY_CONSTANT(ITALIC);
  SIGIL_PY_CONSTANT(BOLD);
  SIGIL_PY_CONSTANT(BOLDITALIC);
  SIGIL_PY_CONSTANT(POLYGON);
  SIGIL_PY_CONSTANT(POINTS);
  SIGIL_PY_CONSTANT(LINES);
  SIGIL_PY_CONSTANT(TRIANGLES);
  SIGIL_PY_CONSTANT(TRIANGLE_FAN);
  SIGIL_PY_CONSTANT(TRIANGLE_STRIP);
  SIGIL_PY_CONSTANT(QUADS);
  SIGIL_PY_CONSTANT(QUAD_STRIP);
  SIGIL_PY_CONSTANT(BLEND);
  SIGIL_PY_CONSTANT(ADD);
  SIGIL_PY_CONSTANT(MULTIPLY);
  SIGIL_PY_CONSTANT(SCREEN);
  SIGIL_PY_CONSTANT(REPLACE);
  SIGIL_PY_CONSTANT(REMOVE);
#undef SIGIL_PY_CONSTANT
  constant.export_values();
  module.attr("PI") = draw::PI;
  module.attr("TWO_PI") = draw::TWO_PI;
  module.attr("TAU") = draw::TAU;
  module.attr("HALF_PI") = draw::HALF_PI;
  module.attr("QUARTER_PI") = draw::QUARTER_PI;
}

}  // namespace

struct CallbackLifetime::Impl {
  std::vector<std::weak_ptr<Callback>> callbacks;
  bool closed = false;
};

CallbackLifetime::CallbackLifetime() : m_impl(std::make_unique<Impl>()) {}

CallbackLifetime::~CallbackLifetime() { clear(); }

void CallbackLifetime::clear() {
  const auto close = [&] {
    if (m_impl->closed) return;
    m_impl->closed = true;
    // Releasing a callable can run Python finalizers. Detach the registry
    // first so reentrant teardown cannot invalidate this iteration.
    auto callbacks = std::move(m_impl->callbacks);
    for (const auto& weak : callbacks)
      if (auto callback = weak.lock()) callback->clear();
  };
  if (Py_IsInitialized()) {
    py::gil_scoped_acquire lock;
    close();
  } else {
    close();
  }
}

CallbackScope::CallbackScope(CallbackLifetime& owner)
    : m_previous(std::exchange(currentLifetime, &owner)) {}

CallbackScope::~CallbackScope() { currentLifetime = m_previous; }

void bindDrawing(py::module_& module) {
  auto drawing = module.def_submodule("draw");
  bindConstants(drawing);
  bindPen(drawing);

  auto movement = module.def_submodule("motion");
  py::class_<Animated>(movement, "Transitioned");
  movement.def(
      "entrance",
      [](float start, float stop, double duration, double delay) {
        motion::Transition spec;
        spec.duration = milliseconds(duration);
        spec.delay = milliseconds(delay);
        return motion::animate(motion::from(start).to(stop), spec);
      },
      py::arg("start"), py::arg("stop"), py::arg("duration") = 0.25,
      py::arg("delay") = 0.0);
  movement.def(
      "transition",
      [](float target, double duration, double delay) {
        motion::Transition spec;
        spec.duration = milliseconds(duration);
        spec.delay = milliseconds(delay);
        return motion::animate(motion::to(target), spec);
      },
      py::arg("target"), py::arg("duration") = 0.25, py::arg("delay") = 0.0);

  auto composition = module.def_submodule("compose");
  py::class_<Element> element(composition, "Element");
  element.def("copy", [](const Element& value) { return value; })
      .def("__copy__", [](const Element& value) { return value; })
      .def("row", &Element::row, kFluent)
      .def("column", &Element::column, kFluent)
      .def("grow", &Element::grow, py::arg("factor") = 1.0f, kFluent)
      .def("shrink", &Element::shrink, kFluent)
      .def("absolute", &Element::absolute, kFluent)
      .def("cover", &Element::cover, kFluent)
      .def("key", &Element::key, kFluent)
      .def(
          "children",
          [](Element& self, const std::vector<Element>& values) -> Element& {
            return self.children(values);
          },
          kFluent)
      .def(
          "size",
          [](Element& self, py::object width, py::object height) -> Element& {
            return self.width(dimension(width)).height(dimension(height));
          },
          py::arg("width"), py::arg("height"), kFluent)
      .def(
          "fill",
          [](Element& self, py::object value) -> Element& {
            return self.fill(color(value));
          },
          kFluent)
      .def(
          "ink",
          [](Element& self, py::object value) -> Element& {
            return self.ink(color(value));
          },
          kFluent)
      .def(
          "fontSize",
          [](Element& self, float value) -> Element& {
            return self.font({.size = value});
          },
          kFluent)
      .def(
          "fontWeight",
          [](Element& self, float value) -> Element& {
            return self.font({.weight = value});
          },
          kFluent)
      .def(
          "padding",
          [](Element& self, float all) -> Element& {
            return self.padding(all);
          },
          kFluent)
      .def(
          "padding",
          [](Element& self, float x, float y) -> Element& {
            return self.padding(x, y);
          },
          kFluent)
      .def(
          "padding",
          [](Element& self, float l, float t, float r, float b) -> Element& {
            return self.padding(l, t, r, b);
          },
          kFluent)
      .def(
          "corners",
          [](Element& self, float all) -> Element& {
            return self.corners({all});
          },
          kFluent)
      .def(
          "corners",
          [](Element& self, float tl, float tr, float br,
             float bl) -> Element& { return self.corners({tl, tr, br, bl}); },
          kFluent)
      .def("inset", py::overload_cast<float>(&Element::inset), kFluent)
      .def("inset",
           py::overload_cast<float, float, float, float>(&Element::inset),
           kFluent)
      .def("transformOrigin",
           py::overload_cast<float, float>(&Element::transformOrigin), kFluent)
      .def(
          "alignItems",
          [](Element& self, const std::string& value) -> Element& {
            if (value == "start") return self.alignItems(compose::Align::Start);
            if (value == "end") return self.alignItems(compose::Align::End);
            if (value == "center")
              return self.alignItems(compose::Align::Center);
            if (value == "stretch")
              return self.alignItems(compose::Align::Stretch);
            if (value == "baseline")
              return self.alignItems(compose::Align::Baseline);
            throw py::value_error("Unknown alignment: " + value);
          },
          kFluent)
      .def(
          "justify",
          [](Element& self, const std::string& value) -> Element& {
            if (value == "start") return self.justify(compose::Justify::Start);
            if (value == "end") return self.justify(compose::Justify::End);
            if (value == "center")
              return self.justify(compose::Justify::Center);
            if (value == "space_between")
              return self.justify(compose::Justify::SpaceBetween);
            if (value == "space_around")
              return self.justify(compose::Justify::SpaceAround);
            if (value == "space_evenly")
              return self.justify(compose::Justify::SpaceEvenly);
            throw py::value_error("Unknown justification: " + value);
          },
          kFluent);

  const auto dimensionMethod =
      [&](const char* name, Element& (Element::*setter)(compose::Dimension)) {
        element.def(
            name,
            [setter](Element& self, py::object value) -> Element& {
              return (self.*setter)(dimension(value));
            },
            kFluent);
      };
  dimensionMethod("width", &Element::width);
  dimensionMethod("height", &Element::height);
  dimensionMethod("minWidth", &Element::minWidth);
  dimensionMethod("minHeight", &Element::minHeight);
  dimensionMethod("maxWidth", &Element::maxWidth);
  dimensionMethod("maxHeight", &Element::maxHeight);
  dimensionMethod("gap", &Element::gap);
  dimensionMethod("left", &Element::left);
  dimensionMethod("top", &Element::top);
  dimensionMethod("right", &Element::right);
  dimensionMethod("bottom", &Element::bottom);
  const auto animatedMethod =
      [&](const char* name,
          Element& (Element::*setter)(motion::Animatable<float>)) {
        element.def(
            name,
            [setter](Element& self, py::object value) -> Element& {
              return (self.*setter)(animatable(value));
            },
            kFluent);
      };
  animatedMethod("opacity", &Element::opacity);
  animatedMethod("rotate", &Element::rotate);
  animatedMethod("scale", &Element::scale);
  animatedMethod("scaleX", &Element::scaleX);
  animatedMethod("scaleY", &Element::scaleY);
  animatedMethod("translateX", &Element::translateX);
  animatedMethod("translateY", &Element::translateY);

  composition.def("box", &compose::box);
  composition.def(
      "text",
      [](const std::string& value, float size, py::object ink) {
        return compose::text(value).font({.size = size}).ink(color(ink));
      },
      py::arg("value"), py::arg("size") = 16.0f, py::arg("color") = "#ffffff");
  composition.def(
      "graphics",
      [](const std::string& key, py::function program) {
        auto callback = std::make_shared<Callback>(std::move(program));
        if (currentLifetime) {
          if (currentLifetime->m_impl->closed)
            throw std::runtime_error("This sketch session has ended.");
          auto& callbacks = currentLifetime->m_impl->callbacks;
          callbacks.push_back(callback);
          if (callbacks.size() % 64 == 0)
            std::erase_if(callbacks,
                          [](const auto& weak) { return weak.expired(); });
        }
        return compose::graphics(key,
                                 [callback](Pen& pen) { callback->draw(pen); });
      },
      py::arg("key"), py::arg("program"));
}

}  // namespace sigil::sketch::python
