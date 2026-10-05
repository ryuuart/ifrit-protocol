// A FLIPBOOK: one sheet of `columns` by `rows` cells read row by row
// from the top left, `frames` of them in use, shown one after another.
//
// Which frame stands is a pure function of the clock and the values the
// material was made with: a bound `frame` wins, then a bound `progress`
// over the whole run, then the clock at `framesPerSecond` — each moved on
// by `startFrame`, and wrapped round or held at the last frame. Between
// two frames the sheet is read twice and the two cells mixed when
// `blendFrames` says so, and with a motion-vector sheet each read is
// pushed along the motion first, so the mix is of two pictures that have
// moved toward each other rather than two that cross-fade.
//
// The sheet's colour reaches this body premultiplied when it is colour
// and straight when it is data; what the body answers is straight.

/** Where cell @p index is read at @p uv over it, held half a texel inside
 *  the cell so a filtered read never takes a texel of its neighbour. */
float2 flipbookCell(float index, float2 uv) {
  let across = max(columns, 1.0);
  let down = max(rows, 1.0);
  let row = floor(index / across);
  let column = index - row * across;
  let cellTexels = max(sheetSize / float2(across, down), float2(1.0, 1.0));
  let inset = float2(0.5, 0.5) / cellTexels;
  let local = clamp(uv, inset, float2(1.0, 1.0) - inset);
  return (float2(column, row) + local) / float2(across, down);
}

/** The frame standing now, fractional between two. */
float flipbookFrame() {
  let count = max(frames, 1.0);
  float shown = uTime * framesPerSecond;
  if (progress >= 0.0) shown = progress * count;
  if (frame >= 0.0) shown = frame;
  shown = shown + startFrame;
  if (loop > 0.5) return shown - floor(shown / count) * count;
  return clamp(shown, 0.0, count - 1.0);
}

float4 surface(float2 uv) {
  let count = max(frames, 1.0);
  let shown = flipbookFrame();
  let first = floor(shown);
  let between = blendFrames > 0.5 ? shown - first : 0.0;
  var second = first + 1.0;
  if (second >= count) second = loop > 0.5 ? 0.0 : count - 1.0;
  var pushFirst = float2(0.0, 0.0);
  var pushSecond = float2(0.0, 0.0);
  if (motionStrength != 0.0 && between > 0.0) {
    let motion = (motionVectors.Sample(flipbookCell(first, uv)).rg * 2.0 -
                  float2(1.0, 1.0)) *
                 motionStrength;
    pushFirst = -motion * between;
    pushSecond = motion * (1.0 - between);
  }
  var read = sheet.Sample(flipbookCell(first, uv + pushFirst));
  if (between > 0.0) {
    let next = sheet.Sample(flipbookCell(second, uv + pushSecond));
    read = read + (next - read) * between;
  }
  if (straight > 0.5) return read;
  let alpha = read.a;
  return float4(alpha > 0.0 ? read.rgb / alpha : read.rgb, alpha);
}
