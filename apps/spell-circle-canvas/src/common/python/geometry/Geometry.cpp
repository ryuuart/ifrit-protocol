#include <pybind11/stl.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>
#include <sigilpython/skia/Values.h>

#include <glm/glm.hpp>

namespace sigil::python {
namespace py = pybind11;
namespace mesh = geometry::mesh;
namespace camera = mesh::camera;
namespace render = mesh::render;

void bindGeometry(py::module_& root) {
  auto arrange = submodule(root, "geometry.arrange");
  py::enum_<geometry::arrange::Turn>(arrange, "Turn")
      .value("Open", geometry::arrange::Turn::Open)
      .value("Closed", geometry::arrange::Turn::Closed);
  arrange
      .def("step", &geometry::arrange::step, py::arg("extent"),
           py::arg("count"), py::arg("turn"))
      .def("along", &geometry::arrange::along, py::arg("start"),
           py::arg("extent"), py::arg("index"), py::arg("count"),
           py::arg("turn"))
      .def("onEllipse", &geometry::arrange::onEllipse, py::arg("center"),
           py::arg("radii"), py::arg("radians"))
      .def("onRing", &geometry::arrange::onRing, py::arg("index"),
           py::arg("count"), py::arg("center"), py::arg("radii"),
           py::arg("startRadians"), py::arg("sweepRadians"), py::arg("turn"));
  py::class_<geometry::arrange::Cell>(arrange, "Cell")
      .def(py::init([](int column, int row) {
             return geometry::arrange::Cell{column, row};
           }),
           py::arg("column") = 0, py::arg("row") = 0)
      .def_readwrite("column", &geometry::arrange::Cell::column)
      .def_readwrite("row", &geometry::arrange::Cell::row);
  arrange
      .def("cellAt", &geometry::arrange::cellAt, py::arg("index"),
           py::arg("columns"))
      .def("moduleSize", &geometry::arrange::moduleSize, py::arg("container"),
           py::arg("columns"), py::arg("rows"), py::arg("gap"));
  arrange.def("cellRect", &geometry::arrange::cellRect, py::arg("cell"),
              py::arg("module"), py::arg("gap") = SkSize{0, 0},
              py::arg("origin") = SkPoint{0, 0}, py::arg("columnSpan") = 1,
              py::arg("rowSpan") = 1);
  auto renderer = submodule(root, "geometry.mesh.render");
  py::class_<render::Light>(renderer, "Light")
      .def(py::init([](glm::vec3 direction, SkColor4f color, float intensity) {
             return render::Light{direction, color, intensity};
           }),
           py::arg("direction") = glm::vec3(-0.5f, -0.8f, -0.4f),
           py::arg("color") = SkColors::kWhite, py::arg("intensity") = 1)
      .def_readwrite("direction", &render::Light::direction)
      .def_readwrite("color", &render::Light::color)
      .def_readwrite("intensity", &render::Light::intensity);
  py::enum_<render::MeshStyle::Mode>(renderer, "Mode")
      .value("Lit", render::MeshStyle::Mode::Lit)
      .value("Normals", render::MeshStyle::Mode::Normals)
      .value("Uv", render::MeshStyle::Mode::Uv);
  py::class_<render::MeshStyle>(renderer, "MeshStyle")
      .def(py::init<>())
      .def_readwrite("mode", &render::MeshStyle::mode)
      .def_readwrite("lit", &render::MeshStyle::lit)
      .def_readwrite("baseColor", &render::MeshStyle::baseColor)
      .def_readwrite("lights", &render::MeshStyle::lights)
      .def_readwrite("ambient", &render::MeshStyle::ambient)
      .def_readwrite("metallic", &render::MeshStyle::metallic)
      .def_readwrite("roughness", &render::MeshStyle::roughness)
      .def_readwrite("specular", &render::MeshStyle::specular)
      .def_readwrite("shininess", &render::MeshStyle::shininess)
      .def_readwrite("rim", &render::MeshStyle::rim)
      .def_readwrite("texture", &render::MeshStyle::texture)
      .def_readwrite("uvTransform", &render::MeshStyle::uvTransform)
      .def_readwrite("tileTexture", &render::MeshStyle::tileTexture)
      .def_readwrite("backfaceCull", &render::MeshStyle::backfaceCull)
      .def_readwrite("depthSort", &render::MeshStyle::depthSort);
  renderer.def(
      "drawMesh",
      [](py::handle borrowed, const mesh::Mesh& mesh, const glm::mat4& model,
         const camera::Camera& camera, const render::MeshStyle& style) {
        for (auto index : mesh.indices)
          if (index >= mesh.positions.size())
            throw py::value_error(
                "A mesh index is outside the position array.");
        auto& p = pen(borrowed);
        render::drawMesh(*p.canvas(), mesh, model, camera, {p.width, p.height},
                         style);
      },
      py::arg("pen"), py::arg("mesh"), py::arg("model"), py::arg("camera"),
      py::arg("style") = render::MeshStyle{});
  renderer.def(
      "drawImagePanel",
      [](py::handle borrowed, sk_sp<SkImage> image, float width, float height,
         const glm::mat4& model, const camera::Camera& camera, float opacity) {
        auto& p = pen(borrowed);
        render::drawImagePanel(*p.canvas(), std::move(image), width, height,
                               model, camera, {p.width, p.height}, opacity);
      },
      py::arg("pen"), py::arg("image"), py::arg("width"), py::arg("height"),
      py::arg("model"), py::arg("camera"), py::arg("opacity") = 1);
}
}  // namespace sigil::python
