/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2014, 2015 <philip@tecnocode.co.uk>
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

/* Utility method to assert that the generated instances in two arrays match,
 * ignoring order. */
static void
assert_generated_instances_match (GPtrArray/*<owned WblGeneratedInstance>*/ *actual,
                                  const gchar **expected  /* NULL terminated */)
{
	guint i;

	for (i = 0; i < actual->len; i++) {
		const gchar *actual_json;
		gboolean found = FALSE;
		guint j;

		actual_json = wbl_generated_instance_get_json (actual->pdata[i]);

		for (j = 0; expected[j] != NULL; j++) {
			if (g_strcmp0 (actual_json, expected[j]) == 0) {
				found = TRUE;
				break;
			}
		}

		/* Nice error message. */
		if (!found) {
			g_assert_cmpstr (actual_json, ==, "not found");
		}
		g_assert (found);
	}

	g_assert_cmpuint (actual->len, ==, g_strv_length ((gchar **) expected));
}

/* Test that construction and finalisation of a WblSchema works. */
static void
test_schema_construction (void)
{
	WblSchema *schema = NULL;  /* owned */

	schema = wbl_schema_new ();
	g_assert (wbl_schema_get_root (schema) == NULL);
	g_object_unref (schema);
}

/* Test simple parsing of a JSON Schema.
 * Example taken from http://json-schema.org/examples.html. */
static void
test_schema_parsing_simple (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"title\": \"Example Schema\","
			"\"type\": \"object\","
			"\"properties\": {"
				"\"firstName\": {"
					"\"type\": \"string\""
				"},"
				"\"lastName\": {"
					"\"type\": \"string\""
				"},"
				"\"age\": {"
					"\"description\": \"Age in years\","
					"\"type\": \"integer\","
					"\"minimum\": 0"
				"}"
			"},"
			"\"required\": [\"firstName\", \"lastName\"]"
		"}", -1, &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test complex parsing of a JSON Schema.
 * Example taken from http://json-schema.org/example1.html. */
static void
test_schema_parsing_complex (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"$schema\": \"http://json-schema.org/draft-04/schema#\","
			"\"title\": \"Product set\","
			"\"type\": \"array\","
			"\"items\": {"
				"\"title\": \"Product\","
				"\"type\": \"object\","
				"\"properties\": {"
					"\"id\": {"
						"\"description\": \"The unique identifier for a product\","
						"\"type\": \"number\""
					"},"
					"\"name\": {"
						"\"type\": \"string\""
					"},"
					"\"price\": {"
						"\"type\": \"number\","
						"\"minimum\": 0,"
						"\"exclusiveMinimum\": true"
					"},"
					"\"tags\": {"
						"\"type\": \"array\","
						"\"items\": {"
							"\"type\": \"string\""
						"},"
						"\"minItems\": 1,"
						"\"uniqueItems\": true"
					"},"
					"\"dimensions\": {"
						"\"type\": \"object\","
						"\"properties\": {"
							"\"length\": {\"type\": \"number\"},"
							"\"width\": {\"type\": \"number\"},"
							"\"height\": {\"type\": \"number\"}"
						"},"
						"\"required\": [\"length\", \"width\", \"height\"]"
					"},"
					"\"warehouseLocation\": {"
						"\"description\": \"Coordinates of the warehouse with the product\","
						"\"$ref\": \"http://json-schema.org/geo\""
					"}"
				"},"
				"\"required\": [\"id\", \"name\", \"price\"]"
			"}"
		"}", -1, &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test parsing the JSON Schema meta-schema.
 * http://json-schema.org/schema */
static void
test_schema_parsing_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test parsing the JSON Hyper Schema meta-schema.
 * http://json-schema.org/hyper-schema */
static void
test_schema_parsing_hyper_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_HYPER_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test applying a schema to an instance using items and additionalItems.
 * Taken from json-schema-validation§5.3.1.3. */
static void
test_schema_application (void)
{
	WblSchema *schema = NULL;  /* owned */
	JsonParser *parser = NULL;  /* owned */
	guint i;
	GError *error = NULL;

	const gchar *valid_cases[] = {
		"[]",
		"[ [ 1, 2, 3, 4 ], [ 5, 6, 7, 8 ] ]",
		"[ 1, 2, 3 ]",
	};

	const gchar *invalid_cases[] = {
		"[ 1, 2, 3, 4 ]",
		"[ null, { \"a\": \"b\" }, true, 31.000002020013 ]",
	};

	schema = wbl_schema_new ();
	parser = json_parser_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"items\": [ {}, {}, {} ],"
			"\"additionalItems\": false"
		"}", -1, &error);
	g_assert_no_error (error);

	/* Valid cases. */
	for (i = 0; i < G_N_ELEMENTS (valid_cases); i++) {
		json_parser_load_from_data (parser, valid_cases[i], -1, &error);
		g_assert_no_error (error);

		wbl_schema_apply (schema,
		                  json_parser_get_root (parser), &error);
		g_assert_no_error (error);
	}

	/* Invalid cases. */
	for (i = 0; i < G_N_ELEMENTS (invalid_cases); i++) {
		json_parser_load_from_data (parser,
		                            invalid_cases[i], -1, &error);
		g_assert_no_error (error);

		wbl_schema_apply (schema,
		                  json_parser_get_root (parser), &error);
		g_assert_error (error,
		                WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID);
		g_clear_error (&error);
	}

	g_object_unref (parser);
	g_object_unref (schema);
}

/* Test generating instances for a simple schema. */
static void
test_schema_instance_generation_simple (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	const gchar *expected_instances[] = {
		"{\"lastName\":\"\"}",
		"{\"lastName\":\"\",\"age\":0}",
		"{\"0\":null,\"age\":null,\"firstName\":null}",
		"{\"lastName\":null,\"age\":-1,\"firstName\":null}",
		"{\"lastName\":\"\",\"0\":null,\"firstName\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"lastName\":\"\",\"0\":null,\"age\":1,\"firstName\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"lastName\":null,\"age\":-1,\"firstName\":null,\"additionalProperties-test-unique\":null}",
		"{\"0\":null,\"firstName\":null}",
		"{\"lastName\":\"\",\"0\":null,\"firstName\":\"\"}",
		"{\"lastName\":null,\"age\":null,\"firstName\":null,\"additionalProperties-test-unique\":null}",
		"{\"firstName\":null}",
		"{\"age\":-1,\"firstName\":null}",
		"{\"0\":null,\"age\":0,\"firstName\":\"\"}",
		"{\"lastName\":\"\",\"age\":1,\"firstName\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"lastName\":null,\"0\":null,\"age\":null,\"firstName\":null,\"additionalProperties-test-unique\":null}",
		"{\"firstName\":\"\"}",
		"{\"lastName\":\"\",\"firstName\":\"\"}",
		"{\"lastName\":\"\",\"0\":null,\"age\":1}",
		"{\"lastName\":\"\",\"0\":null,\"age\":1,\"firstName\":\"\"}",
		"{\"lastName\":\"\",\"0\":null,\"age\":0,\"firstName\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"0\":null,\"age\":-1,\"firstName\":null}",
		"{\"lastName\":null}",
		"{\"lastName\":null,\"firstName\":null}",
		"{\"lastName\":\"\",\"0\":null,\"age\":0}",
		"{\"lastName\":null,\"0\":null,\"age\":null,\"firstName\":null}",
		"{\"lastName\":null,\"0\":null,\"age\":-1,\"firstName\":null,\"additionalProperties-test-unique\":null}",
		"{\"age\":1,\"firstName\":\"\"}",
		"{\"lastName\":\"\",\"firstName\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"lastName\":\"\",\"age\":1}",
		"{\"age\":null,\"firstName\":null}",
		"{\"lastName\":null,\"0\":null,\"age\":null}",
		"{\"lastName\":\"\",\"0\":null,\"age\":0,\"firstName\":\"\"}",
		"null",
		"{\"lastName\":null,\"age\":-1}",
		"{\"lastName\":\"\",\"age\":1,\"firstName\":\"\"}",
		"{\"lastName\":null,\"0\":null,\"age\":-1,\"firstName\":null}",
		"{\"lastName\":\"\",\"0\":null}",
		"{\"lastName\":null,\"0\":null,\"age\":-1}",
		"{\"lastName\":null,\"0\":null}",
		"{\"lastName\":null,\"0\":null,\"firstName\":null}",
		"{\"lastName\":\"\",\"age\":0,\"firstName\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"lastName\":null,\"firstName\":null,\"additionalProperties-test-unique\":null}",
		"{\"0\":null,\"firstName\":\"\"}",
		"{\"lastName\":\"\",\"age\":0,\"firstName\":\"\"}",
		"{\"lastName\":null,\"0\":null,\"firstName\":null,\"additionalProperties-test-unique\":null}",
		"{\"age\":0,\"firstName\":\"\"}",
		"{\"lastName\":null,\"age\":null,\"firstName\":null}",
		"{\"0\":null,\"age\":1,\"firstName\":\"\"}",
		"{}",
		"{\"lastName\":null,\"age\":null}",
		NULL,  /* terminator */
	};

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"title\": \"Example Schema\","
			"\"type\": \"object\","
			"\"properties\": {"
				"\"firstName\": {"
					"\"type\": \"string\""
				"},"
				"\"lastName\": {"
					"\"type\": \"string\""
				"},"
				"\"age\": {"
					"\"description\": \"Age in years\","
					"\"type\": \"integer\","
					"\"minimum\": 0"
				"}"
			"},"
			"\"required\": [\"firstName\", \"lastName\"]"
		"}", -1, &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	assert_generated_instances_match (instances, expected_instances);
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test generating instances for a complex schema. */
static void
test_schema_instance_generation_complex (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	const gchar *expected_instances[] = {
		"[]",
		"[{}]",
		"[{\"id\":0.10000000000000001,\"0\":null,\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"id\":0.10000000000000001,\"tags\":[\"\"],\"name\":\"\"}]",
		"[{\"id\":0.10000000000000001,\"tags\":[null],\"name\":\"\"}]",
		"[{\"id\":null,\"0\":null,\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":null,\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":null,\"name\":null}]",
		"[{\"id\":null,\"dimensions\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"name\":null}]",
		"[{\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"name\":null}]",
		"[{\"id\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"id\":null,\"tags\":[],\"name\":null}]",
		"[{\"id\":null,\"tags\":null,\"name\":null}]",
		"[{\"id\":null,\"tags\":[null,null],\"name\":null}]",
		"[null]",
		"null",
		"[{\"price\":0,\"0\":null,\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"0\":null,\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"0\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"0\":null,\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"0\":null,\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{}}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"0\":null,\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{},\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001}}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001}}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"0\":null,\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001},\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null}}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null},\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null}}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"0\":null,\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"height\":null},\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"tags\":[\"\"]}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"tags\":[\"\"],\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"tags\":[\"\"],\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0.10000000000000001,\"id\":0.10000000000000001,\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"name\":\"\"}]",
		"[{\"price\":0.10000000000000001,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0.10000000000000001,\"tags\":[\"\"],\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"length\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"length\":null,\"height\":null},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"dimensions\":{\"width\":null,\"length\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"dimensions\":{\"width\":null,\"length\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"0\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"0\":null,\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"0\":null,\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001}}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":null,\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":null,\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null}}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"tags\":[],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"length\":null,\"height\":null},\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null}}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001}}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null}}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null,null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001,\"additionalProperties-test-unique\":null},\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[\"\"],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":0.10000000000000001,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null}}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"0\":null,\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"tags\":[null],\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"dimensions\":{\"width\":null,\"length\":null},\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"name\":\"\",\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"tags\":[null]}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"tags\":[null],\"name\":\"\"}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"tags\":[null],\"name\":\"\",\"additionalProperties-test-unique\":null}]",
		"[{\"price\":0,\"id\":0.10000000000000001,\"warehouseLocation\":null}]",
		"[{\"price\":0,\"name\":\"\"}]",
		"[{\"price\":0,\"name\":\"\",\"warehouseLocation\":null}]",
		"[{\"price\":0,\"tags\":[null],\"name\":\"\"}]",
		"[{\"price\":1,\"0\":null,\"name\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"name\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"name\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"name\":null}]",
		"[{\"price\":1,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null}]",
		"[{\"price\":1,\"id\":null,\"0\":null}]",
		"[{\"price\":1,\"id\":null,\"0\":null,\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"0\":null,\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null}}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"0\":null,\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"height\":null},\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null}}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"0\":null,\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null}}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"tags\":[null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"0\":null,\"length\":null},\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null}}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null}}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"0\":null,\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null,\"additionalProperties-test-unique\":null},\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"tags\":[],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"dimensions\":{\"width\":null,\"length\":null,\"height\":null},\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"id\":null,\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"tags\":[null,null]}]",
		"[{\"price\":1,\"id\":null,\"tags\":[null,null],\"name\":null}]",
		"[{\"price\":1,\"id\":null,\"tags\":[null,null],\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":1,\"id\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"name\":null}]",
		"[{\"price\":1,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":1,\"tags\":[null,null],\"name\":null}]",
		"[{\"price\":null,\"0\":null,\"name\":null}]",
		"[{\"price\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"name\":null}]",
		"[{\"price\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":null,\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":null,\"name\":null}]",
		"[{\"price\":null,\"dimensions\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null}]",
		"[{\"price\":null,\"id\":null,\"0\":null}]",
		"[{\"price\":null,\"id\":null,\"0\":null,\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"0\":null,\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001}}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null}}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":null,\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"0\":null,\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"tags\":null,\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"0\":null,\"length\":null,\"height\":null},\"tags\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"0\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"0\":null,\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"tags\":[\"\"],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":null,\"tags\":[\"\"],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001}}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"0\":null,\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"tags\":[],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"height\":0.10000000000000001},\"tags\":[],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001}}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"0\":null,\"tags\":[null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"0\":null,\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001}}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"0\":null,\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[null,null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001,\"height\":0.10000000000000001},\"tags\":[null,null],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"tags\":[null],\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"tags\":[null],\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"dimensions\":{\"width\":0.10000000000000001,\"0\":null,\"length\":0.10000000000000001},\"tags\":[null],\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"id\":null,\"name\":null,\"warehouseLocation\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"tags\":[]}]",
		"[{\"price\":null,\"id\":null,\"tags\":[],\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"tags\":[],\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"tags\":null}]",
		"[{\"price\":null,\"id\":null,\"tags\":null,\"name\":null}]",
		"[{\"price\":null,\"id\":null,\"tags\":null,\"name\":null,\"additionalProperties-test-unique\":null}]",
		"[{\"price\":null,\"id\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"name\":null}]",
		"[{\"price\":null,\"name\":null,\"warehouseLocation\":null}]",
		"[{\"price\":null,\"tags\":[],\"name\":null}]",
		"[{\"price\":null,\"tags\":null,\"name\":null}]",
		NULL,  /* terminator */
	};

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"$schema\": \"http://json-schema.org/draft-04/schema#\","
			"\"title\": \"Product set\","
			"\"type\": \"array\","
			"\"items\": {"
				"\"title\": \"Product\","
				"\"type\": \"object\","
				"\"properties\": {"
					"\"id\": {"
						"\"description\": \"The unique identifier for a product\","
						"\"type\": \"number\""
					"},"
					"\"name\": {"
						"\"type\": \"string\""
					"},"
					"\"price\": {"
						"\"type\": \"number\","
						"\"minimum\": 0,"
						"\"exclusiveMinimum\": true"
					"},"
					"\"tags\": {"
						"\"type\": \"array\","
						"\"items\": {"
							"\"type\": \"string\""
						"},"
						"\"minItems\": 1,"
						"\"uniqueItems\": true"
					"},"
					"\"dimensions\": {"
						"\"type\": \"object\","
						"\"properties\": {"
							"\"length\": {\"type\": \"number\"},"
							"\"width\": {\"type\": \"number\"},"
							"\"height\": {\"type\": \"number\"}"
						"},"
						"\"required\": [\"length\", \"width\", \"height\"]"
					"},"
					"\"warehouseLocation\": {"
						"\"description\": \"Coordinates of the warehouse with the product\","
						"\"$ref\": \"http://json-schema.org/geo\""
					"}"
				"},"
				"\"required\": [\"id\", \"name\", \"price\"]"
			"}"
		"}", -1, &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	assert_generated_instances_match (instances, expected_instances);
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test generating instances for the JSON Schema meta-schema.
 * http://json-schema.org/schema */
static void
test_schema_instance_generation_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	/* FIXME: This needs a lot of mollycoddling to get it doing the right
	 * thing. */
	const gchar *expected_instances[] = {
		"{}",
		"{\"0\":null}",
		"{\"0\":null,\"additionalProperties-test-unique\":null}",
		"{\"additionalItems\":{}}",
		"{\"additionalItems\":{},\"additionalProperties-test-unique\":null}",
		"{\"additionalItems\":null}",
		"{\"additionalItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"additionalProperties\":{}}",
		"{\"additionalProperties\":{},\"additionalProperties-test-unique\":null}",
		"{\"additionalProperties\":null}",
		"{\"additionalProperties\":null,\"additionalProperties-test-unique\":null}",
		"{\"additionalProperties-test-unique\":null}",
		"{\"allOf\":null}",
		"{\"allOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"anyOf\":null}",
		"{\"anyOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"default\":null}",
		"{\"default\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{}}",
		"{\"definitions\":{\"0\":null}}",
		"{\"definitions\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"additionalProperties-test-unique\":null}",
		"{\"definitions\":null}",
		"{\"definitions\":null,\"additionalProperties-test-unique\":null}",
		"{\"dependencies\":{}}",
		"{\"dependencies\":{\"0\":null}}",
		"{\"dependencies\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"dependencies\":{},\"additionalProperties-test-unique\":null}",
		"{\"dependencies\":null}",
		"{\"dependencies\":null,\"additionalProperties-test-unique\":null}",
		"{\"description\":\"\"}",
		"{\"description\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"description\":null}",
		"{\"description\":null,\"additionalProperties-test-unique\":null}",
		"{\"enum\":[]}",
		"{\"enum\":[],\"additionalProperties-test-unique\":null}",
		"{\"enum\":[null]}",
		"{\"enum\":null}",
		"{\"enum\":[null],\"additionalProperties-test-unique\":null}",
		"{\"enum\":null,\"additionalProperties-test-unique\":null}",
		"{\"enum\":[null,null]}",
		"{\"enum\":[null,null],\"additionalProperties-test-unique\":null}",
		"{\"exclusiveMaximum\":false}",
		"{\"exclusiveMaximum\":null}",
		"{\"exclusiveMaximum\":true}",
		"{\"exclusiveMinimum\":false}",
		"{\"exclusiveMinimum\":false,\"minimum\":null}",
		"{\"exclusiveMinimum\":false,\"minimum\":null,\"additionalProperties-test-unique\":null}",
		"{\"exclusiveMinimum\":null}",
		"{\"exclusiveMinimum\":null,\"minimum\":0.10000000000000001}",
		"{\"exclusiveMinimum\":null,\"minimum\":0.10000000000000001,\"additionalProperties-test-unique\":null}",
		"{\"exclusiveMinimum\":true}",
		"{\"exclusiveMinimum\":true,\"minimum\":null}",
		"{\"exclusiveMinimum\":true,\"minimum\":null,\"additionalProperties-test-unique\":null}",
		"{\"id\":\"\"}",
		"{\"id\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"id\":null}",
		"{\"id\":null,\"additionalProperties-test-unique\":null}",
		"{\"items\":{}}",
		"{\"items\":{},\"additionalProperties-test-unique\":null}",
		"{\"maximum\":0.10000000000000001}",
		"{\"maximum\":0.10000000000000001,\"additionalProperties-test-unique\":null}",
		"{\"maximum\":0.10000000000000001,\"exclusiveMaximum\":null}",
		"{\"maximum\":0.10000000000000001,\"exclusiveMaximum\":null,\"additionalProperties-test-unique\":null}",
		"{\"maximum\":null}",
		"{\"maximum\":null,\"additionalProperties-test-unique\":null}",
		"{\"maximum\":null,\"exclusiveMaximum\":false}",
		"{\"maximum\":null,\"exclusiveMaximum\":false,\"additionalProperties-test-unique\":null}",
		"{\"maximum\":null,\"exclusiveMaximum\":true}",
		"{\"maximum\":null,\"exclusiveMaximum\":true,\"additionalProperties-test-unique\":null}",
		"{\"maxItems\":null}",
		"{\"maxItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"maxLength\":null}",
		"{\"maxLength\":null,\"additionalProperties-test-unique\":null}",
		"{\"maxProperties\":null}",
		"{\"maxProperties\":null,\"additionalProperties-test-unique\":null}",
		"{\"minimum\":0.10000000000000001}",
		"{\"minimum\":0.10000000000000001,\"additionalProperties-test-unique\":null}",
		"{\"minimum\":null}",
		"{\"minimum\":null,\"additionalProperties-test-unique\":null}",
		"{\"minItems\":null}",
		"{\"minItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null}",
		"{\"minLength\":null,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false}",
		"{\"minLength\":null,\"patternProperties\":{\"0\":null},\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":false,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[null,null],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":{\"0\":null},\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":1,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":null,\"definitions\":{\"0\":null},\"minItems\":null,\"$schema\":null,\"type\":null,\"exclusiveMaximum\":false,\"properties\":{\"0\":null},\"required\":null,\"uniqueItems\":false}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":0.10000000000000001,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":0.10000000000000001,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":0.10000000000000001,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":0.10000000000000001,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":0.10000000000000001,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":null,\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":null,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0.10000000000000001,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":0.10000000000000001,\"definitions\":null,\"minItems\":null,\"$schema\":\"\",\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maximum\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":null,\"oneOf\":null,\"additionalItems\":null,\"exclusiveMinimum\":true,\"allOf\":null,\"default\":null,\"description\":null,\"enum\":[],\"not\":null,\"items\":{},\"pattern\":null,\"maxLength\":null,\"id\":null,\"dependencies\":null,\"maxItems\":null,\"additionalProperties\":null,\"multipleOf\":null,\"title\":null,\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":null,\"definitions\":null,\"minItems\":null,\"$schema\":null,\"type\":[],\"exclusiveMaximum\":true,\"properties\":null,\"required\":null,\"uniqueItems\":true}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":0.10000000000000001,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":0.10000000000000001,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":0.10000000000000001,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maximum\":0.10000000000000001,\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":0.10000000000000001,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"0\":null,\"minimum\":0.10000000000000001,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null}",
		"{\"minLength\":null,\"patternProperties\":{},\"oneOf\":null,\"additionalItems\":{},\"exclusiveMinimum\":null,\"allOf\":null,\"default\":null,\"description\":\"\",\"enum\":[null],\"not\":null,\"items\":{},\"pattern\":\"\",\"maxLength\":null,\"id\":\"\",\"dependencies\":{},\"maxItems\":null,\"additionalProperties\":{},\"multipleOf\":0,\"title\":\"\",\"anyOf\":null,\"minProperties\":null,\"maxProperties\":null,\"minimum\":0.10000000000000001,\"definitions\":{},\"minItems\":null,\"$schema\":\"\",\"type\":[null,null],\"exclusiveMaximum\":null,\"properties\":{},\"required\":null,\"uniqueItems\":null}",
		"{\"minProperties\":null}",
		"{\"minProperties\":null,\"additionalProperties-test-unique\":null}",
		"{\"multipleOf\":0}",
		"{\"multipleOf\":0.10000000000000001}",
		"{\"multipleOf\":0.10000000000000001,\"additionalProperties-test-unique\":null}",
		"{\"multipleOf\":0,\"additionalProperties-test-unique\":null}",
		"{\"multipleOf\":1}",
		"{\"multipleOf\":1,\"additionalProperties-test-unique\":null}",
		"{\"multipleOf\":null}",
		"{\"multipleOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"not\":null}",
		"{\"not\":null,\"additionalProperties-test-unique\":null}",
		"null",
		"{\"oneOf\":null}",
		"{\"oneOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"pattern\":\"\"}",
		"{\"pattern\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"pattern\":null}",
		"{\"pattern\":null,\"additionalProperties-test-unique\":null}",
		"{\"patternProperties\":{}}",
		"{\"patternProperties\":{\"0\":null}}",
		"{\"patternProperties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"patternProperties\":{},\"additionalProperties-test-unique\":null}",
		"{\"patternProperties\":null}",
		"{\"patternProperties\":null,\"additionalProperties-test-unique\":null}",
		"{\"properties\":{}}",
		"{\"properties\":{\"0\":null}}",
		"{\"properties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"properties\":{},\"additionalProperties-test-unique\":null}",
		"{\"properties\":null}",
		"{\"properties\":null,\"additionalProperties-test-unique\":null}",
		"{\"required\":null}",
		"{\"required\":null,\"additionalProperties-test-unique\":null}",
		"{\"$schema\":\"\"}",
		"{\"$schema\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"$schema\":null}",
		"{\"$schema\":null,\"additionalProperties-test-unique\":null}",
		"{\"title\":\"\"}",
		"{\"title\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"title\":null}",
		"{\"title\":null,\"additionalProperties-test-unique\":null}",
		"{\"type\":[]}",
		"{\"type\":[],\"additionalProperties-test-unique\":null}",
		"{\"type\":null}",
		"{\"type\":null,\"additionalProperties-test-unique\":null}",
		"{\"type\":[null,null]}",
		"{\"type\":[null,null],\"additionalProperties-test-unique\":null}",
		"{\"uniqueItems\":false}",
		"{\"uniqueItems\":false,\"additionalProperties-test-unique\":null}",
		"{\"uniqueItems\":null}",
		"{\"uniqueItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"uniqueItems\":true}",
		"{\"uniqueItems\":true,\"additionalProperties-test-unique\":null}",
		NULL,  /* terminator */
	};

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	assert_generated_instances_match (instances, expected_instances);
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test generating instances for the JSON Hyper Schema meta-schema.
 * http://json-schema.org/hyper-schema */
static void
test_schema_instance_generation_hyper_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	/* FIXME: This needs a lot of mollycoddling to get it doing the right
	 * thing. */
	const gchar *expected_instances[] = {
		"{}",
		"{\"0\":null}",
		"{\"0\":null,\"additionalProperties-test-unique\":null}",
		"{\"additionalItems\":null}",
		"{\"additionalItems\":null,\"additionalProperties-test-unique\":null}",
		"{\"additionalProperties\":null}",
		"{\"additionalProperties\":null,\"additionalProperties-test-unique\":null}",
		"{\"additionalProperties-test-unique\":null}",
		"{\"allOf\":null}",
		"{\"allOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"anyOf\":null}",
		"{\"anyOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{}}",
		"{\"definitions\":{\"0\":null}}",
		"{\"definitions\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":null,\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":null,\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":null,\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":null,\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"type\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"type\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{\"0\":null},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"type\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null]}",
		"{\"definitions\":{\"0\":null},\"fragmentResolution\":\"\",\"pathStart\":\"\",\"items\":null,\"properties\":{\"0\":null},\"patternProperties\":{\"0\":null},\"oneOf\":null,\"media\":{\"type\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{\"0\":null},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[null]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[null]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"binaryEncoding\":\"\",\"type\":\"\"},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[null]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[null]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":\"\",\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null},\"allOf\":null,\"0\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"dependencies\":{},\"additionalProperties\":null,\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[]}",
		"{\"definitions\":{},\"fragmentResolution\":null,\"pathStart\":null,\"items\":null,\"properties\":{},\"patternProperties\":{},\"oneOf\":null,\"media\":{\"type\":null},\"allOf\":null,\"anyOf\":null,\"not\":null,\"additionalItems\":null,\"additionalProperties\":null,\"dependencies\":{},\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"dependencies\":{}}",
		"{\"dependencies\":{\"0\":null}}",
		"{\"dependencies\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"dependencies\":{},\"additionalProperties-test-unique\":null}",
		"{\"fragmentResolution\":\"\"}",
		"{\"fragmentResolution\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"fragmentResolution\":null}",
		"{\"fragmentResolution\":null,\"additionalProperties-test-unique\":null}",
		"{\"items\":null}",
		"{\"items\":null,\"additionalProperties-test-unique\":null}",
		"{\"links\":[]}",
		"{\"links\":[],\"additionalProperties-test-unique\":null}",
		"{\"links\":[null]}",
		"{\"links\":null}",
		"{\"links\":[null],\"additionalProperties-test-unique\":null}",
		"{\"links\":null,\"additionalProperties-test-unique\":null}",
		"{\"media\":{}}",
		"{\"media\":{\"0\":null}}",
		"{\"media\":{\"0\":null,\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"0\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null}}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\"}}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\"},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"0\":null,\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":\"\"}}",
		"{\"media\":{\"binaryEncoding\":\"\",\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"binaryEncoding\":\"\"},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":\"\",\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":null}}",
		"{\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"binaryEncoding\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":null,\"type\":null}}",
		"{\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"binaryEncoding\":null,\"type\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":null,\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":\"\",\"type\":\"\"}}",
		"{\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"binaryEncoding\":\"\",\"type\":\"\"},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"binaryEncoding\":\"\",\"type\":\"\",\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":null}",
		"{\"media\":null,\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"type\":\"\"}}",
		"{\"media\":{\"type\":\"\",\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"type\":\"\"},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"type\":\"\",\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"type\":null}}",
		"{\"media\":{\"type\":null,\"additionalProperties-test-unique\":null}}",
		"{\"media\":{\"type\":null},\"additionalProperties-test-unique\":null}",
		"{\"media\":{\"type\":null,\"additionalProperties-test-unique\":null},\"additionalProperties-test-unique\":null}",
		"{\"not\":null}",
		"{\"not\":null,\"additionalProperties-test-unique\":null}",
		"{\"oneOf\":null}",
		"{\"oneOf\":null,\"additionalProperties-test-unique\":null}",
		"{\"pathStart\":\"\"}",
		"{\"pathStart\":\"\",\"additionalProperties-test-unique\":null}",
		"{\"pathStart\":null}",
		"{\"pathStart\":null,\"additionalProperties-test-unique\":null}",
		"{\"patternProperties\":{}}",
		"{\"patternProperties\":{\"0\":null}}",
		"{\"patternProperties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"patternProperties\":{},\"additionalProperties-test-unique\":null}",
		"{\"properties\":{}}",
		"{\"properties\":{\"0\":null}}",
		"{\"properties\":{\"0\":null},\"additionalProperties-test-unique\":null}",
		"{\"properties\":{},\"additionalProperties-test-unique\":null}",
		NULL,  /* terminator */
	};

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_HYPER_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	assert_generated_instances_match (instances, expected_instances);
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test reference counting of #WblSchemaNode. */
static void
test_schema_node_refs (void)
{
	WblSchema *schema = NULL;  /* owned */
	WblSchemaNode *node = NULL;  /* owned */
	JsonObject *json_object;  /* unowned */
	GError *error = NULL;

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	/* Grab the root schema node. */
	node = wbl_schema_get_root (schema);
	wbl_schema_node_ref (node);

	g_object_unref (schema);

	/* Check the node; the #WblSchemaNode should still be alive. */
	json_object = wbl_schema_node_get_root (node);
	g_assert (json_object_has_member (json_object, "properties"));

	wbl_schema_node_unref (node);
}

/* Test the title property of #WblSchemaNode. */
static void
test_schema_node_title (void)
{
	guint i;
	const struct {
		const gchar *expected_title;
		const gchar *schema_json;
	} vectors[] = {
		{ NULL, "{}" },
		{ "Title", "{ \"title\" : \"Title\" }" },
		{ "\"Title\"", "{ \"title\" : \"\\\"Title\\\"\" }" },
	};

	for (i = 0; i < G_N_ELEMENTS (vectors); i++) {
		WblSchema *schema = NULL;  /* owned */
		WblSchemaNode *node;  /* unowned */
		GError *error = NULL;

		schema = wbl_schema_new ();

		wbl_schema_load_from_data (schema,
		                           vectors[i].schema_json, -1, &error);
		g_assert_no_error (error);

		/* Grab the root schema node. */
		node = wbl_schema_get_root (schema);

		/* Check the title. */
		g_assert_cmpstr (wbl_schema_node_get_title (node), ==,
		                 vectors[i].expected_title);

		g_object_unref (schema);
	}
}

/* Test the description property of #WblSchemaNode. */
static void
test_schema_node_description (void)
{
	guint i;
	const struct {
		const gchar *expected_description;
		const gchar *schema_json;
	} vectors[] = {
		{ NULL, "{}" },
		{ "Description!", "{ \"description\" : \"Description!\" }" },
		{ "\"Description\"", "{ \"description\" : \"\\\"Description\\\"\" }" },
	};

	for (i = 0; i < G_N_ELEMENTS (vectors); i++) {
		WblSchema *schema = NULL;  /* owned */
		WblSchemaNode *node;  /* unowned */
		GError *error = NULL;

		schema = wbl_schema_new ();

		wbl_schema_load_from_data (schema,
		                           vectors[i].schema_json, -1, &error);
		g_assert_no_error (error);

		/* Grab the root schema node. */
		node = wbl_schema_get_root (schema);

		/* Check the description. */
		g_assert_cmpstr (wbl_schema_node_get_description (node), ==,
		                 vectors[i].expected_description);

		g_object_unref (schema);
	}
}

/* Test the default property of #WblSchemaNode. */
static void
test_schema_node_default (void)
{
	guint i;
	const struct {
		const gchar *expected_default_json;
		const gchar *schema_json;
	} vectors[] = {
		{ NULL, "{}" },
		{ "\"Default\"", "{ \"default\" : \"Default\" }" },
		{ "12", "{ \"default\" : 12 }" },
		{ "null", "{ \"default\": null }" },
	};

	for (i = 0; i < G_N_ELEMENTS (vectors); i++) {
		WblSchema *schema = NULL;  /* owned */
		WblSchemaNode *node;  /* unowned */
		JsonNode *default_node;  /* unowned */
		JsonGenerator *generator = NULL;  /* owned */
		gchar *json = NULL;  /* owned */
		GError *error = NULL;

		schema = wbl_schema_new ();

		wbl_schema_load_from_data (schema,
		                           vectors[i].schema_json, -1, &error);
		g_assert_no_error (error);

		/* Grab the root schema node. */
		node = wbl_schema_get_root (schema);

		/* Check the default. */
		default_node = wbl_schema_node_get_default (node);

		generator = json_generator_new ();
		json_generator_set_root (generator, default_node);
		json = json_generator_to_data (generator, NULL);
		g_assert_cmpstr (json, ==, vectors[i].expected_default_json);
		g_free (json);
		g_object_unref (generator);

		g_object_unref (schema);
	}
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

	/* Schema tests. */
	g_test_add_func ("/schema/construction", test_schema_construction);
	g_test_add_func ("/schema/parsing/simple", test_schema_parsing_simple);
	g_test_add_func ("/schema/parsing/complex", test_schema_parsing_complex);
	g_test_add_func ("/schema/parsing/schema", test_schema_parsing_schema);
	g_test_add_func ("/schema/parsing/hyper-schema",
	                 test_schema_parsing_hyper_schema);
	g_test_add_func ("/schema/application", test_schema_application);
	g_test_add_func ("/schema/instance-generation/simple",
	                 test_schema_instance_generation_simple);
	g_test_add_func ("/schema/instance-generation/complex",
	                 test_schema_instance_generation_complex);
	g_test_add_func ("/schema/instance-generation/schema",
	                 test_schema_instance_generation_schema);
	g_test_add_func ("/schema/instance-generation/hyper-schema",
	                 test_schema_instance_generation_hyper_schema);
	g_test_add_func ("/schema/node/refs", test_schema_node_refs);
	g_test_add_func ("/schema/node/title", test_schema_node_title);
	g_test_add_func ("/schema/node/description",
	                 test_schema_node_description);
	g_test_add_func ("/schema/node/default", test_schema_node_default);

	return g_test_run ();
}
