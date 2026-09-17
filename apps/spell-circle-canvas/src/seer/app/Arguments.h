#pragma once

#include <optional>
#include <string>
#include <vector>

namespace seer {

struct Arguments {
  std::vector<std::string> wires;
  std::string receiver;
  std::string schema;
  std::string peer;
  std::optional<std::string> message;
  std::string shot;
  std::string texture;
  std::string application;
  std::string grabPath;
  std::optional<int> frames;
  std::optional<double> timeoutSeconds;
  bool textures = false;
  bool listTextures = false;
  bool help = false;
};

std::optional<Arguments> parseArguments(int argc, char* argv[]);

}  // namespace seer
