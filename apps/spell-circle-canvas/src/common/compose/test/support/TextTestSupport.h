#pragma once
// Support for compose_text_test: text data, the text pass, vertical
// writing and motion along paths.

#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilweave/Choreograph.h>

#include <algorithm>
#include <cmath>

#include "Atlas.h"
#include "Effects.h"
#include "Host.h"
#include "Profile.h"
