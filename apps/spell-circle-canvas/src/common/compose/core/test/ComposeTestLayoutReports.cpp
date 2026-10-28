// What a text leaf keeps on a column short of room, and what the layout
// says rather than keeps quiet about: a flex container whose children run
// past its box where no clip answers for the spill, and a text leaf a
// stated minimum lets a column shrink below its lines.

#include <sigilcore/report/Report.h>

#include <cmath>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

namespace {

/** Every warning SigilCompose says while it stands, heard in place of the
 *  standard error; the reporter it replaced is put back when it goes. */
class Heard {
 public:
  Heard()
      : m_outer(sigil::core::installReporter(
            [this](std::string_view library, std::string_view message) {
              if (library == "SigilCompose") messages.emplace_back(message);
            })) {}
  ~Heard() { sigil::core::installReporter(std::move(m_outer)); }
  Heard(const Heard&) = delete;
  Heard& operator=(const Heard&) = delete;

  std::vector<std::string> messages;

 private:
  sigil::core::Reporter m_outer;
};

bool says(const std::string& message, std::string_view words) {
  return message.find(words) != std::string::npos;
}

std::string everything(const std::vector<std::string>& messages) {
  std::string joined;
  for (const std::string& message : messages) joined += message + "\n";
  return joined;
}

SkRect boundsOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key));
}

/** A column 100 tall holding two children that will not shrink, 80 and
 *  @p lower tall. */
Element column(float lower) {
  return box().children({box().key("sidebar").width(50).height(100).children(
      {box().key("upper").height(80).flexShrink(0),
       box().key("lower").height(lower).flexShrink(0)})});
}

/** A passage long enough to wrap onto several lines at 120 pixels. */
Element passage() {
  return text(
             u8"Several words that wrap onto more than one line of this "
             u8"narrow column",
             styleAt(20))
      .key("passage");
}

/** A column 120 wide and 40 tall, short of room for @p leaf's lines and
 *  the 10 pixel box that follows it. */
Element shortColumn(Element leaf) {
  return box().children({box().key("column").width(120).height(40).children(
      {std::move(leaf), box().height(10).flexShrink(0)})});
}

/** The height of the passage's lines, laid out where nothing shrinks it. */
float linesHeight() {
  Host host;
  host.composer.render(box().width(120).children({passage()}));
  host.frame();
  return boundsOf(host, "passage").height();
}

}  // namespace

TEST(ComposeLayoutReports,
     ATextLeafInAColumnKeepsItsLinesUnlessAnExplicitMinimumSaysOtherwise) {
  const float lines = linesHeight();
  ASSERT_GT(lines, 40.0f) << "the passage must wrap onto several lines";
  // The leaf's extent along the line it stands in: its height down a
  // column, its width along a row.
  struct Row {
    const char* name;
    std::function<Element()> tree;
    float extent;
    bool along = false;
  };
  const std::vector<Row> rows = {
      {"its minimum unstated", [] { return shortColumn(passage()); }, lines},
      {"a stated minimum of zero",
       [] { return shortColumn(passage().minHeight(0)); }, 30.0f},
      {"a stated basis of zero",
       [] { return shortColumn(passage().flexBasis(0)); }, 0.0f},
      {"a stated height shorter than its lines",
       [] { return shortColumn(passage().height(20)); }, 20.0f},
      {"a stated maximum shorter than its lines",
       [] { return shortColumn(passage().maxHeight(20)); }, 20.0f},
      {"a leaf that clips its lines",
       [] { return shortColumn(passage().overflow(Overflow::Clip)); }, 30.0f},
      {"a leaf in a row, whose lines run along its line",
       [] {
         return box().children(
             {box().row().key("column").width(120).height(40).children(
                 {passage(), box().width(100).flexShrink(0)})});
       },
       20.0f, true},
  };
  for (const Row& row : rows) {
    SCOPED_TRACE(row.name);
    Host host;
    host.composer.render(row.tree());
    host.frame();
    const SkRect leaf = boundsOf(host, "passage");
    EXPECT_EQ(row.along ? leaf.width() : leaf.height(), row.extent);
  }
  {
    // The floor follows the container: a row that turns into a column
    // floors the leaf, whose own description stands still.
    SCOPED_TRACE("a row that turns into a column");
    Host host;
    const auto tree = [](bool down) {
      Element line = box().key("column").width(120).height(40).children(
          {passage(), box().height(10).width(10).flexShrink(0)});
      if (!down) line.row();
      return box().children({std::move(line)});
    };
    host.composer.render(tree(false));
    host.frame();
    host.composer.render(tree(true));
    host.frame();
    EXPECT_EQ(boundsOf(host, "passage").height(), lines);
  }
}

// Layout names a spill no clip answers for and a text leaf a stated minimum
// lets a column shrink, each with its cause, once when it starts and again
// only after the node has fit.
TEST(ComposeLayoutReports, LayoutSaysWhatItWouldOtherwiseDrawWithoutAWord) {
  sigil::compose::test::checkRows({
      {"ASquashingMinimum",
       [] {
         const float lines = linesHeight();
         struct Row {
           const char* name;
           Element leaf;
           // Words the one report says, or empty where nothing is said.
           std::string cause;
           float shorter;
         };
         const std::vector<Row> rows = {
             {"its minimum unstated", passage(), "", 0.0f},
             {"a stated height shorter than its lines", passage().height(20),
              "", 0.0f},
             {"a stated minimum of zero", passage().minHeight(0),
              "its stated minimum height lets the column shrink it",
              lines - 30.0f},
             {"a stated basis of zero", passage().flexBasis(0),
              "its stated basis lets the column shrink it", lines},
         };
         for (const Row& row : rows) {
           SCOPED_TRACE(row.name);
           Host host;
           Heard heard;
           host.composer.render(shortColumn(row.leaf));
           host.frame();
           // A leaf that keeps its lines overflows the column, which is said as
           // the column's; only what the leaf itself says is read here.
           std::vector<std::string> squashes;
           for (const std::string& message : heard.messages)
             if (says(message, "shorter than its lines"))
               squashes.push_back(message);
           if (row.cause.empty()) {
             EXPECT_TRUE(squashes.empty()) << everything(squashes);
             continue;
           }
           ASSERT_EQ(squashes.size(), 1u) << everything(heard.messages);
           const std::string& message = squashes.front();
           EXPECT_TRUE(says(message,
                            "the text \"Several words that wrap onto "
                            "more…\" at \"passage\""))
               << message;
           EXPECT_TRUE(says(message, "in the column at \"column\"")) << message;
           EXPECT_TRUE(says(message, row.cause)) << message;
           EXPECT_TRUE(
               says(message, std::to_string((int)std::round(row.shorter)) +
                                 " pixels shorter"))
               << message;
           const size_t said = heard.messages.size();
           host.frame();
           EXPECT_EQ(heard.messages.size(), said) << "a redraw says it again";
         }
       }},
      {"ASpill",
       [] {
         // Two boxes 80 and 60 tall that will not shrink, in a column 100 tall:
         // the line runs 40 pixels past its box.
         const auto spilling = [](Element sidebar) {
           return sidebar.key("sidebar").width(50).height(100).children(
               {box().key("upper").height(80).flexShrink(0),
                box().key("lower").height(60).flexShrink(0)});
         };
         // The spilling column under a clipping ancestor @p height tall.
         const auto clippedAt = [&](float height) {
           return box().children({box()
                                      .height(height)
                                      .overflow(Overflow::Clip)
                                      .children({spilling(box())})});
         };
         struct Row {
           const char* name;
           std::function<Element()> tree;
           // Words the one report says, or empty where nothing is said.
           std::vector<std::string> words;
         };
         const std::vector<Row> rows = {
             {"a spill with nothing over it",
              [&] { return box().children({spilling(box())}); },
              {"the column at \"sidebar\"",
               "140 pixels tall in 100 pixels of room",
               "40 pixels past its box", "the box at \"lower\""}},
             {"a spill the column clips itself",
              [&] {
                return box().children(
                    {spilling(box().overflow(Overflow::Clip))});
              },
              {}},
             {"a spill wholly past a clipping ancestor's box",
              [&] { return clippedAt(100); },
              {}},
             {"a spill a clipping ancestor's box holds whole",
              [&] { return clippedAt(200); },
              {}},
             {"a spill a clipping ancestor cuts in part",
              [&] { return clippedAt(120); },
              {"the column at \"sidebar\""}},
             {"a row whose spill shows",
              [] {
                return box().children(
                    {box().row().key("strip").width(100).height(20).children(
                        {box().width(80).flexShrink(0),
                         box().width(60).flexShrink(0)})});
              },
              {"the row at \"strip\"", "140 pixels wide"}},
             {"a text leaf holding its lines past its column",
              [] { return shortColumn(passage()); },
              {"the column at \"column\"",
               "the text \"Several words that wrap"}},
             {"children that fit", [] { return column(20); }, {}},
             {"children that give the room back",
              [] {
                return box().children(
                    {box().key("sidebar").width(50).height(100).children(
                        {box().height(80), box().height(60)})});
              },
              {}},
             {"a child out of the flow",
              [] {
                return box().children(
                    {box().key("sidebar").width(50).height(100).children(
                        {box().height(80).flexShrink(0),
                         box().height(160).absolute().top(0).left(0)})});
              },
              {}},
         };
         for (const Row& row : rows) {
           SCOPED_TRACE(row.name);
           Host host;
           Heard heard;
           host.composer.render(row.tree());
           host.frame();
           if (row.words.empty()) {
             EXPECT_TRUE(heard.messages.empty()) << everything(heard.messages);
             continue;
           }
           ASSERT_EQ(heard.messages.size(), 1u) << everything(heard.messages);
           for (const std::string& words : row.words)
             EXPECT_TRUE(says(heard.messages.front(), words))
                 << heard.messages.front();
           host.frame();
           EXPECT_EQ(heard.messages.size(), 1u) << "a redraw says it again";
         }
       }},
      {"AStandingOverflowIsSaidAgainOnlyAfterItFits",
       [] {
         Host host;
         Heard heard;
         host.composer.render(column(60));
         host.frame();
         ASSERT_EQ(heard.messages.size(), 1u);

         // A layout that leaves the overflow where it stood says nothing.
         host.composer.render(column(60));
         host.frame();
         host.composer.setSize({300, 300});
         host.frame();
         EXPECT_EQ(heard.messages.size(), 1u) << everything(heard.messages);

         // Once the column fits, an overflow that comes back is news again.
         host.composer.render(column(10));
         host.frame();
         EXPECT_EQ(heard.messages.size(), 1u) << everything(heard.messages);
         host.composer.render(column(60));
         host.frame();
         EXPECT_EQ(heard.messages.size(), 2u) << everything(heard.messages);
       }},
  });
}
