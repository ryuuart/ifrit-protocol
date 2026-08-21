#pragma once

/** @file
 * SigilSubstance — Adobe Substance 3D archives (.sbsar) rendered to
 * images through the Substance Engine.
 *
 * A .sbsar is a procedural material: a graph with named, typed
 * PARAMETERS (sliders, colours, toggles, images) and named OUTPUTS, each
 * output tagged with the material channel it feeds ("baseColor",
 * "normal", "roughness", "metallic", "ambientOcclusion", "height",
 * "emissive"...). This library loads a package, exposes its graphs'
 * parameters as plain values, renders on the CPU engine, and hands each
 * output back as an SkImage keyed by identifier and by usage. Nothing
 * here knows about surfaces or the GPU: SigilWorld's texture-set door
 * takes the by-usage map and makes a Material of it.
 *
 * Namespace sigil::substance, target SigilSubstance. Requires the
 * Substance 3D SDK at configure time; without it the target does not
 * exist.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

/** One graph output, described. `usage` is the canonical channel name
 *  the graph declared for it ("baseColor", "normal", ...; "" when the
 *  output has no channel), which is what a material builder keys on. */
struct Output {
  std::string identifier;
  std::string label;
  std::string usage;
  bool image = true;  ///< false for numerical outputs
  bool srgb = false;  ///< the graph declares this output as encoded colour
};

class Package;

/** One graph of a package: parameters in, images out. Owned by its
 *  Package; a Graph reference is valid as long as the Package lives. */
class Graph {
 public:
  ~Graph();
  Graph(const Graph&) = delete;
  Graph& operator=(const Graph&) = delete;

  const std::string& label() const;
  const std::string& url() const;  ///< the package-internal graph url

  std::vector<Parameter> parameters() const;
  std::vector<Output> outputs() const;

  /** Set a numeric parameter by identifier. The value count must match
   *  the parameter's component count (a Float3 takes three). Returns
   *  false when no such parameter exists or the counts disagree. Ints
   *  are truncated from the floats. */
  bool set(std::string_view identifier, std::initializer_list<float> value);
  bool set(std::string_view identifier, const std::vector<float>& value);
  bool set(std::string_view identifier, float value) {
    return set(identifier, {value});
  }
  /** Set an image parameter. Any raster SkImage; flattened to 8-bit
   *  RGBA on the way in. */
  bool setImage(std::string_view identifier, const sk_sp<SkImage>& image);
  /** Set a text parameter. */
  bool setText(std::string_view identifier, std::string_view text);
  /** Every output's resolution, as log2 — {10, 10} is 1024 x 1024. The
   *  graph's own "$outputsize" input, named for what it does. */
  bool setResolution(int log2Width, int log2Height);
  /** Back to the authored defaults, every parameter. */
  void reset();

  /** Which way the graph's normal output points its green channel: the
   *  standard `$normalformat` input, 0 DirectX (green down the image —
   *  the engine's default) and 1 OpenGL. True when the input is absent
   *  too, since that is what the engine then produces. Read it after
   *  setting the input; hand it to the material builder as its
   *  `normalDirectX`. */
  bool normalsAreDirectX() const;

  /** Cook. Synchronous; every enabled image output is (re)rendered and
   *  can be read with output(). Returns false when the engine reports a
   *  failure. */
  bool render();

  /** An output image by identifier or by usage (identifier first);
   *  null before the first render, for numerical outputs, and for names
   *  the graph does not have. Images are 8-bit RGBA or 8-bit grey,
   *  top-left origin. */
  sk_sp<SkImage> output(std::string_view identifierOrUsage) const;
  /** Every rendered image output keyed by its usage — the map
   *  SigilWorld's texture-set door takes. Outputs with no usage are
   *  keyed by identifier. */
  std::map<std::string, sk_sp<SkImage>> outputsByUsage() const;

 private:
  friend class Package;
  Graph();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/** A loaded .sbsar. */
class Package {
 public:
  /** Load from bytes. nullptr (and @p error) on a malformed archive or
   *  when the engine cannot start. */
  static std::unique_ptr<Package> load(const void* bytes, size_t size,
                                       std::string* error = nullptr);
  static std::unique_ptr<Package> load(const std::filesystem::path& file,
                                       std::string* error = nullptr);
  ~Package();
  Package(const Package&) = delete;
  Package& operator=(const Package&) = delete;

  size_t graphCount() const;
  Graph& graph(size_t index);
  const Graph& graph(size_t index) const;
  /** By label or url; null when absent. */
  Graph* find(std::string_view labelOrUrl);

  /** The engine's own version string, for diagnostics. */
  static std::string engineVersion();

 private:
  Package();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::substance
