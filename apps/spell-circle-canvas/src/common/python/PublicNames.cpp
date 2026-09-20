#include <sigilpython/PublicNames.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::python {

namespace py = pybind11;

namespace {

// Where every registered module surfaces in the package an author imports.
// The same table is written out in the package's typing/surface.py, which
// generates the declarations an editor reads; a module missing here keeps
// the extension's own name and is reported by the package's surface check.
// A row whose public module is shared with another row is a feature lifted
// into the catalogue it serves.
constexpr std::array<std::pair<std::string_view, std::string_view>, 38> kPublic{{
    {"", "sigil.sketch"},
    {"compose", "sigil.compose"},
    {"compose.document", "sigil.compose.document"},
    {"compose.kit", "sigil.compose.kit"},
    {"compose.layouts", "sigil.compose.layouts"},
    {"compose.selectors", "sigil.compose.selectors"},
    {"compose.spans", "sigil.compose.spans"},
    {"core", "sigil.core"},
    {"core.chance", "sigil.core.chance"},
    {"data", "sigil.data"},
    {"draw", "sigil.draw"},
    {"draw.brush", "sigil.draw.brush"},
    {"draw.brush.format", "sigil.draw.brush.format"},
    {"geometry", "sigil.geometry"},
    {"geometry.arrange", "sigil.geometry.arrange"},
    {"geometry.mesh", "sigil.geometry.mesh"},
    {"geometry.mesh.camera", "sigil.geometry.mesh.camera"},
    {"geometry.mesh.render", "sigil.geometry.mesh.render"},
    {"image", "sigil.image"},
    {"io", "sigil.io"},
    {"material", "sigil.material"},
    {"material.field", "sigil.material.field"},
    {"material.kit", "sigil.material.kit"},
    {"material.pattern", "sigil.material.pattern"},
    {"material.skia", "sigil.material"},
    {"motion", "sigil.motion"},
    {"motion.ease", "sigil.motion.ease"},
    {"sketch", "sigil.sketch"},
    {"sketch.kit", "sigil.sketch.kit"},
    {"skia", "sigil.skia"},
    {"weave", "sigil.weave"},
    {"weave.flowshape", "sigil.weave.flowshape"},
    {"weave.kit", "sigil.weave.kit"},
    {"weave.selectors", "sigil.weave.selectors"},
    {"world", "sigil.world"},
    {"world.kit", "sigil.world.kit"},
    {"world.light", "sigil.world.light"},
    {"world.selectors", "sigil.world.selectors"},
}};

py::object attribute(const py::module_& root, std::string_view path) {
  py::object found = root;
  size_t start = 0;
  while (start < path.size()) {
    const size_t stop = std::min(path.find('.', start), path.size());
    const std::string step(path.substr(start, stop - start));
    if (!py::hasattr(found, step.c_str())) return py::object();
    found = found.attr(step.c_str());
    start = stop + 1;
  }
  return found;
}

// Whether this value carries a module of its own that can be written. A
// class keeps it in its own dictionary and a bound free function in a slot
// of its own; everything else reads one off its type, where writing would
// either fail or rename the type through one of its values.
bool carriesItsOwnModule(const py::handle& value) {
  return py::isinstance<py::type>(value) || PyCFunction_Check(value.ptr());
}

void nameMembers(const py::object& holder, const std::string& publicName,
                 const std::string& registeredName) {
  for (auto item : holder.attr("__dict__").cast<py::dict>()) {
    const py::handle value = item.second;
    if (!carriesItsOwnModule(value)) continue;
    if (!py::hasattr(value, "__module__")) continue;
    py::object owner = value.attr("__module__");
    if (!py::isinstance<py::str>(owner)) continue;
    // Only what this module registered is renamed. A value another module
    // put here keeps the module that declares it, because __module__ has to
    // name a module that really exports the class: pydoc and
    // inspect.getmodule import it.
    if (owner.cast<std::string>() != registeredName) continue;
    value.attr("__module__") = publicName;
    if (py::isinstance<py::type>(value))
      nameMembers(py::reinterpret_borrow<py::object>(value), publicName,
                  registeredName);
  }
}

}  // namespace

void namePublicly(py::module_& module) {
  const std::string root = module.attr("__name__").cast<std::string>();
  for (const auto& [registered, publicName] : kPublic) {
    const py::object holder = attribute(module, registered);
    if (!holder) continue;
    const std::string qualified =
        registered.empty() ? root : root + "." + std::string(registered);
    nameMembers(holder, std::string(publicName), qualified);
  }
}

}  // namespace sigil::python
