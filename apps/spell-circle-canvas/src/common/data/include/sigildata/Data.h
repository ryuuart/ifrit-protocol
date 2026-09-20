#pragma once

/** @file
 * Every public SigilData header in one include.
 */

/** @defgroup data-table Tables and columns
 *  A table of named, typed columns, the cells in them, and the
 *  reshapings — select, filter, sort, group — that each answer another
 *  table.
 *  @{ */
/** @} */

/** @defgroup data-values Values
 *  The ranges a domain is made of, and the readings a value carries
 *  while it travels.
 *  @{ */
/** @} */

/** @defgroup data-scale Scales
 *  The one value that maps a domain onto a range, and the intervals and
 *  ticks a domain is described by.
 *  @{ */
/** @} */

/** @defgroup data-decode Decoding
 *  Bytes read as data: tables out of delimited text, documents out of
 *  JSON, records out of a FlatBuffers schema, and the message codecs a
 *  live wire speaks.
 *  @{ */
/** @} */

/** @defgroup data-query Queries
 *  A database over tables, and the statements asked of it.
 *  @{ */
/** @} */

/** @defgroup data-connection Connections
 *  A named door onto a live source, with what arrived on it and what is
 *  sent back down it.
 *  @{ */
/** @} */

#include "sigildata/connection/Connection.h"
#include "sigildata/decode/ArtNet.h"
#include "sigildata/decode/Csv.h"
#include "sigildata/decode/Decoders.h"
#include "sigildata/decode/FlatBuffer.h"
#include "sigildata/decode/Json.h"
#include "sigildata/decode/Midi.h"
#include "sigildata/decode/Osc.h"
#include "sigildata/decode/Schema.h"
#include "sigildata/query/Database.h"
#include "sigildata/scale/Scale.h"
#include "sigildata/table/Table.h"
#include "sigildata/values/Values.h"
