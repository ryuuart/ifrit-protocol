#pragma once

/** @file
 * @ingroup io-hub
 * WHERE A HUB LOOKS: a mount made after the hub, the file a URI resolves
 * to, the files a selector names, and the re-check that reloads what
 * changed on disk. `HubOptions::mounts` is the ordinary way to mount.
 */

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::io {

class Hub;

/** Maps every URI starting with @p prefix to files under @p directory
 *  ("res://" + "ui/logo.png" → directory/ui/logo.png). Longest matching
 *  prefix wins; re-mounting a prefix replaces it. */
void mount(Hub& hub, std::string prefix, std::filesystem::path directory);

/** The mounted filesystem path @p uri resolves to (empty when no mount
 *  matches — the URI is then tried as a plain path). This is what makes
 *  a Hub a ResolvingByteSource. */
std::filesystem::path resolve(const Hub& hub, std::string_view uri);

/** The regular-file URIs named by @p selector, in lexical order: an
 *  exact file, a directory URI read recursively, or a glob in which
 *  `*` matches within one path segment, `?` one non-separator
 *  character and `**` across `/`, a backslash quoting what follows it.
 *  @trap Only LOCAL resources are enumerated: a network selector
 *  without a star is one exact URL and selects itself without a read,
 *  and a network glob cannot be enumerated at all. */
std::vector<std::string> select(const Hub& hub, std::string_view selector);

/** Re-checks every previously loaded resource; reloads changes and
 *  drops entries whose files vanished. True if anything changed. */
bool poll(Hub& hub);

}  // namespace sigil::io
