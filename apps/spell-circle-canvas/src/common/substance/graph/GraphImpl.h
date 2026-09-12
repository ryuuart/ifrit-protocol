#pragma once

/** @file
 * Graph::Impl — the framework's graph instance and renderer the Graph
 * drives (both owned by its Package), the rendered outputs by identifier
 * and by usage, and the input images held alive. Internal: it names
 * SDK types, and the SDK is private to the library.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <substance/framework/framework.h>

#include <boost/container/map.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "sigilsubstance/graph/Graph.h"

namespace sigil::substance {

struct Graph::Impl {
  SubstanceAir::GraphInstance* instance = nullptr;  // owned by the package
  SubstanceAir::Renderer* renderer = nullptr;       // owned by the package
  std::string label;
  std::string url;
  boost::container::map<std::string, sk_sp<SkImage>> byIdentifier;
  boost::container::map<std::string, sk_sp<SkImage>> byUsage;
  /** The image behind each image input, held while the input names it:
   *  the framework takes a reference to the buffer rather than a copy,
   *  so letting go of one while it is set would hand the engine freed
   *  pixels. One per input identifier, replaced when that input is set
   *  again, so a graph fed a new image every cook holds one image per
   *  input and not one per cook. */
  boost::container::map<std::string, SubstanceAir::InputImage::SPtr> heldImages;

  /** The input instance named @p identifier, or null. */
  SubstanceAir::InputInstanceBase* input(std::string_view identifier) const {
    for (SubstanceAir::InputInstanceBase* in : instance->getInputs())
      if (std::string_view(in->mDesc.mIdentifier.data(),
                           in->mDesc.mIdentifier.size()) == identifier)
        return in;
    return nullptr;
  }
};

/** The framework's strings use their own allocator; copy out. */
inline std::string toString(const SubstanceAir::string& s) {
  return {s.data(), s.size()};
}

}  // namespace sigil::substance
