#pragma once
// Support for compose_text_test: text data, the text pass, vertical
// writing, motion along paths and the text-fx presets. The binary links
// the typography headers and the shape catalog — motion along a path and
// an upright column are exercised on `geometry::shapes::` silhouettes — and
// nothing that strokes or fills.

#include <sigilcompose/kit/Legibility.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include "ShapeTestSupport.h"
