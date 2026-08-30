/** @file
 * The point cloud's lane storage: lanes created on first touch and
 * sized to the point count, looked up by name, and two clouds
 * appended with every missing lane padded by the convention its name
 * carries.
 */

#include <algorithm>

#include "sigilgeometry/points/Points.h"

namespace sigil::geometry {

namespace {

/** The pad values Cloud::append fills missing lanes with, BY NAME —
 *  the lane conventions, not one blanket default: a missing "size"
 *  means scale 1 (not invisible instances), a missing "Tex" the
 *  identity uv window, a missing "uv" zeros; other scalars pad 0,
 *  other colors white, vectors {0, 0, 1}. */
float scalarDefault(std::string_view name) {
  return name == "size" ? 1.0f : 0.0f;
}

glm::vec4 colorDefault(std::string_view name) {
  if (name == "Tex") return {0, 0, 1, 1};
  if (name == "uv") return {0, 0, 0, 0};
  return {1, 1, 1, 1};
}

}  // namespace

std::vector<float>& Cloud::scalar(const std::string& name, float fill) {
  std::vector<float>& lane = scalars[name];
  lane.resize(positions.size(), fill);
  return lane;
}

std::vector<glm::vec3>& Cloud::vector(const std::string& name, glm::vec3 fill) {
  std::vector<glm::vec3>& lane = vectors[name];
  lane.resize(positions.size(), fill);
  return lane;
}

std::vector<glm::vec4>& Cloud::color(const std::string& name, glm::vec4 fill) {
  std::vector<glm::vec4>& lane = colors[name];
  lane.resize(positions.size(), fill);
  return lane;
}

const std::vector<float>* Cloud::scalarIf(std::string_view name) const {
  auto it = scalars.find(name);
  return it == scalars.end() ? nullptr : &it->second;
}
const std::vector<glm::vec3>* Cloud::vectorIf(std::string_view name) const {
  auto it = vectors.find(name);
  return it == vectors.end() ? nullptr : &it->second;
}
const std::vector<glm::vec4>* Cloud::colorIf(std::string_view name) const {
  auto it = colors.find(name);
  return it == colors.end() ? nullptr : &it->second;
}

void Cloud::append(const Cloud& other) {
  const size_t oldSize = positions.size();
  const size_t newSize = oldSize + other.positions.size();
  positions.insert(positions.end(), other.positions.begin(),
                   other.positions.end());
  // Union of lanes on both sides, a missing side padded with the lane
  // NAME's conventional default (scalarDefault/colorDefault above) —
  // the same convention on the pad-ours and pad-theirs paths.
  for (auto& [name, lane] : scalars) lane.resize(newSize, scalarDefault(name));
  for (const auto& [name, lane] : other.scalars) {
    std::vector<float>& mine = scalars[name];
    mine.resize(oldSize, scalarDefault(name));
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, scalarDefault(name));
  }
  for (auto& [name, lane] : vectors) lane.resize(newSize, glm::vec3{0, 0, 1});
  for (const auto& [name, lane] : other.vectors) {
    std::vector<glm::vec3>& mine = vectors[name];
    mine.resize(oldSize, glm::vec3{0, 0, 1});
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, glm::vec3{0, 0, 1});
  }
  for (auto& [name, lane] : colors) lane.resize(newSize, colorDefault(name));
  for (const auto& [name, lane] : other.colors) {
    std::vector<glm::vec4>& mine = colors[name];
    mine.resize(oldSize, colorDefault(name));
    mine.insert(mine.end(), lane.begin(), lane.end());
    mine.resize(newSize, colorDefault(name));
  }
}
}  // namespace sigil::geometry
