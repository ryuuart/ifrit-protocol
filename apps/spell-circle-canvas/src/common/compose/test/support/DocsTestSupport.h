#pragma once
// Support for compose_docs_test: the engine walkthroughs and the generated
// README probes, over the whole library. The probe TU includes every
// compose header itself, so a documented name is never reported missing
// merely because the harness left out the file that owns it.

#include <sigilcompose/kit/Kit.h>
// The one SigilWeave header no compose header pulls in and the README
// still names: the OpenType feature presets a style spells by name.
#include <sigilweave/style/Features.h>
// …and the one SigilCore header in the same position: a derivation
// declares what it reads in core's vocabulary, and the declaration is
// carried on a block only the library's own translation units see.
#include <sigilcore/reconcile/Reads.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include "PaintTestSupport.h"
