#include <pybind11/stl.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/render/Painter.h>
#include <sigilgeometry/mesh/render/Runtime.h>
#include <sigilgeometry/mesh/render/Shading.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>
#include <sigilpython/skia/Values.h>

#include <array>
#include <exception>
#include <glm/glm.hpp>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace mesh = geometry::mesh;
namespace camera = mesh::camera;
namespace render = mesh::render;

namespace {

// A ROTATION IS COUNTED BY COLUMNS, as the camera's matrix is and as glm
// counts one: the three vectors below are where the panorama's own x, y
// and z axes point, in that order.
constexpr glm::length_t kAxisCount = 3;

/** The three columns of @p value, each of them three numbers. */
py::tuple orientationColumns(const glm::mat3& value) {
  return py::make_tuple(value[0], value[1], value[2]);
}

/** The rotation @p columns describe, column by column. */
glm::mat3 orientationOf(const std::array<glm::vec3, kAxisCount>& columns) {
  return glm::mat3(columns[0], columns[1], columns[2]);
}

/** Runs @p body over @p canvas through a pen of its own, so a panel's
 *  author draws with the same verbs and the same checks as the frame
 *  around it, and answers whatever the body raised.
 *
 *  The pen ends with the call: `invokePen` closes the loan whichever way
 *  the body returns, and the frame ends on the pen's own save, so a
 *  panel that opens a transform or a clip and abandons it leaves the
 *  canvas as the executor lent it. A raise is carried back rather than
 *  thrown, because the executor holds a save around this call and an
 *  exception through it would leave the host canvas one deep.
 *
 *  @p width and @p height are the panel's own size in world units: the
 *  two numbers `pen.width` and `pen.height` answer inside the body. They
 *  are a size and not a rectangle, because the canvas the body draws on
 *  has its origin at the panel's CENTRE, with x to the right and y down
 *  as any Skia canvas has, so the panel spans half of each either way.
 *  The clock, the step and the frame count are the ones the frame
 *  outside is drawing at. The pointer is not carried in, because a
 *  position measured on the host canvas does not stand in panel-local
 *  coordinates. */
std::exception_ptr drawThroughPanelPen(const py::function& body,
                                       draw::Pen& host, SkCanvas& canvas,
                                       float width, float height) {
  draw::Pen panel;
  draw::Frame frame;
  frame.width = width;
  frame.height = height;
  frame.seconds = host.millis() / 1000.0;
  frame.deltaSeconds = host.deltaTime / 1000.0;
  frame.frameCount = host.frameCount;
  frame.fonts = host.fonts();
  panel.begin(canvas, frame);
  panel.inherit(host.inheritedInk(), host.inheritedFont());
  std::exception_ptr raised;
  try {
    invokePen(body, panel);
  } catch (...) {
    raised = std::current_exception();
  }
  panel.end();
  return raised;
}

}  // namespace

void bindGeometryMeshRender(py::module_& module) {
  auto renderer = submodule(module, "geometry.mesh.render");

  // WHO PERFORMS A DRAW, as a value. The executor behind it is native:
  // the built-in one draws on the CPU, and a feature that owns a device
  // hands back its own. There is no Python executor, because each of the
  // two draws would then need the interpreter on whichever thread the
  // runtime divided the work onto.
  py::class_<render::Runtime> runtime(renderer, "Runtime");
  runtime
      .def_static("cpu", &render::Runtime::cpu)
      // Every `cpu()` answers one value, so two default styles are equal
      // and a consumer caching a drawing can prove two frames asked for
      // the same shading.
      .def(
          "__eq__",
          [](const render::Runtime& value, const render::Runtime& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator())
      .def("__bool__", [](const render::Runtime& value) { return (bool)value; })
      .def("copy", [](const render::Runtime& value) { return value; });
  copyProtocol(runtime);

  // THE PANORAMA A LIT SURFACE SAMPLES. The prefiltered chain a
  // reflection picks a level from, the cosine convolution a diffuse term
  // reads, where the sky is turned to, and how far it is believed.
  auto environment = bindRecord<render::Environment>(
      renderer, "Environment", "An environment has no field named ");
  environment
      // A chain is copied out as a list of images, so a level is added
      // by assigning the list back rather than by appending to what a
      // read handed over.
      .def_readwrite("levels", &render::Environment::levels)
      .def_readwrite("irradiance", &render::Environment::irradiance)
      .def_readwrite("nextLevels", &render::Environment::nextLevels)
      .def_readwrite("nextIrradiance", &render::Environment::nextIrradiance)
      .def_readwrite("crossfade", &render::Environment::crossfade)
      .def_property(
          "orientation",
          [](const render::Environment& value) {
            return orientationColumns(value.orientation);
          },
          [](render::Environment& value,
             const std::array<glm::vec3, kAxisCount>& columns) {
            value.orientation = orientationOf(columns);
          })
      .def_readwrite("tint", &render::Environment::tint)
      .def_readwrite("intensity", &render::Environment::intensity)
      .def_readwrite("diffuse", &render::Environment::diffuse)
      .def_readwrite("specular", &render::Environment::specular)
      .def_readwrite("roughnessBias", &render::Environment::roughnessBias)
      .def_readwrite("exposure", &render::Environment::exposure)
      .def_readwrite("backdrop", &render::Environment::backdrop)
      .def_readwrite("backdropBlur", &render::Environment::backdropBlur)
      .def_readwrite("groundRadius", &render::Environment::groundRadius)
      .def_readwrite("projectionCenter", &render::Environment::projectionCenter)
      .def("valid", &render::Environment::valid)
      .def(
          "__eq__",
          [](const render::Environment& value,
             const render::Environment& other) { return value == other; },
          py::arg("other"), py::is_operator());

  py::class_<render::Light> light(renderer, "Light");
  light
      .def(py::init([](glm::vec3 direction, SkColor4f color, float intensity) {
             return render::Light{direction, color, intensity};
           }),
           py::arg("direction") = glm::vec3(-0.5f, -0.8f, -0.4f),
           py::arg("color") = SkColors::kWhite, py::arg("intensity") = 1)
      .def_readwrite("direction", &render::Light::direction)
      .def_readwrite("color", &render::Light::color)
      .def_readwrite("intensity", &render::Light::intensity)
      .def(
          "__eq__",
          [](const render::Light& value, const render::Light& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator())
      .def("copy", [](const render::Light& value) { return value; });
  copyProtocol(light);

  py::enum_<render::MeshStyle::Mode>(renderer, "Mode")
      .value("Lit", render::MeshStyle::Mode::Lit)
      .value("Normals", render::MeshStyle::Mode::Normals)
      .value("Uv", render::MeshStyle::Mode::Uv);

  auto style = bindRecord<render::MeshStyle>(
      renderer, "MeshStyle", "A mesh style has no field named ");
  style.def_readwrite("mode", &render::MeshStyle::mode)
      .def_readwrite("lit", &render::MeshStyle::lit)
      .def_readwrite("baseColor", &render::MeshStyle::baseColor)
      .def_readwrite("lights", &render::MeshStyle::lights)
      .def_readwrite("ambient", &render::MeshStyle::ambient)
      .def_readwrite("environment", &render::MeshStyle::environment)
      .def_readwrite("metallic", &render::MeshStyle::metallic)
      .def_readwrite("roughness", &render::MeshStyle::roughness)
      .def_readwrite("specular", &render::MeshStyle::specular)
      .def_readwrite("shininess", &render::MeshStyle::shininess)
      .def_readwrite("rim", &render::MeshStyle::rim)
      .def_readwrite("texture", &render::MeshStyle::texture)
      .def_readwrite("uvTransform", &render::MeshStyle::uvTransform)
      .def_readwrite("tileTexture", &render::MeshStyle::tileTexture)
      .def_readwrite("filter", &render::MeshStyle::filter)
      .def_readwrite("primitiveColorLane",
                     &render::MeshStyle::primitiveColorLane)
      .def_readwrite("backfaceCull", &render::MeshStyle::backfaceCull)
      .def_readwrite("depthSort", &render::MeshStyle::depthSort)
      // The runtime a mesh draw executes on travels inside the style,
      // while a panel is handed one beside its arguments; both are
      // spelled, because both are how the native calls take it.
      .def_readwrite("runtime", &render::MeshStyle::runtime)
      .def(
          "__eq__",
          [](const render::MeshStyle& value, const render::MeshStyle& other) {
            return value == other;
          },
          py::arg("other"), py::is_operator());

  renderer.def(
      "drawMesh",
      [](py::handle borrowed, const mesh::Mesh& mesh, const glm::mat4& model,
         const camera::Camera& camera, const render::MeshStyle& style) {
        for (auto index : mesh.indices)
          if (index >= mesh.positions.size())
            throw py::value_error(
                "A mesh index is outside the position array.");
        auto& host = pen(borrowed);
        render::drawMesh(*host.canvas(), mesh, model, camera,
                         {host.width, host.height}, style);
      },
      py::arg("pen"), py::arg("mesh"), py::arg("model"), py::arg("camera"),
      py::arg("style") = render::MeshStyle{});
  renderer.def(
      "drawPanel",
      [](py::handle borrowed, const glm::mat4& model,
         const camera::Camera& camera, const py::function& draw, float width,
         float height, const render::Runtime& runtime) {
        auto& host = pen(borrowed);
        std::exception_ptr raised;
        render::drawPanel(
            *host.canvas(), model, camera, {host.width, host.height},
            [&](SkCanvas& local) {
              raised = drawThroughPanelPen(draw, host, local, width, height);
            },
            runtime);
        if (raised) std::rethrow_exception(raised);
      },
      py::arg("pen"), py::arg("model"), py::arg("camera"), py::arg("draw"),
      py::arg("width") = 0, py::arg("height") = 0,
      py::arg("runtime") = render::Runtime::cpu());
  renderer.def(
      "drawImagePanel",
      [](py::handle borrowed, sk_sp<SkImage> image, float width, float height,
         const glm::mat4& model, const camera::Camera& camera, float opacity,
         const render::Runtime& runtime) {
        auto& host = pen(borrowed);
        render::drawImagePanel(*host.canvas(), std::move(image), width, height,
                               model, camera, {host.width, host.height},
                               opacity, runtime);
      },
      py::arg("pen"), py::arg("image"), py::arg("width"), py::arg("height"),
      py::arg("model"), py::arg("camera"), py::arg("opacity") = 1,
      py::arg("runtime") = render::Runtime::cpu());
  renderer.def(
      "drawBackdrop",
      [](py::handle borrowed, const render::Environment& environment,
         const glm::mat4& projection, const glm::mat4& viewMatrix) {
        auto& host = pen(borrowed);
        render::drawBackdrop(*host.canvas(), environment, projection,
                             viewMatrix, {host.width, host.height});
      },
      py::arg("pen"), py::arg("environment"), py::arg("projection"),
      py::arg("viewMatrix"));

  // THE SHADING TERMS THEMSELVES, the same arithmetic the draw above
  // evaluates per vertex. They are here so an author computing a colour
  // beside the picture — a swatch, a legend, a second tier written in
  // Python — lands on the value the renderer landed on. Each parameter
  // keeps the name the header gives it, and the ones the header spells
  // with a single letter are written out.
  renderer.def("backdropRay", &render::backdropRay, py::arg("environment"),
               py::arg("eye"), py::arg("ray"));
  renderer.def("atan2P", &render::atan2P, py::arg("y"), py::arg("x"));
  renderer.def("acosP", &render::acosP, py::arg("x"));
  renderer.def("equirectangularUv", &render::equirectangularUv,
               py::arg("direction"));
  renderer.def("specularColor", &render::specularColor, py::arg("baseColor"),
               py::arg("metal"));
  renderer.def("fresnelRough", &render::fresnelRough, py::arg("f0"),
               py::arg("cosTheta"), py::arg("roughness"));
  renderer.def("environmentBrdf", &render::environmentBrdf,
               py::arg("roughness"), py::arg("nDotV"));
  renderer.def("environmentSpecular", &render::environmentSpecular,
               py::arg("radiance"), py::arg("f0"), py::arg("roughness"),
               py::arg("nDotV"));
  renderer.def("attenuate", &render::attenuate, py::arg("radiance"),
               py::arg("absorb"), py::arg("thickness"));
  renderer.def("luminance", &render::luminance, py::arg("color"));
  renderer.def("toneMap", &render::toneMap, py::arg("radiance"),
               py::arg("exposure"));
  renderer.def("refraction", &render::refraction, py::arg("incident"),
               py::arg("normal"), py::arg("eta"));
  renderer.def("samplePanorama", &render::samplePanorama, py::arg("panorama"),
               py::arg("uv"));
  renderer.def("environmentRadiance", &render::environmentRadiance,
               py::arg("environment"), py::arg("direction"),
               py::arg("roughness"));
  renderer.def("environmentIrradiance", &render::environmentIrradiance,
               py::arg("environment"), py::arg("normal"));
}

}  // namespace sigil::python
