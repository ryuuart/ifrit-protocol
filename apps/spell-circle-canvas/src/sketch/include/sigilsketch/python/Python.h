#pragma once

#include <sigilsketch/core/Kind.h>

#include <filesystem>

namespace sigil::sketch::python {

/** Loads a Python sketch into a fresh package. Relative imports belong to
 * this generation; installed packages keep their ordinary Python identity.
 * The returned kind owns its code and constructs a fresh body per session.
 * Import failures throw a standard exception carrying the Python traceback. */
[[nodiscard]] Kind load(const std::filesystem::path& source);

}  // namespace sigil::sketch::python
