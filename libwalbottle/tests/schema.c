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
		"{\"firstName\":null}",
		"{\"lastName\":null}",
		"{\"firstName\":null,\"lastName\":null}",
		"{\"firstName\":''}",
		"{\"lastName\":''}",
		"{\"age\":-1}",
		"{\"age\":0}",
		"{\"age\":0.0}",
		"{\"age\":1}",
		"{\"age\":null}",
		"{}",
		"null",
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
		"[null]",
		"[{\"name\":null,\"price\":null}]",
		"[{\"id\":null,\"price\":null}]",
		"[{\"id\":null,\"name\":null}]",
		"[{\"id\":null,\"name\":null,\"price\":null}]",
		"[{}]",
		"[{\"id\":0.1}]",
		"[{\"id\":null}]",
		"[{\"name\":''}]",
		"[{\"name\":null}]",
		"[{\"price\":0}]",
		"[{\"price\":0.0}]",
		"[{\"price\":1}]",
		"[{\"price\":0.1}]",
		"[{\"price\":null}]",
		"[{\"tags\":[]}]",
		"[{\"tags\":[null]}]",
		"[{\"tags\":['']}]",
		"[{\"tags\":null}]",
		"[{\"tags\":[null,null]}]",
		"[{\"dimensions\":{\"width\":null,\"height\":null}}]",
		"[{\"dimensions\":{\"length\":null,\"height\":null}}]",
		"[{\"dimensions\":{\"length\":null,\"width\":null}}]",
		"[{\"dimensions\":{\"length\":null,\"width\":null,\"height\":null}}]",
		"[{\"dimensions\":{}}]",
		"[{\"dimensions\":{\"length\":0.1}}]",
		"[{\"dimensions\":{\"length\":null}}]",
		"[{\"dimensions\":{\"width\":0.1}}]",
		"[{\"dimensions\":{\"width\":null}}]",
		"[{\"dimensions\":{\"height\":0.1}}]",
		"[{\"dimensions\":{\"height\":null}}]",
		"[{\"dimensions\":null}]",
		"null",
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
		"{\"id\":''}",
		"{\"id\":null}",
		"{\"$schema\":''}",
		"{\"$schema\":null}",
		"{\"title\":''}",
		"{\"title\":null}",
		"{\"description\":''}",
		"{\"description\":null}",
		"{\"multipleOf\":0}",
		"{\"multipleOf\":0.0}",
		"{\"multipleOf\":1}",
		"{\"multipleOf\":0.1}",
		"{\"multipleOf\":null}",
		"{\"maximum\":0.1}",
		"{\"maximum\":null}",
		"{\"exclusiveMaximum\":true}",
		"{\"exclusiveMaximum\":null}",
		"{\"minimum\":0.1}",
		"{\"minimum\":null}",
		"{\"exclusiveMinimum\":true}",
		"{\"exclusiveMinimum\":null}",
		"{\"pattern\":''}",
		"{\"pattern\":null}",
		"{\"additionalItems\":null}",
		"{\"uniqueItems\":true}",
		"{\"uniqueItems\":null}",
		"{\"additionalProperties\":null}",
		"{\"definitions\":{}}",
		"{\"definitions\":null}",
		"{\"properties\":{}}",
		"{\"properties\":null}",
		"{\"patternProperties\":{}}",
		"{\"patternProperties\":null}",
		"{\"dependencies\":{}}",
		"{\"dependencies\":null}",
		"{\"enum\":[null]}",
		"{\"enum\":[]}",
		"{\"enum\":[null,null]}",
		"{\"enum\":null}",
		"{\"type\":[]}",
		"{\"type\":[null,null]}",
		"{\"type\":null}",
		"{}",
		"null",
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
		"{\"additionalItems\":null}",
		"{\"additionalProperties\":null}",
		"{\"links\":[]}",
		"{\"links\":[null]}",
		"{\"links\":null}",
		"{\"fragmentResolution\":''}",
		"{\"fragmentResolution\":null}",
		"{\"media\":{}}",
		"{\"media\":{\"type\":''}}",
		"{\"media\":{\"type\":null}}",
		"{\"media\":{\"binaryEncoding\":''}}",
		"{\"media\":{\"binaryEncoding\":null}}",
		"{\"media\":null}",
		"{\"pathStart\":''}",
		"{\"pathStart\":null}",
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
