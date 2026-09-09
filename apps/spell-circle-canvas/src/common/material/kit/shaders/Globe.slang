// The device-side entry: the globe read at the surface point, which for
// a node-filling material is the pixel the paint-side entry is handed.
float4 surface(float2 uv) { return globeAt(uv); }
