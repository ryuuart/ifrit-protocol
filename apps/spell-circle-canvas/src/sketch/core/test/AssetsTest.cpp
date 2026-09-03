/** @file
 * The probe a sketch over fetched art answers its availability with:
 * what stands in the loader's cache, and what does not.
 */

#include <gtest/gtest.h>
#include <sigilloader/hub/Network.h>
#include <sigilsketch/core/Assets.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "ScratchDir.h"

namespace {

using namespace sigil::sketch;

TEST(RequireCached, AnswersFromTheCacheAndNamesTheFirstMissingUrl) {
  sigil::test::ScratchDir cache("sketch_require_cached");
  const char* fetched = "https://sketch.invalid/art/panel.gif";
  const char* missing = "https://sketch.invalid/art/logo.svg";
  // A fetch that happened is a file under the URL's own cache key, and
  // nothing else about it is consulted.
  std::ofstream(cache.path / sigil::loader::networkCacheKey(fetched),
                std::ios::binary)
      << "gif";

  std::string why;
  EXPECT_TRUE(requireCached({fetched}, &why, cache.path));
  EXPECT_TRUE(why.empty());

  EXPECT_FALSE(requireCached({fetched, missing}, &why, cache.path));
  EXPECT_NE(why.find(missing), std::string::npos);
  EXPECT_EQ(why.find(fetched), std::string::npos);

  // Nothing asked for is nothing missing, and a reason nobody wants is
  // not written anywhere.
  EXPECT_TRUE(requireCached({}, nullptr, cache.path));
  EXPECT_FALSE(requireCached({missing}, nullptr, cache.path));

  // A FILE THAT IS THERE AND HOLDS NOTHING is not the art: a fetch that
  // was interrupted leaves one, and a sketch handed it draws its
  // stand-in exactly as if nothing were cached at all.
  std::ofstream(cache.path / sigil::loader::networkCacheKey(missing),
                std::ios::binary);
  EXPECT_FALSE(requireCached({missing}, &why, cache.path));
  EXPECT_NE(why.find(missing), std::string::npos);
}

TEST(RequireCached, AsksTheDirectoryTheLoaderPersistsFetchesTo) {
  // With no directory named, the probe asks where a hub given none of
  // its own puts them — the two cannot drift, because there is one
  // answer and both read it.
  const char* url = "https://sketch.invalid/art/nothing-has-fetched-this.png";
  std::error_code ec;
  std::filesystem::remove(
      sigil::loader::defaultNetworkCacheDir() / sigil::loader::networkCacheKey(url),
      ec);
  std::string why;
  EXPECT_FALSE(requireCached({url}, &why));
  EXPECT_NE(why.find(url), std::string::npos);
}

}  // namespace
