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
#include <string.h>

#include "utils.h"
#include "wbl-json-node.h"
#include "wbl-schema.h"
#include "wbl-meta-schema.h"

/* Load JSON instances from a file, one per line. Each must be a valid JSON
 * instance. No escaping is required. */
gchar **
wbl_test_load_json_instances_from_file (const gchar  *filename)
{
	gchar *path = NULL;
	gchar *file_contents = NULL;
	gchar **lines = NULL;
	GError *error = NULL;
	guint i;

	path = g_test_build_filename (G_TEST_DIST, filename, NULL);
	g_test_message ("Loading JSON instances from ‘%s’.", path);
	g_file_get_contents (filename, &file_contents, NULL, &error);
	g_assert_no_error (error);
	g_free (path);

	lines = g_strsplit (file_contents, "\n", -1);

	/* Clear the last element if it’s an empty string. */
	for (i = g_strv_length (lines) - 1; g_strcmp0 (lines[i], "") == 0; i--) {
		lines[i] = NULL;
	}

	g_free (file_contents);

	return lines;
}

static gint
sort_cb (gconstpointer  a,
         gconstpointer  b)
{
	WblGeneratedInstance **a_instance = (WblGeneratedInstance **) a;
	WblGeneratedInstance **b_instance = (WblGeneratedInstance **) b;

	/* Sorting on NULL values is undefined. */
	g_assert (a != NULL && b != NULL);

	return g_strcmp0 (wbl_generated_instance_get_json (*a_instance),
	                  wbl_generated_instance_get_json (*b_instance));
}

/* Dump all the JSON instances in @actual to a temporary file, one instance per
 * line in a format loadable by load_json_instances_from_file(). */
gchar *
wbl_test_dump_json_instances_to_file (GPtrArray/*<owned WblGeneratedInstance>*/  *actual)
{
	GFile *file = NULL;
	gchar *filename = NULL;
	GFileIOStream *io_stream = NULL;
	GOutputStream *output_stream;
	guint i;
	GError *error = NULL;

	file = g_file_new_tmp ("walbottle-actual-instances-XXXXXX.json",
	                       &io_stream, &error);
	g_assert_no_error (error);

	filename = g_file_get_path (file);
	output_stream = g_io_stream_get_output_stream (G_IO_STREAM (io_stream));

	/* On the assumption that this is an error path and everything’s going
	 * to be aborted anyway, sort the input @actual array. This makes
	 * diffing the dumped file a lot nicer. */
	g_ptr_array_sort (actual, sort_cb);

	for (i = 0; i < actual->len; i++) {
		WblGeneratedInstance *instance = actual->pdata[i];
		const gchar *json;

		json = wbl_generated_instance_get_json (instance);
		g_output_stream_write_all (output_stream, json, strlen (json),
		                           NULL, NULL, &error);
		g_assert_no_error (error);

		g_output_stream_write_all (output_stream, "\n", 1, NULL, NULL,
		                           &error);
		g_assert_no_error (error);
	}

	g_io_stream_close (G_IO_STREAM (io_stream), NULL, NULL);

	g_object_unref (io_stream);
	g_object_unref (file);

	return filename;
}

/* Utility method to assert that the generated instances in two arrays match,
 * ignoring order. @expected_filename is nullable. */
void
wbl_test_assert_generated_instances_match (GPtrArray/*<owned WblGeneratedInstance>*/  *actual,
                                           const gchar                               **expected  /* NULL terminated */,
                                           const gchar                                *expected_filename)
{
	guint i;
	JsonParser *parser = NULL;
	GError *error = NULL;

	parser = json_parser_new ();

	for (i = 0; i < actual->len; i++) {
		const gchar *actual_json;
		gboolean found = FALSE;
		guint j;
		JsonNode *actual_json_node = NULL, *expected_node;

		actual_json = wbl_generated_instance_get_json (actual->pdata[i]);

		json_parser_load_from_data (parser, actual_json, -1, &error);
		g_assert_no_error (error);
		actual_json_node = json_node_copy (json_parser_get_root (parser));

		for (j = 0; expected[j] != NULL; j++) {
			json_parser_load_from_data (parser, expected[j], -1, &error);
			g_assert_no_error (error);
			expected_node = json_parser_get_root (parser);

			if (wbl_json_node_equal (actual_json_node, expected_node)) {
				found = TRUE;
				break;
			}
		}

		json_node_free (actual_json_node);

		/* If there’s going to be an error, dump the actual instances
		 * to a file which can be diffed easily against the expected
		 * file. */
		if (!found) {
			gchar *dump_filename = NULL;

			dump_filename = wbl_test_dump_json_instances_to_file (actual);
			g_print ("Error: Dumped actual instances to ‘%s’.\n",
			         dump_filename);

			if (expected_filename != NULL) {
				g_print ("Compare actual to expected using:\n"
				         "   diff -u \"%s\" \"%s\" | less\n",
				         expected_filename, dump_filename);
			}

			g_free (dump_filename);
		}

		/* Nice error message. */
		if (!found) {
			g_assert_cmpstr (actual_json, ==, "not found");
		}
		g_assert (found);
	}

	g_assert_cmpuint (actual->len, ==, g_strv_length ((gchar **) expected));

	g_clear_object (&parser);
}

/* Utility method combining the above two. */
void
wbl_test_assert_generated_instances_match_file (GPtrArray/*<owned WblGeneratedInstance>*/  *actual,
                                                const gchar                                *expected_filename)
{
	gchar **expected = NULL;

	expected = wbl_test_load_json_instances_from_file (expected_filename);
	wbl_test_assert_generated_instances_match (actual,
	                                           (const gchar **) expected,
	                                           expected_filename);
	g_strfreev (expected);
}
