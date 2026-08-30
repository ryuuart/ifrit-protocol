#pragma once

/** @file
 * A Graph: one graph of a loaded package — parameters in, images out.
 * Describe it with parameters() and outputs(), change it with the
 * setters, cook it with render(), read it with output() and
 * outputsByUsage().
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilsubstance/graph/Output.h>
#include <sigilsubstance/graph/Parameter.h>

#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::substance {

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

  struct Impl;

 private:
  friend class Package;
  Graph();
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::substance
