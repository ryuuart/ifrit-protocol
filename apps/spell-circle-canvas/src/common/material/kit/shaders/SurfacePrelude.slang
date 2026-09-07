float chanS(float4 c, float which) {
  return which < 0.5 ? c.r : which < 1.5 ? c.g : which < 2.5 ? c.b : c.a;
}

float mixS(float a, float b, float t) {
  return a + (b - a) * t;
}

float cutS(float a) {
  return alphaCutoff > 0.0 ? (a < alphaCutoff ? 0.0 : 1.0) : a;
}

bool dressedS(float4 c) {
  return c.r < 1.0 || c.g < 1.0 || c.b < 1.0;
}
