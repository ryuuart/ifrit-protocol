#include <pybind11/stl.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Faces.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/Vec.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace mesh = geometry::mesh;

namespace {

// A SHEET IS EVALUATED ONCE PER VERTEX, on this thread and inside this
// call, so a grid larger than a machine can hold is refused before the
// first evaluation rather than part way through one.
constexpr std::int64_t kMaximumGridVertices = 4000000;

void checkGridExtent(int nu, int nv) {
  if (nu < 2 || nv < 2 ||
      static_cast<std::int64_t>(nu) * nv > kMaximumGridVertices)
    throw py::value_error(
        "A mesh grid needs at least two stations per axis and at most four "
        "million vertices.");
}

}  // namespace

void bindGeometryMesh(py::module_& module) {
  auto meshes = submodule(module, "geometry.mesh");

  auto meshClass =
      bindRecord<mesh::Mesh>(meshes, "Mesh", "A mesh has no field named ");
  meshClass.def_readwrite("positions", &mesh::Mesh::positions)
      .def_readwrite("normals", &mesh::Mesh::normals)
      .def_readwrite("uvs", &mesh::Mesh::uvs)
      .def_readwrite("colors", &mesh::Mesh::colors)
      .def_readwrite("indices", &mesh::Mesh::indices)
      .def("vertexCount", &mesh::Mesh::vertexCount)
      .def("triangleCount", &mesh::Mesh::triangleCount)
      .def("append", &mesh::Mesh::append, py::arg("mesh"))
      .def("transform", &mesh::Mesh::transform, py::arg("matrix"))
      .def("computeNormals", &mesh::Mesh::computeNormals)
      .def("bounds",
           [](const mesh::Mesh& value) {
             glm::vec3 low, high;
             value.bounds(&low, &high);
             return py::make_tuple(low, high);
           })
      // A PRIMITIVE LANE IS READ AND WRITTEN AS TWO VERBS, never as one
      // reference. The native lane is a vector inside a map that
      // reallocates whenever another lane is created or another mesh is
      // appended, so what Python holds is a copy; a list handed back
      // would otherwise appear to be writable and quietly write nothing.
      .def(
          "primitive",
          [](mesh::Mesh& value, const std::string& name,
             glm::vec4 fill) -> std::vector<glm::vec4> {
            return value.primitive(name, fill);
          },
          py::arg("name"), py::arg("fill") = glm::vec4{1, 1, 1, 1})
      .def(
          "primitiveIf",
          [](const mesh::Mesh& value,
             std::string_view name) -> std::optional<std::vector<glm::vec4>> {
            const std::vector<glm::vec4>* lane = value.primitiveIf(name);
            if (lane == nullptr) return std::nullopt;
            return *lane;
          },
          py::arg("name"))
      .def(
          "setPrimitive",
          [](mesh::Mesh& value, const std::string& name,
             const std::vector<glm::vec4>& values) {
            // A lane holds one value per triangle, and every reader of
            // one treats a short lane as no lane at all, so the length
            // is checked here rather than discovered as a mesh that
            // draws untinted.
            if (values.size() != value.triangleCount())
              throw py::value_error(
                  "The primitive lane " + name +
                  " holds one value per triangle: " +
                  std::to_string(value.triangleCount()) + " wanted, " +
                  std::to_string(values.size()) + " given.");
            value.primitive(name) = values;
          },
          py::arg("name"), py::arg("values"))
      .def("primitiveNames", &mesh::Mesh::primitiveNames)
      .def(
          "__eq__",
          [](const mesh::Mesh& value, const mesh::Mesh& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator());

  // An edge is a value: two corners and the two faces they stand
  // between. It is hashed by all four so a reader can put the edges of a
  // solid in a set and ask which of them another solid shares. `from` is
  // a Python keyword, so the field is spelled with the trailing
  // underscore that escapes one, which is the spelling the declarations
  // carry.
  auto edgeClass =
      bindRecord<mesh::Edge>(meshes, "Edge", "An edge has no field named ");
  edgeClass.def_readwrite("from_", &mesh::Edge::from)
      .def_readwrite("to", &mesh::Edge::to)
      .def_readwrite("face", &mesh::Edge::face)
      .def_readwrite("opposite", &mesh::Edge::opposite)
      .def(
          "__eq__",
          [](const mesh::Edge& value, const mesh::Edge& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator())
      .def("__hash__",
           [](const mesh::Edge& value) {
             return py::hash(py::make_tuple(value.from, value.to, value.face,
                                            value.opposite));
           })
      // The reading is the call that rebuilds it, so an edge printed in
      // a session can be pasted back into one.
      .def("__repr__", [](const mesh::Edge& value) {
        return "Edge(from_=" + std::to_string(value.from) +
               ", to=" + std::to_string(value.to) +
               ", face=" + std::to_string(value.face) +
               ", opposite=" + std::to_string(value.opposite) + ")";
      });
  meshes.attr("kNoFace") = mesh::kNoFace;

  auto boxOptions = bindRecord<mesh::BoxOptions>(meshes, "BoxOptions",
                                                 "A box has no option named ");
  boxOptions.def_readwrite("front", &mesh::BoxOptions::front)
      .def_readwrite("back", &mesh::BoxOptions::back)
      .def_readwrite("left", &mesh::BoxOptions::left)
      .def_readwrite("right", &mesh::BoxOptions::right)
      .def_readwrite("top", &mesh::BoxOptions::top)
      .def_readwrite("bottom", &mesh::BoxOptions::bottom)
      .def_readwrite("tint", &mesh::BoxOptions::tint)
      .def_readwrite("sideShade", &mesh::BoxOptions::sideShade);

  auto extrudeOptions = bindRecord<mesh::ExtrudeOptions>(
      meshes, "ExtrudeOptions", "An extrusion has no option named ");
  extrudeOptions.def_readwrite("depth", &mesh::ExtrudeOptions::depth)
      .def_readwrite("tolerance", &mesh::ExtrudeOptions::tolerance)
      .def_readwrite("frontCap", &mesh::ExtrudeOptions::frontCap)
      .def_readwrite("backCap", &mesh::ExtrudeOptions::backCap)
      .def_readwrite("walls", &mesh::ExtrudeOptions::walls);

  auto revolveOptions = bindRecord<mesh::RevolveOptions>(
      meshes, "RevolveOptions", "A revolution has no option named ");
  revolveOptions.def_readwrite("segments", &mesh::RevolveOptions::segments)
      .def_readwrite("sweepDeg", &mesh::RevolveOptions::sweepDeg)
      .def_readwrite("close", &mesh::RevolveOptions::close);

  auto platonicOptions = bindRecord<mesh::PlatonicOptions>(
      meshes, "PlatonicOptions", "A regular solid has no option named ");
  platonicOptions
      .def_readwrite("circumradius", &mesh::PlatonicOptions::circumradius)
      .def_readwrite("sharedVertices", &mesh::PlatonicOptions::sharedVertices);

  py::enum_<mesh::Platonic>(meshes, "Platonic")
      .value("Tetrahedron", mesh::Platonic::Tetrahedron)
      .value("Cube", mesh::Platonic::Cube)
      .value("Octahedron", mesh::Platonic::Octahedron)
      .value("Dodecahedron", mesh::Platonic::Dodecahedron)
      .value("Icosahedron", mesh::Platonic::Icosahedron);

  // WHAT THE FORMULA ANSWERS DECIDES WHERE THE NORMALS COME FROM, and
  // pybind11 cannot tell two callables apart by what they answer, so the
  // caller says which of the two native overloads to call. Left alone
  // the sheet is differenced, which costs four extra evaluations per
  // vertex and has no direction to normalize at a pole; asked for
  // normals, the formula answers the position and its normal as a pair
  // and is taken at its word.
  meshes.def(
      "grid",
      [](int nu, int nv, const py::function& surface, bool normals) {
        checkGridExtent(nu, nv);
        if (normals)
          return mesh::grid(
              nu, nv,
              core::Callable<std::pair<glm::vec3, glm::vec3>(float, float)>{
                  [surface](float u, float v) {
                    return surface(u, v)
                        .cast<std::pair<glm::vec3, glm::vec3>>();
                  }});
        return mesh::grid(nu, nv,
                          core::Callable<glm::vec3(float, float)>{
                              [surface](float u, float v) {
                                return surface(u, v).cast<glm::vec3>();
                              }});
      },
      py::arg("nu"), py::arg("nv"), py::arg("surface"),
      py::arg("normals") = false);
  meshes.def("quad", &mesh::quad, py::arg("width"), py::arg("height"));
  meshes.def("box", &mesh::box, py::arg("lo"), py::arg("hi"),
             py::arg("options") = mesh::BoxOptions{});

  // EACH GENERATOR WITH OPTIONS IS SPELLED TWICE: once taking the record,
  // which is the whole of what the native call offers and can be held
  // and re-edited, and once taking the fields a caller most often sets,
  // which is the shorter line at a call site that sets one of them. The
  // record form is registered first, so a caller who hands one is not
  // asked to convert it to a number.
  meshes.def("platonic", &mesh::platonic, py::arg("solid"),
             py::arg("options") = mesh::PlatonicOptions{});
  meshes.def(
      "platonic",
      [](mesh::Platonic solid, float radius, bool sharedVertices) {
        return mesh::platonic(
            solid, {.circumradius = radius, .sharedVertices = sharedVertices});
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
  meshes.def("extrude", &mesh::extrude, py::arg("path"),
             py::arg("options") = mesh::ExtrudeOptions{});
  meshes.def(
      "extrude",
      [](const SkPath& path, float depth, float tolerance) {
        return mesh::extrude(path, {.depth = depth, .tolerance = tolerance});
      },
      py::arg("path"), py::arg("depth") = 24, py::arg("tolerance") = 0.25f);
  meshes.def("revolve", &mesh::revolve, py::arg("profile"),
             py::arg("options") = mesh::RevolveOptions{});
  meshes.def(
      "revolve",
      [](const std::vector<glm::vec2>& profile, int segments, float sweepDeg,
         bool close) {
        return mesh::revolve(
            profile,
            {.segments = segments, .sweepDeg = sweepDeg, .close = close});
      },
      py::arg("profile"), py::arg("segments") = 48, py::arg("sweepDeg") = 360,
      py::arg("close") = true);
  meshes.def("bakePrimitiveColor", &mesh::bakePrimitiveColor, py::arg("mesh"),
             py::arg("lane") = "Color");

  meshes.def("normalized", &mesh::normalized, py::arg("vector"),
             py::arg("fallback") = glm::vec3{0, 0, 1});
  // The three axes come back as a triple rather than through three
  // outputs a caller would have to make first. It is the basis a stamp
  // is placed with, so a lane written by hand orients the way the
  // instancing path does.
  meshes.def(
      "basisFor",
      [](glm::vec3 direction, glm::vec3 up) {
        glm::vec3 xAxis, yAxis, zAxis;
        mesh::basisFor(direction, up, &xAxis, &yAxis, &zAxis);
        return py::make_tuple(xAxis, yAxis, zAxis);
      },
      py::arg("direction"), py::arg("up"));

  meshes.def("faceCount", &mesh::faceCount, py::arg("mesh"));
  meshes.def("faceNormal", &mesh::faceNormal, py::arg("mesh"),
             py::arg("face"));
  meshes.def("faceCentroid", &mesh::faceCentroid, py::arg("mesh"),
             py::arg("face"));
  meshes.def("opposedFace", &mesh::opposedFace, py::arg("mesh"),
             py::arg("face"), py::arg("tolerance") = 1e-3f);
  meshes.def("edges", &mesh::edges, py::arg("mesh"));
  meshes.def("faceUp", &mesh::faceUp, py::arg("mesh"), py::arg("face"),
             py::arg("up") = glm::vec3{0, 1, 0});
}

}  // namespace sigil::python
