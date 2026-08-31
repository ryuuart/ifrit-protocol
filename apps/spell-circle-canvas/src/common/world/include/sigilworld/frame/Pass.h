#pragma once

/** @file
 * A PASS: one stage of making a frame, declared as a comparable value —
 * what it reads, what it writes, which bodies it addresses, and the work
 * it does. A pass is never a scene child: it is a step of turning the
 * scene into pixels, not a thing standing in the world.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <sigilcore/reconcile/Erased.h>
#include <sigilworld/element/Geometry.h>
#include <sigilworld/element/Selector.h>
#include <sigilworld/frame/View.h>

#include <concepts>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sigil::world {

class Targets;

/** WHAT KIND OF WORK a pass does. */
enum class Stage : uint8_t {
  Geometry,  ///< paints bodies into a target
  Compute,   ///< cooks points, writing no pixels
  Post,      ///< reads targets and writes one
};

/** HOW A PASS'S SELECTION REACHES THE PIXELS.
 *
 *  `Auto` is what a declaration carries until the ordering rules it on;
 *  the other four are answers, and an ordered pass never carries `Auto`.
 */
enum class Selection : uint8_t {
  Auto,     ///< let the declaration decide
  None,     ///< the pass addresses every body
  Cull,     ///< only the selected bodies are drawn
  Mask,     ///< every pixel is drawn, the selected ones through coverage
  Variant,  ///< the selection is drawn again in the variant surface
};

/** Soften what the pass reads. */
struct Blur {
  float sigma = 4.0f;
  bool operator==(const Blur&) const = default;
};

/** Grade what the pass reads: each channel scaled by @c gain, lifted by
 *  @c lift, and multiplied by @c tint. */
struct Levels {
  float gain = 1.0f;
  float lift = 0.0f;
  SkColor4f tint{1.0f, 1.0f, 1.0f, 1.0f};
  bool operator==(const Levels& other) const {
    return gain == other.gain && lift == other.lift && tint == other.tint;
  }
};

/** Lay what the pass reads one over another: the first layer lands as
 *  it is and every layer after it arrives under @c mode at @c opacity,
 *  so a pass reading its own previous output at less than one decays. */
struct Composite {
  SkBlendMode mode = SkBlendMode::kPlus;
  float opacity = 1.0f;
  bool operator==(const Composite&) const = default;
};

/** WHAT A POST PASS DOES to what it reads. An empty op copies. */
using PostOp = std::variant<std::monostate, Blur, Levels, Composite>;

/** THE PASS ESCAPE: a body that does the work itself, given what the
 *  frame extracted and the frame's targets. A pass carrying one runs it
 *  instead of its stage's own work, and the stage still declares the
 *  reads and writes the ordering needs. */
class PassBodyOps {
 public:
  PassBodyOps() = default;
  PassBodyOps(const PassBodyOps&) = default;
  PassBodyOps(PassBodyOps&&) = default;
  PassBodyOps& operator=(const PassBodyOps&) = default;
  PassBodyOps& operator=(PassBodyOps&&) = default;
  virtual ~PassBodyOps() = default;

  /** Does the pass's work over what the frame extracted (@p view) and
   *  the frame's resources (@p targets). */
  virtual void run(const View& view, Targets& targets) const = 0;
};

/** A body carried as a comparable value. A model with `==` declares its
 *  own identity; a model without one — the lambda door below — compares
 *  equal to nothing but its own copies. */
using PassBody = core::Erased<PassBodyOps>;

/** ONE STAGE OF MAKING A FRAME.
 *
 *  A pass declares the resources it reads and writes by name, and the
 *  ordering derives the sequence, the barriers and the transient
 *  aliasing from those declarations alone — a pass never states its own
 *  position. `previous()` names a resource read as it stood at the end
 *  of the frame BEFORE, which is how a feedback effect declares itself
 *  without asking anything to run before it has run. */
class Pass {
 public:
  Pass() = default;
  Pass(Stage stage, std::string name);

  // ---- what it touches ----
  /** Resources this pass reads, in the order it reads them. */
  template <std::convertible_to<std::string_view>... N>
  Pass& reads(N&&... names) {
    (addRead(std::string(std::string_view(names))), ...);
    return *this;
  }
  /** Resources this pass writes. The first image among them is the one
   *  its stage paints into. */
  template <std::convertible_to<std::string_view>... N>
  Pass& writes(N&&... names) {
    (addWrite(std::string(std::string_view(names))), ...);
    return *this;
  }
  /** @p name AS IT STOOD at the end of the frame before. It orders
   *  nothing: a pass may name its own output here, which is what makes
   *  a feedback loop declarable without a cycle. */
  Pass& previous(std::string name);

  // ---- which bodies ----
  /** The bodies this pass addresses. A pass that narrows nothing
   *  addresses every one of them. */
  Pass& only(Selector selector);
  /** …drawn again in @p surface, which is what makes the selection
   *  visible where the pass paints everything. */
  Pass& variant(::sigil::material::Material surface);
  /** Override how the selection is realised, for a pass that knows
   *  better than the rule. */
  Pass& realise(Selection realisation);

  // ---- the work ----
  /** What a geometry pass clears its target to before it paints. */
  Pass& clear(SkColor4f colour);
  /** The points a compute pass cooks, and the executor it cooks them
   *  on. They land in the pass's first written resource. */
  Pass& chain(Chain c, PopRuntime runtime = PopRuntime::cpu());
  /** The body a geometry pass stands at every point of every point set
   *  it reads. */
  Pass& stamp(Mesh body);
  /** Soften what the pass reads. A pass carries ONE post op, so this
   *  replaces whatever `levels` or `composite` set. */
  Pass& blur(float sigma);
  /** Grade what the pass reads, replacing any other post op. */
  Pass& levels(float gain, float lift, SkColor4f tint = {1, 1, 1, 1});
  /** Lay what the pass reads one layer over another, replacing any other
   *  post op. */
  Pass& composite(SkBlendMode mode, float opacity = 1.0f);
  /** THE ESCAPE, as a comparable seam value. */
  Pass& body(PassBody b);
  /** …and as a lambda, which compares equal to nothing but its own
   *  copies, so a frame carrying one never prunes on it. */
  Pass& body(std::function<void(const View&, Targets&)> fn);

  // ---- what it declared ----
  [[nodiscard]] Stage stage() const { return m_stage; }
  [[nodiscard]] const std::string& name() const { return m_name; }
  [[nodiscard]] std::span<const std::string> reads() const { return m_reads; }
  [[nodiscard]] std::span<const std::string> writes() const { return m_writes; }
  [[nodiscard]] std::span<const std::string> previous() const {
    return m_previous;
  }
  [[nodiscard]] const Selector& selector() const { return m_selector; }
  [[nodiscard]] bool narrowed() const { return m_narrowed; }
  [[nodiscard]] const std::optional<::sigil::material::Material>& variant()
      const {
    return m_variant;
  }
  [[nodiscard]] Selection realisation() const { return m_realisation; }
  [[nodiscard]] SkColor4f clear() const { return m_clear; }
  [[nodiscard]] const Chain& chain() const { return m_chain; }
  [[nodiscard]] const PopRuntime& popRuntime() const { return m_popRuntime; }
  [[nodiscard]] const Mesh& stamp() const { return m_stamp; }
  [[nodiscard]] const PostOp& op() const { return m_op; }
  [[nodiscard]] const PassBody& body() const { return m_body; }

  /** Value equality, field by field. A pass carrying a lambda body is
   *  equal only to its own copies, because a callable is not a value. */
  bool operator==(const Pass& other) const;

 private:
  void addRead(std::string name);
  void addWrite(std::string name);

  Stage m_stage = Stage::Geometry;
  std::string m_name;
  std::vector<std::string> m_reads;
  std::vector<std::string> m_writes;
  std::vector<std::string> m_previous;
  Selector m_selector;
  bool m_narrowed = false;
  std::optional<::sigil::material::Material> m_variant;
  Selection m_realisation = Selection::Auto;
  SkColor4f m_clear{0.0f, 0.0f, 0.0f, 0.0f};
  Chain m_chain;
  PopRuntime m_popRuntime = PopRuntime::cpu();
  Mesh m_stamp;
  PostOp m_op;
  PassBody m_body;
};

/** A pass that paints bodies into a target. */
Pass geometryPass(std::string name);
/** A pass that cooks points and writes no pixels. */
Pass computePass(std::string name);
/** A pass that reads targets and writes one. */
Pass postPass(std::string name);

/** WHAT THE ORDERING DECIDED about one pass, in terms an execution can
 *  act on without knowing how the decision was made. It points at the
 *  pass it describes, so it stands only as long as the passes it was
 *  built from do. */
struct PassWork {
  const Pass* pass = nullptr;
  Selection realisation = Selection::None;
  /** The resource this pass ALSO paints the coverage of a masked pass
   *  downstream into; empty when nothing asked for one. */
  std::string coverageOut;
  /** Whose coverage `coverageOut` receives. */
  Selector coverageOf;
  /** The resource a masked pass reads its coverage from; empty when the
   *  pass is not masked. */
  std::string coverageIn;
};

}  // namespace sigil::world
