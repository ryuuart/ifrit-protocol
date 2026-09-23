#pragma once

/** @file
 * @ingroup compose-core
 *
 * The verb families EVERY node has, gathered so the list is stated once:
 * a kind of node with verbs of its own inherits this and adds them.
 */

#include <sigilcompose/core/verbs/Box.h>
#include <sigilcompose/core/verbs/Cascade.h>
#include <sigilcompose/core/verbs/Decoration.h>
#include <sigilcompose/core/verbs/Depth.h>
#include <sigilcompose/core/verbs/Effects.h>
#include <sigilcompose/core/verbs/Flex.h>
#include <sigilcompose/core/verbs/Font.h>
#include <sigilcompose/core/verbs/Mask.h>
#include <sigilcompose/core/verbs/Paint.h>
#include <sigilcompose/core/verbs/Placement.h>
#include <sigilcompose/core/verbs/Shape.h>
#include <sigilcompose/core/verbs/Structure.h>
#include <sigilcompose/core/verbs/Transform.h>

namespace sigil::compose {

/** WHAT ANY NODE CAN SAY: its box, its flex factors, its placement, its
 *  shape, its mask, the cascade it declares, its font and ink, its
 *  paint, its decorations, the compositing lanes, the two transform
 *  stacks, and what it is in the tree. A verb family NOT here belongs to
 *  one kind of leaf, and calling it on a node that cannot use it does not
 *  compile. */
template <class Derived>
class NodeVerbs : public BoxVerbs<Derived>,
                  public FlexVerbs<Derived>,
                  public PlacementVerbs<Derived>,
                  public ShapeVerbs<Derived>,
                  public MaskVerbs<Derived>,
                  public CascadeVerbs<Derived>,
                  public FontVerbs<Derived>,
                  public PaintVerbs<Derived>,
                  public DecorationVerbs<Derived>,
                  public EffectVerbs<Derived>,
                  public TransformVerbs<Derived>,
                  public DepthVerbs<Derived>,
                  public StructureVerbs<Derived> {};

}  // namespace sigil::compose
