#include <sigilcore/report/Report.h>
#include <sigilsketch/kit/Passage.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <optional>
#include <string>

namespace sigil::sketch::kit {

std::u8string passage(Scene2DContext& ctx, std::string_view name) {
  const std::string uri = ctx.local(name);
  std::optional<std::string> text = ctx.assets.hub().text(uri);
  if (!text) {
    core::report("SigilSketch", "no passage at " + uri);
    return {};
  }
  while (!text->empty() && text->back() == '\n') text->pop_back();
  return std::u8string(reinterpret_cast<const char8_t*>(text->data()),
                       text->size());
}

}  // namespace sigil::sketch::kit
