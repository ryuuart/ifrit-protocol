// What a typed leaf IS: the kind, the key and the prune are the node's,
// unchanged by holding it as `Text`, `Image` or `Band`; the verbs only that
// leaf can use are declared only there, so writing one anywhere else is a
// compile error rather than a silent nothing.

#include <sigilgeometry/path/Band.h>

#include <concepts>
#include <utility>

#include "support/CoreTestSupport.h"

// -------------------------------------------------------------------------
// The verbs each leaf alone declares. These are the whole point of the typed
// leaves: a wrong-kind call must not compile. Asking whether a member is
// there needs a dependent expression, so each question is a concept over the
// node type; the answers cost nothing at run time and are checked by every
// build of this file.

template <class Node>
concept SaysMaxTextLines = requires(Node node) { node.maxTextLines(2); };
template <class Node>
concept SaysInitialLetter =
    requires(Node node) { node.initialLetter(sigil::weave::InitialLetter{}); };
template <class Node>
concept SaysTextWillChange = requires(Node node) { node.textWillChange(); };
template <class Node>
concept SaysAtRest = requires(Node node) { node.atRest(); };
template <class Node>
concept SaysImageRegion = requires(Node node) { node.imageRegion(SkRect{}); };
template <class Node>
concept SaysBandAlignment = requires(Node node) {
  node.bandAlignment(sigil::geometry::path::Formation::Inner);
};

static_assert(SaysMaxTextLines<Text>);
static_assert(SaysInitialLetter<Text>);
static_assert(SaysTextWillChange<Text>);
static_assert(SaysAtRest<Text>);
static_assert(SaysImageRegion<Image>);
static_assert(SaysBandAlignment<Band>);

static_assert(!SaysMaxTextLines<Element>);
static_assert(!SaysInitialLetter<Element>);
static_assert(!SaysTextWillChange<Element>);
static_assert(!SaysAtRest<Element>);
static_assert(!SaysImageRegion<Element>);
static_assert(!SaysBandAlignment<Element>);

static_assert(!SaysMaxTextLines<Image>);
static_assert(!SaysImageRegion<Text>);
static_assert(!SaysBandAlignment<Text>);

// A leaf is a node wherever a node is wanted, and a chain through the verbs
// every node has hands the leaf's own type back, so the leaf-only verbs are
// still there to write after one.
static_assert(std::convertible_to<Text, Element>);
static_assert(std::convertible_to<Image, Element>);
static_assert(std::convertible_to<Band, Element>);
static_assert(std::same_as<
              decltype(std::declval<Text&>().width(10).opacity(0.5f)), Text&>);
static_assert(std::same_as<decltype(std::declval<Text&>().atRest()), Text>);

// -------------------------------------------------------------------------
// The node underneath is the same node.

TEST(ComposeTypedLeaves, ATextDescribedThroughTheLeafPrunes) {
  // Holding the leaf as `Text` for the length of a chain must not change its
  // kind, its key or any declaration it carries: a re-describe of the same
  // passage patches nothing and records no picture, exactly as a re-describe
  // of any other node does.
  Host host;
  auto tree = [] {
    Text leaf = text(u8"held", styleAt(18));
    leaf.key("passage").width(120).maxTextLines(2);
    return box().padding(8).children({leaf});
  };
  host.composer.render(tree());
  host.frame();

  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "a text re-described through the typed leaf must prune";
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeTypedLeaves, ARestPoseKeepsTheSuffixRuleAndTheLeafType) {
  // `atRest` is the one verb that ends a chain, and it hands back the same
  // kind of leaf so the text verbs are still there. The copy's key is the
  // original's with `-rest` after it, which is what makes both addressable
  // and both prune.
  Host host;
  auto tree = [] {
    Text moving = text(u8"held", styleAt(18));
    moving.key("passage");
    Text rest = moving.atRest();
    rest.opacity(0.25f);
    return box().children({rest, moving});
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_TRUE(host.composer.bounds("passage-rest").has_value())
      << "the rest pose takes the original's key with `-rest` after it";
  EXPECT_TRUE(host.composer.bounds("passage").has_value());

  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "a rest pose re-described must prune like the leaf it copies";
}
