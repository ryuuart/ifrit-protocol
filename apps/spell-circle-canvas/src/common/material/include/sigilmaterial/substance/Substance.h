#pragma once

/** @file
 * @ingroup material-substance
 *
 * AN ADOBE SUBSTANCE 3D ARCHIVE AS A MATERIAL. `material::substance(hub,
 * uri)` cooks a `.sbsar` graph and answers a Material like any other: its
 * base colour is the graph's base-colour output, its surface channels are
 * filled from the outputs by the channel each declares, and the graph's
 * inputs are written with the Material's own `set()` and followed with
 * its `bind()`, each of which schedules a cook. Without the SDK the
 * feature still links: `sbsar::available()` is false and every entrance
 * answers an empty material or an empty description.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmedia/core/PixelSource.h>

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::io {
class Hub;
}  // namespace sigil::io

/** The words a Substance graph is described and cooked with: its inputs,
 *  its outputs, the value an input is written with and the precision an
 *  output is cooked at. The tier-1 entrance is `material::substance()`;
 *  what stands here describes and tunes it. */
namespace sigil::material::sbsar {

/** Whether this build found the Substance SDK and its engine starts. */
bool available();

/** What a graph input holds, which decides how many numbers it takes. */
enum class InputType : uint8_t {
  Float,
  Float2,
  Float3,
  Float4,
  Integer,
  Integer2,
  Integer3,
  Integer4,
  Image,
  Text,
  Other,
};

/** The control the author asked for, for a tool that builds one. */
enum class Widget : uint8_t {
  None,
  Slider,
  Angle,
  Color,
  Toggle,
  Buttons,
  Combobox,
  Image,
  Position,
};

/** One entry of a combobox or button input: the value and its label. */
struct Choice {
  int value = 0;
  std::string label;
  bool operator==(const Choice&) const = default;
};

/** ONE GRAPH INPUT, DESCRIBED as its author stated it. Numbers are floats
 *  whatever the type, one per component; an integer input's are whole.
 *  `visibleIf` is the author's expression over other inputs that says
 *  when a tool shows this one, empty when it always shows. */
struct Input {
  /** The graph's own identifier (`Hue_Shift`, `$outputsize`). */
  std::string name;
  std::string label;
  std::string group;
  std::string description;
  InputType type = InputType::Other;
  Widget widget = Widget::None;
  std::vector<float> defaultValue;
  std::vector<float> minimum;
  std::vector<float> maximum;
  float step = 0;
  bool clamp = false;
  std::vector<Choice> choices;
  std::string visibleIf;
  /** How many numbers a value of this input takes. */
  size_t components() const { return defaultValue.size(); }
  bool operator==(const Input&) const = default;
};

/** How an output's values are to be read. */
enum class Encoding : uint8_t {
  Srgb,    ///< colour, sRGB-encoded (a base colour)
  Linear,  ///< colour, linear (an emission cooked to floats)
  Raw,     ///< data, no colour space (a normal, a roughness, a height)
};

/** ONE GRAPH OUTPUT, DESCRIBED: its identifier, the channel it feeds
 *  (`baseColor`, `normal`, `roughness`… as the graph declares it, or the
 *  identifier when it declares none), and how its values are read. */
struct Output {
  std::string name;
  std::string label;
  std::string usage;
  Encoding encoding = Encoding::Raw;
  /** False for an output that answers a number rather than a picture. */
  bool image = true;
  bool operator==(const Output&) const = default;
};

/** WHAT A GRAPH TAKES AND GIVES: its inputs and outputs in the author's
 *  order, and the presets it can be set to — embedded in the archive or
 *  in a `.sbsprs` file of the same name beside it. */
struct Description {
  /** The graph's label. */
  std::string graph;
  std::vector<Input> inputs;
  std::vector<std::string> presets;
  std::vector<Output> outputs;
  bool operator==(const Description&) const = default;
};

/** The precision an output is cooked at. */
enum class Format : uint8_t {
  /** The graph's own, raised to 16 bits for a normal and a height, whose
   *  gradients band at 8. */
  Automatic,
  Unorm8,
  Unorm16,
  Float16,
  Float32,
};

/** AN OUTPUT TO COOK, by the channel it feeds or by its identifier, and
 *  the precision to cook it at. */
struct OutputRequest {
  std::string usage;
  Format format = Format::Automatic;
  bool operator==(const OutputRequest&) const = default;
};

/** A VALUE FOR ONE INPUT, by its identifier: a number, or one number per
 *  component. What `SubstanceOptions::inputs` lists and a generated
 *  input struct's `inputs()` answers. */
struct InputValue {
  InputValue(std::string identifier, float value)
      : identifier(std::move(identifier)), values{value} {}
  InputValue(std::string identifier, std::initializer_list<float> values)
      : identifier(std::move(identifier)), values(values) {}
  InputValue(std::string identifier, std::vector<float> values)
      : identifier(std::move(identifier)), values(std::move(values)) {}
  std::string identifier;
  std::vector<float> values;
  bool operator==(const InputValue&) const = default;
};

/** THE GRAPH NAMED BY @p uri, DESCRIBED — @p graph by label or package
 *  url, the first when empty. Empty when the archive cannot be read, the
 *  graph is not in it, or the SDK is not available. */
Description describe(io::Hub& hub, std::string_view uri,
                     std::string_view graph = {});

/** THE COOKED OUTPUT @p usage (a channel or an identifier) of a material
 *  `material::substance()` made, as a pixel source whose `revision()`
 *  bumps on every cook — an output the surface has no channel for, a
 *  height, read on its own. Empty when @p material is not a cooked graph
 *  or the output was not cooked. */
media::PixelSource output(const Material& material, std::string_view usage);

/** Holds until every cook @p material has scheduled has landed — what a
 *  host capturing a still calls, so the picture is the inputs' values at
 *  that moment. No wait for a material that is not a cooked graph. */
void settle(const Material& material);

}  // namespace sigil::material::sbsar

namespace sigil::material {

/** HOW A GRAPH IS COOKED: initial input values by identifier, a preset,
 *  the random seed, the size, the outputs to cook and at what precision,
 *  and which graph of a many-graph archive. */
struct SubstanceOptions {
  /** Initial values by identifier; a later entry for the same input
   *  wins. Written after the preset. */
  std::vector<sbsar::InputValue> inputs;
  /** A preset by label, embedded in the archive or in a `.sbsprs` beside
   *  it; merged over the authored values. Empty for none. */
  std::string preset;
  /** The graph's random seed; unset keeps the author's. */
  std::optional<int> seed;
  /** Pixels per side, rounded up to a power of two; zero keeps the size
   *  the author cooked at. */
  int resolution = 0;
  /** The outputs to cook. Empty cooks what the material reads: base
   *  colour, normal, roughness, metallic, ambient occlusion, emission and
   *  opacity, each where the graph has it. */
  std::vector<sbsar::OutputRequest> outputs;
  /** The graph by label or package url; empty for the first. */
  std::string graph;
  bool operator==(const SubstanceOptions&) const = default;
};

/** THE GRAPH AT @p uri, COOKED, AS A MATERIAL. The base is the base-colour
 *  output (masked by the opacity output where there is one); `surface()`
 *  carries the normal, roughness, metallic, ambient-occlusion and
 *  emissive outputs. `set(name, value)` and `bind(name, animatable)` on
 *  the answer write the graph's inputs by identifier and schedule a cook
 *  that runs apart from the caller, the newest value replacing a cook
 *  still waiting; `isRunning()` is true until it lands. The first cook
 *  happens here, before the answer returns.
 *  @trap Copies of the answer share ONE cooked graph: an input written
 *  through any copy re-cooks the pictures every copy shows. Call again
 *  for an independent one.
 *  An empty material, reported once, when the archive cannot be read or
 *  the SDK is not available. */
Material substance(io::Hub& hub, std::string_view uri,
                   SubstanceOptions options = {});

/** The same with a GENERATED INPUT STRUCT — the header the build writes
 *  beside a `.sbsar` (`AutumnLeaves{.hueShift = 0.1f}`): its set fields
 *  are written before @p options' own `inputs`. */
template <class Generated>
  requires requires(const Generated& inputs) {
    { inputs.inputs() } -> std::convertible_to<std::vector<sbsar::InputValue>>;
  }
Material substance(io::Hub& hub, std::string_view uri, const Generated& inputs,
                   SubstanceOptions options = {}) {
  std::vector<sbsar::InputValue> stated = inputs.inputs();
  options.inputs.insert(options.inputs.begin(), stated.begin(), stated.end());
  return substance(hub, uri, std::move(options));
}

}  // namespace sigil::material
