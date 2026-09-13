/** @file
 * Reading the command line into the value every lane is asked through.
 */

#include "Arguments.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace receiver {

namespace {

/** A flag's value read as a number, or nothing when the token is not one
 *  whole number and nothing else. Nothing is refused by name rather than
 *  read as a zero, so a mistyped figure does not become a silent one. */
std::optional<double> number(const char* token) {
  char* end = nullptr;
  const double read = std::strtod(token, &end);
  if (end == token || *end != '\0') return std::nullopt;
  return read;
}

}  // namespace

std::optional<Arguments> parseArguments(int argc, char* argv[]) {
  Arguments args;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--list") {
      args.list = true;
    } else if (arg == "--app" && i + 1 < argc) {
      args.app = argv[++i];
    } else if (arg == "--grab" && i + 1 < argc) {
      args.grabPath = argv[++i];
    } else if (arg == "--frames" && i + 1 < argc) {
      const std::optional<double> read = number(argv[++i]);
      if (!read || *read < 1) {
        std::fprintf(stderr, "--frames wants a count of one or more\n");
        return std::nullopt;
      }
      args.frames = (int)*read;
    } else if (arg == "--timeout" && i + 1 < argc) {
      const std::optional<double> read = number(argv[++i]);
      if (!read || !(*read > 0)) {
        std::fprintf(stderr, "--timeout wants seconds above zero\n");
        return std::nullopt;
      }
      args.timeoutSeconds = *read;
    } else if (args.server.empty() && !arg.empty() && arg[0] != '-') {
      // THE ONE POSITIONAL ARGUMENT IS THE PUBLICATION'S NAME. A second
      // one is not a second publication — this run subscribes to exactly
      // one — so it is a mistyped flag or a stray word, and saying so is
      // better than silently taking the first.
      args.server = arg;
    } else {
      std::fprintf(stderr, "unknown argument \"%s\"\n", arg.c_str());
      return std::nullopt;
    }
  }

  // WHAT THE LINE ASKS FOR HAS TO BE ONE THING. Every refusal below is a
  // line that would otherwise have to ignore something it was told.
  if (args.list) {
    if (!args.server.empty() || !args.grabPath.empty() || !args.app.empty() ||
        args.frames || args.timeoutSeconds) {
      std::fprintf(stderr,
                   "--list answers with every publication on this machine "
                   "and takes nothing else\n");
      return std::nullopt;
    }
    return args;
  }
  if (args.server.empty()) {
    std::fprintf(stderr,
                 "name the publication to subscribe to, or --list what is "
                 "being offered\n");
    return std::nullopt;
  }
  if (args.grabPath.empty() && (args.frames || args.timeoutSeconds)) {
    std::fprintf(stderr,
                 "--frames and --timeout are how a --grab waits; a window "
                 "draws whatever arrives for as long as it is open\n");
    return std::nullopt;
  }
  return args;
}

}  // namespace receiver
