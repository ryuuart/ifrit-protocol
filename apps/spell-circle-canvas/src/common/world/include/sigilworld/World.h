#pragma once

/** @file
 * @ingroup world-kit
 * THE FRONT DOOR: everything a scene is written in — the node kinds, the
 * frame and its environment, the collections, and the kit's stock
 * arrangements. A sketch includes this and nothing else from here; what
 * a host, an executor or a test reaches for is under `advanced/`.
 */

#include <sigilworld/frame/Environment.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/kit/Kit.h>
#include <sigilworld/node/Camera.h>
#include <sigilworld/node/Collection.h>
#include <sigilworld/node/Group.h>
#include <sigilworld/node/Light.h>
#include <sigilworld/node/Mesh.h>
#include <sigilworld/node/Null.h>

/** THREE-DIMENSIONAL SCENES, DESCRIBED AS VALUES. A scene is a tree of
 *  nodes built fresh every frame — groups and nulls placing what is under
 *  them or standing to be referred to, meshes wearing surfaces, lights
 *  and cameras — handed to a
 *  frame that names its environment and its passes, reconciled into a
 *  retained scene and drawn on a device. Reach for it when a drawing has
 *  depth, a camera or a light in it. It consumes SigilGeometry's meshes
 *  and SigilMaterial's surfaces and hands neither back. */
namespace sigil::world {}
