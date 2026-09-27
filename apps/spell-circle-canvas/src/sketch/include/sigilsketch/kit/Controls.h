#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * THE CONTROL SURFACE A STUDY PUTS BESIDE A MATERIAL: one row per
 * parameter a description lists — a slider, a toggle or a run of
 * choices, as the parameter asks — grouped as the description groups
 * them, each row a live value the material follows.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilmotion/values/Animatable.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material {
class Material;
namespace sbsar {
/** A Substance graph's inputs and outputs; its own words are
 *  `<sigilmaterial/substance/Substance.h>`, which this names and does not
 *  include. */
struct Description;
}  // namespace sbsar
}  // namespace sigil::material

namespace sigil::sketch::kit {

/** THE CONTROL A PARAMETER ASKS FOR. */
enum class Widget {
  /** A value dragged along a rail between the parameter's two ends. */
  Slider,
  /** Off or on: zero or one. */
  Toggle,
  /** One of a few named values, each a segment of the rail. */
  Choice,
};

/** ONE NAMED VALUE of a choice. */
struct Option {
  float value = 0;
  compose::Utf8 label;
};

/** ONE PARAMETER A CONTROL SURFACE MOVES: the name the material is set
 *  and bound by, what a reader sees, the group it stands in, the control
 *  it asks for, its range and step, and the value it starts at. */
struct Control {
  std::string name;
  /** Empty shows the name. */
  compose::Utf8 label;
  /** Rows of one group stand together under the group's name; empty
   *  stands first, unheaded. */
  std::string group;
  Widget widget = Widget::Slider;
  float minimum = 0;
  float maximum = 1;
  /** The value moves in whole steps of this from `minimum`; zero moves it
   *  freely. */
  float step = 0;
  float value = 0;
  /** The values a choice offers, in order. */
  std::vector<Option> options;
};

/** THE ROWS A SUBSTANCE GRAPH'S DESCRIPTION ASKS FOR: every number input
 *  of one component, with the widget, range, step and group its author
 *  stated. An input of several components, an image or a text has no row:
 *  a colour picker and a position pad are controls of their own. */
[[nodiscard]] std::vector<Control> controlsOf(
    const material::sbsar::Description& description);

/** THE ROWS A PROGRAM MATERIAL'S PARAMETERS ASK FOR: each float field of
 *  its recipe as a slider from 0 to 1, starting at the value the material
 *  holds. The range is the author's to state with `Controls::range`,
 *  because a field names no range. A material whose base is not a program
 *  answers none. */
[[nodiscard]] std::vector<Control> controlsOf(
    const material::Material& material);

/** HOW A CONTROL SURFACE IS SET. */
struct ControlsView {
  /** The groups to show, in the description's order; empty shows every
   *  group. */
  std::vector<std::string> groups;
  /** The whole width of a row, px. */
  float width = 280;
  /** The width the label takes, px. */
  float labelWidth = 104;
  /** The width the figure after the rail takes, px. */
  float figureWidth = 48;
  /** The height of a row, px. */
  float rowHeight = 18;
};

/** THE LIVE VALUES OF A CONTROL SURFACE, held by the sketch across frames.
 *  Copies share the values, so the copy a description holds and the one
 *  the sketch binds from are one surface. */
class Controls {
 public:
  explicit Controls(std::vector<Control> parameters);
  /** The rows `controlsOf(description)` answers. */
  explicit Controls(const material::sbsar::Description& description);
  /** The rows `controlsOf(material)` answers. */
  explicit Controls(const material::Material& material);

  /** Sets the range and step of the row @p name, clamping its value into
   *  the range; an unknown name is ignored. */
  Controls& range(std::string_view name, float minimum, float maximum,
                  float step = 0);
  /** THE LIVE VALUE of the row @p name, which the row writes as it is
   *  dragged — what `Material::bind` takes. An unknown name answers a
   *  constant zero. */
  [[nodiscard]] motion::Animatable<float> value(std::string_view name) const;
  /** Writes @p value into the row @p name, stepped and clamped as the row
   *  says. */
  void set(std::string_view name, float value);
  /** Binds every row's live value to the parameter of @p material it
   *  names, so the material follows the surface. */
  void bind(material::Material& material) const;

  /** The rows, in the order they were described. */
  [[nodiscard]] std::vector<Control> parameters() const;

  /** @private */
  struct Row;

 private:
  friend compose::Element controls(const Controls&, const ControlsView&);
  std::vector<std::shared_ptr<Row>> m_rows;
};

/** THE CONTROL SURFACE: @p surface's rows, grouped, in the theme in force.
 *
 *      sketch::kit::Controls leavesControls(
 *          material::sbsar::describe(hub, "res://leaves.sbsar"));
 *      leavesControls.bind(leaves);
 *      …
 *      sketch::kit::controls(leavesControls, {.groups = {"Colors"}})
 *
 *  A row answers the pointer the host feeds the composer: pressing on a
 *  rail drags its value, pressing a toggle flips it, pressing a choice's
 *  segment picks it. The rows repaint every frame, since what they draw
 *  moves with no new description. */
[[nodiscard]] compose::Element controls(const Controls& surface,
                                        const ControlsView& how = {});

}  // namespace sigil::sketch::kit
