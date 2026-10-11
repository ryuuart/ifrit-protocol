#pragma once

/** @file
 * WHAT A DECODED PART SAYS ITS SURFACE IS, as SigilMaterial's options: the
 * factors a file states, lowered onto the one surface vocabulary every
 * renderer here reads.
 */

#include <sigilmaterial/core/Material.h>

#include "sigilgeometry/mesh/codec/Model.h"

namespace sigil::geometry::mesh::codec::decode {

/** THE SURFACE @p part STATES: its metallic and roughness factors, its
 *  emission and alpha cutoff, the whole transmission lobe — weight, index,
 *  thickness, attenuation and dispersion — and every optional lobe the file
 *  carried, each left neutral where the file stated none. The maps are not
 *  among them: a part holds their encoded bytes, which only an image
 *  library decodes, so a caller decodes them and places each in its slot. */
material::SurfaceOptions surfaceOptions(const Part& part);

}  // namespace sigil::geometry::mesh::codec::decode
