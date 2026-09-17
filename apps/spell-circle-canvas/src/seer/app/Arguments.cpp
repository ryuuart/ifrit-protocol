#include "Arguments.h"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace seer {

std::optional<Arguments> parseArguments(int argc, char* argv[]) {
  Arguments args;
  for (int i = 1; i < argc; ++i) {
    const std::string_view flag = argv[i];
    if (flag == "--help")
      args.help = true;
    else if (flag == "--textures")
      args.textures = true;
    else if (flag == "--list-textures")
      args.listTextures = true;
    else if (!flag.empty() && flag.front() != '-')
      args.wires.emplace_back(flag);
    else {
      if (i + 1 == argc || (flag != "--say" && argv[i + 1][0] == '-')) {
        std::fprintf(stderr, "%s needs a value\n", argv[i]);
        return std::nullopt;
      }
      const std::string value = argv[++i];
      if (flag == "--receiver")
        args.receiver = value;
      else if (flag == "--schema")
        args.schema = value;
      else if (flag == "--peer")
        args.peer = value;
      else if (flag == "--say")
        args.message = value;
      else if (flag == "--shot")
        args.shot = value;
      else if (flag == "--texture") {
        args.texture = value;
        args.textures = true;
      } else if (flag == "--app")
        args.application = value;
      else if (flag == "--grab")
        args.grabPath = value;
      else if (flag == "--frames") {
        int count = 0;
        const auto [end, error] =
            std::from_chars(value.data(), value.data() + value.size(), count);
        if (error != std::errc{} || end != value.data() + value.size() ||
            count < 1) {
          std::fprintf(stderr, "--frames needs a whole number above zero\n");
          return std::nullopt;
        }
        args.frames = count;
      } else if (flag == "--timeout") {
        char* end = nullptr;
        const double seconds = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end || !std::isfinite(seconds) ||
            seconds <= 0) {
          std::fprintf(stderr, "--timeout needs finite seconds above zero\n");
          return std::nullopt;
        }
        args.timeoutSeconds = seconds;
      } else {
        std::fprintf(stderr, "unknown option: %.*s\n",
                     static_cast<int>(flag.size()), flag.data());
        return std::nullopt;
      }
    }
  }
  if (args.help) return args;
  const auto refuse = [](const char* reason) -> std::optional<Arguments> {
    std::fprintf(stderr, "%s\n", reason);
    return std::nullopt;
  };
  if (args.message && args.peer.empty())
    return refuse("--say requires --peer <uri>");
  if ((!args.grabPath.empty() || !args.application.empty()) &&
      args.texture.empty())
    return refuse("--grab and --app require --texture <publication>");
  if ((args.frames || args.timeoutSeconds) && args.grabPath.empty())
    return refuse("--frames and --timeout require --grab <png>");
  const bool windowOptions = !args.wires.empty() || !args.receiver.empty() ||
                             !args.schema.empty() || !args.peer.empty() ||
                             args.message || !args.shot.empty();
  if (args.listTextures &&
      (windowOptions || args.textures || !args.texture.empty() ||
       !args.grabPath.empty() || !args.application.empty() || args.frames ||
       args.timeoutSeconds))
    return refuse("--list-textures runs alone");
  if (!args.grabPath.empty() && windowOptions)
    return refuse("--grab cannot be combined with window or wire options");
  return args;
}

}  // namespace seer
