// substance_demo — describe a .sbsar and render its outputs to PNGs.
//
//   substance_demo <file.sbsar> [outdir] [log2size] [name=value ...]
//
// Prints every graph's parameters (identifier, kind, range, default) and
// outputs (identifier, usage), sets any name=value pairs given (a value
// is one number or a comma list, "$outputsize=8,8"), renders, and writes
// <outdir>/<graph>_<usage-or-identifier>.png per image output.

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkStream.h>
#include <include/encode/SkPngEncoder.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "sigilsubstance/Substance.h"

using namespace sigil;

namespace {

const char* kindName(substance::Parameter::Kind k) {
  using K = substance::Parameter::Kind;
  switch (k) {
    case K::Float:
      return "float";
    case K::Float2:
      return "float2";
    case K::Float3:
      return "float3";
    case K::Float4:
      return "float4";
    case K::Int:
      return "int";
    case K::Int2:
      return "int2";
    case K::Int3:
      return "int3";
    case K::Int4:
      return "int4";
    case K::Image:
      return "image";
    case K::Text:
      return "text";
    default:
      return "other";
  }
}

std::string list(const std::vector<float>& v) {
  std::ostringstream out;
  for (size_t i = 0; i < v.size(); ++i) out << (i ? "," : "") << v[i];
  return out.str();
}

bool writePng(const sk_sp<SkImage>& image, const std::filesystem::path& path) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(image->width(), image->height()));
  if (!image->readPixels(nullptr, bm.pixmap(), 0, 0)) return false;
  SkFILEWStream stream(path.string().c_str());
  return SkPngEncoder::Encode(&stream, bm.pixmap(), {});
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || (argv[1][0] == '-')) {
    const bool help = argc >= 2 && (std::strcmp(argv[1], "-h") == 0 ||
                                    std::strcmp(argv[1], "--help") == 0);
    std::fprintf(help ? stdout : stderr,
                 "usage: substance_demo <file.sbsar> [outdir] [log2size] "
                 "[name=value ...]\n");
    return help ? 0 : 1;
  }
  std::string error;
  std::unique_ptr<substance::Package> package =
      substance::Package::load(std::filesystem::path(argv[1]), &error);
  if (!package) {
    std::fprintf(stderr, "substance_demo: %s\n", error.c_str());
    return 2;
  }
  const std::filesystem::path outDir =
      argc > 2 ? argv[2] : "substance_demo_out";
  const int log2 = argc > 3 ? std::atoi(argv[3]) : 9;
  std::filesystem::create_directories(outDir);
  std::printf("engine: %s\n", substance::Package::engineVersion().c_str());

  for (size_t g = 0; g < package->graphCount(); ++g) {
    substance::Graph& graph = package->graph(g);
    std::printf("graph %zu: %s  (%s)\n", g, graph.label().c_str(),
                graph.url().c_str());
    for (const substance::Parameter& p : graph.parameters()) {
      std::printf("  param %-28s %-7s", p.identifier.c_str(), kindName(p.kind));
      if (!p.defaults.empty())
        std::printf(" default=%s range=[%s..%s]", list(p.defaults).c_str(),
                    list(p.minimum).c_str(), list(p.maximum).c_str());
      if (!p.label.empty()) std::printf("  \"%s\"", p.label.c_str());
      if (!p.group.empty()) std::printf("  {%s}", p.group.c_str());
      std::printf("\n");
    }
    for (const substance::Output& o : graph.outputs())
      std::printf("  output %-27s usage=%s%s\n", o.identifier.c_str(),
                  o.usage.empty() ? "-" : o.usage.c_str(),
                  o.image ? "" : " (numeric)");
    graph.setResolution(log2, log2);
    for (int i = 4; i < argc; ++i) {
      const char* eq = std::strchr(argv[i], '=');
      if (!eq) continue;
      const std::string name(argv[i], (size_t)(eq - argv[i]));
      std::vector<float> values;
      std::stringstream ss(eq + 1);
      std::string tok;
      while (std::getline(ss, tok, ',')) values.push_back(std::stof(tok));
      if (!graph.set(name, values))
        std::fprintf(stderr, "  could not set %s\n", name.c_str());
    }
    if (!graph.render()) {
      std::fprintf(stderr, "  render failed\n");
      continue;
    }
    for (const auto& [usage, image] : graph.outputsByUsage()) {
      std::string stem =
          graph.label().empty() ? "graph" + std::to_string(g) : graph.label();
      for (char& c : stem)
        if (c == ' ' || c == '/') c = '_';
      const std::filesystem::path path = outDir / (stem + "_" + usage + ".png");
      std::printf("  wrote %s (%dx%d)\n", path.string().c_str(), image->width(),
                  image->height());
      writePng(image, path);
    }
  }
  return 0;
}
