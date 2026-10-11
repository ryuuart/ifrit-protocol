/** @file
 * A font file's bytes as a face, read through the platform font manager.
 */

#include <include/core/SkData.h>
#include <include/core/SkFontMgr.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>

#include "sigilweave/advanced/Skia.h"

namespace sigil::weave::ports {

Face faceFromBytes(std::span<const std::byte> bytes) {
  if (bytes.empty()) return {};
  // A copy, because the face reads its tables lazily for as long as it
  // stands, and the bytes a caller hands in are theirs to release.
  sk_sp<SkData> data = SkData::MakeWithCopy(bytes.data(), bytes.size());
  return fromSk(systemFontManager()->makeFromData(std::move(data)));
}

}  // namespace sigil::weave::ports
