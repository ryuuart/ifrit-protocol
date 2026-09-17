#pragma once

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>

#include <vector>

namespace sigil::python {
compose::Dimension dimension(pybind11::handle value);
compose::Fill fill(pybind11::handle value);
compose::SurfacePaint surfacePaint(pybind11::handle value);
compose::Align alignment(pybind11::handle value);
compose::Justify justification(pybind11::handle value);
compose::Shape shape(pybind11::handle value);
std::vector<compose::Element> elements(pybind11::args children);
void bindCompose(pybind11::module_& module);
}  // namespace sigil::python
