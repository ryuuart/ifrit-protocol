#include <sigilsketch/live/Host.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) return 2;
  sigil::weave::FontContext fonts(sigil::weave::ports::systemFontManager());
  sigil::sketch::Host::Options options;
  options.pluginPath = argv[1];
  options.compiler = "/no/compiler/is/needed";
  options.flagsFile = "/no/flags/are/needed";
  options.clock = sigil::motion::ClockPolicy::Advance;
  sigil::sketch::Host host(options, fonts);
  const bool reject = argc == 3 && std::string_view(argv[2]) == "--reject";
  if (reject) {
    if (host.live() || host.errorLog().find("mismatch") == std::string::npos)
      return 1;
    return 0;
  }
  if (!host.live() || host.compiling() ||
      host.canvasSize() != SkSize::Make(37, 19)) {
    std::cerr << host.errorLog() << '\n';
    return 1;
  }
  const auto picture = host.photograph();
  return picture.width() == 37 && picture.height() == 19 ? 0 : 1;
}
