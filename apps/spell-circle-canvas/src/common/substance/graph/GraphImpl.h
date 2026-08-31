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

#include <map>
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
  std::map<std::string, sk_sp<SkImage>> byIdentifier;
  std::map<std::string, sk_sp<SkImage>> byUsage;
  std::vector<SubstanceAir::InputImage::SPtr> heldImages;  // keep inputs alive

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
inline std::string str(const SubstanceAir::string& s) {
  return {s.data(), s.size()};
}

}  // namespace sigil::substance
