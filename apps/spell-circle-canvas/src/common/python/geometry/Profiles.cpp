#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

#include <utility>
#include <vector>

namespace sigil::python {

namespace {

namespace path = geometry::path;

/** Stops written as `(along, width)` pairs. */
std::vector<path::Stop> stopsOf(
    const std::vector<std::pair<float, float>>& pairs) {
  std::vector<path::Stop> stops;
  stops.reserve(pairs.size());
  for (const auto& [along, width] : pairs) stops.push_back({along, width});
  return stops;
}

}  // namespace

void bindGeometryProfiles(pybind11::module_& module) {
  namespace py = pybind11;
  py::module_ paths = submodule(module, "geometry.path");
  py::module_ profiles = submodule(module, "geometry.path.profile");
  submodule(module, "geometry.shapers");

  py::enum_<path::Formation>(paths, "Formation")
      .value("Center", path::Formation::Center)
      .value("Inner", path::Formation::Inner)
      .value("Outer", path::Formation::Outer);
  py::enum_<path::Between>(paths, "Between")
      .value("Linear", path::Between::Linear)
      .value("Step", path::Between::Step)
      .value("Smooth", path::Between::Smooth);

  py::class_<path::Profile> profile(
      paths, "Profile",
      "A width law: how wide a mark is across its spine at each place "
      "along it. A number is one width the whole way; a list of "
      "(along, width) stops runs through them, along a fraction of the "
      "spine's arc length unless inPixels says pixels.");
  profile.def(py::init<float>(), py::arg("width"))
      .def(py::init([](const std::vector<std::pair<float, float>>& stops,
                       path::Between between, bool inPixels) {
             return path::Profile(stopsOf(stops),
                                  {.between = between, .inPixels = inPixels});
           }),
           py::arg("stops"), py::arg("between") = path::Between::Linear,
           py::arg("inPixels") = false)
      .def("across", &path::Profile::across, py::arg("along"),
           "The width at `along`, in the profile's own key.")
      .def("acrossAt", &path::Profile::acrossAt, py::arg("along"),
           py::arg("length"),
           "The width at `along`, a fraction of a spine `length` pixels "
           "long, whichever key the profile is written in.")
      .def("keyedInPx", &path::Profile::keyedInPx)
      .def("max", &path::Profile::max, "The widest the profile ever reaches.")
      .def(py::self == py::self);
  copyProtocol(profile);
  // Wherever a profile is taken, a number is one width and a sequence of
  // pairs a law through stops.
  py::implicitly_convertible<py::float_, path::Profile>();
  py::implicitly_convertible<py::int_, path::Profile>();
  py::implicitly_convertible<py::list, path::Profile>();
  py::implicitly_convertible<py::tuple, path::Profile>();

  profiles.def("offset", &path::profile::offset, py::arg("width"),
               "One width the whole length of the spine.");
  profiles.def("taper", &path::profile::taper, py::arg("start"), py::arg("end"),
               "A width running evenly from `start` at the spine's start "
               "to `end` at its end.");
  profiles.def("steps", &path::profile::steps, py::arg("upTo"),
               py::arg("widths"),
               "A stepped width: `widths` holds each step's width and "
               "`upTo` the fraction of arc length each step ends at, one "
               "fewer than the widths.");
}

}  // namespace sigil::python
