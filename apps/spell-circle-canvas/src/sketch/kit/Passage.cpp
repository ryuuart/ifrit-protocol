#include <sigilsketch/kit/Passage.h>

#include <cstdio>
#include <optional>
#include <string>

namespace sigil::sketch::kit {

std::u8string passage(SketchContext& ctx, std::string_view name) {
  const std::string uri = "res://passages/" + std::string(name);
  std::optional<std::string> text = ctx.assets.hub().text(uri);
  if (!text) {
    std::fprintf(stderr, "[sketch] no passage at %s\n", uri.c_str());
    return {};
  }
  while (!text->empty() && text->back() == '\n') text->pop_back();
  return std::u8string(reinterpret_cast<const char8_t*>(text->data()),
                       text->size());
}

}  // namespace sigil::sketch::kit
