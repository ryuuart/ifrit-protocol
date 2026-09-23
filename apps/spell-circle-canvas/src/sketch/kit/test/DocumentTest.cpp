/** @file
 * The document a sketch's words stand in: what it answers for a record, a
 * sentence, a run of lines and a passage, and what it answers when the
 * file is not there.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
namespace data = sigil::data;
using sigil::sketch::kit::test::sameDrawing;
using sigil::sketch::test::fonts;

/** A sketch's own directory holding one document under data/, mounted as
 *  the files of the sketch keyed `document`. */
struct Beside {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() / "sketch_kit_document";
  sigil::sketch::Assets store{""};
  sigil::motion::Ticker ticker;
  compose::Composer composer{ticker, fonts()};
  sigil::sketch::CanvasSpecification specification;
  sigil::sketch::SketchContext ctx{composer, ticker,         store,
                                   {0, 0},   &specification, &fonts(),
                                   false,    nullptr,        "document"};

  explicit Beside(std::string_view json) {
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "data");
    std::ofstream(root / "data" / "content.json", std::ios::binary) << json;
    store.mountSketch("document", root);
  }
  ~Beside() { std::filesystem::remove_all(root); }

  kit::Document read() { return kit::Document(ctx, "data/content.json"); }
};

/** Narrowed for the failure message: what is being asserted is words, and
 *  a test framework prints them as text. */
std::string said(const compose::Utf8& words) {
  return std::string(reinterpret_cast<const char*>(words.bytes().data()),
                     words.bytes().size());
}

constexpr const char* kContent = R"({
  "masthead": {"title": "THE RULE AND THE STRANDS"},
  "lead": "the rest length came to {rest} mm",
  "notes": ["a line in the running voice",
            {"words": "and one in {rest} mm", "class": "figure"}],
  "runs": [{"words": "plain "}, {"words": "emphatic", "class": "figure"}]
})";

TEST(SketchKitDocument, ARecordIsReadAndItsNodeIsText) {
  Beside beside(kContent);
  const kit::Document doc = beside.read();
  EXPECT_TRUE(doc.loaded());
  EXPECT_EQ(said(compose::Utf8(doc["masthead"]["title"])),
            "THE RULE AND THE STRANDS");
}

TEST(SketchKitDocument, ASentenceCarriesTheFigureTheRunMeasured) {
  Beside beside(kContent);
  kit::Document doc = beside.read();
  doc.figures({{"rest", "18.4"}});
  EXPECT_EQ(said(doc.phrase("lead")), "the rest length came to 18.4 mm");
}

TEST(SketchKitDocument, AFigureNoNameCarriesStandsAsItWasWritten) {
  Beside beside(kContent);
  const kit::Document doc = beside.read();
  EXPECT_EQ(said(doc.phrase("lead")), "the rest length came to {rest} mm");
}

TEST(SketchKitDocument, ARunIsALineEachAndABareStringNamesNoClass) {
  Beside beside(kContent);
  kit::Document doc = beside.read();
  doc.figures({{"rest", "18.4"}});
  const std::vector<kit::Document::Line> lines = doc.run("notes");
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(said(lines[0].words), "a line in the running voice");
  EXPECT_EQ(lines[0].styleClass, "");
  EXPECT_EQ(said(lines[1].words), "and one in 18.4 mm");
  EXPECT_EQ(lines[1].styleClass, "figure");
}

TEST(SketchKitDocument, ALineIsALeafInTheClassTheDocumentNamed) {
  Beside beside(kContent);
  kit::Document doc = beside.read();
  doc.figures({{"rest", "18.4"}});
  compose::StyleSheet dressed =
      kit::houseTheme().styleSheet() +
      compose::StyleSheet{
          compose::rule("figure, .figure")
              .font(sigil::weave::Type{
                  .color = sigil::material::Color{0, 1, 0, 1}})};
  const auto under = [&dressed](compose::Element leaf) {
    return compose::box().applyStyleSheet(dressed).children({std::move(leaf)});
  };
  EXPECT_TRUE(sameDrawing(
      under(kit::lineOf(doc.run("notes")[1])),
      under(compose::text("and one in 18.4 mm").styleClass("figure"))));
}

TEST(SketchKitDocument, APassageIsOneValueWithTheNamedRunsKept) {
  Beside beside(kContent);
  const sigil::weave::RichText woven = beside.read().passage("runs");
  ASSERT_EQ(woven.runs().size(), 2u);
  EXPECT_EQ(woven.runs()[0].styleName, "");
  EXPECT_EQ(woven.runs()[1].styleName, "figure");
}

TEST(SketchKitDocument, AMissingFileAnswersNoRecordNoWordsAndNoLines) {
  Beside beside(kContent);
  const kit::Document doc(nullptr);
  EXPECT_FALSE(doc.loaded());
  EXPECT_TRUE(doc["masthead"].null());
  EXPECT_TRUE(said(doc.phrase("lead")).empty());
  EXPECT_TRUE(doc.run("notes").empty());
  EXPECT_TRUE(doc.passage("runs").runs().empty());
}

TEST(SketchKitDocument, ARunNestedInARecordIsReadFromTheNode) {
  Beside beside(R"({"page": {"runs": ["one", {"words": "two",
                                              "class": "figure"}]}})");
  const kit::Document doc = beside.read();
  const std::vector<kit::Document::Line> lines = doc.run(doc["page"]["runs"]);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(said(lines[1].words), "two");
}

}  // namespace
