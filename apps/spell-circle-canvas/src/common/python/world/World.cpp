#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>
#include <sigilpython/world/Registration.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Light.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/scene/Scene.h>

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** A group's children: nodes of any kind, or one iterable of them. */
std::vector<world::Node> worldChildren(py::args values) {
  try {
    if (values.size() == 1 && (py::isinstance<py::str>(values[0]) ||
                               py::isinstance<py::bytes>(values[0])))
      throw py::cast_error();
    const py::tuple children =
        values.size() == 1 && !py::isinstance<world::Node>(values[0])
            ? py::tuple(values[0])
            : py::tuple(values);
    return children.cast<std::vector<world::Node>>();
  } catch (const py::cast_error&) {
    throw py::type_error(
        "group expects World nodes or one iterable of World nodes");
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
    throw py::value_error("A World frame extent must be positive.");
  return {size[0], size[1]};
}

// Scene borrows its engine. This owner keeps that native dependency alive
// through scene destruction and lets a headless caller state frame steps.
class Scene {
 public:
  Scene() : m_owner(std::this_thread::get_id()), m_scene(m_engine) {}
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
    m_engine.advance(m_engine.elapsed() + motion::Duration(seconds));
    if (m_frame) {
      m_scene.render(*m_frame);
      if (!m_scene.error().empty()) throw py::value_error(m_scene.error());
    }
  }

 private:
  const std::thread::id m_owner;
  motion::Engine m_engine;
  world::Scene m_scene;
  std::optional<world::Frame> m_frame;
};

/** A light's tint, as an author types it: encoded for display and
 *  decoded into light by the renderer. Four numbers are taken as written,
 *  because a light may be brighter than white and a colour's channels are
 *  read as unit values; an animatable colour is kept live; every other
 *  spelling is a colour. */
motion::Animatable<material::Color> lightColor(py::handle value) {
  if (!py::isinstance<py::str>(value) && py::isinstance<py::sequence>(value) &&
      py::len(value) == 4) {
    const glm::vec4 tint = py::cast<glm::vec4>(value);
    return material::Color{tint.x, tint.y, tint.z, tint.w};
  }
  return motionInk(value);
}

/** A collection read from a name, or from the `Collection` value. */
world::Collection collectionOf(py::handle value) {
  if (py::isinstance<py::str>(value))
    return world::Collection{.include = {py::cast<std::string>(value)}};
  return py::cast<world::Collection>(value);
}

constexpr auto fluent = py::return_value_policy::reference_internal;

/** The verbs every kind shares: where it stands, whether it is there,
 *  what it is called and which collections it joins. */
template <class Kind>
void bindNodeVerbs(py::class_<Kind, world::Node>& kind) {
  kind.def("copy", [](const Kind& self) { return self; })
      .def(
          "key",
          [](Kind& self, const std::string& name) -> Kind& {
            return self.key(name);
          },
          py::arg("key"), fluent)
      .def(
          "collection",
          [](Kind& self, std::string name) -> Kind& {
            return self.collection(std::move(name));
          },
          py::arg("name"), fluent)
      .def(
          "at",
          [](Kind& self, glm::vec3 position) -> Kind& {
            return self.at(position);
          },
          py::arg("position"), fluent)
      .def(
          "transformOrigin",
          [](Kind& self, glm::vec3 origin) -> Kind& {
            return self.transformOrigin(origin);
          },
          py::arg("origin"), fluent)
      .def(
          "transform",
          [](Kind& self, const glm::mat4& matrix) -> Kind& {
            return self.transform(matrix);
          },
          py::arg("matrix"), fluent)
      .def(
          "visible",
          [](Kind& self, bool shown) -> Kind& { return self.visible(shown); },
          py::arg("shown"), fluent)
      .def(
          "rotate",
          [](Kind& self, glm::vec3 axis, py::handle degrees) -> Kind& {
            return self.rotate(axis, motionAnimatable(degrees));
          },
          py::arg("axis"), py::arg("degrees"), fluent)
      .def(
          "transition",
          [](Kind& self, py::handle how) -> Kind& {
            return self.transition(motionTransition(how));
          },
          py::arg("transition"), fluent,
          "The node's default transition for the plain values on it: a "
          "Tween, whose from_ and to are not read, or a number of seconds "
          "for one of that duration.");
  using Lane =
      Kind& (world::PlacementVerbs<Kind>::*)(motion::Animatable<float>);
  for (const auto& [name, member] :
       std::initializer_list<std::pair<const char*, Lane>>{
           {"translateX", &world::PlacementVerbs<Kind>::translateX},
           {"translateY", &world::PlacementVerbs<Kind>::translateY},
           {"translateZ", &world::PlacementVerbs<Kind>::translateZ},
           {"rotateX", &world::PlacementVerbs<Kind>::rotateX},
           {"rotateY", &world::PlacementVerbs<Kind>::rotateY},
           {"rotateZ", &world::PlacementVerbs<Kind>::rotateZ},
           {"scale", &world::PlacementVerbs<Kind>::scale},
           {"scaleX", &world::PlacementVerbs<Kind>::scaleX},
           {"scaleY", &world::PlacementVerbs<Kind>::scaleY},
           {"scaleZ", &world::PlacementVerbs<Kind>::scaleZ}}) {
    kind.def(
        name,
        [member](Kind& self, py::handle value) -> Kind& {
          return (self.*member)(motionAnimatable(value));
        },
        py::arg("value"), fluent);
  }
}

/** …and what a body is made of. */
template <class Kind>
void bindSurfaceVerbs(py::class_<Kind, world::Node>& kind) {
  kind.def(
          "fill",
          [](Kind& self, material::Material surface) -> Kind& {
            return self.fill(std::move(surface));
          },
          py::arg("material"), fluent)
      .def(
          "fill",
          [](Kind& self, const std::vector<material::Material>& materials)
              -> Kind& { return self.fill(materials); },
          py::arg("materials"), fluent)
      .def(
          "backface",
          [](Kind& self, material::Backface facing) -> Kind& {
            return self.backface(facing);
          },
          py::arg("facing"), fluent);
}

void bindLights(py::module_& module) {
  py::enum_<world::LightKind>(module, "LightKind")
      .value("Directional", world::LightKind::Directional)
      .value("Point", world::LightKind::Point)
      .value("Spot", world::LightKind::Spot)
      .value("Ambient", world::LightKind::Ambient)
      .value("Hemisphere", world::LightKind::Hemisphere);
  bindRecord<world::Collection>(module, "Collection",
                                "Unknown collection field: ")
      .def_readwrite("include", &world::Collection::include)
      .def_readwrite("exclude", &world::Collection::exclude)
      .def("everything", &world::Collection::everything)
      .def(py::self == py::self);
  py::class_<world::LightDescription>(module, "LightDescription")
      .def_readonly("kind", &world::LightDescription::kind)
      .def_readonly("direction", &world::LightDescription::direction)
      .def_property_readonly("color",
                             [](const world::LightDescription& light) {
                               return light.color.value();
                             })
      .def_property_readonly("intensity",
                             [](const world::LightDescription& light) {
                               return light.intensity.value();
                             })
      .def_property_readonly("groundColor",
                             [](const world::LightDescription& light) {
                               return light.groundColor.value();
                             })
      .def_readonly("range", &world::LightDescription::range)
      .def_readonly("innerAngle", &world::LightDescription::innerAngle)
      .def_readonly("outerAngle", &world::LightDescription::outerAngle)
      .def_readonly("lightLink", &world::LightDescription::lightLink)
      .def_readonly("shadowLink", &world::LightDescription::shadowLink);
  py::class_<world::PlacedLight>(module, "PlacedLight")
      .def_readonly("light", &world::PlacedLight::light)
      .def_readonly("position", &world::PlacedLight::position)
      .def_readonly("key", &world::PlacedLight::key);
  module.def("attenuation", &world::attenuation, py::arg("light"),
             py::arg("at"));
  module.def("radiance", &world::radiance, py::arg("light"));
  module.def(
      "intensityAt",
      [](py::handle strength, float distance) -> py::object {
        // A number answers a number; a live value answers a value that
        // follows it, scaled the same way.
        if (py::isinstance<py::float_>(strength) ||
            py::isinstance<py::int_>(strength))
          return py::cast(
              world::intensityAt(py::cast<float>(strength), distance));
        return py::cast(
            world::intensityAt(motionAnimatable(strength), distance));
      },
      py::arg("strength"), py::arg("distance"),
      "The intensity a point or spot light needs to deliver `strength` at "
      "`distance`: the strength times the distance squared.");
}

void bindKinds(py::module_& module) {
  py::class_<world::Node>(module, "Node")
      .def("copy", [](const world::Node& self) { return self; });

  auto group = py::class_<world::Group, world::Node>(module, "Group");
  bindNodeVerbs(group);
  module.def(
      "group",
      [](py::args children) {
        const std::vector<world::Node> nodes = worldChildren(children);
        return world::group(nodes);
      },
      "A group of the nodes given, in order: nodes of any kind, or one "
      "iterable of them.");

  py::enum_<world::Facing>(module, "Facing")
      .value("None_", world::Facing::None)
      .value("Camera", world::Facing::Camera)
      .value("CameraUpright", world::Facing::CameraUpright);
  auto mesh = py::class_<world::Mesh, world::Node>(module, "Mesh");
  bindNodeVerbs(mesh);
  bindSurfaceVerbs(mesh);
  mesh.def("facing", &world::Mesh::facing, py::arg("facing"), fluent);
  module.def(
      "mesh",
      [](geometry::mesh::Mesh triangles) {
        return world::mesh(std::move(triangles));
      },
      py::arg("mesh"));
  module.def(
      "mesh",
      [](geometry::mesh::Instances instances) {
        return world::mesh(std::move(instances));
      },
      py::arg("instances"),
      "A body of instanced geometry: its stamp drawn once per instance.");

  auto null = py::class_<world::Null, world::Node>(module, "Null");
  bindNodeVerbs(null);
  module.def("null", &world::null,
             "A placement with a key and nothing else, there to be referred "
             "to: a pivot, or what a light or a camera looks at.");

  auto light = py::class_<world::Light, world::Node>(module, "Light");
  bindNodeVerbs(light);
  light
      .def(
          "lightLink",
          [](world::Light& self, py::handle bodies) -> world::Light& {
            return self.lightLink(collectionOf(bodies));
          },
          py::arg("bodies"), fluent)
      .def(
          "shadowLink",
          [](world::Light& self, py::handle blockers) -> world::Light& {
            return self.shadowLink(collectionOf(blockers));
          },
          py::arg("blockers"), fluent)
      .def("lookAt", &world::Light::lookAt, py::arg("key"), fluent)
      .def_property_readonly("description", &world::Light::description);
  module.def(
      "directionalLight",
      [](const glm::vec3& direction, py::handle tint, py::handle intensity) {
        return world::directionalLight(
            direction, {.color = lightColor(tint),
                        .intensity = motionAnimatable(intensity)});
      },
      py::arg("direction"), py::arg("color") = glm::vec4(1),
      py::arg("intensity") = 1);
  module.def(
      "pointLight",
      [](py::handle tint, py::handle intensity, float range) {
        return world::pointLight({.color = lightColor(tint),
                                  .intensity = motionAnimatable(intensity),
                                  .range = range});
      },
      py::arg("color") = glm::vec4(1), py::arg("intensity") = 1,
      py::arg("range") = 0);
  module.def(
      "spotLight",
      [](const glm::vec3& direction, py::handle tint, py::handle intensity,
         float range, float innerAngle, float outerAngle) {
        return world::spotLight(direction,
                                {.color = lightColor(tint),
                                 .intensity = motionAnimatable(intensity),
                                 .range = range,
                                 .innerAngle = innerAngle,
                                 .outerAngle = outerAngle});
      },
      py::arg("direction"), py::arg("color") = glm::vec4(1),
      py::arg("intensity") = 1, py::arg("range") = 0, py::arg("innerAngle") = 0,
      py::arg("outerAngle") = 45);
  module.def(
      "ambientLight",
      [](py::handle tint, py::handle intensity) {
        return world::ambientLight({.color = lightColor(tint),
                                    .intensity = motionAnimatable(intensity)});
      },
      py::arg("color") = glm::vec4(1), py::arg("intensity") = 1);
  module.def(
      "hemisphereLight",
      [](py::handle tint, py::handle ground, py::handle intensity) {
        return world::hemisphereLight(
            {.color = lightColor(tint),
             .groundColor = lightColor(ground),
             .intensity = motionAnimatable(intensity)});
      },
      py::arg("color") = glm::vec4(1),
      py::arg("groundColor") = glm::vec4(0, 0, 0, 1), py::arg("intensity") = 1);

  py::enum_<world::ToneCurve>(module, "ToneCurve")
      .value("Reinhard", world::ToneCurve::Reinhard)
      .value("Neutral", world::ToneCurve::Neutral)
      .value("Linear", world::ToneCurve::Linear);
  auto camera = py::class_<world::Camera, world::Node>(module, "Camera");
  bindNodeVerbs(camera);
  camera.def("lookAt", &world::Camera::lookAt, py::arg("key"), fluent);
  module.def(
      "camera",
      [](const geometry::mesh::camera::Camera& lens, py::handle exposure,
         world::ToneCurve toneCurve) {
        return world::camera(lens, {.exposure = motionAnimatable(exposure),
                                    .toneCurve = toneCurve});
      },
      py::arg("lens"), py::arg("exposure") = 1,
      py::arg("toneCurve") = world::ToneCurve::Reinhard);
}

void bindEnvironment(py::module_& module, py::module_& kit) {
  py::class_<world::EnvironmentMap>(module, "EnvironmentMap")
      .def("valid", &world::EnvironmentMap::valid);
  bindRecord<world::kit::SoftBox>(kit, "SoftBox", "Unknown soft box field: ")
      .def_readwrite("centre", &world::kit::SoftBox::centre)
      .def_readwrite("size", &world::kit::SoftBox::size)
      .def_readwrite("radiance", &world::kit::SoftBox::radiance);
  kit.def(
      "environmentMap",
      [](const glm::vec3& zenith, const glm::vec3& nadir, float exponent,
         const std::vector<world::kit::SoftBox>& softBoxes, int width) {
        return world::kit::environmentMap({.width = width,
                                           .zenith = zenith,
                                           .nadir = nadir,
                                           .exponent = exponent,
                                           .softBoxes = softBoxes});
      },
      py::arg("zenith") = glm::vec3(0), py::arg("nadir") = glm::vec3(0),
      py::arg("exponent") = 1,
      py::arg("softBoxes") = std::vector<world::kit::SoftBox>{},
      py::arg("width") = 256);
}

void bindWorldKit(py::module_& kit) {
  using Rig = world::kit::Rig;
  using Turntable = world::kit::Turntable;
  using Set = world::kit::Set;
  bindRecord<Rig>(kit, "Rig", "Unknown rig field: ")
      .def_readwrite("extent", &Rig::extent)
      .def_readwrite("bearing", &Rig::bearing)
      .def_readwrite("elevation", &Rig::elevation)
      .def_readwrite("fillIntensity", &Rig::fillIntensity)
      .def_property(
          "fillColor", [](const Rig& self) { return self.fillColor; },
          [](Rig& self, py::handle value) {
            self.fillColor = material::Color(color(value));
          })
      .def_readwrite("ambientIntensity", &Rig::ambientIntensity)
      .def_property(
          "ambientColor", [](const Rig& self) { return self.ambientColor; },
          [](Rig& self, py::handle value) {
            self.ambientColor = material::Color(color(value));
          })
      .def_readwrite("backIntensity", &Rig::backIntensity)
      .def_property(
          "backColor", [](const Rig& self) { return self.backColor; },
          [](Rig& self, py::handle value) {
            self.backColor = material::Color(color(value));
          })
      .def_property(
          "intensity", [](const Rig& self) { return self.intensity.value(); },
          [](Rig& self, py::handle value) {
            self.intensity = motionAnimatable(value);
          })
      .def_property(
          "color", [](const Rig& self) { return self.color.value(); },
          [](Rig& self, py::handle value) { self.color = lightColor(value); });
  bindRecord<Turntable>(kit, "Turntable", "Unknown turntable field: ")
      .def_readwrite("at", &Turntable::at)
      .def_readwrite("radius", &Turntable::radius)
      .def_readwrite("height", &Turntable::height)
      .def_property(
          "period", [](const Turntable& self) { return self.period.count(); },
          [](Turntable& self, double seconds) {
            self.period = motion::Duration(seconds);
          })
      .def_readwrite("fieldOfView", &Turntable::fieldOfView)
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
  kit.def(
      "turntable",
      [](const Turntable& table, double now) {
        return world::kit::turntable(table, motion::Duration(now));
      },
      py::arg("table"), py::arg("now"));
  kit.def(
      "litSet",
      [](const world::Node& subject, const Set& set, double now) {
        return world::kit::litSet(subject, set, motion::Duration(now));
      },
      py::arg("subject"), py::arg("set") = Set{}, py::arg("now") = 0);
}

}  // namespace

void bindWorld(py::module_& root) {
  auto module = root.def_submodule("world");
  auto kit = module.def_submodule("kit");
  bindLights(module);
  bindKinds(module);
  bindEnvironment(module, kit);
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
      .def(
          "only",
          [](world::Pass& self, py::handle bodies) -> world::Pass& {
            return self.only(collectionOf(bodies));
          },
          py::arg("bodies"), fluent)
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
          [](world::Pass& self, float gain, float lift,
             py::handle tint) -> world::Pass& {
            return self.levels(
                {.gain = gain, .lift = lift, .tint = color(tint)});
          },
          py::arg("gain") = 1, py::arg("lift") = 0, py::arg("tint") = "#ffffff",
          fluent)
      .def(
          "composite",
          [](world::Pass& self, material::BlendMode mode,
             float opacity) -> world::Pass& {
            return self.composite({.mode = mode, .opacity = opacity});
          },
          py::arg("mode") = material::BlendMode::PlusLighter,
          py::arg("opacity") = 1, fluent)
      .def(
          "bloom",
          [](world::Pass& self, float threshold, float intensity,
             float radius) -> world::Pass& {
            return self.bloom({.threshold = threshold,
                               .intensity = intensity,
                               .radius = radius});
          },
          py::arg("threshold") = 1.0f, py::arg("intensity") = 0.6f,
          py::arg("radius") = 24.0f, fluent);
  module.def(
      "bloom",
      [](float threshold, float intensity, float radius) {
        return world::bloom(
            {.threshold = threshold, .intensity = intensity, .radius = radius});
      },
      py::kw_only(), py::arg("threshold") = 1.0f, py::arg("intensity") = 0.6f,
      py::arg("radius") = 24.0f);
  module.def("geometryPass", &world::geometryPass, py::arg("name"));
  module.def("postPass", &world::postPass, py::arg("name"));
  py::class_<world::Frame>(module, "Frame")
      .def(py::init<world::Node>(), py::arg("scene") = world::Node{})
      .def("copy", [](const world::Frame& self) { return self; })
      .def("scene", py::overload_cast<world::Node>(&world::Frame::scene),
           py::arg("scene"), fluent)
      .def(
          "extent",
          [](world::Frame& self, std::array<int, 2> size) -> world::Frame& {
            const SkISize pixels = extent(size);
            return self.extent(glm::ivec2(pixels.width(), pixels.height()));
          },
          py::arg("size"), fluent)
      .def(
          "camera",
          [](world::Frame& self, std::string key) -> world::Frame& {
            return self.camera(std::move(key));
          },
          py::arg("key"), fluent)
      .def(
          "defaultCamera",
          [](world::Frame& self, const geometry::mesh::camera::Camera& lens)
              -> world::Frame& { return self.defaultCamera(lens); },
          py::arg("lens"), fluent)
      .def(
          "cameraOverride",
          [](world::Frame& self, const geometry::mesh::camera::Camera& lens)
              -> world::Frame& { return self.cameraOverride(lens); },
          py::arg("lens"), fluent)
      .def(
          "environment",
          [](world::Frame& self, world::EnvironmentMap map,
             py::handle intensity, py::handle diffuse, py::handle specular,
             py::handle roughnessBias,
             std::optional<world::EnvironmentMap> next, py::handle crossfade,
             py::handle rotation, py::handle shown, py::handle blur,
             float groundRadius) -> world::Frame& {
            return self.environment(
                std::move(map),
                {.intensity = motionAnimatable(intensity),
                 .diffuse = motionAnimatable(diffuse),
                 .specular = motionAnimatable(specular),
                 .roughnessBias = motionAnimatable(roughnessBias),
                 .next = next.value_or(world::EnvironmentMap{}),
                 .crossfade = motionAnimatable(crossfade),
                 .rotation = motionAnimatable(rotation),
                 .shown = motionAnimatable(shown),
                 .blur = motionAnimatable(blur),
                 .groundRadius = groundRadius});
          },
          py::arg("map"), py::arg("intensity") = 1, py::arg("diffuse") = 1,
          py::arg("specular") = 1, py::arg("roughnessBias") = 0,
          py::arg("next") = py::none(), py::arg("crossfade") = 0,
          py::arg("rotation") = 0, py::arg("shown") = 0, py::arg("blur") = 0,
          py::arg("groundRadius") = 0, fluent)
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
           {"surfaces", &world::SceneStats::surfaces},
           {"texturesPainted", &world::SceneStats::texturesPainted},
           {"texturesSkipped", &world::SceneStats::texturesSkipped}})
    stats.def_readonly(name, member);
  py::class_<Scene>(module, "Scene")
      .def(py::init<>())
      .def("render", &Scene::render, py::arg("frame"))
      .def(
          "render",
          [](Scene& self, world::Node root) {
            self.render(world::Frame(std::move(root)));
          },
          py::arg("scene"))
      .def("advance", &Scene::advance, py::arg("seconds"))
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
  bindWorldKit(kit);
}

}  // namespace sigil::python
