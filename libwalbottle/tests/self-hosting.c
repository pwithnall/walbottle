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
	"{\"$schema\":{\"0\":null}}",
	"{\"definitions\":[null],\"additionalProperties-test-unique\":null}",
	"{\"id\":{},\"additionalProperties-test-unique\":null}",
	"{\"definitions\":null}",
	"{\"definitions\":[],\"additionalProperties-test-unique\":null}",
	"{\"id\":null,\"additionalProperties-test-unique\":null}",
	"{\"$schema\":null,\"additionalProperties-test-unique\":null}",
	"{\"$schema\":{},\"additionalProperties-test-unique\":null}",
	"{\"definitions\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"id\":{\"0\":null}}",
	"{\"id\":[null],\"additionalProperties-test-unique\":null}",
	"{\"$schema\":null}",
	"{\"$schema\":{}}",
	"{\"id\":{}}",
	"{\"id\":[null]}",
	"{\"id\":[],\"additionalProperties-test-unique\":null}",
	"{\"id\":null}",
	"{\"$schema\":[null]}",
	"{\"$schema\":[null],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":null,\"additionalProperties-test-unique\":null}",
	"{\"id\":[]}",
	"{\"definitions\":[null]}",
	"{\"definitions\":[]}",
	"{\"definitions\":\"\"}",
	"{\"$schema\":[]}",
	"{\"$schema\":[],\"additionalProperties-test-unique\":null}",
	"{\"id\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"$schema\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	NULL
};

static const gchar *meta_schema_expected_failures2[] = {
	"{\"maxItems\":[]}",
	"{\"required\":{\"0\":null}}",
	"{\"maxItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"items\":[null]}",
	"{\"required\":[null]}",
	"{\"required\":{}}",
	"{\"allOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[]}",
	"{\"type\":[null]}",
	"{\"required\":[],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[null]}",
	"{\"oneOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[]}",
	"{\"additionalProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"type\":{}}",
	"{\"maxItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[]}}",
	"{\"maxProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":\"\"}",
	"{\"allOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":\"\"}",
	"{\"items\":\"\"}",
	"{\"type\":[null],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":\"\"}}",
	"{\"patternProperties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"type\":null}",
	"{\"minLength\":{},\"additionalProperties-test-unique\":null}",
	"{\"minItems\":{\"0\":null}}",
	"{\"type\":null,\"additionalProperties-test-unique\":null}",
	"{\"not\":[null]}",
	"{\"allOf\":[]}",
	"{\"maxProperties\":[]}",
	"{\"minItems\":\"\"}",
	"{\"not\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[]}",
	"{\"maxLength\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":{\"0\":null}}",
	"{\"allOf\":\"\"}",
	"{\"minLength\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[]}",
	"{\"type\":\"\"}",
	"{\"additionalProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{\"0\":null}}",
	"{\"additionalItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[null]}",
	"{\"additionalProperties\":null,\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":\"\"}",
	"{\"maxProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[null]}",
	"{\"required\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":[null]}",
	"{\"minItems\":{},\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":\"\"}",
	"{\"not\":[],\"additionalProperties-test-unique\":null}",
	"{\"minItems\":[]}",
	"{\"properties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"minLength\":{\"0\":null}}",
	"{\"minLength\":[null],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":\"\"}",
	"{\"minLength\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[null]}}",
	"{\"oneOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"not\":\"\"}",
	"{\"additionalItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":{\"0\":null}}",
	"{\"type\":[null,null]}",
	"{\"type\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"minLength\":{}}",
	"{\"properties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"minItems\":{}}",
	"{\"oneOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":\"\"}",
	"{\"maxLength\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[null]}",
	"{\"type\":{\"0\":null}}",
	"{\"additionalItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":[null]}",
	"{\"additionalProperties\":null}",
	"{\"additionalProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":\"\"}",
	"{\"minItems\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[]}",
	"{\"additionalProperties\":\"\"}",
	"{\"required\":[]}",
	"{\"anyOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":{},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[null]}}",
	"{\"type\":[null,null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{\"0\":null}}",
	"{\"additionalItems\":[]}",
	"{\"patternProperties\":{\"0\":[]}}",
	"{\"minLength\":\"\"}",
	"{\"type\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":[null],\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[null]}",
	"{\"required\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{}}",
	"{\"patternProperties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"minItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":{}}",
	"{\"maxLength\":[],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[]}}",
	"{\"oneOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null]}",
	"{\"anyOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"required\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[]}",
	"{\"dependencies\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"not\":[null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":{}}",
	"{\"maxProperties\":{\"0\":null}}",
	"{\"oneOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":{},\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":\"\"}}",
	"{\"required\":{},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"maxItems\":{}}",
	"{\"maxItems\":{\"0\":null}}",
	"{\"maxProperties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"maxProperties\":[null]}",
	"{\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"minLength\":[],\"additionalProperties-test-unique\":null}",
	"{\"minProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":\"\"}}",
	"{\"minProperties\":{},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null]}",
	"{\"minProperties\":{}}",
	"{\"minProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"items\":[null],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{}}",
	"{\"items\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":null}",
	"{\"required\":\"\"}",
	"{\"minProperties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"maxLength\":{}}",
	"{\"dependencies\":{\"0\":[null]}}",
	"{\"additionalItems\":null,\"additionalProperties-test-unique\":null}",
	"{\"not\":[]}",
	"{\"anyOf\":{\"0\":null}}",
	"{\"patternProperties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null]}",
	NULL
};

/* FIXME: The same for the hyper-meta-schema. */
static const gchar *hyper_meta_schema_expected_failures[] = {
	"{\"links\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"media\":[null],\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null}}",
	"{\"pathStart\":{},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null}}",
	"{\"media\":{\"binaryEncoding\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{\"0\":null}}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":[],\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null]}}",
	"{\"media\":{\"type\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"pathStart\":null,\"additionalProperties-test-unique\":null}",
	"{\"media\":[]}",
	"{\"media\":{\"type\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":[null],\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[]},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null]}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":null,\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{}}}",
	"{\"links\":{\"0\":null}}",
	"{\"media\":{\"type\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{}}}",
	"{\"media\":{\"type\":{}}}",
	"{\"fragmentResolution\":null,\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null}}",
	"{\"media\":{\"type\":[null]}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{}}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":{}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[]}}",
	"{\"media\":{\"binaryEncoding\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":[null]}",
	"{\"links\":{}}",
	"{\"media\":{\"type\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"links\":null,\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"links\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":{}}",
	"{\"pathStart\":[null]}",
	"{\"media\":{\"type\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"links\":\"\"}",
	"{\"media\":{\"binaryEncoding\":{}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[]}}",
	"{\"links\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"type\":null},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[],\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":[],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"type\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}}}",
	"{\"media\":{\"type\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"fragmentResolution\":{},\"additionalProperties-test-unique\":null}",
	"{\"links\":{},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[]}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}}}",
	"{\"fragmentResolution\":[null]}",
	"{\"media\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"type\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null]}}",
	"{\"fragmentResolution\":null}",
	"{\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":{},\"type\":{},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":\"\"}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[null],\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":null}",
	"{\"fragmentResolution\":[]}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[null],\"type\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null},\"type\":{\"0\":null}},\"additionalProperties-test-unique\":null}",
	"{\"pathStart\":[]}",
	"{\"media\":{\"binaryEncoding\":[],\"type\":[]}}",
	"{\"media\":{\"binaryEncoding\":null}}",
	"{\"media\":[],\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"media\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"type\":null},\"additionalProperties-test-unique\":null}",
	"{\"media\":{\"binaryEncoding\":[null],\"additionalProperties-test-unique\":null}}",
	"{\"media\":{\"binaryEncoding\":{\"0\":null}}}",
	"{\"media\":{\"binaryEncoding\":{},\"type\":{}},\"additionalProperties-test-unique\":null}",
	"{\"fragmentResolution\":{\"0\":null}}",
	"{\"media\":{\"0\":null,\"binaryEncoding\":[],\"type\":[],\"additionalProperties-test-unique\":null}}",
	"{\"pathStart\":{\"0\":null}}",
	NULL
};

static const gchar *hyper_meta_schema_expected_failures2[] = {
	"{\"dependencies\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":[],\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":\"\"}}",
	"{\"oneOf\":[null]}",
	"{\"items\":\"\"}",
	"{\"dependencies\":{\"0\":[null]}}",
	"{\"allOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"items\":[null]}",
	"{\"not\":[null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":[]}",
	"{\"patternProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[]}}",
	"{\"not\":\"\"}",
	"{\"dependencies\":\"\"}",
	"{\"dependencies\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":\"\"}",
	"{\"properties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":\"\"}",
	"[null]",
	"{\"definitions\":[],\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":[],\"patternProperties\":[],\"oneOf\":[],\"media\":{\"0\":null},\"allOf\":[],\"anyOf\":[],\"not\":[],\"additionalItems\":\"\",\"additionalProperties\":\"\",\"dependencies\":{\"0\":\"\"},\"links\":[{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[]}}",
	"{\"properties\":{\"0\":[null]}}",
	"{\"patternProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{\"0\":null}}",
	"{\"patternProperties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null]}",
	"{\"additionalProperties\":[],\"additionalProperties-test-unique\":null}",
	"{\"definitions\":[],\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":[],\"patternProperties\":[],\"oneOf\":[],\"media\":{\"0\":null},\"allOf\":[],\"anyOf\":[],\"not\":[],\"additionalItems\":\"\",\"additionalProperties\":\"\",\"dependencies\":{\"0\":\"\"},\"links\":[{\"0\":null}]}",
	"\"\"",
	"{\"oneOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"not\":[],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":\"\"}}",
	"{\"properties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":[],\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[]}",
	"{\"dependencies\":[null]}",
	"{\"dependencies\":{\"0\":null}}",
	"{\"dependencies\":[]}",
	"{\"oneOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"definitions\":[],\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":[],\"patternProperties\":[],\"oneOf\":[],\"media\":{\"0\":null},\"allOf\":[],\"0\":null,\"anyOf\":[],\"not\":[],\"additionalItems\":\"\",\"dependencies\":{\"0\":\"\"},\"additionalProperties\":\"\",\"links\":[{\"0\":null}],\"additionalProperties-test-unique\":null}",
	"{\"patternProperties\":\"\"}",
	"{\"anyOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"items\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"dependencies\":{\"0\":[null]},\"additionalProperties-test-unique\":null}",
	"{\"properties\":[],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{}}",
	"{\"items\":[null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":\"\"}",
	"{\"anyOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":null,\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":null,\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[]}",
	"{\"patternProperties\":{\"0\":[null]}}",
	"{\"properties\":{\"0\":[]},\"additionalProperties-test-unique\":null}",
	"[]",
	"{\"patternProperties\":[]}",
	"{\"patternProperties\":[null]}",
	"{\"allOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"not\":[]}",
	"{\"properties\":[null]}",
	"{\"dependencies\":{\"0\":\"\"}}",
	"{\"additionalItems\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":{}}",
	"{\"anyOf\":\"\"}",
	"{\"allOf\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"additionalItems\":null}",
	"{\"oneOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":[null],\"additionalProperties-test-unique\":null}",
	"{\"not\":[null]}",
	"{\"oneOf\":\"\"}",
	"{\"patternProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"definitions\":[],\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":[],\"properties\":[],\"patternProperties\":[],\"oneOf\":[],\"media\":{\"0\":null},\"allOf\":[],\"0\":null,\"anyOf\":[],\"not\":[],\"additionalItems\":\"\",\"dependencies\":{\"0\":\"\"},\"additionalProperties\":\"\",\"links\":[{\"0\":null}]}",
	"{\"anyOf\":{\"0\":null}}",
	"{\"anyOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":{}}",
	"{\"anyOf\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[null]}",
	"{\"anyOf\":{},\"additionalProperties-test-unique\":null}",
	"{\"anyOf\":[null]}",
	"{\"dependencies\":{\"0\":\"\"},\"additionalProperties-test-unique\":null}",
	"{\"allOf\":\"\"}",
	"{\"not\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":null}",
	"{\"additionalItems\":[null],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":{\"0\":null}}",
	"{\"additionalItems\":[]}",
	"{\"additionalItems\":[null]}",
	"{\"allOf\":[]}",
	"{\"additionalProperties\":\"\",\"additionalProperties-test-unique\":null}",
	"{\"additionalProperties\":[]}",
	"{\"dependencies\":{\"0\":null},\"additionalProperties-test-unique\":null}",
	"{\"oneOf\":[],\"additionalProperties-test-unique\":null}",
	"{\"allOf\":[null],\"additionalProperties-test-unique\":null}",
	"{\"properties\":{\"0\":[]}}",
	NULL
};

static gboolean
is_failure_expected (const gchar        *json,
                     gboolean            instance_is_valid,
                     WblMetaSchemaType   meta_schema_type)
{
	const gchar * const *strv;

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

	return g_strv_contains (strv, json);
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
g_message ("instances: %u", instances->len);
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
