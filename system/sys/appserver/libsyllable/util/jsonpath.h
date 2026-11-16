/*  JSONPath (RFC 9535) queries on cJSON trees, for os::JSON.  Internal to
 *  libsyllable.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 */

#ifndef __F_UTIL_JSONPATH_H__
#define __F_UTIL_JSONPATH_H__

#include <string>
#include <vector>

struct cJSON;

#pragma GCC visibility push(hidden)

namespace os
{
namespace jsonpath
{

/* Whether a name is a JSONPath query: "$", or "$" followed by '.', '[' or
 * blank space ("$schema" is a name) */
bool IsQuery( const char* pzName );

/* The nodes a query selects from psRoot ($), in document order.  false, with
 * a message ("column 7: ..."), for a query that isn't valid. */
bool Query( cJSON* psRoot, const char* pzQuery, std::vector<cJSON*>* pcResult, std::string* pcError );

} // namespace jsonpath
} // namespace os

#pragma GCC visibility pop

#endif // __F_UTIL_JSONPATH_H__
