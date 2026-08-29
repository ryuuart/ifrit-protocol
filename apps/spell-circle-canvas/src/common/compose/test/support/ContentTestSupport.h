#pragma once
// Support for compose_content_test: leaves, feed, routers, layouts, text
// effects and colour management.

#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Feed.h>
#include <sigilcompose/Layouts.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/testing/Checks.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "Atlas.h"
#include "Effects.h"
#include "Host.h"
