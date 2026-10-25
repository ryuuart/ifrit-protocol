/** @file
 * The retained side of the scene description: the one composer class
 * Python has, the queries it answers about the tree it laid out, the
 * dials a host sets on it, and the per-node reports it keeps about the
 * last frame.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkSize.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/typography/TextUnit.h>
#include <sigilcompose/typography/Track.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Composer.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/draw/Canvas.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <typeinfo>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

/** EVERYTHING AN OWNED COMPOSER BORROWS, held beside it. The native
 *  composer keeps references to its engine and its font context, so the
 *  two are declared ahead of it and outlive it. The font context is a
 *  system-backed one of the composer's own. */
struct ComposerHandle::Owned {
  explicit Owned(EngineHandle drivenBy)
      : engine(std::move(drivenBy)),
        fonts(weave::ports::systemFontManager()),
        composer(engine.get(), fonts) {}

  EngineHandle engine;
  weave::FontContext fonts;
  compose::Composer composer;
  std::thread::id thread = std::this_thread::get_id();
  bool drawing = false;
};

ComposerHandle::ComposerHandle()
    : m_owner(std::make_shared<Owned>(EngineHandle())) {}

ComposerHandle::ComposerHandle(const EngineHandle& engine)
    : m_owner(std::make_shared<Owned>(engine)) {}

ComposerHandle::ComposerHandle(std::function<compose::Composer&()> access)
    : m_access(std::move(access)) {}

compose::Composer& ComposerHandle::get() const {
  // A borrowed composer is checked by the host that lent it: the access
  // it was made with reports the closed session and the foreign thread
  // in the host's own words.
  if (!m_owner) {
    if (!m_access)
      throw std::runtime_error("This composer has no tree behind it");
    return m_access();
  }
  if (m_owner->thread != std::this_thread::get_id())
    throw std::runtime_error(
        "A composer is touched only on the thread that made it");
  if (m_owner->drawing)
    throw std::runtime_error(
        "A composer is not described, asked or drawn from inside its own "
        "draw");
  // The engine is asked first because the composer reads it on nearly
  // every call, and one a host lent refuses here once its session has
  // closed instead of being read after it has gone.
  (void)m_owner->engine.get();
  return m_owner->composer;
}

compose::Composer& ComposerHandle::ownedComposer(const char* refusal) const {
  if (!m_owner) {
    // The session is asked first, so a closed one is reported as closed
    // rather than as a call its host would have refused anyway.
    (void)get();
    throw std::runtime_error(refusal);
  }
  return get();
}

void ComposerHandle::draw(SkCanvas& canvas) const {
  auto& composer = ownedComposer("A host draws the composer it lends");
  struct Drawing {
    explicit Drawing(bool& flag) : m_flag(flag) { m_flag = true; }
    ~Drawing() { m_flag = false; }
    Drawing(const Drawing&) = delete;
    Drawing& operator=(const Drawing&) = delete;

   private:
    bool& m_flag;
  };
  const Drawing drawing(m_owner->drawing);
  // A paint program that raises leaves the canvas wherever its saves
  // stood, so the stack is put back to where this draw found it.
  const SkAutoCanvasRestore restore(&canvas, true);
  composer.draw(canvas);
}

namespace {

using compose::Composer;

/** A layout viewport read from @p value: a size, or a width and a
 *  height. */
SkSize viewport(py::handle value) {
  constexpr const char* refusal =
      "A composer's size is a Size, or a width and a height.";
  // None loads as a null size, which no conversion error reports.
  if (value.is_none()) throw py::type_error(refusal);
  try {
    return py::cast<SkSize>(value);
  } catch (const py::cast_error&) {
    throw py::type_error(refusal);
  }
}

/** A point in the composer's canvas units read from @p value: a point,
 *  or an x and a y. */
SkPoint canvasPosition(py::handle value) {
  try {
    return point(value);
  } catch (const py::cast_error&) {
    throw py::type_error("A canvas point is a Point, or an x and a y.");
  }
}

void bindReports(py::class_<ComposerHandle>& composer) {
  py::enum_<Composer::CacheState>(composer, "CacheState")
      .value("Live", Composer::CacheState::Live)
      .value("Picture", Composer::CacheState::Picture)
      .value("Texture", Composer::CacheState::Texture)
      .value("Group", Composer::CacheState::Group)
      .value("Moving", Composer::CacheState::Moving);

  // A record, registered by hand because it is nested in the class it
  // reports about: constructible from keywords, copyable, and answering
  // the copy protocol, as `bindRecord` gives a record at module scope.
  py::class_<Composer::NodeCost> cost(composer, "NodeCost");
  cost.def(py::init([](py::kwargs fields) {
        return keywordValue<Composer::NodeCost>(fields,
                                                "Unknown NodeCost field: ");
      }))
      .def("copy", [](const Composer::NodeCost& value) { return value; })
      .def_readwrite("label", &Composer::NodeCost::label)
      .def_readwrite("selfMilliseconds", &Composer::NodeCost::selfMilliseconds)
      .def_readwrite("totalMilliseconds",
                     &Composer::NodeCost::totalMilliseconds)
      .def_readwrite("depth", &Composer::NodeCost::depth)
      .def_readwrite("cacheState", &Composer::NodeCost::cacheState)
      .def_readwrite("effectDeferred", &Composer::NodeCost::effectDeferred)
      .def("cached", &Composer::NodeCost::cached);
  copyProtocol(cost);
}

void bindDescribePath(py::class_<ComposerHandle>& composer) {
  composer
      .def(py::init([](std::optional<EngineHandle> engine) {
             return engine ? ComposerHandle(*engine) : ComposerHandle();
           }),
           py::arg("engine") = py::none(),
           "A composer of Python's own, measuring and shaping with a "
           "system-backed font context it keeps. Its transitions run on "
           "the engine it is given; with none, nothing moves them.")
      .def(
          "setSize",
          [](const ComposerHandle& self, py::handle size) {
            const SkSize value = viewport(size);
            self.ownedComposer(
                    "A host sizes the composer it lends; a body describes "
                    "what goes in it")
                .setSize(glm::vec2{value.width(), value.height()});
          },
          py::arg("size"))
      .def(
          "setInherited",
          [](const ComposerHandle& self, const weave::Type& font,
             SkColor4f ink) { self.get().setInherited(font, ink); },
          py::arg("font"), py::arg("ink"))
      .def(
          "setView",
          [](const ComposerHandle& self, material::Filter view) {
            self.get().setView(std::move(view));
          },
          py::arg("view"))
      .def(
          "setView",
          [](const ComposerHandle& self, const material::Material& view) {
            self.get().setView(view);
          },
          py::arg("view"))
      .def(
          "render",
          [](const ComposerHandle& self, const compose::Element& root) {
            self.get().render(root);
          },
          py::arg("root"))
      .def(
          "renderSlot",
          [](const ComposerHandle& self, const std::string& name,
             const compose::Element& content) {
            self.get().renderSlot(name, content);
          },
          py::arg("name"), py::arg("content"))
      .def("dirty",
           [](const ComposerHandle& self) { return self.get().dirty(); })
      .def("isRunning",
           [](const ComposerHandle& self) { return self.get().isRunning(); })
      .def(
          "draw",
          [](const ComposerHandle& self, BorrowedCanvas& canvas) {
            self.draw(canvas.get());
          },
          py::arg("canvas"))
      .def("purgeCaches",
           [](const ComposerHandle& self) { self.get().purgeCaches(); });
}

void bindQueries(py::class_<ComposerHandle>& composer) {
  composer
      .def(
          "bounds",
          [](const ComposerHandle& self,
             const std::string& key) -> std::optional<SkRect> {
            const auto bounds = self.get().bounds(key);
            if (!bounds) return std::nullopt;
            return geometry::path::toSk(*bounds);
          },
          py::arg("key"))
      .def(
          "settling",
          [](const ComposerHandle& self, const std::string& key) {
            return self.get().settling(key);
          },
          py::arg("key"))
      .def(
          "scheduleSpan",
          [](const ComposerHandle& self, const std::string& key,
             std::size_t trackIndex) {
            return self.get().scheduleSpan(key, trackIndex);
          },
          py::arg("key"), py::arg("trackIndex"))
      .def(
          "hitTest",
          [](const ComposerHandle& self, py::handle canvasPoint) {
            const SkPoint at = canvasPosition(canvasPoint);
            return self.get().hitTest(geometry::path::fromSk(at));
          },
          py::arg("canvasPoint"));
  // The two text queries answer in the typography vocabulary, which
  // another file registers ahead of this one.
  if (registered<compose::Beat>())
    composer.def(
        "beatsOf",
        [](const ComposerHandle& self, const std::string& key,
           std::size_t trackIndex) {
          return self.get().beatsOf(key, trackIndex);
        },
        py::arg("key"), py::arg("trackIndex"));
  if (registered<compose::TextUnit>())
    composer.def(
        "units",
        [](const ComposerHandle& self, const std::string& key,
           const weave::Selector& selector,
           weave::Unit unit) { return self.get().units(key, selector, unit); },
        py::arg("key"), py::arg("selector"), py::arg("unit"));
}

void bindIntrospection(py::class_<ComposerHandle>& composer) {
  composer
      .def("stats",
           [](const ComposerHandle& self) -> Composer::Stats {
             return self.get().stats();
           })
      .def(
          "setProfiling",
          [](const ComposerHandle& self, bool on) {
            self.get().setProfiling(on);
          },
          py::arg("on"))
      .def("profiling",
           [](const ComposerHandle& self) { return self.get().profiling(); })
      .def("profile",
           [](const ComposerHandle& self) -> std::vector<Composer::NodeCost> {
             return self.get().profile();
           });
}

void bindSettings(py::class_<ComposerHandle>& composer) {
  composer
      .def(
          "setBakeDensity",
          [](const ComposerHandle& self, float devicePixelsPerUnit) {
            self.get().setBakeDensity(devicePixelsPerUnit);
          },
          py::arg("devicePixelsPerUnit"))
      .def("bakeDensity",
           [](const ComposerHandle& self) { return self.get().bakeDensity(); })
      .def(
          "setLive",
          [](const ComposerHandle& self, bool live) {
            self.get().setLive(live);
          },
          py::arg("live"))
      .def("live", [](const ComposerHandle& self) { return self.get().live(); })
      .def(
          "setGroundHeld",
          [](const ComposerHandle& self, bool held) {
            self.get().setGroundHeld(held);
          },
          py::arg("held"))
      .def("groundHeld",
           [](const ComposerHandle& self) { return self.get().groundHeld(); })
      .def(
          "setPointer",
          [](const ComposerHandle& self, py::handle canvasPoint, bool pressed) {
            const SkPoint at = canvasPosition(canvasPoint);
            self.get().setPointer(geometry::path::fromSk(at), pressed);
          },
          py::arg("canvasPoint"), py::arg("pressed"))
      .def(
          "setKey",
          [](const ComposerHandle& self, const std::string& name, int code,
             bool pressed) { self.get().setKey(name, code, pressed); },
          py::arg("name"), py::arg("code"), py::arg("pressed"));
}

}  // namespace

void bindComposeComposer(py::module_& module) {
  auto composition = submodule(module, "compose");

  // ONE COMPOSER CLASS, WHOEVER OWNS THE TREE. A host hands its own out
  // as a borrowed handle, so a sketch's composer and one a body builds
  // for itself answer the same calls. The reports are registered ahead
  // of the calls that answer in them, because a signature is written
  // when its function is registered.
  py::class_<ComposerHandle> composer(composition, "Composer");
  bindReports(composer);
  bindDescribePath(composer);
  bindQueries(composer);
  bindIntrospection(composer);
  bindSettings(composer);
}

}  // namespace sigil::python
