#pragma once

/** @file
 * SigilSubstance — Adobe Substance 3D archives (.sbsar) rendered to
 * images through the Substance Engine. The umbrella header: the Package
 * and the Graphs it holds, for a consumer of the whole library.
 *
 * A .sbsar is a procedural material: a graph with named, typed
 * PARAMETERS (sliders, colours, toggles, images) and named OUTPUTS, each
 * output tagged with the material channel it feeds ("baseColor",
 * "normal", "roughness", "metallic", "ambientOcclusion", "height",
 * "emissive"...). This library loads a package, exposes its graphs'
 * parameters as plain values, renders on the CPU engine, and hands each
 * output back as an SkImage keyed by identifier and by usage. Nothing
 * here knows about surfaces or the GPU: SigilMaterial's texture-set door
 * takes the by-usage map and makes a Material of it.
 *
 * Namespace sigil::substance, target SigilSubstance. Requires the
 * Substance 3D SDK at configure time; without it the target does not
 * exist.
 */

#include <sigilsubstance/graph/Graph.h>
#include <sigilsubstance/graph/Output.h>
#include <sigilsubstance/graph/Parameter.h>
#include <sigilsubstance/package/Package.h>
