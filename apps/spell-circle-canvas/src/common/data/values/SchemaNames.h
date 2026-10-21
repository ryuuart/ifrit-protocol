/** @file
 * WHAT THE WRITTEN HEADER CALLS THINGS: the generated type, the value
 * type beside it, a union's tag enumerator, the C++ spelling of a
 * scalar the schema declared, and the one field a value gives no name
 * to at all.
 */

#pragma once

#include <flatbuffers/idl.h>

#include <string>

namespace sigil::data::schema {

using flatbuffers::BaseType;
using flatbuffers::Definition;
using flatbuffers::EnumDef;
using flatbuffers::EnumVal;
using flatbuffers::FieldDef;
using flatbuffers::StructDef;
using flatbuffers::Type;

/** The stem of a path: `a/b/feed_sky.fbs` is `feed_sky`. */
std::string stemOf(const std::string& path);

/** A schema's namespace as C++ spells it: `feed_sky`, or empty for
 *  a schema that declares none. */
std::string namespaceOf(const flatbuffers::Namespace* space);

/** A generated type as the generated header declares it, qualified from
 *  the global namespace so the value namespace under it cannot shadow
 *  anything: `::feed_sky::Sky`. */
std::string wireName(const std::string& space, const std::string& name);

/** A value type as a reader outside the value namespace spells it:
 *  `::feed_sky::values::Sky`. */
std::string valueName(const std::string& valueSpace, const std::string& name);

/** One enumerator of a union's tag enum: `::feed_sky::Message_Sky`. */
std::string wireEnumerator(const std::string& space, const EnumDef& tag,
                           const std::string& value);

/** A field's name with its first letter raised, which is how the local
 *  holding what the field points at is named. */
std::string raised(const std::string& name);

/** The C++ spelling of a scalar the schema declares; empty for a base
 *  type that is no scalar. */
std::string scalarName(BaseType type);

/** The field a union's tag travels in. It is a field of the table on
 *  the wire and no member of the value: the variant's alternative is
 *  the type, so a second place to say it could only disagree. */
bool isUnionTag(const Type& type);

}  // namespace sigil::data::schema
