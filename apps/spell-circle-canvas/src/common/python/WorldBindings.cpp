#include <include/core/SkSurface.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/GeometryBindings.h>
#include <sigilpython/MotionBindings.h>
#include <sigilpython/ValueBindings.h>
#include <sigilpython/WorldBindings.h>
#include <sigilworld/kit/Kit.h>
#include <sigilworld/scene/Scene.h>

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

std::vector<world::Element> worldChildren(py::args values) {
  try {
    if (values.size() == 1 && (py::isinstance<py::str>(values[0]) ||
                               py::isinstance<py::bytes>(values[0])))
      throw py::cast_error();
    const py::tuple children =
        values.size() == 1 && !py::isinstance<world::Element>(values[0])
            ? py::tuple(values[0])
            : py::tuple(values);
    return children.cast<std::vector<world::Element>>();
  } catch (const py::cast_error&) {
    throw py::type_error(
        "children expects World Elements or one iterable of World Elements");
  }
}

std::vector<std::string> resourceNames(py::args values) {
  const py::tuple names =
      values.size() == 1 && !py::isinstance<py::str>(values[0])
          ? py::tuple(values[0])
          : py::tuple(values);
  return names.cast<std::vector<std::string>>();
}

SkISize extent(std::array<int, 2> size) {
  if (size[0] <= 0 || size[1] <= 0)
    throw py::value_error("A World image extent must be positive.");
  return {size[0], size[1]};
}

// Scene borrows its ticker. This owner keeps that native dependency alive
// through scene destruction and lets a headless caller supply frame deltas.
class Scene {
 public:
  Scene() : m_owner(std::this_thread::get_id()), m_scene(m_ticker) {}
  Scene(const Scene&) = delete;
  Scene& operator=(const Scene&) = delete;
  Scene(Scene&&) = delete;
  Scene& operator=(Scene&&) = delete;

  world::Scene& get() {
    if (std::this_thread::get_id() != m_owner)
      throw std::runtime_error(
          "A World scene must be used on its creating thread.");
    return m_scene;
  }

  void render(world::Frame frame) {
    get().render(frame);
    if (!m_scene.error().empty()) throw py::value_error(m_scene.error());
    m_frame = std::move(frame);
  }

  void advance(double seconds) {
    (void)get();
    if (!std::isfinite(seconds) || seconds < 0)
      throw py::value_error("A World step must be finite and nonnegative.");
    const CallbackBoundary boundary;
    m_ticker.tick(seconds);
    if (m_frame) {
      m_scene.render(*m_frame);
      if (!m_scene.error().empty()) throw py::value_error(m_scene.error());
    }
  }

  sk_sp<SkImage> image(std::array<int, 2> size, py::handle background) {
    (void)get();
    const SkISize pixels = extent(size);
    auto surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(pixels.width(), pixels.height()));
    if (!surface)
      throw std::runtime_error("The World image could not be allocated.");
    surface->getCanvas()->clear(color(background));
    m_scene.draw(*surface->getCanvas());
    return surface->makeImageSnapshot();
  }

 private:
  const std::thread::id m_owner;
  motion::Ticker m_ticker;
  world::Scene m_scene;
  std::optional<world::Frame> m_frame;
};

void bindSurfaceKit(py::module_& root) {
  auto material = root.attr("material").cast<py::module_>();
  auto kit = material.def_submodule("kit");
  using Parameters = material::kit::SurfaceParameters;
  auto parameters = bindRecord<Parameters>(kit, "SurfaceParameters",
                                           "Unknown surface parameter: ");
  for (const auto& [name, member] : std::initializer_list<
           std::pair<const char*, material::Color Parameters::*>>{
           {"baseColor", &Parameters::baseColor},
           {"emissive", &Parameters::emissive},
           {"absorption", &Parameters::absorption}}) {
    parameters.def_property(
        name,
        [member](const Parameters& self) {
          const auto& value = self.*member;
          return std::array<float, 4>{value.r, value.g, value.b, value.a};
        },
        [member](Parameters& self, py::handle value) {
          self.*member = color(value);
        });
  }
  parameters.def_readwrite("metallic", &Parameters::metallic)
      .def_readwrite("roughness", &Parameters::roughness)
      .def_readwrite("emissiveStrength", &Parameters::emissiveStrength)
      .def_readwrite("normalScale", &Parameters::normalScale)
      .def_readwrite("normalDirectX", &Parameters::normalDirectX)
      .def_readwrite("roughnessChannel", &Parameters::roughnessChannel)
      .def_readwrite("metallicChannel", &Parameters::metallicChannel)
      .def_readwrite("occlusionChannel", &Parameters::occlusionChannel)
      .def_readwrite("occlusionStrength", &Parameters::occlusionStrength)
      .def_readwrite("opacityChannel", &Parameters::opacityChannel)
      .def_readwrite("alphaCutoff", &Parameters::alphaCutoff)
      .def_readwrite("transmission", &Parameters::transmission)
      .def_readwrite("ior", &Parameters::ior)
      .def_readwrite("thickness", &Parameters::thickness)
      .def_readwrite("reflectionWeight", &Parameters::reflectionWeight)
      .def_static("chrome", &Parameters::chrome)
      .def_static("gold", &Parameters::gold)
      .def_static("glass", &Parameters::glass)
      .def_static(
          "metal",
          [](py::handle tint, float roughness) {
            return Parameters::metal(color(tint), roughness);
          },
          py::arg("tint"), py::arg("roughness"))
      .def_static(
          "dielectric",
          [](py::handle baseColor, float roughness) {
            return Parameters::dielectric(color(baseColor), roughness);
          },
          py::arg("baseColor"), py::arg("roughness"));
  py::enum_<material::kit::Reflection>(kit, "Reflection")
      .value("SplitSum", material::kit::Reflection::SplitSum)
      .value("Additive", material::kit::Reflection::Additive);
  kit.def("surface",
          py::overload_cast<const Parameters&, material::kit::Reflection>(
              &material::kit::surface),
          py::arg("parameters") = Parameters{},
          py::arg("reflection") = material::kit::Reflection::SplitSum);
  kit.def("unlit", &material::kit::unlit, py::arg("parameters") = Parameters{});
}

void bindLight(py::module_& module) {
  auto light = module.def_submodule("light");
  using Light = world::light::Light;
  py::enum_<world::light::Kind>(light, "Kind")
      .value("Sun", world::light::Kind::Sun)
      .value("Point", world::light::Kind::Point)
      .value("Spot", world::light::Kind::Spot);
  bindRecord<Light>(light, "Light", "Unknown light field: ")
      .def_readwrite("kind", &Light::kind)
      .def_readwrite("color", &Light::color)
      .def_readwrite("intensity", &Light::intensity)
      .def_readwrite("direction", &Light::direction)
      .def_readwrite("position", &Light::position)
      .def_readwrite("range", &Light::range)
      .def_readwrite("innerDeg", &Light::innerDeg)
      .def_readwrite("outerDeg", &Light::outerDeg)
      .def(py::self == py::self);
  light.def("sun", &world::light::sun, py::arg("direction"),
            py::arg("color") = glm::vec4(1), py::arg("intensity") = 1);
  light.def("point", &world::light::point, py::arg("position"),
            py::arg("color") = glm::vec4(1), py::arg("intensity") = 1,
            py::arg("range") = 600);
  light.def("spot", &world::light::spot, py::arg("position"),
            py::arg("direction"), py::arg("outerDeg") = 45,
            py::arg("innerDeg") = 0, py::arg("color") = glm::vec4(1),
            py::arg("intensity") = 1, py::arg("range") = 600);
  light.def("attenuation", &world::light::attenuation, py::arg("light"),
            py::arg("at"));
  light.def("radiance", &world::light::radiance, py::arg("light"));
}

void bindWorldKit(py::module_& module) {
  auto kit = module.def_submodule("kit");
  using Rig = world::kit::Rig;
  using Turntable = world::kit::Turntable;
  using Set = world::kit::Set;
  bindRecord<Rig>(kit, "Rig", "Unknown rig field: ")
      .def_readwrite("at", &Rig::at)
      .def_readwrite("extent", &Rig::extent)
      .def_readwrite("distance", &Rig::distance)
      .def_readwrite("bearing", &Rig::bearing)
      .def_readwrite("elevation", &Rig::elevation)
      .def_readwrite("fill", &Rig::fill)
      .def_readwrite("back", &Rig::back)
      .def_readwrite("intensity", &Rig::intensity)
      .def_readwrite("color", &Rig::color);
  bindRecord<Turntable>(kit, "Turntable", "Unknown turntable field: ")
      .def_readwrite("at", &Turntable::at)
      .def_readwrite("radius", &Turntable::radius)
      .def_readwrite("height", &Turntable::height)
      .def_readwrite("period", &Turntable::period)
      .def_readwrite("fovYDeg", &Turntable::fovYDeg)
      .def_readwrite("stations", &Turntable::stations);
  bindRecord<Set>(kit, "Set", "Unknown set field: ")
      .def_readwrite("rig", &Set::rig)
      .def_readwrite("table", &Set::table)
      .def_readwrite("ground", &Set::ground)
      .def_readwrite("drop", &Set::drop)
      .def_property(
          "surface", [](const Set& self) { return self.surface; },
          [](Set& self, std::optional<material::Material> value) {
            self.surface = std::move(value);
          });
  kit.def("threePoint", &world::kit::threePoint, py::arg("rig") = Rig{});
  kit.def("turntable", &world::kit::turntable, py::arg("table"),
          py::arg("seconds"));
  kit.def("litSet", &world::kit::litSet, py::arg("subject"),
          py::arg("set") = Set{}, py::arg("seconds") = 0);
}

}  // namespace

void bindWorld(py::module_& root) {
  bindSurfaceKit(root);
  auto module = root.def_submodule("world");
  bindLight(module);
  constexpr auto fluent = py::return_value_policy::reference_internal;
  py::enum_<world::Backface>(module, "Backface")
      .value("Hidden", world::Backface::Hidden)
      .value("Visible", world::Backface::Visible);
  auto element =
      py::class_<world::Element>(module, "Element")
          .def(py::init<>())
          .def("copy", [](const world::Element& self) { return self; })
          .def("key", &world::Element::key, py::arg("key"), fluent)
          .def(
              "children",
              [](world::Element& self, py::args children) -> world::Element& {
                return self.children(worldChildren(children));
              },
              fluent)
          .def("at", &world::Element::at, py::arg("position"), fluent)
          .def("transformOrigin", &world::Element::transformOrigin,
               py::arg("origin"), fluent)
          .def("transform", &world::Element::transform, py::arg("matrix"),
               fluent)
          .def("mesh", &world::Element::mesh, py::arg("mesh"), fluent)
          .def("fill",
               py::overload_cast<material::Material>(&world::Element::fill),
               py::arg("material"), fluent)
          .def(
              "fill",
              [](world::Element& self,
                 const std::vector<material::Material>& materials)
                  -> world::Element& { return self.fill(materials); },
              py::arg("materials"), fluent)
          .def("backface", &world::Element::backface, py::arg("facing"), fluent)
          .def("tag", &world::Element::tag, py::arg("tag"), fluent)
          .def("light", &world::Element::light, py::arg("light"), fluent)
          .def("camera", &world::Element::camera, py::arg("camera"), fluent)
          .def("transition", &world::Element::transition, py::arg("transition"),
               fluent);
  using Lane = world::Element& (world::Element::*)(motion::Animatable<float>);
  for (const auto& [name, member] :
       std::initializer_list<std::pair<const char*, Lane>>{
           {"translateX", &world::Element::translateX},
           {"translateY", &world::Element::translateY},
           {"translateZ", &world::Element::translateZ},
           {"rotateX", &world::Element::rotateX},
           {"rotateY", &world::Element::rotateY},
           {"rotateZ", &world::Element::rotateZ},
           {"scale", &world::Element::scale},
           {"scaleX", &world::Element::scaleX},
           {"scaleY", &world::Element::scaleY},
           {"scaleZ", &world::Element::scaleZ},
           {"intensity", &world::Element::intensity},
           {"exposure", &world::Element::exposure}}) {
    element.def(
        name,
        [member](world::Element& self, py::handle value) -> world::Element& {
          return (self.*member)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  }
  element.def(
      "rotate",
      [](world::Element& self, glm::vec3 axis,
         py::handle degrees) -> world::Element& {
        return self.rotate(axis, motionAnimatable(degrees));
      },
      py::arg("axis"), py::arg("degrees"), fluent);
  element.def(
      "emission",
      [](world::Element& self, py::handle red, py::handle green,
         py::handle blue) -> world::Element& {
        return self.emission(motionAnimatable(red), motionAnimatable(green),
                             motionAnimatable(blue));
      },
      py::arg("red"), py::arg("green"), py::arg("blue"), fluent);

  py::class_<world::Selector>(module, "Selector")
      .def(py::init<>())
      .def(
          "__or__",
          [](world::Selector self, world::Selector other) {
            return std::move(self) | std::move(other);
          },
          py::arg("other"), py::is_operator())
      .def(
          "__and__",
          [](world::Selector self, world::Selector other) {
            return std::move(self) & std::move(other);
          },
          py::arg("other"), py::is_operator())
      .def("__invert__", [](world::Selector self) { return !std::move(self); })
      .def(py::self == py::self);
  auto selectors = module.def_submodule("selectors");
  selectors.def("tag", &world::selectors::tag, py::arg("tag"));
  selectors.def("key", &world::selectors::key, py::arg("key"));
  selectors.def("under", &world::selectors::under, py::arg("key"));
  selectors.def("material", &world::selectors::material, py::arg("material"));
  py::enum_<world::Selection>(module, "Selection")
      .value("Auto", world::Selection::Auto)
      .value("All", world::Selection::None)
      .value("Cull", world::Selection::Cull)
      .value("Mask", world::Selection::Mask)
      .value("Variant", world::Selection::Variant);
  py::class_<world::Pass>(module, "Pass")
      .def("copy", [](const world::Pass& self) { return self; })
      .def(
          "reads",
          [](world::Pass& self, py::args values) -> world::Pass& {
            const auto names = resourceNames(values);
            for (const auto& name : names) self.reads(name);
            return self;
          },
          fluent)
      .def(
          "writes",
          [](world::Pass& self, py::args values) -> world::Pass& {
            const auto names = resourceNames(values);
            for (const auto& name : names) self.writes(name);
            return self;
          },
          fluent)
      .def("previous", py::overload_cast<std::string>(&world::Pass::previous),
           py::arg("resource"), fluent)
      .def("only", &world::Pass::only, py::arg("selector"), fluent)
      .def("variant",
           py::overload_cast<material::Material>(&world::Pass::variant),
           py::arg("material"), fluent)
      .def("realise", &world::Pass::realise, py::arg("selection"), fluent)
      .def(
          "clear",
          [](world::Pass& self, py::handle ink) -> world::Pass& {
            return self.clear(color(ink));
          },
          py::arg("color"), fluent)
      .def("blur", &world::Pass::blur, py::arg("sigma"), fluent)
      .def(
          "levels",
          [](world::Pass& self, float gain, float lift, py::handle tint)
              -> world::Pass& { return self.levels(gain, lift, color(tint)); },
          py::arg("gain"), py::arg("lift"), py::arg("tint") = "#ffffff", fluent)
      .def("composite", &world::Pass::composite, py::arg("mode"),
           py::arg("opacity") = 1, fluent);
  module.def("geometryPass", &world::geometryPass, py::arg("name"));
  module.def("postPass", &world::postPass, py::arg("name"));
  py::class_<world::Frame>(module, "Frame")
      .def(py::init<world::Element>(), py::arg("scene") = world::Element{})
      .def("copy", [](const world::Frame& self) { return self; })
      .def("scene", py::overload_cast<world::Element>(&world::Frame::scene),
           py::arg("scene"), fluent)
      .def(
          "extent",
          [](world::Frame& self, std::array<int, 2> size) -> world::Frame& {
            return self.extent(extent(size));
          },
          py::arg("size"), fluent)
      .def("camera",
           py::overload_cast<geometry::mesh::camera::Camera>(
               &world::Frame::camera),
           py::arg("camera"), fluent)
      .def("pass_", &world::Frame::pass, py::arg("pass_"), fluent)
      .def("present", py::overload_cast<std::string>(&world::Frame::present),
           py::arg("resource"), fluent);
  auto stats = py::class_<world::SceneStats>(module, "SceneStats");
  for (const auto& [name, member] : std::initializer_list<
           std::pair<const char*, int64_t world::SceneStats::*>>{
           {"nodes", &world::SceneStats::nodes},
           {"extracted", &world::SceneStats::extracted},
           {"cooked", &world::SceneStats::cooked},
           {"resources", &world::SceneStats::resources},
           {"baked", &world::SceneStats::baked},
           {"replayed", &world::SceneStats::replayed},
           {"drawn", &world::SceneStats::drawn},
           {"rounds", &world::SceneStats::rounds},
           {"passes", &world::SceneStats::passes},
           {"barriers", &world::SceneStats::barriers},
           {"aliased", &world::SceneStats::aliased},
           {"surfaces", &world::SceneStats::surfaces}})
    stats.def_readonly(name, member);
  py::class_<Scene>(module, "Scene")
      .def(py::init<>())
      .def("render", &Scene::render, py::arg("frame"))
      .def(
          "render",
          [](Scene& self, world::Element root) {
            self.render(world::Frame(std::move(root)));
          },
          py::arg("scene"))
      .def("advance", &Scene::advance, py::arg("seconds"))
      .def("image", &Scene::image, py::arg("size"),
           py::arg("background") = "#00000000")
      .def(
          "draw",
          [](Scene& self, BorrowedPen& pen) {
            self.get().draw(*pen.get().canvas());
          },
          py::arg("pen"))
      .def(
          "handleOf",
          [](Scene& self, const std::string& key) {
            return self.get().handleOf(key);
          },
          py::arg("key"))
      .def(
          "transformOf",
          [](Scene& self, const std::string& key) {
            return self.get().transformOf(key);
          },
          py::arg("key"))
      .def(
          "referencesOf",
          [](Scene& self, const std::string& key) {
            return self.get().referencesOf(key);
          },
          py::arg("key"))
      .def_property_readonly("camera",
                             [](Scene& self) { return self.get().camera(); })
      .def_property_readonly("lights",
                             [](Scene& self) { return self.get().lights(); })
      .def_property_readonly("stats",
                             [](Scene& self) { return self.get().stats(); })
      .def_property_readonly("error",
                             [](Scene& self) { return self.get().error(); });
  bindWorldKit(module);
}

}  // namespace sigil::python
