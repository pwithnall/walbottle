/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Collabora Ltd. 2015
 *
 * Walbottle is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Walbottle is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Walbottle.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <locale.h>
#include <string.h>

#include "wbl-json-node.h"
#include "wbl-schema.h"
#include "wbl-meta-schema.h"

/* Unfortunately Walbottle is not yet perfect, and cannot entirely self-host
 * its unit tests. Here are some instances which Walbottle generates from the
 * JSON Schema meta-schema, but which it cannot later parse. This is typically
 * due to missing support for $ref or $schema.
 *
 * FIXME: Eliminate these.
 */
static const gchar *meta_schema_expected_failures[] = {
	"{\"definitions\":[]}",
	"{\"definitions\":\"\"}",
	"{\"definitions\":[],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"definitions\":[null]}",
	"{\"definitions\":null}",
	"{\"definitions\":[null],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":null,\"additionalProperties-test-unique\":null}",
	"{\"definitions\":[null,null]}",
	"{\"definitions\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"id\":[]}",
	"{\"id\":{}}",
	"{\"id\":{\"0\":null}}",
	"{\"id\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"id\":[],\"additionalProperties-test-unique\":null}",
	"{\"id\":{},\"additionalProperties-test-unique\":null}",
	"{\"id\":[null]}",
	"{\"id\":null}",
	"{\"id\":[null],\"additionalProperties-test-unique\":null}",
	"{\"id\":null,\"additionalProperties-test-unique\":null}",
	"{\"id\":[null,null]}",
	"{\"id\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"$schema\":[]}",
	"{\"$schema\":{}}",
	"{\"$schema\":{\"0\":null}}",
	"{\"$schema\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"$schema\":[],\"additionalProperties-test-unique\":null}",
	"{\"$schema\":{},\"additionalProperties-test-unique\":null}",
	"{\"$schema\":[null]}",
	"{\"$schema\":null}",
	"{\"$schema\":[null],\"additionalProperties-test-unique\":null}",
	"{\"$schema\":null,\"additionalProperties-test-unique\":null}",
	"{\"$schema\":[null,null]}",
	"{\"$schema\":[null,null],\"additionalProperties-test-unique\":null}",
	NULL
};

static const gchar *meta_schema_expected_failures2[] = {
	"{\"additionalItems\":[]}",
	"{\"additionalItems\":\"\"}",
	"{\"additionalItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":[null]}",
	"{\"additionalItems\":null}",
	"{\"additionalItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":null,\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":[null,null]}",
	"{\"additionalItems\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[]}",
	"{\"additionalProperties\":\"\"}",
	"{\"additionalProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null]}",
	"{\"additionalProperties\":null}",
	"{\"additionalProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":null,\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null,null]}",
	"{\"additionalProperties\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[]}",
	"{\"allOf\":{}}",
	"{\"allOf\":\"\"}",
	"{\"allOf\":{\"0\":null}}",
	"{\"allOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null]}",
	"{\"allOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null,null]}",
	"{\"allOf\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[]}",
	"{\"anyOf\":{}}",
	"{\"anyOf\":\"\"}",
	"{\"anyOf\":{\"0\":null}}",
	"{\"anyOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null]}",
	"{\"anyOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null,null]}",
	"{\"anyOf\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[]}}",
	"{\"dependencies\":{\"0\":\"\"}}",
	"{\"dependencies\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[null]}}",
	"{\"dependencies\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[null,null]}}",
	"{\"dependencies\":{\"0\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"items\":\"\"}",
	"{\"items\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"items\":[null]}",
	"{\"items\":[null],\"additionalProperties-test-unique\":null}",
	"{\"items\":[null,null]}",
	"{\"items\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":[]}",
	"{\"maxItems\":{}}",
	"{\"maxItems\":\"\"}",
	"{\"maxItems\":{\"0\":null}}",
	"{\"maxItems\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":[null]}",
	"{\"maxItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":[null,null]}",
	"{\"maxItems\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[]}",
	"{\"maxLength\":{}}",
	"{\"maxLength\":\"\"}",
	"{\"maxLength\":{\"0\":null}}",
	"{\"maxLength\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[null]}",
	"{\"maxLength\":[null],\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[null,null]}",
	"{\"maxLength\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":[]}",
	"{\"maxProperties\":{}}",
	"{\"maxProperties\":\"\"}",
	"{\"maxProperties\":{\"0\":null}}",
	"{\"maxProperties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":[null]}",
	"{\"maxProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":[null,null]}",
	"{\"maxProperties\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[]}",
	"{\"minItems\":{}}",
	"{\"minItems\":\"\"}",
	"{\"minItems\":{\"0\":null}}",
	"{\"minItems\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"minItems\":{},\"additionalProperties-test-unique\":null}",
	"{\"minItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[null]}",
	"{\"minItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[null,null]}",
	"{\"minItems\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[]}",
	"{\"minLength\":{}}",
	"{\"minLength\":\"\"}",
	"{\"minLength\":{\"0\":null}}",
	"{\"minLength\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[],\"additionalProperties-test-unique\":null}",
	"{\"minLength\":{},\"additionalProperties-test-unique\":null}",
	"{\"minLength\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[null]}",
	"{\"minLength\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[null,null]}",
	"{\"minLength\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[]}",
	"{\"minProperties\":{}}",
	"{\"minProperties\":\"\"}",
	"{\"minProperties\":{\"0\":null}}",
	"{\"minProperties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":{},\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[null]}",
	"{\"minProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[null,null]}",
	"{\"minProperties\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"not\":[]}",
	"{\"not\":\"\"}",
	"{\"not\":[],\"additionalProperties-test-unique\":null}",
	"{\"not\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"not\":[null]}",
	"{\"not\":[null],\"additionalProperties-test-unique\":null}",
	"{\"not\":[null,null]}",
	"{\"not\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[]}",
	"{\"oneOf\":{}}",
	"{\"oneOf\":\"\"}",
	"{\"oneOf\":{\"0\":null}}",
	"{\"oneOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[null]}",
	"{\"oneOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[null,null]}",
	"{\"oneOf\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[]}}",
	"{\"patternProperties\":{\"0\":\"\"}}",
	"{\"patternProperties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[null]}}",
	"{\"patternProperties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[null,null]}}",
	"{\"patternProperties\":{\"0\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[]}}",
	"{\"properties\":{\"0\":\"\"}}",
	"{\"properties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[null]}}",
	"{\"properties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[null,null]}}",
	"{\"properties\":{\"0\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"required\":[]}",
	"{\"required\":{}}",
	"{\"required\":\"\"}",
	"{\"required\":{\"0\":null}}",
	"{\"required\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"required\":[],\"additionalProperties-test-unique\":null}",
	"{\"required\":{},\"additionalProperties-test-unique\":null}",
	"{\"required\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"required\":[null]}",
	"{\"required\":[null],\"additionalProperties-test-unique\":null}",
	"{\"required\":[null,null]}",
	"{\"required\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[],[],[]]}",
	"{\"type\":[[],[]]}",
	"{\"type\":[[]]}",
	"{\"type\":[{},{},{}]}",
	"{\"type\":[{}]}",
	"{\"type\":[\"\",\"\",\"\"]}",
	"{\"type\":[\"\",\"\"]}",
	"{\"type\":[\"\"]}",
	"{\"type\":{}}",
	"{\"type\":\"\"}",
	"{\"type\":[{},{}]}",
	"{\"type\":[{\"0\":null}]}",
	"{\"type\":{\"0\":null}}",
	"{\"type\":[{\"0\":null},{\"0\":null}]}",
	"{\"type\":[{\"0\":null},{\"0\":null},{\"0\":null}]}",
	"{\"type\":[{\"0\":null},{\"0\":null},{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"type\":[{\"0\":null},{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"type\":[{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"type\":[[],[],[]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[],[]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[{},{},{}],\"additionalProperties-test-unique\":null}",
	"{\"type\":[{},{}],\"additionalProperties-test-unique\":null}",
	"{\"type\":[{}],\"additionalProperties-test-unique\":null}",
	"{\"type\":[\"\",\"\",\"\"],\"additionalProperties-test-unique\":null}",
	"{\"type\":[\"\",\"\"],\"additionalProperties-test-unique\":null}",
	"{\"type\":[\"\"],\"additionalProperties-test-unique\":null}",
	"{\"type\":{},\"additionalProperties-test-unique\":null}",
	"{\"type\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"type\":[[null]]}",
	"{\"type\":[null]}",
	"{\"type\":null}",
	"{\"type\":[[null]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[null],\"additionalProperties-test-unique\":null}",
	"{\"type\":null,\"additionalProperties-test-unique\":null}",
	"{\"type\":[[null,null]]}",
	"{\"type\":[[null],[null]]}",
	"{\"type\":[null,null]}",
	"{\"type\":[[null,null]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[null],[null]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[null],[null],[null]]}",
	"{\"type\":[[null],[null],[null]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[null,null],[null,null]]}",
	"{\"type\":[[null,null],[null,null]],\"additionalProperties-test-unique\":null}",
	"{\"type\":[[null,null],[null,null],[null,null]]}",
	"{\"type\":[[null,null],[null,null],[null,null]],\"additionalProperties-test-unique\":null}",
	NULL
};

/* FIXME: The same for the hyper-meta-schema. */
static const gchar *hyper_meta_schema_expected_failures[] = {
	"{\"fragmentResolution\":[]}",
	"{\"fragmentResolution\":{}}",
	"{\"fragmentResolution\":{\"0\":null}}",
	"{\"fragmentResolution\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":[],\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":{},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":[null]}",
	"{\"fragmentResolution\":null}",
	"{\"fragmentResolution\":[null],\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":null,\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":[null,null]}",
	"{\"fragmentResolution\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"links\":{}}",
	"{\"links\":\"\"}",
	"{\"links\":{\"0\":null}}",
	"{\"links\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"links\":{},\"additionalProperties-test-unique\":null}",
	"{\"links\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"links\":null}",
	"{\"links\":null,\"additionalProperties-test-unique\":null}",
	"{\"media\":[]}",
	"{\"media\":\"\"}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null,null],\"type\":[null,null]}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null,null],\"type\":[null,null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null,null],\"type\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null,null],\"type\":[null,null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null]}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[]}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{}}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":[],\"additionalProperties-test-unique\":null}",
	"{\"media\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[]}}",
	"{\"media\":{\"binaryEncoding\":{}}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null}}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null]}}",
	"{\"media\":{\"binaryEncoding\":null}}",
	"{\"media\":{\"binaryEncoding\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null,null]}}",
	"{\"media\":{\"binaryEncoding\":[null,null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null,null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null,null],\"type\":[null,null]}}",
	"{\"media\":{\"binaryEncoding\":[null,null],\"type\":[null,null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[null,null],\"type\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null,null],\"type\":[null,null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null]}}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null}}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[]}}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{}}}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":[null]}",
	"{\"media\":null}",
	"{\"media\":[null],\"additionalProperties-test-unique\":null}",
	"{\"media\":null,\"additionalProperties-test-unique\":null}",
	"{\"media\":[null,null]}",
	"{\"media\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[]}}",
	"{\"media\":{\"type\":{}}}",
	"{\"media\":{\"type\":{\"0\":null}}}",
	"{\"media\":{\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[null]}}",
	"{\"media\":{\"type\":null}}",
	"{\"media\":{\"type\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[null,null]}}",
	"{\"media\":{\"type\":[null,null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[null,null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[]}",
	"{\"pathStart\":{}}",
	"{\"pathStart\":{\"0\":null}}",
	"{\"pathStart\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[],\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":{},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[null]}",
	"{\"pathStart\":null}",
	"{\"pathStart\":[null],\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":null,\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[null,null]}",
	"{\"pathStart\":[null,null],\"additionalProperties-test-unique\":null}",
	NULL
};

static const gchar *hyper_meta_schema_expected_failures2[] = {
	"[]",
	"\"\"",
	"{\"additionalItems\":[]}",
	"{\"additionalItems\":\"\"}",
	"{\"additionalItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":[null]}",
	"{\"additionalItems\":null}",
	"{\"additionalItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":null,\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":[null,null]}",
	"{\"additionalItems\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[]}",
	"{\"additionalProperties\":\"\"}",
	"{\"additionalProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null]}",
	"{\"additionalProperties\":null}",
	"{\"additionalProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":null,\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null,null]}",
	"{\"additionalProperties\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[]}",
	"{\"allOf\":{}}",
	"{\"allOf\":\"\"}",
	"{\"allOf\":{\"0\":null}}",
	"{\"allOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null]}",
	"{\"allOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null,null]}",
	"{\"allOf\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[]}",
	"{\"anyOf\":{}}",
	"{\"anyOf\":\"\"}",
	"{\"anyOf\":{\"0\":null}}",
	"{\"anyOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null]}",
	"{\"anyOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null,null]}",
	"{\"anyOf\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{\"0\":{}},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":{\"0\":{}},\"patternProperties\":{\"0\":{}},\"oneOf\":[],\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":[],\"0\":null,\"anyOf\":[],\"not\":[],\"additionalItems\":{\"0\":null},\"dependencies\":{\"0\":[]},\"additionalProperties\":{\"0\":null},\"links\":[[null,null]],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{\"0\":[null,null]},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":{},\"properties\":{\"0\":[null,null]},\"patternProperties\":{\"0\":[null,null]},\"oneOf\":{},\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":{},\"anyOf\":{},\"not\":{},\"additionalItems\":[null],\"additionalProperties\":[null],\"dependencies\":{\"0\":[null,null]},\"links\":[{}]}",
	"{\"definitions\":{\"0\":[null,null]},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":{},\"properties\":{\"0\":[null,null]},\"patternProperties\":{\"0\":[null,null]},\"oneOf\":{},\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":{},\"0\":null,\"anyOf\":{},\"not\":{},\"additionalItems\":[null],\"dependencies\":{\"0\":[null,null]},\"additionalProperties\":[null],\"links\":[{}]}",
	"{\"definitions\":{\"0\":{}},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":{\"0\":{}},\"patternProperties\":{\"0\":{}},\"oneOf\":[],\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":[],\"anyOf\":[],\"not\":[],\"additionalItems\":{\"0\":null},\"additionalProperties\":{\"0\":null},\"dependencies\":{\"0\":[]},\"links\":[[null,null]],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[null],\"properties\":{},\"patternProperties\":{},\"oneOf\":[null],\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":[null],\"0\":null,\"anyOf\":[null],\"not\":[null],\"additionalItems\":true,\"dependencies\":{\"0\":\"\"},\"additionalProperties\":true,\"links\":[{\"0\":null},{\"0\":null}]}",
	"{\"definitions\":{\"0\":[null,null]},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":{},\"properties\":{\"0\":[null,null]},\"patternProperties\":{\"0\":[null,null]},\"oneOf\":{},\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":{},\"anyOf\":{},\"not\":{},\"additionalItems\":[null],\"additionalProperties\":[null],\"dependencies\":{\"0\":[null,null]},\"links\":[{}],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[null],\"properties\":{},\"patternProperties\":{},\"oneOf\":[null],\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":[null],\"anyOf\":[null],\"not\":[null],\"additionalItems\":true,\"additionalProperties\":true,\"dependencies\":{\"0\":\"\"},\"links\":[{\"0\":null},{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{\"0\":{}},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":{\"0\":{}},\"patternProperties\":{\"0\":{}},\"oneOf\":[],\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":[],\"0\":null,\"anyOf\":[],\"not\":[],\"additionalItems\":{\"0\":null},\"dependencies\":{\"0\":[]},\"additionalProperties\":{\"0\":null},\"links\":[[null,null]]}",
	"{\"definitions\":{\"0\":{}},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":{\"0\":{}},\"patternProperties\":{\"0\":{}},\"oneOf\":[],\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":[],\"anyOf\":[],\"not\":[],\"additionalItems\":{\"0\":null},\"additionalProperties\":{\"0\":null},\"dependencies\":{\"0\":[]},\"links\":[[null,null]]}",
	"{\"definitions\":{\"0\":[null,null]},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":{},\"properties\":{\"0\":[null,null]},\"patternProperties\":{\"0\":[null,null]},\"oneOf\":{},\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":{},\"0\":null,\"anyOf\":{},\"not\":{},\"additionalItems\":[null],\"dependencies\":{\"0\":[null,null]},\"additionalProperties\":[null],\"links\":[{}],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[null],\"properties\":{},\"patternProperties\":{},\"oneOf\":[null],\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":[null],\"0\":null,\"anyOf\":[null],\"not\":[null],\"additionalItems\":true,\"dependencies\":{\"0\":\"\"},\"additionalProperties\":true,\"links\":[{\"0\":null},{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":{},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[null],\"properties\":{},\"patternProperties\":{},\"oneOf\":[null],\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":[null],\"anyOf\":[null],\"not\":[null],\"additionalItems\":true,\"additionalProperties\":true,\"dependencies\":{\"0\":\"\"},\"links\":[{\"0\":null},{\"0\":null}]}",
	"{\"dependencies\":[]}",
	"{\"dependencies\":\"\"}",
	"{\"dependencies\":{\"0\":[]}}",
	"{\"dependencies\":{\"0\":\"\"}}",
	"{\"dependencies\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[null]}}",
	"{\"dependencies\":{\"0\":null}}",
	"{\"dependencies\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[null,null]}}",
	"{\"dependencies\":{\"0\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":[],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":[null]}",
	"{\"dependencies\":[null],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":[null,null]}",
	"{\"dependencies\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"items\":\"\"}",
	"{\"items\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"items\":[null]}",
	"{\"items\":[null],\"additionalProperties-test-unique\":null}",
	"{\"items\":[null,null]}",
	"{\"items\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"not\":[]}",
	"{\"not\":\"\"}",
	"{\"not\":[],\"additionalProperties-test-unique\":null}",
	"{\"not\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"not\":[null]}",
	"{\"not\":[null],\"additionalProperties-test-unique\":null}",
	"{\"not\":[null,null]}",
	"{\"not\":[null,null],\"additionalProperties-test-unique\":null}",
	"[null]",
	"[null,null]",
	"{\"oneOf\":[]}",
	"{\"oneOf\":{}}",
	"{\"oneOf\":\"\"}",
	"{\"oneOf\":{\"0\":null}}",
	"{\"oneOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[null]}",
	"{\"oneOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[null,null]}",
	"{\"oneOf\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":[]}",
	"{\"patternProperties\":\"\"}",
	"{\"patternProperties\":{\"0\":[]}}",
	"{\"patternProperties\":{\"0\":\"\"}}",
	"{\"patternProperties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[null]}}",
	"{\"patternProperties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[null,null]}}",
	"{\"patternProperties\":{\"0\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":[null]}",
	"{\"patternProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":[null,null]}",
	"{\"patternProperties\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":[]}",
	"{\"properties\":\"\"}",
	"{\"properties\":{\"0\":[]}}",
	"{\"properties\":{\"0\":\"\"}}",
	"{\"properties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[null]}}",
	"{\"properties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[null,null]}}",
	"{\"properties\":{\"0\":[null,null]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":[],\"additionalProperties-test-unique\":null}",
	"{\"properties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"properties\":[null]}",
	"{\"properties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":[null,null]}",
	"{\"properties\":[null,null],\"additionalProperties-test-unique\":null}",
	NULL
};

static gboolean
is_failure_expected (const gchar        *json,
                     gboolean            instance_is_valid,
                     WblMetaSchemaType   meta_schema_type)
{
	const gchar * const *strv;
	JsonParser *parser = NULL;
	JsonNode *actual_json_node = NULL;
	gboolean retval;
	GError *error = NULL;

	switch (meta_schema_type) {
	case WBL_META_SCHEMA_META_SCHEMA:
		if (instance_is_valid) {
			strv = meta_schema_expected_failures2;
		} else {
			strv = meta_schema_expected_failures;
		}
		break;
	case WBL_META_SCHEMA_HYPER_META_SCHEMA:
		if (instance_is_valid) {
			strv = hyper_meta_schema_expected_failures2;
		} else {
			strv = hyper_meta_schema_expected_failures;
		}
		break;
	default:
		g_assert_not_reached ();
	}

	/* See if @json is in @strv. */
	parser = json_parser_new ();

	json_parser_load_from_data (parser, json, -1, &error);
	g_assert_no_error (error);
	actual_json_node = json_node_copy (json_parser_get_root (parser));

	retval = FALSE;

	for (; *strv != NULL; strv++) {
		JsonNode *expected_node;  /* unowned */

		json_parser_load_from_data (parser, *strv, -1, &error);
		g_assert_no_error (error);
		expected_node = json_parser_get_root (parser);

		if (wbl_json_node_equal (actual_json_node, expected_node)) {
			retval = TRUE;
			break;
		}
	}

	json_node_free (actual_json_node);
	g_clear_object (&parser);

	return retval;
}

/* Test generating instances for the given JSON meta-schema.
 * http://json-schema.org/schema or http://json-schema.org/hyper-schema
 *
 * For each of those instances (which should be a valid JSON Schema), try
 * parsing the schema and generating instances (which should be valid JSON
 * instances, but not schemas) from it.
 *
 * Ideally, this will result in 100% coverage of the libwalbottle code, from
 * a few tens of lines of unit test. */
static void
test_self_hosting_meta_schema (gconstpointer user_data)
{
	WblMetaSchemaType meta_schema_type;
	WblSchema *schema = NULL;
	GPtrArray/*<owned WblGeneratedInstance>*/ *instances = NULL;
	GError *error = NULL;
	guint i;

	meta_schema_type = GPOINTER_TO_UINT (user_data);

	schema = wbl_meta_schema_load_schema (meta_schema_type, &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);

	for (i = 0; i < instances->len; i++) {
		WblSchema *child_schema = NULL;
		GPtrArray/*<owned WblGeneratedInstance>*/ *child_instances = NULL;
		const gchar *json;

		json = wbl_generated_instance_get_json (instances->pdata[i]);
		child_schema = wbl_schema_new ();
		wbl_schema_load_from_data (child_schema, json, -1, &error);

		if (wbl_generated_instance_is_valid (instances->pdata[i])) {
			if (is_failure_expected (json, TRUE, meta_schema_type)) {
				/* Expected failure. */
				if (error == NULL) {
					g_message ("Expected failure did not "
					           "happen for instance %s.",
					           json);
				}
			} else {
				if (error != NULL) {
					g_message ("JSON: %s", json);
				}

				g_assert_no_error (error);
			}

			/* Try generating child instances from the generated
			 * schema. */
			if (error == NULL) {
				child_instances = wbl_schema_generate_instances (child_schema,
				                                                 WBL_GENERATE_INSTANCE_NONE);
				g_assert_cmpuint (child_instances->len, >, 0);
				g_ptr_array_unref (child_instances);
			}
		} else {
			if (is_failure_expected (json, FALSE, meta_schema_type)) {
				/* Expected non-failure. */
				if (error != NULL) {
					g_message ("Expected non-failure did "
					           "not happen for instance "
					           "%s.",
					           json);
				}
			} else {
				if (error == NULL) {
					g_message ("JSON: %s", json);
				}

				g_assert_error (error, WBL_SCHEMA_ERROR,
				                WBL_SCHEMA_ERROR_MALFORMED);
			}
		}

		g_object_unref (child_schema);
		g_clear_error (&error);
	}

	g_ptr_array_unref (instances);
	g_object_unref (schema);
}

int
main (int argc, char *argv[])
{
#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	/* Self-hosting tests. */
	g_test_add_data_func ("/self-hosting/schema",
	                      GUINT_TO_POINTER (WBL_META_SCHEMA_META_SCHEMA),
	                      test_self_hosting_meta_schema);
	g_test_add_data_func ("/self-hosting/hyper-schema",
	                      GUINT_TO_POINTER (WBL_META_SCHEMA_HYPER_META_SCHEMA),
	                      test_self_hosting_meta_schema);

	return g_test_run ();
}
