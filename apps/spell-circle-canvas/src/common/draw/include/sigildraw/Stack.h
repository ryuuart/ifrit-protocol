#pragma once

/** @file
 * @ingroup draw-pen
 *
 * A stack of pixel layers composited bottom to top: each layer an
 * offscreen buffer with a name, a blend, an opacity, a placement and a
 * mask, and the stack the picture they make together, re-made only when
 * one of them moved.
 */

#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>
#include <sigildraw/PenTypes.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::draw {

class Stack;

/** WHERE A LAYER LANDS ON ITS STACK: an affine placement about a stated
 *  origin, in the stack's canvas units, built in the order the pen's own
 *  verbs would build it — translate to the origin moved by
 *  @ref translation, rotate, shear along x then along y, scale, and
 *  translate back by the origin. Angles are radians, the pen's default
 *  angle mode, turning clockwise on screen as `rotate` does. The default
 *  places the layer where it was drawn. */
struct LayerTransform {
  /** The point the rotation, the shear and the scale hold still. */
  SkPoint origin{0.0f, 0.0f};
  /** How far the layer moves, after it is turned and scaled. */
  SkVector translation{0.0f, 0.0f};
  /** The scale along x and along y; unequal factors squash the layer. */
  SkVector scale{1.0f, 1.0f};
  /** The shear along x and along y, each an angle as `shearX` and
   *  `shearY` take one. */
  SkVector shear{0.0f, 0.0f};
  /** The turn, clockwise on screen. */
  float rotation = 0.0f;

  /** The placement as one matrix, from layer units to stack units. */
  [[nodiscard]] SkMatrix matrix() const;

  bool operator==(const LayerTransform&) const = default;
};

/** WHAT OF ANOTHER LAYER A MASK READS: its alpha, or the luma of its
 *  colour multiplied into its alpha — CSS's `mask-mode: luminance`, so
 *  white ink shows the masked layer, black ink and no ink hide it. */
enum class MaskChannel : uint8_t {
  Alpha,
  Luminance,
};

/** ONE LAYER OF A STACK: an offscreen buffer the size of the stack, drawn
 *  on with its own pen as a `Graphics` is, with a name and the words that
 *  say how it meets the layers beneath it — a blend from the pen's own
 *  blend words, an opacity, a placement, a mask and whether it shows.
 *
 *  A LAYER COUNTS ITS CHANGES: every `begin`, `end`, `bake` and every
 *  setting that changes what it composites to moves its `revision`, and
 *  its stack re-makes the composite only when some layer's revision has
 *  moved. A setting given the value it already holds moves nothing.
 *
 *  A layer is made by its stack (`Stack::layer`) and lives until the
 *  stack removes it; a reference held past `Stack::remove` dangles. */
class Layer {
 public:
  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;

  /** The name the stack knows this layer by. */
  [[nodiscard]] const std::string& name() const { return m_name; }

  /** @name Drawing on the layer
   *  The layer's pixels, put down as a `Graphics`'s are.
   *  @{ */
  /** Opens a frame on the layer's buffer and hands back its pen, as
   *  `Graphics::begin` does: formed at the host's density or the stack's
   *  density floor, whichever is greater, with the host's clock, fonts
   *  and input. Pixels drawn earlier stand; `clear()` on the pen starts
   *  from nothing. */
  Pen& begin(Pen& host);
  /** Closes the frame. */
  void end();
  /** A RETAINED GUEST BAKED INTO THE LAYER — a Compose element, or any
   *  value whose library declares `paintRetained` — laid out at @p size
   *  in the layer's canvas units with its corner at the layer's origin,
   *  painted through `Pen::element` at the layer's density onto a cleared
   *  layer. The guest is kept between bakes under the slot @p index and
   *  @p where name, so a baked tree keeps its layout, shaping and caches
   *  from one bake to the next; it inherits the host's ink and font. A
   *  helper wrapping this forwards its caller's location, as one wrapping
   *  `Pen::element` does. */
  template <Retainable G>
  void bake(Pen& host, const G& guest, SkSize size, int index = 0,
            std::source_location where = std::source_location::current()) {
    Pen& pen = begin(host);
    try {
      pen.inherit(host.inheritedInk(), host.inheritedFont());
      pen.clear();
      pen.element(guest, 0.0f, 0.0f, size.width(), size.height(), index,
                  where);
    } catch (...) {
      end();
      throw;
    }
    end();
  }
  /** The layer's pixels alone, before its blend, opacity, placement and
   *  mask; null before its first `begin`. */
  [[nodiscard]] sk_sp<SkImage> image() const { return m_buffer.image(); }
  /** The buffer itself, for `pen.image(layer.buffer(), x, y)`. */
  [[nodiscard]] const Graphics& buffer() const { return m_buffer; }
  /** The layer's own pen, between frames as well as during one, so a
   *  style set once holds for every frame after. */
  [[nodiscard]] Pen& pen() { return m_buffer.pen; }
  /** @} */

  /** @name How the layer meets the layers beneath it
   *  @{ */
  /** The pen's blend words, read when the layer is composited: `BLEND`
   *  over by alpha, where a layer starts; `REMOVE` erases what lies
   *  beneath wherever this layer has ink, which is p5's `erase()`;
   *  `MULTIPLY`, `SCREEN`, `ADD` and the rest as the pen has them. */
  void blendMode(Constant mode);
  [[nodiscard]] Constant blendMode() const { return m_blend; }
  /** How much of the layer composites, from 0 to 1; outside that it is
   *  clamped. A nonfinite value throws std::invalid_argument. */
  void opacity(float amount);
  [[nodiscard]] float opacity() const { return m_opacity; }
  /** Where the layer lands on the stack. A nonfinite number in
   *  @p placement throws std::invalid_argument. */
  void transform(const LayerTransform& placement);
  [[nodiscard]] const LayerTransform& transform() const { return m_transform; }
  /** Back to the placement the layer was drawn in. */
  void resetMatrix() { transform(LayerTransform{}); }
  /** THE LAYER SHOWS ONLY WHERE @p source HAS INK, read through
   *  @p channel: @p source's pixels where its own placement puts them,
   *  whatever its blend, opacity or visibility — so a hidden layer serves
   *  as a mask alone. @p source belongs to the same stack and is not this
   *  layer, or std::invalid_argument is thrown. Removing @p source from
   *  the stack removes the mask. */
  void mask(const Layer& source, MaskChannel channel = MaskChannel::Alpha);
  void noMask();
  /** The layer masking this one, or null. */
  [[nodiscard]] const Layer* maskSource() const { return m_mask; }
  [[nodiscard]] MaskChannel maskChannel() const { return m_maskChannel; }
  /** THE LAYER SHOWS ONLY WHERE @p shape COVERS, as the pen's `clip`
   *  confines: @p shape is run with the compositing pen in the stack's
   *  canvas units each time the stack is re-made, and its shape verbs
   *  record the outline. A clip and a mask together keep what falls in
   *  both. Each call moves the revision, since a shape has no equality.
   *  @trap @p shape is read only when the composite is re-made; a shape
   *  that reads changing state says so by setting it again. */
  void clip(std::function<void(Pen&)> shape, ClipOptions options = {});
  void noClip();
  [[nodiscard]] bool isClipped() const { return static_cast<bool>(m_clip); }
  /** p5's words for an element shown and hidden: a hidden layer
   *  composites nothing and still serves as a mask. */
  void show();
  void hide();
  [[nodiscard]] bool isVisible() const { return m_visible; }
  /** @} */

  /** How many times the layer has changed — every `begin`, `end`,
   *  `bake` and moved setting. */
  [[nodiscard]] uint64_t revision() const { return m_revision; }

 private:
  friend class Stack;
  Layer(Stack& stack, std::string name, float width, float height);
  void touch() { ++m_revision; }

  Stack* m_stack;
  std::string m_name;
  Graphics m_buffer;
  Constant m_blend = BLEND;
  float m_opacity = 1.0f;
  LayerTransform m_transform;
  const Layer* m_mask = nullptr;
  MaskChannel m_maskChannel = MaskChannel::Alpha;
  std::function<void(Pen&)> m_clip;
  ClipOptions m_clipOptions;
  bool m_visible = true;
  uint64_t m_revision = 0;
  uint64_t m_compositedRevision = 0;
};

/** A PICTURE MADE OF LAYERS: an ordered list of layers at one canvas
 *  size, composited bottom to top, each with its blend, opacity,
 *  placement and mask, into the stack's own `Graphics`. What p5 builds
 *  with `createGraphics` and `erase()` is a stack: a buffer is a layer,
 *  and erasing is a layer blended `REMOVE`.
 *
 *  THE COMPOSITE IS KEPT: it is re-made only when a layer's revision has
 *  moved, a layer was added, removed or reordered, the stack was resized
 *  or the host's density changed; otherwise `composite` costs one image
 *  drawn. Its pixels are a function of the layers' pixels and settings,
 *  so a sketch drawing a stack is as deterministic as its layers are. */
class Stack {
 public:
  /** @p width by @p height in canvas units, the size of every layer.
   *  Finite dimensions are clamped to at least one; nonfinite ones throw
   *  std::invalid_argument. */
  Stack(float width, float height);
  ~Stack();
  Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  /** @name The layers
   *  @{ */
  /** THE LAYER NAMED @p name, made on top when the stack has none — so
   *  asking again edits the layer rather than piling up another. */
  Layer& layer(std::string_view name);
  /** The layer named @p name, or null. */
  [[nodiscard]] Layer* find(std::string_view name);
  [[nodiscard]] const Layer* find(std::string_view name) const;
  /** Removes the layer named @p name and every mask it served; answers
   *  whether there was one. References to it dangle from here. */
  bool remove(std::string_view name);
  /** Moves the layer named @p name to @p position counting from the
   *  bottom, which is zero; a position past the top is the top. Answers
   *  whether there was such a layer. */
  bool reorder(std::string_view name, std::size_t position);
  /** The layers' names, bottom to top. */
  [[nodiscard]] std::vector<std::string> names() const;
  [[nodiscard]] std::size_t count() const { return m_layers.size(); }
  /** @} */

  /** @name The composite
   *  @{ */
  /** THE STACK PUT DOWN ON @p pen with its corner at (@p x, @p y) and at
   *  the stack's canvas size, under the pen's own blend, transform, clip
   *  and image mode, as `pen.image` puts a buffer down. The composite is
   *  re-made first when anything in the stack moved, at @p pen's density
   *  or the stack's density floor, whichever is greater. */
  void composite(Pen& pen, float x = 0.0f, float y = 0.0f);
  /** The composite, re-made against @p host first when anything moved. */
  [[nodiscard]] sk_sp<SkImage> image(Pen& host);
  /** The composite as last made; null before the first. */
  [[nodiscard]] sk_sp<SkImage> image() const { return m_composite.image(); }
  /** How many times the composite has been made: what a consumer keying
   *  a cache on the stack's picture reads. */
  [[nodiscard]] uint64_t revision() const { return m_revision; }
  /** @} */

  /** @name The canvas
   *  @{ */
  [[nodiscard]] float width() const { return m_width; }
  [[nodiscard]] float height() const { return m_height; }
  /** Another canvas size for the stack and every layer, pixels kept and
   *  scaled as `Graphics::resize` keeps them. Nonfinite dimensions throw
   *  std::invalid_argument without changing the size. */
  void resize(float width, float height);
  /** A floor on the density every layer and the composite are formed
   *  at, as `Graphics::setDensityFloor` takes one. */
  void setDensityFloor(float devicePixelsPerUnit);
  /** @} */

 private:
  [[nodiscard]] bool moved(float density) const;
  void remake(Pen& host);
  void drawLayer(Pen& pen, const Layer& layer);

  float m_width;
  float m_height;
  float m_densityFloor = 0.0f;
  std::vector<std::unique_ptr<Layer>> m_layers;
  Graphics m_composite;
  uint64_t m_structure = 0;
  uint64_t m_compositedStructure = 0;
  float m_compositedDensity = 0.0f;
  bool m_made = false;
  uint64_t m_revision = 0;
};

}  // namespace sigil::draw
