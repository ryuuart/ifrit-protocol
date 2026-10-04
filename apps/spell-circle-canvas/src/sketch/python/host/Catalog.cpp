/** @file
 * The registry and a catalog of sketch files, as the rows a script reads.
 */

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilsketch/core/Catalog.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;

void bindSketchCatalog(py::module_& module) {
  auto sketches = module.def_submodule("sketch");
  py::class_<SourceMetadata>(sketches, "SourceMetadata")
      .def_readonly("subject", &SourceMetadata::subject)
      .def_readonly("editFirst", &SourceMetadata::editFirst)
      .def_readonly("tags", &SourceMetadata::tags)
      .def_readonly("lines", &SourceMetadata::lines);
  py::class_<RegistryRow>(sketches, "RegistryRow")
      .def_readonly("name", &RegistryRow::name)
      .def_readonly("key", &RegistryRow::key)
      .def_readonly("category", &RegistryRow::category)
      .def_readonly("blurb", &RegistryRow::blurb)
      .def_readonly("kind", &RegistryRow::kind)
      .def_readonly("available", &RegistryRow::available)
      .def_readonly("reason", &RegistryRow::reason);
  py::class_<CatalogRow, RegistryRow>(sketches, "CatalogRow")
      .def_readonly("index", &CatalogRow::index)
      .def_readonly("title", &CatalogRow::title)
      .def_readonly("path", &CatalogRow::path)
      .def_readonly("entryPath", &CatalogRow::entryPath)
      .def_readonly("external", &CatalogRow::external)
      .def_readonly("videoExportable", &CatalogRow::videoExportable)
      .def_readonly("source", &CatalogRow::source);
  // The registry is the process's: in a host it holds every sketch the
  // application was built with, and in a plain interpreter the extension
  // registers none.
  sketches.def("registryRows", &registryRows, py::arg("kind") = "");
  sketches.def(
      "catalog",
      [](const std::vector<std::filesystem::path>& files,
         const std::optional<std::filesystem::path>& sketchDirectory,
         const std::optional<std::filesystem::path>& workspace) {
        return catalog({.sketchDirectory = sketchDirectory.value_or(""),
                        .files = files,
                        .workspaceRoot = workspace.value_or("")});
      },
      py::arg("files") = std::vector<std::filesystem::path>{},
      py::arg("sketchDirectory") = py::none(),
      py::arg("workspace") = py::none());
}

}  // namespace sigil::sketch::python
