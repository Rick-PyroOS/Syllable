/*  JSON Schema validation on cJSON trees, for os::JSON.  Internal to
 *  libsyllable.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 */

#ifndef __F_UTIL_JSONSCHEMA_H__
#define __F_UTIL_JSONSCHEMA_H__

#include <string>
#include <vector>

struct cJSON;

#pragma GCC visibility push(hidden)

namespace os
{
namespace jsonschema
{

/* Whether psInstance is valid against psSchema (a schema object, or true or
 * false).  Each failure goes into pcErrors (when not NULL) as
 * "$.path.to.value: what is wrong". */
bool Validate( const cJSON* psSchema, const cJSON* psInstance, std::vector<std::string>* pcErrors );

} // namespace jsonschema
} // namespace os

#pragma GCC visibility pop

#endif // __F_UTIL_JSONSCHEMA_H__
