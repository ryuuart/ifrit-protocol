#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/path/Arrange.h>

#include <glm/glm.hpp>

#include "Bindings.h"
#include "ValueBindings.h"

namespace pybind11::detail {
template <glm::length_t N, glm::qualifier Q>
struct type_caster<glm::vec<N, float, Q>> {
  using Value = glm::vec<N, float, Q>;
  PYBIND11_TYPE_CASTER(Value, const_name("tuple"));
  bool load(handle input, bool) {
    if (!isinstance<sequence>(input)) return false;
    const auto seq = reinterpret_borrow<sequence>(input);
    if (seq.size() != N) return false;
    for (glm::length_t i = 0; i < N; ++i)
      value[i] = pybind11::cast<float>(seq[i]);
    return true;
  }
  static handle cast(const Value& input, return_value_policy, handle) {
    tuple result(N);
    for (glm::length_t i = 0; i < N; ++i)
      result[i] = pybind11::float_(input[i]);
    return result.release();
  }
};
}  // namespace pybind11::detail

namespace sigil::sketch::python {
namespace py = pybind11;
namespace mesh = geometry::mesh;
namespace camera = mesh::camera;
namespace render = mesh::render;

void bindGeometry(py::module_& root) {
  auto geometry = root.def_submodule("geometry");
  auto arrange = geometry.def_submodule("arrange");
  py::enum_<geometry::arrange::Turn>(arrange, "Turn")
      .value("Open", geometry::arrange::Turn::Open)
      .value("Closed", geometry::arrange::Turn::Closed);
  arrange.def("step", &geometry::arrange::step)
      .def("along", &geometry::arrange::along)
      .def("onEllipse", &geometry::arrange::onEllipse)
      .def("onRing", &geometry::arrange::onRing);
  py::class_<geometry::arrange::Cell>(arrange, "Cell")
      .def(py::init([](int column, int row) {
             return geometry::arrange::Cell{column, row};
           }),
           py::arg("column") = 0, py::arg("row") = 0)
      .def_readwrite("column", &geometry::arrange::Cell::column)
      .def_readwrite("row", &geometry::arrange::Cell::row);
  arrange.def("cellAt", &geometry::arrange::cellAt)
      .def("moduleSize", &geometry::arrange::moduleSize);
  arrange.def("cellRect", &geometry::arrange::cellRect, py::arg("cell"),
              py::arg("module"), py::arg("gap") = SkSize{0, 0},
              py::arg("origin") = SkPoint{0, 0}, py::arg("columnSpan") = 1,
              py::arg("rowSpan") = 1);
  auto meshes = geometry.def_submodule("mesh");
  auto cameras = meshes.def_submodule("camera");
  auto renderer = meshes.def_submodule("render");
  py::class_<glm::mat4>(cameras, "Matrix")
      .def(py::init([] { return glm::mat4(1); }))
      .def(
          "__matmul__",
          [](const glm::mat4& a, const glm::mat4& b) { return a * b; },
          py::is_operator());
  py::class_<camera::Camera>(cameras, "Camera")
      .def(py::init<>())
      .def_readwrite("eye", &camera::Camera::eye)
      .def_readwrite("target", &camera::Camera::target)
      .def_readwrite("up", &camera::Camera::up)
      .def_readwrite("fovYDeg", &camera::Camera::fovYDeg)
      .def_readwrite("zNear", &camera::Camera::zNear)
      .def_readwrite("zFar", &camera::Camera::zFar)
      .def("project", [](const camera::Camera& camera, glm::vec3 p,
                         std::array<float, 2> viewport) {
        return camera.project(p, {viewport[0], viewport[1]});
      });
  py::class_<camera::Orbit>(cameras, "Orbit")
      .def(py::init([](float yaw, float pitch, float distance) {
             return camera::Orbit{yaw, pitch, distance};
           }),
           py::arg("yawDeg") = 0, py::arg("pitchDeg") = 0,
           py::arg("distance") = 480)
      .def_readwrite("yawDeg", &camera::Orbit::yawDeg)
      .def_readwrite("pitchDeg", &camera::Orbit::pitchDeg)
      .def_readwrite("distance", &camera::Orbit::distance);
  cameras.def("orbitOf", &camera::orbitOf).def("cameraAt", &camera::cameraAt);
  cameras.def("place", &camera::place, py::arg("position") = glm::vec3(0),
              py::arg("yawDeg") = 0, py::arg("pitchDeg") = 0,
              py::arg("rollDeg") = 0, py::arg("scale") = 1);
  cameras.def("faceCamera", &camera::faceCamera, py::arg("eye"), py::arg("at"),
              py::arg("up") = glm::vec3(0, 1, 0));
  py::class_<mesh::Mesh>(meshes, "Mesh")
      .def(py::init<>())
      .def("copy", [](const mesh::Mesh& mesh) { return mesh; })
      .def_readwrite("positions", &mesh::Mesh::positions)
      .def_readwrite("normals", &mesh::Mesh::normals)
      .def_readwrite("uvs", &mesh::Mesh::uvs)
      .def_readwrite("colors", &mesh::Mesh::colors)
      .def_readwrite("indices", &mesh::Mesh::indices)
      .def("vertexCount", &mesh::Mesh::vertexCount)
      .def("triangleCount", &mesh::Mesh::triangleCount)
      .def("append", &mesh::Mesh::append)
      .def("transform", &mesh::Mesh::transform)
      .def("computeNormals", &mesh::Mesh::computeNormals)
      .def("bounds", [](const mesh::Mesh& mesh) {
        glm::vec3 lo, hi;
        mesh.bounds(&lo, &hi);
        return py::make_tuple(lo, hi);
      });
  meshes.def(
      "grid",
      [](int nu, int nv, py::function function) {
        if (nu < 2 || nv < 2 || static_cast<int64_t>(nu) * nv > 4000000)
          throw py::value_error(
              "A mesh grid needs at least two stations per axis and at most "
              "four million vertices.");
        return mesh::grid(nu, nv,
                          core::Callable<glm::vec3(float, float)>{
                              [function](float u, float v) {
                                return function(u, v).cast<glm::vec3>();
                              }});
      },
      py::arg("nu"), py::arg("nv"), py::arg("surface"));
  meshes.def("quad", &mesh::quad);
  py::class_<mesh::BoxOptions>(meshes, "BoxOptions")
      .def(py::init<>())
      .def_readwrite("front", &mesh::BoxOptions::front)
      .def_readwrite("back", &mesh::BoxOptions::back)
      .def_readwrite("left", &mesh::BoxOptions::left)
      .def_readwrite("right", &mesh::BoxOptions::right)
      .def_readwrite("top", &mesh::BoxOptions::top)
      .def_readwrite("bottom", &mesh::BoxOptions::bottom)
      .def_readwrite("tint", &mesh::BoxOptions::tint)
      .def_readwrite("sideShade", &mesh::BoxOptions::sideShade);
  meshes.def("box", &mesh::box, py::arg("lo"), py::arg("hi"),
             py::arg("options") = mesh::BoxOptions{});
  py::enum_<mesh::Platonic>(meshes, "Platonic")
      .value("Tetrahedron", mesh::Platonic::Tetrahedron)
      .value("Cube", mesh::Platonic::Cube)
      .value("Octahedron", mesh::Platonic::Octahedron)
      .value("Dodecahedron", mesh::Platonic::Dodecahedron)
      .value("Icosahedron", mesh::Platonic::Icosahedron);
  meshes.def(
      "platonic",
      [](mesh::Platonic solid, float radius, bool shared) {
        return mesh::platonic(
            solid, {.circumradius = radius, .sharedVertices = shared});
      },
      py::arg("solid"), py::arg("radius") = 1,
      py::arg("sharedVertices") = false);
  meshes.def("torus", &mesh::torus, py::arg("radius"), py::arg("tube"),
             py::arg("nu") = 64, py::arg("nv") = 32);
  meshes.def("superellipsoid", &mesh::superellipsoid, py::arg("radii"),
             py::arg("exponent"), py::arg("nu") = 48, py::arg("nv") = 32);
  meshes.def("cylinderPanel", &mesh::cylinderPanel, py::arg("width"),
             py::arg("height"), py::arg("radius"), py::arg("nu") = 32,
             py::arg("nv") = 8);
  meshes.def(
      "extrude",
      [](const SkPath& path, float depth, float tolerance) {
        return mesh::extrude(path, {.depth = depth, .tolerance = tolerance});
      },
      py::arg("path"), py::arg("depth") = 24, py::arg("tolerance") = 0.25f);
  meshes.def(
      "revolve",
      [](const std::vector<glm::vec2>& profile, int segments, float sweep,
         bool close) {
        return mesh::revolve(
            profile, {.segments = segments, .sweepDeg = sweep, .close = close});
      },
      py::arg("profile"), py::arg("segments") = 48, py::arg("sweepDeg") = 360,
      py::arg("close") = true);
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
}  // namespace sigil::sketch::python
