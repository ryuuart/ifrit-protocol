#pragma once

/** @file
 * A graph input, described as plain values: its identifier and label,
 * the kind and widget the graph authored, and its current, default and
 * range values as floats regardless of kind.
 */

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sigil::substance {

/** One graph input, described. `values` carries the current value,
 *  `defaults`/`minimum`/`maximum` the authored ones, all as floats
 *  regardless of kind (integers exactly representable; a Bool is an Int
 *  with a toggle widget). `choices` lists a combobox's labels in value
 *  order; `image` and `text` inputs carry no numbers. */
struct Parameter {
  enum class Kind : uint8_t {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    Image,
    Text,
    Other,
  };
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
  std::string identifier;  ///< the graph's own name for it ("$outputsize",
                           ///< "roughness_amount")
  std::string label;       ///< the artist-facing label
  std::string group;       ///< the GUI group it sits in ("" = top level)
  Kind kind = Kind::Other;
  Widget widget = Widget::None;
  std::vector<float> values;
  std::vector<float> defaults;
  std::vector<float> minimum;
  std::vector<float> maximum;
  std::vector<std::pair<int, std::string>> choices;
  int components() const { return (int)defaults.size(); }
};

}  // namespace sigil::substance
