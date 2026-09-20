#pragma once

/** @file
 * @ingroup sketch-python
 *
 * A Python source file as a sketch kind: what a host may ask about an
 * interpreter and a source before Python starts, and how a source
 * becomes a kind a session can be opened on.
 */

#include <sigilsketch/core/Kind.h>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

/** A PYTHON FILE AS A SKETCH, on the same footing as a compiled one: it
 *  registers a kind, a session opens on it, and the host drives it
 *  through the calls every kind answers.
 *
 *  The doors here are ordered by how much they start. `interpreterAbi`,
 *  `interpreterVersion` and `source` cost nothing and touch no
 *  interpreter, so a browser may list a Python sketch that is missing or
 *  will not import. `available` probes for the modules a source
 *  declares, and `load` imports it into a fresh generation — which is
 *  what makes an edit reloadable, since relative imports belong to that
 *  generation and installed packages keep their ordinary identity. */
namespace sigil::sketch::python {

/** The embedded interpreter's extension ABI, without initializing Python. */
[[nodiscard]] std::string_view interpreterAbi();

/** The embedded interpreter's major and minor versions, without startup. */
[[nodiscard]] std::pair<int, int> interpreterVersion();

/** Selects the executable whose Python environment an embedded host uses.
 * Relative paths become absolute without resolving virtual-environment
 * symlinks. Python discovers that executable's environment when it starts;
 * an empty path leaves the default discovery unchanged. Must be called
 * before Python initializes, including module availability probes. */
void configureInterpreter(const std::filesystem::path& executable);

/** Describes a Python source file without reading it or initializing Python.
 * The canvas kind can be listed even when the file is missing or invalid.
 * Each opened session imports the current source and its local modules into
 * a fresh generation. Relative paths are anchored to the current directory
 * when this value is constructed; import failures surface when it opens. */
[[nodiscard]] Kind source(const std::filesystem::path& source);

/** Checks that a source exists and its declared external modules can be
 * discovered, without importing the sketch. An empty module list never
 * initializes Python; otherwise discovery uses the host's interpreter.
 * Missing sources, modules and discovery failures return false and write
 * their reason to @p why when supplied. */
[[nodiscard]] bool available(const std::filesystem::path& source,
                             std::initializer_list<const char*> modules,
                             std::string* why = nullptr);

/** The same check, and the fetched art in @p cachedUrls required to be
 * in the IO hub's network cache on this machine as well — the probe a
 * Python sketch over fetched art is registered with, answered without
 * importing it. The cache is asked before the interpreter is, because
 * reading a directory costs nothing and starting Python does. */
[[nodiscard]] bool available(const std::filesystem::path& source,
                             std::initializer_list<const char*> modules,
                             std::initializer_list<std::string_view> cachedUrls,
                             std::string* why = nullptr);

/** Loads a Python sketch into a fresh package. Relative imports belong to
 * this generation; installed packages keep their ordinary Python identity.
 * The returned kind owns its code and constructs a fresh body per session.
 * Import failures throw a standard exception carrying the Python traceback. */
[[nodiscard]] Kind load(const std::filesystem::path& source);

}  // namespace sigil::sketch::python
