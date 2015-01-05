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

/* Utility method to assert that the generated instances in two arrays match,
 * ignoring order. */
static void
assert_generated_instances_match (GPtrArray/*<owned WblGeneratedInstance>*/ *actual,
                                  const gchar **expected  /* NULL terminated */)
{
	guint i;

	g_assert_cmpuint (actual->len, ==, g_strv_length ((gchar **) expected));

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
test_schema_parsing (void)
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
test_schema_instance_generation (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	const gchar *expected_instances[] = {
		"{\"firstName\":null}",
		"{\"lastName\":null}",
		"{\"firstName\":null,\"lastName\":null}",
		"{\"firstName\":''}",
		"{\"firstName\":null}",
		"{\"lastName\":''}",
		"{\"lastName\":null}",
		"{\"age\":-1}",
		"{\"age\":0}",
		"{\"age\":1}",
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
	g_test_add_func ("/schema/parsing", test_schema_parsing);
	g_test_add_func ("/schema/application", test_schema_application);
	g_test_add_func ("/schema/instance-generation",
	                 test_schema_instance_generation);

	return g_test_run ();
}
