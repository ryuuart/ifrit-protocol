#include <sigildraw/Color.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilpython/Bindings.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::python {

namespace py = pybind11;

namespace {

std::string loweredTrimmed(std::string_view text) {
  while (!text.empty() && std::isspace((unsigned char)text.front()))
    text.remove_prefix(1);
  while (!text.empty() && std::isspace((unsigned char)text.back()))
    text.remove_suffix(1);
  std::string lowered(text);
  for (char& character : lowered)
    character = (char)std::tolower((unsigned char)character);
  return lowered;
}

}  // namespace

material::Color colorFromText(std::string_view text) {
  if (const std::optional<material::Color> read = material::readColor(text))
    return *read;
  const std::string lowered = loweredTrimmed(text);
  const std::string quoted = "'" + std::string(text) + "'";
  if (lowered == "currentcolor")
    throw py::value_error(
        quoted +
        " names the ink in force, and a flat colour resolves nothing from "
        "the tree. Write it where a fill is taken, or spell "
        "compose.Fill.currentInk().");
  if (lowered.starts_with("var(") && lowered.ends_with(")"))
    throw py::value_error(
        quoted +
        " names a custom property, and a flat colour resolves nothing from "
        "the tree. Write it where a fill is taken, or spell "
        "compose.var(name).");
  throw py::value_error(
      quoted +
      " is no CSS colour. A colour is written as hex (#rgb, #rgba, #rrggbb "
      "or #rrggbbaa), a CSS name such as 'steelblue', or rgb(), hsl(), "
      "hwb(), lab(), lch(), oklab(), oklch(), color() or color-mix().");
}

SkColor4f color(py::handle value) {
  // The colour class itself is read first and read directly: the caster
  // that gives every SkColor4f parameter this reading calls in here, so
  // asking pybind11 to cast to an SkColor4f would ask for this function
  // again.
  if (py::isinstance<material::Color>(value))
    return material::skia::toSkColor(py::cast<material::Color>(value));
  if (py::isinstance<py::str>(value))
    return material::skia::toSkColor(
        colorFromText(py::cast<std::string>(value)));
  if (!py::isinstance<py::tuple>(value) && !py::isinstance<py::list>(value))
    throw py::type_error(
        "A color is a Color, a CSS string, or an RGB or RGBA sequence of "
        "unit floats.");
  auto channels = py::reinterpret_borrow<py::sequence>(value);
  if (channels.size() != 3 && channels.size() != 4)
    throw py::value_error("A color needs three or four channels.");
  float rgba[4] = {0, 0, 0, 1};
  for (py::ssize_t i = 0; i < channels.size(); ++i) {
    rgba[i] = py::cast<float>(channels[i]);
    if (!std::isfinite(rgba[i]) || rgba[i] < 0 || rgba[i] > 1)
      throw py::value_error(
          "Color sequence channels are unit floats between zero and one; "
          "channels on 0..255 are written material.Color.rgb(red, green, "
          "blue).");
  }
  return {rgba[0], rgba[1], rgba[2], rgba[3]};
}

core::angle::Angle angleFromText(std::string_view text) {
  if (const std::optional<core::angle::Angle> read = core::angle::read(text))
    return *read;
  throw py::value_error(
      "'" + std::string(text) +
      "' is no CSS angle. An angle is written as a number with its unit "
      "straight after it: '45deg', '1.5rad', '50grad' or '0.25turn'. A bare "
      "number is written as a number, not as text, and is read in the "
      "measure the slot counts in.");
}

std::optional<core::angle::Angle> writtenAngle(py::handle value) {
  if (py::isinstance<core::angle::Angle>(value))
    return py::cast<core::angle::Angle>(value);
  if (py::isinstance<py::str>(value))
    return angleFromText(py::cast<std::string>(value));
  return std::nullopt;
}

float angleOnPen(const draw::Pen& pen, const AngleInPenMode& angle) {
  if (!angle.written) return angle.number;
  return pen.angleMode() == draw::DEGREES ? angle.written->inDegrees()
                                          : angle.written->inRadians();
}

motion::Duration durationFromText(std::string_view text) {
  if (const std::optional<motion::Duration> read = motion::readDuration(text))
    return *read;
  throw py::value_error(
      "'" + std::string(text) +
      "' is no CSS time. A time is written as a number with its unit "
      "straight after it, '300ms' or '0.3s', or as a number of seconds or a "
      "datetime.timedelta.");
}

namespace {

thread_local CallbackLifetime* currentLifetime = nullptr;

}  // namespace

struct CallbackLifetime::Impl {
  std::vector<std::weak_ptr<PythonValue>> callbacks;
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

PythonValue::PythonValue(py::object value) : m_value(value.release().ptr()) {}
PythonValue::~PythonValue() { clear(); }

py::object PythonValue::get() const {
  if (!m_value)
    throw std::runtime_error("This Python value's lifetime has ended.");
  return py::reinterpret_borrow<py::object>(m_value);
}

void PythonValue::clear() {
  if (!Py_IsInitialized()) {
    m_value = nullptr;
    return;
  }
  const py::gil_scoped_acquire lock;
  Py_XDECREF(std::exchange(m_value, nullptr));
}

std::shared_ptr<PythonValue> retainValue(py::object value) {
  auto retained = std::make_shared<PythonValue>(std::move(value));
  if (currentLifetime) currentLifetime->retain(retained);
  return retained;
}

std::shared_ptr<PythonCallback> retainCallback(py::function function) {
  auto callback = std::make_shared<PythonCallback>(std::move(function));
  if (currentLifetime) currentLifetime->retain(callback);
  return callback;
}

void CallbackLifetime::retain(const std::shared_ptr<PythonValue>& value) {
  if (m_impl->closed)
    throw std::runtime_error("This Python callback lifetime has ended.");
  m_impl->callbacks.push_back(value);
  if (m_impl->callbacks.size() % 64 == 0)
    std::erase_if(m_impl->callbacks,
                  [](const auto& weak) { return weak.expired(); });
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

}  // namespace sigil::python
