#pragma once

/** @file
 * How a map is uploaded to the device: the depth of the chain under it.
 */

namespace sigil::world::diligent {

/**
 * How many mip levels a map of @p width × @p height is uploaded with.
 *
 * A map is minified whenever the surface wearing it is smaller on screen
 * than the map is in texels, and a sampler with one level to read then
 * walks the texels of a shrinking triangle and answers a different one
 * every frame — which is what aliasing is. So a map wide enough to halve
 * carries the whole chain, and the device is asked to derive it.
 *
 * A MAP CAN BE TOO SMALL TO HAVE A CHAIN. Halving a single texel arrives
 * nowhere, so a flat colour handed over as one texel — which is how an
 * emissive tint or any other constant slot is spelled — is the whole
 * texture at its one level, and asking for none below it is not a
 * shortcut: a device handed a view with one level in it and told to fill
 * the levels beneath has nowhere to put them, and refuses.
 *
 * One for a map with no extent at all.
 */
int mapMipLevels(int width, int height);

}  // namespace sigil::world::diligent
