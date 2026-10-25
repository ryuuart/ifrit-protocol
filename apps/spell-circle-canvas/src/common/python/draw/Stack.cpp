/** @file
 * The layer stack in Python: named pixel layers, each drawn with a pen of
 * its own, composited bottom to top and put down on any pen. A stack held
 * in Python is drawn with no frame open on the process's host-less device
 * — the device its frames are drawn on, raster where it has none — so it
 * can be drawn on, composited and looked at with no window; a pen on that
 * device puts the composite down as it stands, and a picture handed to
 * Python is read back.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Hostless.h>
#include <sigildraw/Stack.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Encode.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/draw/Registration.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

// ---- the host a stack is drawn under when no frame lends one ---------------

/** The font context text drawn with no host shapes with: the platform's
 *  installed families, one context per thread, as a context belongs to
 *  the thread that made it. */
weave::FontContext& hostlessFonts() {
  thread_local weave::FontContext fonts(weave::ports::systemFontManager());
  return fonts;
}

/** A pen with no frame, at @p density, over a canvas @p width by
 *  @p height, shaping with the platform's installed families. */
draw::HostlessPen hostless(float density, float width, float height) {
  return draw::HostlessPen(density, width, height, &hostlessFonts());
}

/** @p image on the CPU, read back where it stands on a device, for a
 *  caller that reads its pixels. */
sk_sp<SkImage> readable(const sk_sp<SkImage>& image) {
  if (!image) return nullptr;
  sk_sp<SkImage> raster = draw::HostlessPen::rasterImage(image);
  if (!raster)
    throw std::runtime_error("The picture could not be read back from the device.");
  return raster;
}

// ---- the stack as Python holds it -------------------------------------------

/** A STACK AND WHAT PYTHON KEEPS BESIDE IT: which layers are still
 *  standing, so a layer handle outliving its layer says so instead of
 *  dangling; which layers have a frame open; and the density the stack is
 *  drawn at with no host, which is the density a pen last put it down at
 *  and one before any pen has. */
class StackHandle {
 public:
  StackHandle(float width, float height)
      : stack(width, height), m_thread(std::this_thread::get_id()) {}

  /** A STACK GOES WITH THE THREAD USING IT, as a host may run a sketch's
   *  setup on one thread and its frames on another: its layers follow the
   *  recorder of the thread that draws or composites them. A frame left
   *  open belongs to the thread that opened it, so while one is open the
   *  stack is used there and nowhere else. */
  void check() const {
    const std::thread::id here = std::this_thread::get_id();
    if (here == m_thread) return;
    if (!open.empty())
      throw std::runtime_error(
          "A stack with a frame open is used on the thread that opened it.");
    m_thread = here;
  }

  draw::Stack stack;
  /** Every standing layer and the serial it was made under: a layer
   *  removed and another made at the same address is a different layer. */
  std::unordered_map<const draw::Layer*, uint64_t> serials;
  /** The pen of each layer with a frame open. */
  std::unordered_map<const draw::Layer*, std::shared_ptr<BorrowedPen>> open;
  uint64_t nextSerial = 1;
  float density = 1.0f;
  /** The first error a clip shape raised while the composite was made,
   *  raised again once the composite is done. */
  std::exception_ptr failure;
  /** The composite read back to the CPU, and the revision of the stack
   *  it was read at: a composite that has not been made again is read
   *  once, however often it is looked at or put down on a pen that draws
   *  elsewhere. */
  sk_sp<SkImage> readBack;
  uint64_t readBackAt = 0;

 private:
  mutable std::thread::id m_thread;
};

/** ONE LAYER OF A STACK, AS PYTHON HOLDS IT: the stack it belongs to, kept
 *  alive by the handle, and the layer, checked to be still standing at
 *  every use. */
class LayerHandle {
 public:
  LayerHandle(std::shared_ptr<StackHandle> owner, draw::Layer& layer)
      : m_owner(std::move(owner)),
        m_layer(&layer),
        m_serial(m_owner->serials.at(&layer)) {}

  draw::Layer& get() const {
    m_owner->check();
    if (!standing())
      throw std::runtime_error("This layer was removed from its stack.");
    return *m_layer;
  }
  /** Whether the layer is still on its stack. */
  bool standing() const {
    const auto found = m_owner->serials.find(m_layer);
    return found != m_owner->serials.end() && found->second == m_serial;
  }
  const std::shared_ptr<StackHandle>& owner() const { return m_owner; }
  bool operator==(const LayerHandle& other) const {
    return m_layer == other.m_layer && m_serial == other.m_serial;
  }
  size_t hash() const {
    return std::hash<const void*>{}(m_layer) ^ std::hash<uint64_t>{}(m_serial);
  }

 private:
  std::shared_ptr<StackHandle> m_owner;
  draw::Layer* m_layer;
  uint64_t m_serial;
};

/** The layer named @p name, made on top when the stack has none. A layer
 *  is formed with no frame as it is made, on the host-less device at the
 *  stack's density, so a frame's pen beginning it later finds it formed
 *  and leaves it there. */
LayerHandle layerNamed(const std::shared_ptr<StackHandle>& owner,
                       const std::string& name) {
  owner->check();
  if (draw::Layer* existing = owner->stack.find(name))
    return {owner, *existing};
  draw::Layer& made = owner->stack.layer(name);
  owner->serials[&made] = owner->nextSerial++;
  draw::HostlessPen host =
      hostless(owner->density, owner->stack.width(), owner->stack.height());
  made.begin(host.pen());
  made.end();
  return {owner, made};
}

/** The name a layer argument means: a name, or a layer of @p owner. */
std::string layerName(const std::shared_ptr<StackHandle>& owner,
                      py::handle value) {
  if (py::isinstance<py::str>(value)) return value.cast<std::string>();
  const auto& layer = value.cast<const LayerHandle&>();
  if (layer.owner() != owner)
    throw py::value_error("The layer belongs to another stack.");
  return layer.get().name();
}

/** The composite, re-made with no frame at the stack's density when
 *  anything in it moved, on the device its layers stand on, with any error
 *  a clip shape raised raised here. */
sk_sp<SkImage> composite(StackHandle& owner) {
  owner.check();
  draw::HostlessPen host =
      hostless(owner.density, owner.stack.width(), owner.stack.height());
  owner.failure = nullptr;
  sk_sp<SkImage> image = owner.stack.image(host.pen());
  if (owner.failure) {
    std::exception_ptr failure = std::exchange(owner.failure, nullptr);
    std::rethrow_exception(failure);
  }
  return image;
}

/** The composite of @p owner on the CPU, read back only when it moved
 *  since the last read. */
sk_sp<SkImage> readableComposite(StackHandle& owner) {
  const sk_sp<SkImage> image = composite(owner);
  if (!image || !image->isTextureBacked()) return image;
  if (!owner.readBack || owner.readBackAt != owner.stack.revision()) {
    owner.readBack = readable(image);
    owner.readBackAt = owner.stack.revision();
  }
  return owner.readBack;
}

/** @p image as PNG bytes at one pixel per canvas unit of a canvas
 *  @p width by @p height: what Jupyter and IPython show inline. */
py::object canvasPng(const sk_sp<SkImage>& standing, float width,
                     float height) {
  if (!standing) return py::none();
  const sk_sp<SkImage> image = readable(standing);
  const int columns = std::max(1, static_cast<int>(std::lround(width)));
  const int rows = std::max(1, static_cast<int>(std::lround(height)));
  sk_sp<SkImage> shown = image;
  if (image->width() != columns || image->height() != rows) {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(columns, rows));
    if (!surface) throw std::bad_alloc();
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    surface->getCanvas()->drawImageRect(
        image, SkRect::MakeIWH(columns, rows),
        SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
        nullptr);
    shown = surface->makeImageSnapshot();
  }
  const std::vector<std::byte> bytes =
      media::encode(media::fromSk(shown), media::Format::Png);
  if (bytes.empty()) throw py::value_error("The picture could not be encoded.");
  return py::bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

// ---- the words a layer is set with ------------------------------------------

/** A word with case, spaces, hyphens and underscores aside. */
std::string folded(std::string_view word) {
  std::string out;
  for (const char letter : word)
    if (letter != ' ' && letter != '-' && letter != '_')
      out.push_back(static_cast<char>(
          std::tolower(static_cast<unsigned char>(letter))));
  return out;
}

struct BlendWord {
  const char* word;
  draw::Constant mode;
};

constexpr BlendWord kBlendWords[] = {
    {"blend", draw::BLEND},       {"add", draw::ADD},
    {"multiply", draw::MULTIPLY}, {"screen", draw::SCREEN},
    {"replace", draw::REPLACE},   {"remove", draw::REMOVE},
    {"darkest", draw::DARKEST},   {"lightest", draw::LIGHTEST},
    {"difference", draw::DIFFERENCE}, {"exclusion", draw::EXCLUSION},
    {"overlay", draw::OVERLAY},   {"hardlight", draw::HARD_LIGHT},
    {"softlight", draw::SOFT_LIGHT}, {"dodge", draw::DODGE},
    {"burn", draw::BURN},         {"subtract", draw::SUBTRACT},
};

std::string blendWords() {
  return "blend, add, multiply, screen, replace, remove, darkest, lightest, "
         "difference, exclusion, overlay, hard light, soft light, dodge, burn "
         "and subtract";
}

/** The blend @p value names: one of the pen's blend constants, or its
 *  word in any case, with spaces, hyphens and underscores aside. */
draw::Constant blendMode(py::handle value) {
  if (py::isinstance<draw::Constant>(value)) {
    const auto mode = value.cast<draw::Constant>();
    for (const BlendWord& entry : kBlendWords)
      if (entry.mode == mode) return mode;
    throw py::value_error("A layer's blend is one of the pen's blend words: " +
                          blendWords() + ".");
  }
  const std::string word = folded(value.cast<std::string>());
  for (const BlendWord& entry : kBlendWords)
    if (word == entry.word) return entry.mode;
  throw py::value_error("\"" + value.cast<std::string>() +
                        "\" is not one of the pen's blend words: " +
                        blendWords() + ".");
}

draw::MaskChannel maskChannel(const std::string& word) {
  const std::string channel = folded(word);
  if (channel == "alpha") return draw::MaskChannel::Alpha;
  if (channel == "luminance") return draw::MaskChannel::Luminance;
  throw py::value_error("A mask reads \"alpha\" or \"luminance\", not \"" +
                        word + "\".");
}

/** A pair along x and y, or one number: the same along both when
 *  @p uniform, along x alone otherwise. */
SkVector pairOr(py::handle value, bool uniform) {
  if (py::isinstance<py::float_>(value) || py::isinstance<py::int_>(value)) {
    const float number = value.cast<float>();
    return {number, uniform ? number : 0.0f};
  }
  return point(value);
}

py::tuple pair(SkPoint point) { return py::make_tuple(point.fX, point.fY); }

// ---- an open frame on a layer -----------------------------------------------

/** WHAT `layer.pen()` ANSWERS: a frame on the layer, opened as it is
 *  entered and closed as it is left, whose pen draws on the layer. */
class LayerFrame {
 public:
  LayerFrame(LayerHandle layer, std::shared_ptr<BorrowedPen> host)
      : m_layer(std::move(layer)), m_host(std::move(host)) {}

  std::shared_ptr<BorrowedPen> enter() {
    draw::Layer& layer = m_layer.get();
    StackHandle& owner = *m_layer.owner();
    if (owner.open.count(&layer))
      throw std::runtime_error("This layer already has an open frame.");
    draw::Pen* pen = nullptr;
    if (m_host) {
      pen = &layer.begin(m_host->get());
    } else {
      draw::HostlessPen host =
          hostless(owner.density, owner.stack.width(), owner.stack.height());
      pen = &layer.begin(host.pen());
    }
    m_pen = std::make_shared<BorrowedPen>(*pen);
    owner.open[&layer] = m_pen;
    if (m_host)
      m_host->whenClosed([frame = std::weak_ptr<BorrowedPen>(m_pen),
                          layer = m_layer] {
        if (auto open = frame.lock()) close(layer, open);
      });
    return m_pen;
  }

  void leave() {
    if (m_pen) close(m_layer, m_pen);
    m_pen.reset();
  }

  /** Ends the frame @p pen is the pen of, when it is still the layer's
   *  open frame. */
  static void close(const LayerHandle& handle,
                    const std::shared_ptr<BorrowedPen>& pen) {
    if (!handle.standing()) return;
    StackHandle& owner = *handle.owner();
    draw::Layer& layer = handle.get();
    const auto open = owner.open.find(&layer);
    if (open == owner.open.end() || open->second != pen) return;
    owner.open.erase(open);
    pen->invalidate();
    layer.end();
    // What the frame drew goes to the device as it closes, as a host-less
    // pen's drawing does as the pen ends.
    if (draw::HostlessDevice* device = draw::hostlessDevice())
      device->submit();
  }

 private:
  LayerHandle m_layer;
  std::shared_ptr<BorrowedPen> m_host;
  std::shared_ptr<BorrowedPen> m_pen;
};

/** Ends the open frame of @p layer, if it has one, without asking whether
 *  the layer still stands: what removing the layer does first. */
void closeOpenFrame(StackHandle& owner, draw::Layer& layer) {
  const auto open = owner.open.find(&layer);
  if (open == owner.open.end()) return;
  std::shared_ptr<BorrowedPen> pen = open->second;
  owner.open.erase(open);
  pen->invalidate();
  layer.end();
}

/** Puts @p owner's composite down on @p pen with its corner at @p x,
 *  @p y and at the stack's canvas size, re-made first at the pen's density
 *  when anything moved: as it stands where the pen draws on the device
 *  the stack stands on, read back only where it draws elsewhere. */
void putDown(const std::shared_ptr<StackHandle>& owner, draw::Pen& pen,
             float x, float y) {
  owner->density = pen.contentScale();
  sk_sp<SkImage> image = composite(*owner);
  if (image && !draw::HostlessPen::drawnAsItStands(pen, *image))
    image = readableComposite(*owner);
  if (image)
    pen.image(image, x, y, owner->stack.width(), owner->stack.height());
}

// ---- the bindings -----------------------------------------------------------

void bindLayerTransform(py::module_& module) {
  py::class_<draw::LayerTransform>(
      module, "LayerTransform",
      "Where a layer lands on its stack: an affine placement about origin, "
      "built as the pen's own verbs build one — translate to the origin "
      "moved by translation, rotate, shear along x then y, scale, and "
      "translate back by the origin. Angles are radians, turning clockwise "
      "on screen.")
      .def(py::init([](py::object origin, py::object translation,
                       py::object scale, py::object shear,
                       AngleInRadians rotation) {
             draw::LayerTransform placement;
             if (!origin.is_none()) placement.origin = point(origin);
             if (!translation.is_none())
               placement.translation = point(translation);
             if (!scale.is_none()) placement.scale = pairOr(scale, true);
             if (!shear.is_none()) placement.shear = pairOr(shear, false);
             placement.rotation = rotation.value;
             return placement;
           }),
           py::kw_only(), py::arg("origin") = py::none(),
           py::arg("translation") = py::none(), py::arg("scale") = py::none(),
           py::arg("shear") = py::none(),
           py::arg("rotation") = AngleInRadians{})
      .def_property_readonly(
          "origin",
          [](const draw::LayerTransform& self) { return pair(self.origin); },
          "The point the rotation, the shear and the scale hold still.")
      .def_property_readonly(
          "translation",
          [](const draw::LayerTransform& self) {
            return pair(self.translation);
          },
          "How far the layer moves, after it is turned and scaled.")
      .def_property_readonly(
          "scale",
          [](const draw::LayerTransform& self) { return pair(self.scale); },
          "The scale along x and along y.")
      .def_property_readonly(
          "shear",
          [](const draw::LayerTransform& self) { return pair(self.shear); },
          "The shear along x and along y, each an angle.")
      .def_readonly("rotation", &draw::LayerTransform::rotation,
                    "The turn, clockwise on screen.")
      .def("__eq__", [](const draw::LayerTransform& self,
                        const draw::LayerTransform& other) {
        return self == other;
      }, py::arg("other"))
      .def("__repr__", [](const draw::LayerTransform& self) {
        return "LayerTransform(origin=(" + std::to_string(self.origin.fX) +
               ", " + std::to_string(self.origin.fY) + "), translation=(" +
               std::to_string(self.translation.fX) + ", " +
               std::to_string(self.translation.fY) + "), scale=(" +
               std::to_string(self.scale.fX) + ", " +
               std::to_string(self.scale.fY) + "), shear=(" +
               std::to_string(self.shear.fX) + ", " +
               std::to_string(self.shear.fY) +
               "), rotation=" + std::to_string(self.rotation) + ")";
      });
}

void bindLayer(py::module_& module) {
  py::class_<LayerFrame>(
      module, "LayerFrame",
      "A frame on a layer: entered, it opens and answers the layer's pen; "
      "left, it closes, and what the pen drew stands on the layer.")
      .def("__enter__", &LayerFrame::enter)
      .def("__exit__",
           [](LayerFrame& self, const py::args&) {
             self.leave();
             return false;
           });

  py::class_<LayerHandle>(
      module, "Layer",
      "One layer of a stack: a buffer the size of the stack, drawn with a "
      "pen of its own, with a name, a blend, an opacity, a placement, a "
      "mask, a clip and whether it shows. Made by Stack.layer.")
      .def_property_readonly(
          "name",
          [](const LayerHandle& self) { return self.get().name(); },
          "The name the stack knows this layer by.")
      .def(
          "pen",
          [](const LayerHandle& self, std::shared_ptr<BorrowedPen> host) {
            (void)self.get();
            return LayerFrame(self, std::move(host));
          },
          py::arg("host") = py::none(),
          "A frame on the layer, for a with statement: `with layer.pen() as "
          "pen:` draws on the layer, and what was drawn before stands until "
          "the pen clears it. Under a host pen, the frame reads the host's "
          "clock, input, fonts and density and closes with the host's frame; "
          "with none, it is drawn at the stack's density on the device the "
          "process draws its frames on, or on raster where it has none, with "
          "the platform's installed families.")
      .def(
          "bake",
          [](const LayerHandle& self, const compose::Element& element,
             std::optional<py::object> size, int index) {
            draw::Layer& layer = self.get();
            StackHandle& owner = *self.owner();
            if (owner.open.count(&layer))
              throw std::runtime_error("This layer already has an open frame.");
            SkSize extent{owner.stack.width(), owner.stack.height()};
            if (size && !size->is_none()) {
              const SkPoint given = point(*size);
              extent = {given.fX, given.fY};
            }
            draw::HostlessPen host = hostless(
                owner.density, owner.stack.width(), owner.stack.height());
            layer.bake(host.pen(), element, extent, index);
          },
          py::arg("element"), py::arg("size") = py::none(),
          py::arg("index") = 0,
          "Clears the layer and paints a compose element into it, laid out "
          "at size — the stack's size when none is given — with its corner "
          "at the layer's origin. The element is kept between bakes under "
          "index, so a tree baked again keeps its layout and caches.")
      .def(
          "image",
          [](const LayerHandle& self) { return readable(self.get().image()); },
          "The layer's pixels alone, before its blend, opacity, placement "
          "and mask, read back from the device it is drawn on.")
      .def(
          "blend",
          [](const LayerHandle& self) { return self.get().blendMode(); },
          "The blend the layer meets the layers beneath it with.")
      .def(
          "blend",
          [](const LayerHandle& self, py::handle mode) {
            self.get().blendMode(blendMode(mode));
          },
          py::arg("mode"),
          "Sets the blend: one of the pen's blend constants or its word — "
          "\"blend\", where a layer starts; \"remove\", which erases what "
          "lies beneath wherever this layer has ink; \"multiply\", "
          "\"screen\", \"add\" and the rest.")
      .def_property(
          "opacity", [](const LayerHandle& self) { return self.get().opacity(); },
          [](const LayerHandle& self, float amount) {
            self.get().opacity(amount);
          },
          "How much of the layer composites, from 0 to 1; outside that it is "
          "clamped.")
      .def_property_readonly(
          "placement",
          [](const LayerHandle& self) { return self.get().transform(); },
          "Where the layer lands on the stack, as transform last placed it.")
      .def(
          "transform",
          [](const LayerHandle& self, const draw::LayerTransform& placement) {
            self.get().transform(placement);
          },
          py::arg("placement"),
          "Places the layer where another layer's placement says.")
      .def(
          "transform",
          [](const LayerHandle& self, py::object scale, py::object shear,
             std::optional<AngleInRadians> rotateRadians, py::object translate,
             py::object origin) {
            draw::LayerTransform placement;
            if (!scale.is_none()) placement.scale = pairOr(scale, true);
            if (!shear.is_none()) placement.shear = pairOr(shear, false);
            if (rotateRadians) placement.rotation = rotateRadians->value;
            if (!translate.is_none()) placement.translation = point(translate);
            if (!origin.is_none()) placement.origin = point(origin);
            self.get().transform(placement);
          },
          py::kw_only(), py::arg("scale") = py::none(),
          py::arg("shear") = py::none(), py::arg("rotateRadians") = py::none(),
          py::arg("translate") = py::none(), py::arg("origin") = py::none(),
          "Places the layer on the stack, replacing the placement before: "
          "scale is one number or a pair along x and y, and unequal factors "
          "squash the layer; shear is a pair of angles along x and y, or one "
          "along x; rotateRadians is the turn, clockwise on screen; translate "
          "moves the layer after it is turned and scaled; origin is the point "
          "the rest hold still. A bare number of an angle is radians, and "
          "angle text such as '45deg' or a core.angle.Angle names its own "
          "measure; a word left out takes its default, so transform() with "
          "none is resetMatrix().")
      .def(
          "resetMatrix",
          [](const LayerHandle& self) { self.get().resetMatrix(); },
          "Back to the placement the layer was drawn in.")
      .def(
          "mask",
          [](const LayerHandle& self) -> std::optional<LayerHandle> {
            const draw::Layer* source = self.get().maskSource();
            if (!source) return std::nullopt;
            return LayerHandle(self.owner(), const_cast<draw::Layer&>(*source));
          },
          "The layer masking this one, or None.")
      .def(
          "mask",
          [](const LayerHandle& self, std::optional<LayerHandle> source,
             const std::string& channel) {
            draw::Layer& layer = self.get();
            if (!source) return layer.noMask();
            if (source->owner() != self.owner())
              throw py::value_error("A mask is a layer of the same stack.");
            layer.mask(source->get(), maskChannel(channel));
          },
          py::arg("source"), py::arg("channel") = "alpha",
          "Shows the layer only where source has ink, read by its \"alpha\" "
          "or by its \"luminance\" — white ink shows, black ink and no ink "
          "hide — where source's own placement puts it, whatever its blend, "
          "opacity or visibility. None removes the mask.")
      .def(
          "noMask", [](const LayerHandle& self) { self.get().noMask(); },
          "Removes the mask.")
      .def(
          "clip",
          [](const LayerHandle& self, std::optional<py::function> shape,
             bool invert) {
            draw::Layer& layer = self.get();
            if (!shape) return layer.noClip();
            // The shape is read whenever the composite is made again, so
            // the stack holds it, and lets it go with the interpreter's
            // lock held.
            std::shared_ptr<py::object> held(
                new py::object(std::move(*shape)), [](py::object* object) {
                  const py::gil_scoped_acquire lock;
                  delete object;
                });
            StackHandle* owner = self.owner().get();
            layer.clip(
                [held, owner](draw::Pen& pen) {
                  const py::gil_scoped_acquire lock;
                  auto borrowed = std::make_shared<BorrowedPen>(pen);
                  try {
                    (*held)(borrowed);
                  } catch (...) {
                    if (!owner->failure)
                      owner->failure = std::current_exception();
                  }
                  borrowed->invalidate();
                },
                {.invert = invert});
          },
          py::arg("shape"), py::arg("invert") = false,
          "Shows the layer only where shape covers: shape is drawn with a pen "
          "in the stack's units whenever the composite is made again, and its "
          "shape verbs record the outline; invert cuts it out instead. None "
          "removes the clip.")
      .def(
          "noClip", [](const LayerHandle& self) { self.get().noClip(); },
          "Removes the clip.")
      .def_property_readonly(
          "isClipped",
          [](const LayerHandle& self) { return self.get().isClipped(); },
          "Whether a clip shape confines the layer.")
      .def(
          "show", [](const LayerHandle& self) { self.get().show(); },
          "Shows the layer.")
      .def(
          "hide", [](const LayerHandle& self) { self.get().hide(); },
          "Hides the layer: it composites nothing and still serves as a "
          "mask.")
      .def_property_readonly(
          "isVisible",
          [](const LayerHandle& self) { return self.get().isVisible(); },
          "Whether the layer shows.")
      .def_property_readonly(
          "revision",
          [](const LayerHandle& self) { return self.get().revision(); },
          "How many times the layer has changed.")
      .def(
          "_repr_png_",
          [](const LayerHandle& self) {
            const draw::Layer& layer = self.get();
            const draw::Stack& stack = self.owner()->stack;
            return canvasPng(layer.image(), stack.width(), stack.height());
          },
          "The layer's pixels alone as PNG bytes, one pixel per canvas unit: "
          "what Jupyter and IPython show inline.")
      .def("__eq__",
           [](const LayerHandle& self, const LayerHandle& other) {
             return self == other;
           },
           py::arg("other"))
      .def("__hash__", &LayerHandle::hash)
      .def("__repr__", [](const LayerHandle& self) {
        return "Layer(" + py::repr(py::str(self.get().name())).cast<std::string>() +
               ")";
      });
}

void bindStackClass(py::module_& root, py::module_& module) {
  py::class_<StackHandle, std::shared_ptr<StackHandle>>(
      module, "Stack",
      "A picture made of named pixel layers at one canvas size, composited "
      "bottom to top, each with its blend, opacity, placement and mask. The "
      "composite is kept, and made again only when a layer changed.")
      .def(py::init([](float width, float height) {
             return std::make_shared<StackHandle>(width, height);
           }),
           py::arg("width"), py::arg("height"))
      .def(py::init([](py::object size) {
             const SkPoint extent = point(size);
             return std::make_shared<StackHandle>(extent.fX, extent.fY);
           }),
           py::arg("size"))
      .def(
          "layer",
          [](const std::shared_ptr<StackHandle>& self,
             const std::string& name) { return layerNamed(self, name); },
          py::arg("name"),
          "The layer named so, made on top when the stack has none: asking "
          "again edits the layer rather than piling up another.")
      .def(
          "find",
          [](const std::shared_ptr<StackHandle>& self,
             const std::string& name) -> std::optional<LayerHandle> {
            self->check();
            if (draw::Layer* found = self->stack.find(name))
              return LayerHandle(self, *found);
            return std::nullopt;
          },
          py::arg("name"), "The layer named so, or None.")
      .def(
          "remove",
          [](const std::shared_ptr<StackHandle>& self, py::handle layer) {
            self->check();
            const std::string name = layerName(self, layer);
            draw::Layer* found = self->stack.find(name);
            if (!found) return false;
            closeOpenFrame(*self, *found);
            self->serials.erase(found);
            return self->stack.remove(name);
          },
          py::arg("layer"),
          "Removes the layer, named or given, and every mask it served; "
          "answers whether there was one. A handle to it raises from here.")
      .def(
          "reorder",
          [](const std::shared_ptr<StackHandle>& self, py::handle layer,
             std::size_t position) {
            self->check();
            return self->stack.reorder(layerName(self, layer), position);
          },
          py::arg("layer"), py::arg("position"),
          "Moves the layer, named or given, to position counting from the "
          "bottom, which is zero; a position past the top is the top. "
          "Answers whether there was such a layer.")
      .def_property_readonly(
          "names",
          [](const StackHandle& self) {
            self.check();
            return self.stack.names();
          },
          "The layers' names, bottom to top.")
      .def_property_readonly(
          "layers",
          [](const std::shared_ptr<StackHandle>& self) {
            self->check();
            std::vector<LayerHandle> layers;
            for (const std::string& name : self->stack.names())
              layers.emplace_back(self, *self->stack.find(name));
            return layers;
          },
          "The layers, bottom to top.")
      .def("__len__",
           [](const StackHandle& self) {
             self.check();
             return self.stack.count();
           })
      .def(
          "__contains__",
          [](const std::shared_ptr<StackHandle>& self, py::handle layer) {
            self->check();
            if (py::isinstance<py::str>(layer))
              return self->stack.find(layer.cast<std::string>()) != nullptr;
            const auto& handle = layer.cast<const LayerHandle&>();
            return handle.owner() == self && handle.standing();
          },
          py::arg("layer"))
      .def_property_readonly(
          "width",
          [](const StackHandle& self) { return self.stack.width(); },
          "The canvas width, in canvas units.")
      .def_property_readonly(
          "height",
          [](const StackHandle& self) { return self.stack.height(); },
          "The canvas height, in canvas units.")
      .def_property_readonly(
          "size",
          [](const StackHandle& self) {
            return py::make_tuple(self.stack.width(), self.stack.height());
          },
          "The canvas size, the size of every layer.")
      .def_property_readonly(
          "density",
          [](const StackHandle& self) { return self.density; },
          "The device pixels per canvas unit a layer's pen and the composite "
          "are formed at when no host pen lends one: the density a pen last "
          "put the stack down at, and one before any pen has.")
      .def(
          "resize",
          [](StackHandle& self, float width, float height) {
            self.check();
            self.stack.resize(width, height);
          },
          py::arg("width"), py::arg("height"),
          "Another canvas size for the stack and every layer, pixels kept "
          "and scaled.")
      .def(
          "setDensityFloor",
          [](StackHandle& self, float density) {
            self.check();
            self.stack.setDensityFloor(density);
          },
          py::arg("density"),
          "A floor on the density every layer and the composite are formed "
          "at.")
      .def_property_readonly(
          "revision",
          [](const StackHandle& self) { return self.stack.revision(); },
          "How many times the composite has been made.")
      .def(
          "image", [](StackHandle& self) { return readableComposite(self); },
          "The composite as an image, made again first when anything in the "
          "stack moved, at the stack's density, and read back from the device "
          "it is drawn on.")
      .def(
          "composite",
          [](const std::shared_ptr<StackHandle>& self,
             const std::shared_ptr<BorrowedPen>& pen, float x, float y) {
            if (!pen) throw py::type_error("A stack is put down on a pen.");
            putDown(self, pen->get(), x, y);
          },
          py::arg("pen"), py::arg("x") = 0.0f, py::arg("y") = 0.0f,
          "Puts the stack down on pen with its corner at x, y and at the "
          "stack's size, under the pen's own blend, transform, clip and "
          "image mode, as pen.image puts a picture down. The composite is "
          "made again first, at the pen's density, when anything moved.")
      .def(
          "_repr_png_",
          [](StackHandle& self) {
            return canvasPng(readableComposite(self), self.stack.width(),
                             self.stack.height());
          },
          "The composite as PNG bytes, one pixel per canvas unit: what "
          "Jupyter and IPython show inline.")
      .def("__repr__", [](const StackHandle& self) {
        std::string names;
        for (const std::string& name : self.stack.names()) {
          if (!names.empty()) names += ", ";
          names += py::repr(py::str(name)).cast<std::string>();
        }
        auto number = [](float value) {
          return std::to_string(static_cast<long long>(std::lround(value)));
        };
        return "Stack((" + number(self.stack.width()) + ", " +
               number(self.stack.height()) + "), layers=[" + names + "])";
      });

  extend<BorrowedPen, std::shared_ptr<BorrowedPen>>(root, "draw.Pen")
      .def(
          "stack",
          [](const std::shared_ptr<BorrowedPen>& pen,
             const std::shared_ptr<StackHandle>& stack, float x, float y) {
            putDown(stack, pen->get(), x, y);
          },
          py::arg("stack"), py::arg("x") = 0.0f, py::arg("y") = 0.0f,
          "Puts a layer stack down with its corner at x, y, as "
          "stack.composite(pen, x, y) does.");
}

}  // namespace

void bindDrawStack(py::module_& root) {
  py::module_ module = submodule(root, "draw");
  bindLayerTransform(module);
  bindLayer(module);
  bindStackClass(root, module);
}

}  // namespace sigil::python
