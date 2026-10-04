/** @file
 * Adoption of complete, compatible artifacts published by an external build.
 */

#include <sigilsketch/live/Host.h>

#include <chrono>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <system_error>

#include "BuildCache.h"
#include "SigilSketchBuildIdentity.h"

namespace sigil::sketch {

std::optional<Host::PluginStamp> Host::pluginStamp() const {
  std::error_code error;
  const auto modified =
      std::filesystem::last_write_time(m_options.pluginPath, error);
  if (error) return std::nullopt;
  const uintmax_t bytes =
      std::filesystem::file_size(m_options.pluginPath, error);
  if (error) return std::nullopt;
  const auto manifest = m_options.pluginPath.string() + ".sigil-build";
  const auto manifestModified =
      std::filesystem::last_write_time(manifest, error);
  if (error) return std::nullopt;
  const uintmax_t manifestBytes = std::filesystem::file_size(manifest, error);
  if (error || manifestBytes > 65536) return std::nullopt;
  return PluginStamp{modified, bytes, manifestModified, manifestBytes};
}

void Host::loadPlugin() {
  const auto before = pluginStamp();
  if (!before) {
    m_pluginStamp.reset();
    m_unsettledPluginStamp.reset();
    m_errorLog = "plugin artifact or .sigil-build manifest is unavailable: " +
                 m_options.pluginPath.string();
    m_status = live() ? "plugin unavailable — keeping previous sketch"
                      : "plugin unavailable";
    return;
  }
  if (m_pluginStamp == before) return;
  std::ifstream manifest(m_options.pluginPath.string() + ".sigil-build");
  std::string format, identity, digest;
  std::getline(manifest, format);
  std::getline(manifest, digest);
  std::getline(manifest, identity);
  std::string compiledIdentity = identity;
  bool compatible = manifest && format == "sigil-sketch-plugin-2" &&
                    digest.size() == 32 && identity == hostBuildIdentity();
  std::set<std::string> origins;
  std::string boundary;
  while (std::getline(manifest, boundary)) {
    const auto space = boundary.find(' ');
    const std::string name = boundary.substr(0, space);
    const auto expected = nativeBoundaryIdentity(name);
    if (space == std::string::npos || !origins.insert(name).second ||
        expected.empty() || boundary.substr(space + 1) != expected) {
      compatible = false;
      break;
    }
    compiledIdentity += '\n' + boundary;
  }
  if (!compatible || !origins.contains("SigilSketch")) {
    m_pluginStamp = before;
    m_unsettledPluginStamp.reset();
    m_errorLog =
        "plugin build manifest mismatch — rebuild with this host's "
        "SigilSketchSDK";
    m_status = live() ? "load failed — keeping previous sketch" : "load failed";
    return;
  }
  const auto copy =
      m_buildDirectory / ("plugin_" + std::to_string(m_hostId) + "_" +
                          std::to_string(++m_pluginCopies) +
                          m_options.pluginPath.extension().string());
  std::error_code error;
  std::filesystem::copy_file(m_options.pluginPath, copy, error);
  if (error) {
    // Recording the stamp makes the retry wait for the artifact to change;
    // copying the same files again would fail the same way on every poll.
    std::error_code ignored;
    std::filesystem::remove(copy, ignored);
    m_pluginStamp = before;
    m_unsettledPluginStamp.reset();
    m_errorLog = "could not copy plugin artifact: " + error.message();
    m_status = live() ? "plugin copy failed — keeping previous sketch"
                      : "plugin copy failed";
    return;
  }
  // A publisher replacing an artifact while it is copied has not handed
  // over one complete generation. Leave the old stamp so the next poll
  // retries, and prefer publishing completed modules by atomic rename.
  if (pluginStamp() != before) {
    std::filesystem::remove(copy, error);
    return;
  }
  std::ifstream copied(copy, std::ios::binary);
  const std::string bytes(std::istreambuf_iterator<char>(copied), {});
  if (!copied || buildDigest(bytes) != digest) {
    std::filesystem::remove(copy, error);
    // The sidecar names this host's build, so a build that writes the
    // module before stamping the sidecar may still be publishing. Leave
    // the stamp unset so the next poll looks again, and report only a
    // disagreement that the same files keep across consecutive polls for
    // the publication grace.
    const auto now = std::chrono::steady_clock::now();
    if (m_unsettledPluginStamp != before) {
      m_unsettledPluginStamp = before;
      m_unsettledPluginSince = now;
      return;
    }
    if (now - m_unsettledPluginSince < m_options.pluginPublicationGrace) return;
    m_unsettledPluginStamp.reset();
    m_pluginStamp = before;
    m_errorLog = "plugin artifact does not match its build manifest";
    m_status = live() ? "load failed — keeping previous sketch" : "load failed";
    return;
  }
  m_unsettledPluginStamp.reset();
  m_pluginStamp = before;
  adopt(copy, compiledIdentity);
}

}  // namespace sigil::sketch
