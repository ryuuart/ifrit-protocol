#include <pybind11/stl.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace camera = geometry::mesh::camera;

namespace {

// A matrix is four columns of four floats, and that is also the order
// its sixteen numbers stand in memory.
constexpr glm::length_t kAxisCount = 4;
constexpr py::ssize_t kValueCount = 16;

/** @p index as a column or a row, counting back from the last one when
 *  it is negative, as an index into a Python sequence does. @p axis
 *  names which of the two the message reports. */
glm::length_t matrixIndex(glm::length_t index, const char* axis) {
  const glm::length_t within = index < 0 ? index + kAxisCount : index;
  if (within < 0 || within >= kAxisCount)
    throw py::index_error(std::string("A matrix ") + axis +
                          " is a number from 0 to 3.");
  return within;
}

/** A viewport read from @p value. The frustum extent a camera answers is
 *  a size, so a size passes straight back into the calls that take one;
 *  two numbers are the shorter spelling of the same thing. */
SkSize viewportSize(py::handle value) {
  if (py::isinstance<SkSize>(value)) return py::cast<SkSize>(value);
  try {
    const auto pair = py::cast<std::array<float, 2>>(value);
    return SkSize{pair[0], pair[1]};
  } catch (const py::cast_error&) {
    throw py::type_error(
        "A viewport is a size or two numbers: its width and its height.");
  }
}

/** A device's target extent read from @p value, which is whole pixels
 *  and at least one of them along each axis: a projection onto no
 *  pixels has no answer. */
SkISize clipExtent(std::array<int, 2> value) {
  if (value[0] <= 0 || value[1] <= 0)
    throw py::value_error("A clip extent is positive along both axes.");
  return SkISize{value[0], value[1]};
}

/** The sixteen floats of @p value, in the order they stand in memory. */
std::vector<float> matrixValues(const glm::mat4& value) {
  const float* start = glm::value_ptr(value);
  return std::vector<float>(start, start + kValueCount);
}

}  // namespace

void bindGeometryMeshCamera(py::module_& module) {
  auto cameras = submodule(module, "geometry.mesh.camera");
  // A MATRIX IS COUNTED BY COLUMNS, as glm counts one and as its sixteen
  // floats stand in memory: one index names a column, a pair names a
  // column and the row within it, and `values` and the buffer hand the
  // sixteen out in that same order. Nothing here offers a row on its
  // own, because a reader who takes the pair the other way about is
  // answered the transpose with no error to tell them.
  py::class_<glm::mat4> matrix(cameras, "Matrix", py::buffer_protocol());
  matrix.def(py::init([] { return glm::mat4(1); }))
      .def_static(
          "fromColumns",
          [](glm::vec4 column0, glm::vec4 column1, glm::vec4 column2,
             glm::vec4 column3) {
            return glm::mat4(column0, column1, column2, column3);
          },
          py::arg("column0"), py::arg("column1"), py::arg("column2"),
          py::arg("column3"))
      .def_static(
          "fromValues",
          [](const std::array<float, kValueCount>& values) {
            glm::mat4 result(1);
            std::copy(values.begin(), values.end(), glm::value_ptr(result));
            return result;
          },
          py::arg("values"))
      // The buffer is the matrix's own storage, read only: a reader that
      // wants the numbers without sixteen boxed floats gets them, and a
      // writer goes through the setters, which is where the bounds are
      // checked.
      .def_buffer([](const glm::mat4& value) {
        return py::buffer_info(glm::value_ptr(value), kValueCount);
      })
      .def("values", &matrixValues)
      .def(
          "column",
          [](const glm::mat4& value, glm::length_t index) {
            return value[matrixIndex(index, "column")];
          },
          py::arg("index"))
      .def(
          "setColumn",
          [](glm::mat4& value, glm::length_t index, glm::vec4 column) {
            value[matrixIndex(index, "column")] = column;
          },
          py::arg("index"), py::arg("column"))
      .def("__len__", [](const glm::mat4&) { return kAxisCount; })
      .def(
          "__getitem__",
          [](const glm::mat4& value, glm::length_t index) {
            return value[matrixIndex(index, "column")];
          },
          py::arg("index"))
      .def(
          "__getitem__",
          [](const glm::mat4& value,
             std::pair<glm::length_t, glm::length_t> index) {
            return value[matrixIndex(index.first, "column")]
                        [matrixIndex(index.second, "row")];
          },
          py::arg("index"))
      .def(
          "__setitem__",
          [](glm::mat4& value, glm::length_t index, glm::vec4 column) {
            value[matrixIndex(index, "column")] = column;
          },
          py::arg("index"), py::arg("value"))
      .def(
          "__setitem__",
          [](glm::mat4& value, std::pair<glm::length_t, glm::length_t> index,
             float element) {
            value[matrixIndex(index.first, "column")]
                 [matrixIndex(index.second, "row")] = element;
          },
          py::arg("index"), py::arg("value"))
      .def(
          "__matmul__",
          [](const glm::mat4& value, const glm::mat4& other) {
            return value * other;
          },
          py::arg("other"), py::is_operator())
      .def(
          "__matmul__",
          [](const glm::mat4& value, glm::vec4 other) { return value * other; },
          py::arg("other"), py::is_operator())
      // Each comparison is a lambda with a named input, so the
      // declarations carry the name. Defining one drops the hash a
      // Python object is born with, and a matrix is a value a memoised
      // model keys on, so it hashes by the sixteen floats it holds at
      // the moment it is asked.
      .def(
          "__eq__",
          [](const glm::mat4& value, const glm::mat4& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator())
      .def("__hash__",
           [](const glm::mat4& value) {
             const float* start = glm::value_ptr(value);
             py::tuple values(kValueCount);
             for (py::ssize_t index = 0; index < kValueCount; ++index)
               values[index] = py::float_(start[index]);
             return py::hash(values);
           })
      // The reading is the call that rebuilds it, so a matrix printed in
      // a session can be pasted back into one.
      .def("__repr__",
           [](const glm::mat4& value) {
             const float* start = glm::value_ptr(value);
             std::string text = "Matrix.fromValues([";
             for (py::ssize_t index = 0; index < kValueCount; ++index) {
               if (index > 0) text += ", ";
               text +=
                   py::cast<std::string>(py::repr(py::float_(start[index])));
             }
             return text + "])";
           })
      .def("copy", [](const glm::mat4& value) { return value; });
  copyProtocol(matrix);

  auto cameraClass = bindRecord<camera::Camera>(cameras, "Camera",
                                                "A camera has no field named ");
  cameraClass.def_readwrite("eye", &camera::Camera::eye)
      .def_readwrite("target", &camera::Camera::target)
      .def_readwrite("up", &camera::Camera::up)
      .def_readwrite("fovYDeg", &camera::Camera::fovYDeg)
      .def_readwrite("zNear", &camera::Camera::zNear)
      .def_readwrite("zFar", &camera::Camera::zFar)
      .def("view", &camera::Camera::view)
      .def("projection", &camera::Camera::projection, py::arg("aspect"))
      .def(
          "viewProjection",
          [](const camera::Camera& value, py::handle viewport) {
            return value.viewProjection(viewportSize(viewport));
          },
          py::arg("viewport"))
      .def(
          "clipProjection",
          [](const camera::Camera& value, std::array<int, 2> extent) {
            return value.clipProjection(clipExtent(extent));
          },
          py::arg("extent"))
      .def("extentAt", &camera::Camera::extentAt, py::arg("distance"),
           py::arg("aspect"))
      .def(
          "project",
          [](const camera::Camera& value, glm::vec3 point,
             py::handle viewport) {
            return value.project(point, viewportSize(viewport));
          },
          py::arg("point"), py::arg("viewport"))
      .def(
          "__eq__",
          [](const camera::Camera& value, const camera::Camera& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator());

  // An orbit written by hand stands where a default camera's eye stands,
  // so `Orbit()` and the orbit read off `Camera()` name the same place.
  py::class_<camera::Orbit> orbit(cameras, "Orbit");
  orbit
      .def(py::init([](float yawDeg, float pitchDeg, float distance) {
             return camera::Orbit{yawDeg, pitchDeg, distance};
           }),
           py::arg("yawDeg") = 0, py::arg("pitchDeg") = 0,
           py::arg("distance") = 480)
      .def_readwrite("yawDeg", &camera::Orbit::yawDeg)
      .def_readwrite("pitchDeg", &camera::Orbit::pitchDeg)
      .def_readwrite("distance", &camera::Orbit::distance)
      .def("copy", [](const camera::Orbit& value) { return value; })
      .def(
          "__eq__",
          [](const camera::Orbit& value, const camera::Orbit& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator());
  copyProtocol(orbit);

  cameras.def("orbitOf", &camera::orbitOf, py::arg("camera"))
      .def("cameraAt", &camera::cameraAt, py::arg("pivot"), py::arg("orbit"));
  cameras.def("place", &camera::place, py::arg("position") = glm::vec3(0),
              py::arg("yawDeg") = 0, py::arg("pitchDeg") = 0,
              py::arg("rollDeg") = 0, py::arg("scale") = 1);
  cameras.def("faceCamera", &camera::faceCamera, py::arg("eye"), py::arg("at"),
              py::arg("up") = glm::vec3(0, 1, 0));
}

}  // namespace sigil::python
