#include "Bindings.h"

#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcore/compute/Chance.h>
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

#include "PenBindings.h"

namespace sigil::sketch::python {

namespace py = pybind11;

SkColor4f color(py::handle value) {
  if (py::isinstance<SkColor4f>(value)) return py::cast<SkColor4f>(value);
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

}  // namespace

struct CallbackLifetime::Impl {
  std::vector<std::weak_ptr<PythonCallback>> callbacks;
  bool closed = false;
};

BorrowedPen::BorrowedPen(draw::Pen& pen)
    : m_thread(std::this_thread::get_id()), m_pen(&pen) {}

draw::Pen& BorrowedPen::get() const {
  if (std::this_thread::get_id() != m_thread)
    throw std::runtime_error("A pen can only be used on its drawing thread.");
  if (!m_pen)
    throw std::runtime_error(
        "This pen is no longer inside its drawing callback.");
  return *m_pen;
}

void BorrowedPen::invalidate() {
  if (!m_pen) return;
  auto cleanup = std::move(m_cleanup);
  for (auto it = cleanup.rbegin(); it != cleanup.rend(); ++it) (*it)();
  m_pen = nullptr;
}

void BorrowedPen::whenClosed(std::function<void()> cleanup) {
  (void)get();
  m_cleanup.push_back(std::move(cleanup));
}

draw::Pen& pen(py::handle value) {
  const auto borrowed = py::cast<std::shared_ptr<BorrowedPen>>(value);
  if (!borrowed) throw py::type_error("Drawing requires a pen.");
  return borrowed->get();
}

void invokePen(const py::function& function, draw::Pen& native) {
  const py::gil_scoped_acquire lock;
  auto borrowed = std::make_shared<BorrowedPen>(native);
  struct Invalidate {
    BorrowedPen& value;
    ~Invalidate() { value.invalidate(); }
  } invalidate{*borrowed};
  try {
    function(borrowed);
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

PythonCallback::PythonCallback(py::function function)
    : m_callable(function.release().ptr()) {}
PythonCallback::~PythonCallback() { clear(); }

py::function PythonCallback::get() const {
  if (!m_callable)
    throw std::runtime_error(
        "This drawing callback's sketch session has ended.");
  return py::reinterpret_borrow<py::function>(m_callable);
}

void PythonCallback::clear() {
  if (!Py_IsInitialized()) {
    m_callable = nullptr;
    return;
  }
  const py::gil_scoped_acquire lock;
  Py_XDECREF(std::exchange(m_callable, nullptr));
}

std::shared_ptr<PythonCallback> retainCallback(py::function function) {
  auto callback = std::make_shared<PythonCallback>(std::move(function));
  if (currentLifetime) {
    if (currentLifetime->m_impl->closed)
      throw std::runtime_error("This sketch session has ended.");
    auto& callbacks = currentLifetime->m_impl->callbacks;
    callbacks.push_back(callback);
    if (callbacks.size() % 64 == 0)
      std::erase_if(callbacks, [](const auto& weak) { return weak.expired(); });
  }
  return callback;
}

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
  auto chance = module.def_submodule("core").def_submodule("chance");
  namespace chanceNative = core::chance;
  py::enum_<chanceNative::Source>(chance, "Source")
      .value("Pcg", chanceNative::Source::Pcg)
      .value("Mix64", chanceNative::Source::Mix64)
      .value("Xorshift", chanceNative::Source::Xorshift)
      .value("Halton", chanceNative::Source::Halton)
      .value("Sobol", chanceNative::Source::Sobol)
      .value("Golden", chanceNative::Source::Golden)
      .value("Stratified", chanceNative::Source::Stratified);
  using Stream = chanceNative::Stream;
  py::class_<Stream>(chance, "Stream")
      .def(py::init<>())
      .def_static("pcg", &Stream::pcg)
      .def_static("mix64", &Stream::mix64)
      .def_static("xorshift", &Stream::xorshift)
      .def_static("halton", &Stream::halton, py::arg("base"),
                  py::arg("skip") = 0)
      .def_static("sobol", &Stream::sobol, py::arg("skip") = 0)
      .def_static("golden", &Stream::golden, py::arg("seed") = 0)
      .def_static("stratified", &Stream::stratified, py::arg("strata"),
                  py::arg("seed") = 0)
      .def_static("of", &Stream::of, py::arg("source"), py::arg("seed"),
                  py::arg("parameter") = 0)
      .def("copy", [](const Stream& self) { return self; })
      .def("__copy__", [](const Stream& self) { return self; })
      .def("bits", &Stream::bits)
      .def("unit", &Stream::unit)
      .def("signedUnit", &Stream::signedUnit)
      .def("range", &Stream::range)
      .def("below", &Stream::below)
      .def("normal", &Stream::normal)
      .def("source", &Stream::source)
      .def("parameter", &Stream::parameter)
      .def("drawn", &Stream::drawn);
  auto drawing = module.def_submodule("draw");
  bindConstants(drawing);

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
  py::enum_<compose::Cache>(composition, "Cache")
      .value("Auto", compose::Cache::Auto)
      .value("Picture", compose::Cache::Picture)
      .value("Texture", compose::Cache::Texture)
      .value("Group", compose::Cache::Group)
      .value("None_", compose::Cache::None);
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
      .def("cache", &Element::cache, kFluent)
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
            if (py::isinstance<material::skia::Paint>(value))
              return self.fill(py::cast<material::skia::Paint>(value));
            return self.fill(color(value));
          },
          kFluent)
      .def(
          "ink",
          [](Element& self, py::object value) -> Element& {
            return self.ink(color(value));
          },
          kFluent)
      .def("font", &Element::font, kFluent)
      .def(
          "fontTrack",
          [](Element& self, float value) -> Element& {
            return self.font({.track = value});
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
      [](const std::string& value, py::object size, py::object ink) {
        auto element = compose::text(value);
        if (!size.is_none()) element.font({.size = py::cast<float>(size)});
        if (!ink.is_none()) element.ink(color(ink));
        return element;
      },
      py::arg("value"), py::arg("size") = py::none(),
      py::arg("color") = py::none());
  composition.def(
      "graphics",
      [](const std::string& key, py::function program, compose::Cache cache) {
        auto callback = retainCallback(std::move(program));
        return compose::graphics(
            key,
            [callback](Pen& pen) {
              const py::gil_scoped_acquire lock;
              invokePen(callback->get(), pen);
            },
            cache);
      },
      py::arg("key"), py::arg("program"),
      py::arg("cache") = compose::Cache::None);
  composition.def(
      "pen",
      [](const std::string& key, py::function program, compose::Cache cache) {
        auto callback = retainCallback(std::move(program));
        return compose::pen(
            key,
            [callback](Pen& pen) {
              const py::gil_scoped_acquire lock;
              invokePen(callback->get(), pen);
            },
            cache);
      },
      py::arg("key"), py::arg("program"),
      py::arg("cache") = compose::Cache::None);
  bindPen(drawing);
}

}  // namespace sigil::sketch::python
