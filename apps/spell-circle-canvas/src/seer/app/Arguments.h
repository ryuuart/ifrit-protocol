#pragma once

#include <cstdint>
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
  /** Where this run keeps its state: the protocol's address file under
   *  it. Empty for the platform's own location for Seer. */
  std::string state;
  /** The port `--inspect=PORT` named for the protocol's endpoint, 0 for
   *  any free one; the window mounts one whether or not it was asked. */
  std::optional<uint16_t> inspectPort;
};

std::optional<Arguments> parseArguments(int argc, char* argv[]);

}  // namespace seer
