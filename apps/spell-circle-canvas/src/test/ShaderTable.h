#pragma once

/** @file
 * The question every embedded shader table is asked: does it hold the
 * whole of the directory the library keeps its shaders in, and does it
 * hold each one's bytes. Test support belonging to no library — every
 * library that embeds shaders asks the same one, and a file added to a
 * shader directory that no build picked up must fail somewhere.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace sigil::test {

/** Fails when @p table and @p directory disagree by a name, or when an
 *  entry carries no text. @p table is any range of entries with `name`
 *  and `text` — the shape every generated table has. */
template <class Table>
void expectShaderTableIsWholeDirectory(const Table& table,
                                       const std::filesystem::path& directory) {
  std::vector<std::string> embedded;
  for (const auto& source : table) {
    embedded.emplace_back(source.name);
    EXPECT_FALSE(source.text.empty()) << source.name << " embedded empty";
  }
  std::vector<std::string> authored;
  for (const std::filesystem::directory_entry& entry :
       std::filesystem::recursive_directory_iterator(directory)) {
    if (!entry.is_regular_file()) continue;
    const std::string extension = entry.path().extension().string();
    if (extension != ".sksl" && extension != ".slang") continue;
    authored.push_back(
        std::filesystem::relative(entry.path(), directory).generic_string());
  }
  std::sort(embedded.begin(), embedded.end());
  std::sort(authored.begin(), authored.end());
  EXPECT_FALSE(authored.empty()) << directory << " holds no shader";
  EXPECT_EQ(embedded, authored);
}

}  // namespace sigil::test
