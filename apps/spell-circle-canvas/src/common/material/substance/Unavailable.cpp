/** @file
 * The Substance feature in a build that found no SDK: nothing is
 * available, no archive decodes, and a cook of nothing answers empty at
 * every member, so `material::substance()` answers an empty material and
 * a consumer's path through the feature is the same on every machine.
 */

#include <sigilmaterial/substance/advanced/Archive.h>
#include <sigilmaterial/substance/advanced/Cook.h>

namespace sigil::material::sbsar {

bool available() { return false; }
Engine engine() { return Engine::None; }
bool available(Engine) { return false; }
uint64_t deviceReadbacks() { return 0; }
std::string engineVersion() { return {}; }

struct Archive::Decoded {};

std::optional<Archive> Archive::decode(std::span<const std::byte>) {
  return std::nullopt;
}
size_t Archive::graphCount() const { return 0; }
const Description& Archive::graph(size_t) const {
  static const Description none;
  return none;
}
const std::string& Archive::url(size_t) const {
  static const std::string none;
  return none;
}
std::optional<size_t> Archive::find(std::string_view) const {
  return std::nullopt;
}

void registerDecoder(io::Hub&) {}
std::shared_ptr<const Archive> load(io::Hub&, std::string_view) {
  return nullptr;
}

struct CookScheduler::State {};

CookScheduler::CookScheduler(std::shared_ptr<const Archive>, size_t,
                             const CookOptions&) {}
const Description& CookScheduler::description() const {
  static const Description none;
  return none;
}
Engine CookScheduler::engine() const { return Engine::None; }
bool CookScheduler::set(std::string_view, std::span<const float>) {
  return false;
}
bool CookScheduler::setText(std::string_view, std::string_view) {
  return false;
}
bool CookScheduler::setImage(std::string_view, const media::PixelSource&) {
  return false;
}
std::vector<float> CookScheduler::get(std::string_view) const { return {}; }
bool CookScheduler::isHeavyDuty(std::string_view) const { return false; }
bool CookScheduler::setHeavyDuty(std::string_view, bool) { return false; }
void CookScheduler::reset() {}
bool CookScheduler::normalsAreDirectX() const { return true; }
void CookScheduler::bind(std::string_view, motion::Animatable<float>) {}
void CookScheduler::unbind(std::string_view) {}
void CookScheduler::follow() const {}
size_t CookScheduler::addPresets(std::string_view) { return 0; }
std::vector<std::string> CookScheduler::presets() const { return {}; }
bool CookScheduler::applyPreset(std::string_view, PresetMode) { return false; }
void CookScheduler::cook() const {}
bool CookScheduler::cookNow() const { return false; }
void CookScheduler::wait() const {}
bool CookScheduler::isPending() const { return false; }
bool CookScheduler::isRunning() const { return false; }
uint64_t CookScheduler::revision() const { return 0; }
media::PixelSource CookScheduler::output(std::string_view) const { return {}; }
std::vector<std::string> CookScheduler::cooked() const { return {}; }

}  // namespace sigil::material::sbsar
