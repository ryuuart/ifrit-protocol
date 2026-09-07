/** @file
 * The runtime probe: asks the plugin registry for each file format by
 * extension and creates one in-memory stage.
 */

#include "sigilusd/runtime/Runtime.h"

#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/usd/stage.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace sigil::usd {

bool available(std::string* why) {
  std::string missing;
  for (const char* extension : {"usdc", "usda", "usdz"}) {
    if (!SdfFileFormat::FindByExtension(extension)) {
      if (!missing.empty()) missing += ", ";
      missing += std::string(".") + extension;
    }
  }
  if (!missing.empty()) {
    if (why)
      *why = "USD file formats not registered (" + missing +
             "): the plugin registry beside the USD libraries is missing";
    return false;
  }
  if (!UsdStage::CreateInMemory()) {
    if (why) *why = "USD cannot create a stage";
    return false;
  }
  return true;
}

}  // namespace sigil::usd
