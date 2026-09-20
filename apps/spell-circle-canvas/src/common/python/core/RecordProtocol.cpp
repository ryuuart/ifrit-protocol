#include <sigilmaterial/color/Color.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/core/Registration.h>

namespace sigil::python {

// THE PROTOCOL IS A TEMPLATE, NOT A REGISTRATION. `copyProtocol` in
// Bindings.h is what gives a bound value `__copy__` and `__deepcopy__`,
// and `bindRecord` applies it as each record is registered, so nearly
// every record answers the protocol without anything being registered
// here. What is left for this function is the value that is spelled by
// hand instead of through `bindRecord` and is registered early enough to
// reach: the colour class, which a memoised model holds more often than
// any other bound value.
void bindRecordProtocol(pybind11::module_& module) {
  auto color = extend<material::Color>(module, "material.Color");
  copyProtocol(color);
}

}  // namespace sigil::python
