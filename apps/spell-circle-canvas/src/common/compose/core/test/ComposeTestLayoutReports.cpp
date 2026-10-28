// What the layout says rather than keeps quiet about: a flex container whose
// children run past its box, and a text leaf a column has shrunk below its
// lines.

#include <sigilcore/report/Report.h>

#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/CoreTestSupport.h"

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

/** The height of the passage's lines, laid out where nothing shrinks it. */
float linesHeight() {
  Host host;
  host.composer.render(box().width(120).children({passage()}));
  host.frame();
  return boundsOf(host, "passage").height();
}

}  // namespace

TEST(ComposeLayoutReports, AnOverFullColumnIsReportedOnceByName) {
  Host host;
  Heard heard;
  host.composer.render(column(60));
  host.frame();
  ASSERT_EQ(heard.messages.size(), 1u)
      << "an over-full column is said once when it is laid out";
  const std::string& message = heard.messages.front();
  EXPECT_TRUE(says(message, "the column at \"sidebar\"")) << message;
  EXPECT_TRUE(says(message, "140 pixels tall in 100 pixels of room"))
      << message;
  EXPECT_TRUE(says(message, "40 pixels past its box")) << message;
  EXPECT_TRUE(says(message, "the box at \"lower\"")) << message;

  // A redraw that lays nothing out says nothing, and neither does a
  // layout that leaves the overflow where it stood.
  host.frame();
  host.composer.render(column(60));
  host.frame();
  host.composer.setSize({300, 300});
  host.frame();
  EXPECT_EQ(heard.messages.size(), 1u)
      << "a standing overflow is said again: " << heard.messages.back();

  // Once the column fits, an overflow that comes back is news again.
  host.composer.render(column(10));
  host.frame();
  EXPECT_EQ(heard.messages.size(), 1u) << heard.messages.back();
  host.composer.render(column(60));
  host.frame();
  EXPECT_EQ(heard.messages.size(), 2u)
      << "an overflow that returns after the column fit is said again";
}

TEST(ComposeLayoutReports, ALineThatFitsOrShrinksOrClipsSaysNothing) {
  Heard heard;
  {
    // Children that fit.
    Host host;
    host.composer.render(column(20));
    host.frame();
  }
  {
    // Children that give the room back.
    Host host;
    host.composer.render(
        box().children({box().key("sidebar").width(50).height(100).children(
            {box().height(80), box().height(60)})}));
    host.frame();
    EXPECT_EQ(boundsOf(host, "sidebar").height(), 100.0f);
  }
  {
    // A spill the author clipped is meant.
    Host host;
    host.composer.render(
        box().children({box()
                            .key("sidebar")
                            .width(50)
                            .height(100)
                            .overflow(Overflow::Clip)
                            .children({box().height(80).flexShrink(0),
                                       box().height(60).flexShrink(0)})}));
    host.frame();
  }
  {
    // A child out of the flow is not part of the line.
    Host host;
    host.composer.render(
        box().children({box().key("sidebar").width(50).height(100).children(
            {box().height(80).flexShrink(0),
             box().height(160).absolute().top(0).left(0)})}));
    host.frame();
  }
  EXPECT_TRUE(heard.messages.empty()) << heard.messages.front();
}

TEST(ComposeLayoutReports, AnOverFullRowIsReportedAcross) {
  Host host;
  Heard heard;
  host.composer.render(
      box().children({box().row().key("strip").width(100).height(20).children(
          {box().width(80).flexShrink(0), box().width(60).flexShrink(0)})}));
  host.frame();
  ASSERT_EQ(heard.messages.size(), 1u);
  EXPECT_TRUE(says(heard.messages.front(), "the row at \"strip\""))
      << heard.messages.front();
  EXPECT_TRUE(says(heard.messages.front(), "140 pixels wide"))
      << heard.messages.front();
}

TEST(ComposeLayoutReports, ATextLeafAColumnShrinksBelowItsLinesIsReported) {
  const float lines = linesHeight();
  ASSERT_GT(lines, 40.0f) << "the passage must wrap onto several lines";
  Host host;
  Heard heard;
  host.composer.render(
      box().children({box().key("column").width(120).height(40).children(
          {passage(), box().height(10).flexShrink(0)})}));
  host.frame();
  // A flex line floors an item at zero, so the column, short of room,
  // shrinks the passage and fits.
  EXPECT_EQ(boundsOf(host, "passage").height(), 30.0f);
  ASSERT_EQ(heard.messages.size(), 1u)
      << "the shrunk leaf is said and the column, which fits, is not";
  const std::string& message = heard.messages.front();
  EXPECT_TRUE(says(message, "at \"passage\" is laid out")) << message;
  EXPECT_TRUE(says(message, "in the column at \"column\"")) << message;
  EXPECT_TRUE(says(message, "state flexShrink(0) on it")) << message;
  host.frame();
  EXPECT_EQ(heard.messages.size(), 1u) << heard.messages.back();
}

TEST(ComposeLayoutReports, ATextLeafThatKeepsItsLinesOverflowsItsColumn) {
  const float lines = linesHeight();
  Host host;
  Heard heard;
  host.composer.render(
      box().children({box().key("column").width(120).height(40).children(
          {passage().flexShrink(0), box().height(10).flexShrink(0)})}));
  host.frame();
  EXPECT_EQ(boundsOf(host, "passage").height(), lines);
  ASSERT_EQ(heard.messages.size(), 1u);
  EXPECT_TRUE(says(heard.messages.front(), "the column at \"column\""))
      << heard.messages.front();
  EXPECT_TRUE(
      says(heard.messages.front(), "the text \"Several words that wrap"))
      << heard.messages.front();
}

TEST(ComposeLayoutReports,
     ATextLeafSetShorterThanItsLinesOnPurposeSaysNothing) {
  // A stated height is the author's own floor: the leaf has not been shrunk.
  Host host;
  Heard heard;
  host.composer.render(
      box().children({box().key("column").width(120).height(40).children(
          {passage().height(20).flexShrink(0),
           box().height(10).flexShrink(0)})}));
  host.frame();
  EXPECT_EQ(boundsOf(host, "passage").height(), 20.0f);
  EXPECT_TRUE(heard.messages.empty()) << heard.messages.front();
}

TEST(ComposeLayoutReports, ATextLeafAStatedMinimumLetsShrinkIsReported) {
  const float lines = linesHeight();
  Host host;
  Heard heard;
  host.composer.render(
      box().children({box().key("column").width(120).height(40).children(
          {passage().minHeight(0), box().height(10).flexShrink(0)})}));
  host.frame();
  EXPECT_EQ(boundsOf(host, "passage").height(), 30.0f)
      << "a stated minimum of zero is CSS's min-height: 0, and the leaf "
         "shrinks";
  ASSERT_EQ(heard.messages.size(), 1u)
      << "the squashed leaf is said and the column, which fits, is not";
  const std::string& message = heard.messages.front();
  EXPECT_TRUE(says(message,
                   "the text \"Several words that wrap onto "
                   "more\u2026\" at \"passage\""))
      << message;
  EXPECT_TRUE(says(message, "stated minimum height")) << message;
  EXPECT_TRUE(says(message, std::to_string((int)std::round(lines - 30.0f)) +
                                " pixels shorter"))
      << message;
  host.frame();
  EXPECT_EQ(heard.messages.size(), 1u);
}
