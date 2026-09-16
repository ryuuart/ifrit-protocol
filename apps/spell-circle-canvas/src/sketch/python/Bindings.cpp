#include "Bindings.h"

#include <sigildraw/Color.h>
#include <sigildraw/Pen.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "KitBindings.h"

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
    const KitScopeBoundary boundary;
    function(borrowed);
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

PythonValue::PythonValue(py::object value) : m_value(value.release().ptr()) {}
PythonValue::~PythonValue() { clear(); }

py::object PythonValue::get() const {
  if (!m_value)
    throw std::runtime_error("This Python value's sketch session has ended.");
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
    throw std::runtime_error("This sketch session has ended.");
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

}  // namespace sigil::sketch::python
