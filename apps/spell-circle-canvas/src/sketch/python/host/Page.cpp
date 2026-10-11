/** @file
 * A page a baked 3D frame lights, as Python holds it: the pass that paints
 * a compose tree as the maps of a surface, and the material a body wears
 * to show it.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilpython/Extend.h>
#include <sigilsketch/scene3d/Page.h>
#include <sigilworld/frame/Pass.h>

#include <string>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;

void bindSketchPage(py::module_& module) {
  auto sketches = sigil::python::submodule(module, "sketch");
  py::class_<Page>(
      sketches, "Page",
      "A compose tree painted as the maps of a surface by a pass on a 3D "
      "frame, and the one material a body wears to show it, lit by the "
      "frame's own lights and environment. Made by ctx.page((width, "
      "height)); put page.render(tree) on the frame with pass_ and "
      "page.surface() on the body that shows it.")
      .def(py::init<>(),
           "No page: its pass paints nothing and its surface is the stock "
           "one.")
      .def(
          "render",
          [](const Page& page, const compose::Element& tree) {
            return page.render(tree);
          },
          py::arg("tree"),
          "The pass that paints tree as this page's maps at the frame's "
          "time, before anything draws a body. A tree that has not changed "
          "since the page last painted is not painted again.")
      .def("surface", py::overload_cast<>(&Page::surface, py::const_),
           "The one material a body wears to show this page. A body wearing "
           "it on a frame without the page's pass is an error naming both.")
      .def("surface",
           py::overload_cast<material::PixelRect>(&Page::surface, py::const_),
           py::arg("region"),
           "The material a body wears to show one part of this page: "
           "region, a material.PixelRect of page pixels, spread across the "
           "body's texture coordinates as the whole page is by surface(). "
           "One page lays its parts out and each body wears its own; the "
           "light, depth and displacement its maps are read against are the "
           "whole page's.")
      .def_property_readonly(
          "name", [](const Page& page) { return std::string(page.name()); },
          "The name the page's pass goes by, and that an error names: "
          "\"page\" for the first a session made, numbered after it.");
}

}  // namespace sigil::sketch::python
