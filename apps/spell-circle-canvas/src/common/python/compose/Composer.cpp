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
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
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
 *  composer keeps references to its ticker and its font context, so the
 *  two are declared ahead of it and outlive it. The font context is a
 *  system-backed one of the composer's own. */
struct ComposerHandle::Owned {
  explicit Owned(TickerHandle drivenBy)
      : ticker(std::move(drivenBy)),
        fonts(weave::ports::systemFontManager()),
        composer(ticker.get(), fonts) {}

  TickerHandle ticker;
  weave::FontContext fonts;
  compose::Composer composer;
  std::thread::id thread = std::this_thread::get_id();
  bool drawing = false;
};

ComposerHandle::ComposerHandle()
    : m_owner(std::make_shared<Owned>(TickerHandle())) {}

ComposerHandle::ComposerHandle(const TickerHandle& ticker)
    : m_owner(std::make_shared<Owned>(ticker)) {}

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
  // The ticker is asked first because the composer reads it on nearly
  // every call, and one a host lent refuses here once its session has
  // closed instead of being read after it has gone.
  (void)m_owner->ticker.get();
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
namespace mskia = material::skia;

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

/** Whether the class Python reads @p Native back as is registered yet.
 *  A signature is written when its function is registered, so a query
 *  whose answer is a value another file registers is offered only once
 *  that value has a Python name to be written under. */
template <class Native>
bool registered() {
  return py::detail::get_type_info(typeid(Native)) != nullptr;
}

/** The row-major plane as a read-only two-dimensional byte buffer, rows
 *  first, so an array library reads the counts without copying them. */
py::buffer_info planeBuffer(Composer::CompositePlane& plane) {
  const auto width = static_cast<py::ssize_t>(plane.width);
  const auto height = static_cast<py::ssize_t>(plane.height);
  return py::buffer_info(plane.counts.data(), sizeof(std::uint8_t),
                         py::format_descriptor<std::uint8_t>::format(), 2,
                         {height, width},
                         {width * static_cast<py::ssize_t>(sizeof(std::uint8_t)),
                          static_cast<py::ssize_t>(sizeof(std::uint8_t))},
                         true);
}

void bindReports(py::class_<ComposerHandle>& composer) {
  py::enum_<Composer::InputSpace>(composer, "InputSpace")
      .value("EncodedSRGB", Composer::InputSpace::EncodedSRGB)
      .value("LinearSRGB", Composer::InputSpace::LinearSRGB)
      .value("DisplayP3", Composer::InputSpace::DisplayP3);

  py::enum_<Composer::CacheState>(composer, "CacheState")
      .value("Live", Composer::CacheState::Live)
      .value("Picture", Composer::CacheState::Picture)
      .value("Texture", Composer::CacheState::Texture)
      .value("Promoted", Composer::CacheState::Promoted)
      .value("SplitOwn", Composer::CacheState::SplitOwn)
      .value("Group", Composer::CacheState::Group);

  py::enum_<Composer::Promotion>(composer, "Promotion")
      .value("Cheap", Composer::Promotion::Cheap)
      .value("Warming", Composer::Promotion::Warming)
      .value("Promoted", Composer::Promotion::Promoted)
      .value("AskedFor", Composer::Promotion::AskedFor)
      .value("OptedOut", Composer::Promotion::OptedOut)
      .value("Volatile", Composer::Promotion::Volatile)
      .value("Composited", Composer::Promotion::Composited)
      .value("Transformed", Composer::Promotion::Transformed)
      .value("Filtered", Composer::Promotion::Filtered)
      .value("ReadsBackdrop", Composer::Promotion::ReadsBackdrop)
      .value("TooBig", Composer::Promotion::TooBig)
      .value("SplitBaked", Composer::Promotion::SplitBaked)
      .value("HostsSpace", Composer::Promotion::HostsSpace);

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
      .def_readwrite("selfMs", &Composer::NodeCost::selfMs)
      .def_readwrite("totalMs", &Composer::NodeCost::totalMs)
      .def_readwrite("depth", &Composer::NodeCost::depth)
      .def_readwrite("cacheState", &Composer::NodeCost::cacheState)
      .def_readwrite("promotion", &Composer::NodeCost::promotion)
      .def_readwrite("refusals", &Composer::NodeCost::refusals)
      .def_readwrite("effectDeferred", &Composer::NodeCost::effectDeferred)
      .def("refused", &Composer::NodeCost::refused, py::arg("promotion"),
           "Whether this condition is among the ones that refused a bake, "
           "which holds every refusal where `promotion` holds the first.")
      .def("cached", &Composer::NodeCost::cached);
  copyProtocol(cost);

  py::class_<Composer::CompositePlane> plane(composer, "CompositePlane",
                                             py::buffer_protocol());
  plane.def_readonly("width", &Composer::CompositePlane::width)
      .def_readonly("height", &Composer::CompositePlane::height)
      .def_property_readonly(
          "counts",
          [](const Composer::CompositePlane& value) {
            return py::bytes(
                reinterpret_cast<const char*>(value.counts.data()),
                value.counts.size());
          },
          "One saturating count per device pixel, row-major, copied.")
      .def("at", &Composer::CompositePlane::at, py::arg("x"), py::arg("y"))
      .def("copy",
           [](const Composer::CompositePlane& value) { return value; })
      .def_buffer(&planeBuffer);
  copyProtocol(plane);
}

void bindDescribePath(py::class_<ComposerHandle>& composer) {
  composer
      .def(py::init([](std::optional<TickerHandle> ticker) {
             return ticker ? ComposerHandle(*ticker) : ComposerHandle();
           }),
           py::arg("ticker") = py::none(),
           "A composer of Python's own, measuring and shaping with a "
           "system-backed font context it keeps. Its transitions run on "
           "the ticker it is given; with none, nothing steps them.")
      .def(
          "setSize",
          [](const ComposerHandle& self, py::handle size) {
            const SkSize value = viewport(size);
            self.ownedComposer(
                    "A host sizes the composer it lends; a body describes "
                    "what goes in it")
                .setSize(value);
          },
          py::arg("size"))
      // The native composer keeps the pointer, so the clock is kept
      // alive for as long as the composer's Python object is.
      .def(
          "setClock",
          [](const ComposerHandle& self, const motion::FrameClock* clock) {
            self.ownedComposer(
                    "A host sets the clock of the composer it lends")
                .setClock(clock);
          },
          py::arg("clock").none(true), py::keep_alive<1, 2>())
      .def(
          "setInherited",
          [](const ComposerHandle& self, const weave::Type& font,
             SkColor4f ink) { self.get().setInherited(font, ink); },
          py::arg("font"), py::arg("ink"))
      .def(
          "setView",
          [](const ComposerHandle& self, mskia::Effect view) {
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
          "declareInputSpace",
          [](const ComposerHandle& self, Composer::InputSpace space) {
            self.get().declareInputSpace(space);
          },
          py::arg("space"))
      .def("declaredInputSpace",
           [](const ComposerHandle& self) {
             return self.get().declaredInputSpace();
           })
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
      .def("active",
           [](const ComposerHandle& self) { return self.get().active(); })
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
          [](const ComposerHandle& self, const std::string& key) {
            return self.get().bounds(key);
          },
          py::arg("key"))
      .def(
          "settling",
          [](const ComposerHandle& self, const std::string& key) {
            return self.get().settling(key);
          },
          py::arg("key"))
      .def(
          "cascadeSpanMs",
          [](const ComposerHandle& self, const std::string& key,
             std::size_t trackIndex) {
            return self.get().cascadeSpanMs(key, trackIndex);
          },
          py::arg("key"), py::arg("trackIndex"))
      .def(
          "hitTest",
          [](const ComposerHandle& self, py::handle canvasPoint) {
            const SkPoint at = canvasPosition(canvasPoint);
            return self.get().hitTest(at);
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
           const weave::Selector& selector, weave::Unit unit) {
          return self.get().units(key, selector, unit);
        },
        py::arg("key"), py::arg("selector"), py::arg("unit"));
}

void bindIntrospection(py::class_<ComposerHandle>& composer) {
  composer
      .def("stats",
           [](const ComposerHandle& self) -> Composer::Stats {
             return self.get().stats();
           })
      .def_static(
          "promotionReason",
          [](Composer::Promotion promotion) {
            return std::string(Composer::promotionReason(promotion));
          },
          py::arg("promotion"))
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
           })
      .def(
          "setCompositeCounting",
          [](const ComposerHandle& self, bool on) {
            self.get().setCompositeCounting(on);
          },
          py::arg("on"))
      .def("compositeCounting",
           [](const ComposerHandle& self) {
             return self.get().compositeCounting();
           })
      .def("compositePlane",
           [](const ComposerHandle& self) -> Composer::CompositePlane {
             return self.get().compositePlane();
           });
}

void bindSettings(py::class_<ComposerHandle>& composer) {
  composer
      // The policy stands ahead of the two-state form, so a policy is
      // read as one and only a plain truth value reaches the second.
      .def(
          "setAutoTexturePromotion",
          [](const ComposerHandle& self, compose::PromotionPolicy policy) {
            self.get().setAutoTexturePromotion(policy);
          },
          py::arg("policy"))
      .def(
          "setAutoTexturePromotion",
          [](const ComposerHandle& self, bool on) {
            self.get().setAutoTexturePromotion(on);
          },
          py::arg("on"))
      .def("autoTexturePromotionPolicy",
           [](const ComposerHandle& self) {
             return self.get().autoTexturePromotionPolicy();
           })
      .def("autoTexturePromotion",
           [](const ComposerHandle& self) {
             return self.get().autoTexturePromotion();
           })
      .def(
          "setBakeDensity",
          [](const ComposerHandle& self, float devicePixelsPerUnit) {
            self.get().setBakeDensity(devicePixelsPerUnit);
          },
          py::arg("devicePixelsPerUnit"))
      .def("bakeDensity",
           [](const ComposerHandle& self) { return self.get().bakeDensity(); })
      .def(
          "setPointer",
          [](const ComposerHandle& self, py::handle canvasPoint,
             bool pressed) {
            const SkPoint at = canvasPosition(canvasPoint);
            self.get().setPointer(at, pressed);
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

  // What decides a promotion is a value a paint context carries too, so
  // whichever registration reaches it first names it and the other is
  // handed the same class.
  if (!registered<compose::PromotionPolicy>())
    py::enum_<compose::PromotionPolicy>(composition, "PromotionPolicy")
        .value("Off", compose::PromotionPolicy::Off)
        .value("ByCost", compose::PromotionPolicy::ByCost)
        .value("Eager", compose::PromotionPolicy::Eager);

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
