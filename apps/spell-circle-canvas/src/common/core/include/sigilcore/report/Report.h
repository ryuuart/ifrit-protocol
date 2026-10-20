#pragma once

/** @file
 * @ingroup core-report
 *
 * THE ONE REPORTING DOOR: a library's warning, said once in one format,
 * and the reporter a host installs to hear every library's warnings.
 */

/** @defgroup core-report The reporting door
 *  A library that has something to say and no caller to say it to — a
 *  device it could not adopt, a table whose length disagrees with its
 *  units, a sketch that failed to compile — says it through report(),
 *  naming itself. By default the line goes to the standard error as
 *  `[library] message`; a host that keeps its own log, a test that reads
 *  what was said, or a window that shows it installs a reporter and hears
 *  every library through it. */

#include <functional>
#include <string>
#include <string_view>

namespace sigil::core {

/** What hears a library's warning: the library's name, as it names itself
 *  (`SigilSketch`, `SigilGeometry`), and the message, which may run over
 *  several lines and carries no trailing newline. It may be called from
 *  any thread, and from several at once. */
using Reporter =
    std::function<void(std::string_view library, std::string_view message)>;

/** Says @p message on behalf of @p library to the installed reporter, or
 *  to the standard error as reportLine() spells it when none is installed.
 *  A reporter that itself reports is called again, so it must not report
 *  what it hears. */
void report(std::string_view library, std::string_view message);

/** Installs @p reporter as the one every report() reaches, and answers the
 *  one it replaced so a scope can put it back. An empty reporter restores
 *  the standard error. */
Reporter installReporter(Reporter reporter);

/** The one format a warning is written in: `[library] message`, with no
 *  trailing newline. */
[[nodiscard]] std::string reportLine(std::string_view library,
                                     std::string_view message);

}  // namespace sigil::core
