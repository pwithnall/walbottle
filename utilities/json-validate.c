/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2015 <philip@tecnocode.co.uk>
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

#include "config.h"

#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "wbl-schema.h"

/* Exit statuses. */
typedef enum {
	/* Success. */
	EXIT_SUCCESS = 0,
	/* Error parsing command line options. */
	EXIT_INVALID_OPTIONS = 1,
	/* JSON file could not be parsed. */
	EXIT_INVALID_JSON = 2,
	/* JSON schema could not be parsed. */
	EXIT_INVALID_SCHEMA = 3,
	/* JSON file does not validate against a schema. */
	EXIT_SCHEMA_VALIDATION_FAILED = 4,
} ExitStatus;

/* Command line parameters. */
static gboolean option_quiet = FALSE;
static gboolean option_ignore_errors = FALSE;
static gchar **option_schema_filenames = NULL;
static gchar **option_json_filenames = NULL;

static const GOptionEntry entries[] = {
	{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &option_quiet,
	  N_("Quieten command line output"), NULL },
	{ "schema", 's', 0, G_OPTION_ARG_FILENAME_ARRAY,
	  &option_schema_filenames,
	  N_("JSON Schema to validate against; may be specified multiple "
	     "times"), N_("JSON-SCHEMA") },
	{ "ignore-errors", 'i', 0, G_OPTION_ARG_NONE, &option_ignore_errors,
	  N_("Continue validating after errors are encountered, rather than "
	     "stopping at the first error"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY,
	  &option_json_filenames,
	  N_("JSON files to validate"), N_("JSON-FILE [JSON-FILE …]") },
	{ NULL, },
};

int
main (int argc, char *argv[])
{
	GOptionContext *context = NULL;  /* owned */
	ExitStatus retval = EXIT_SUCCESS;
	guint i, j;
	GPtrArray/*<owned JsonParser>*/ *json_files = NULL;  /* owned */
	GPtrArray/*<owned WblSchema>*/ *schemas = NULL;  /* owned */
	GError *error = NULL;

#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif

	setlocale (LC_ALL, "");

	/* Command line parsing. */
	context = g_option_context_new (_("— validate JSON documents"));
	g_option_context_set_summary (context,
	                              _("Validate one or more JSON documents, "
	                                "checking for well-formedness. If JSON "
	                                "Schemas are provided by using "
	                                "--schema one or more times, all JSON "
	                                "documents will be validated against "
	                                "all schemas.\n\nRead about JSON "
	                                "Schema here: "
	                                "http://json-schema.org/."));
	g_option_context_add_main_entries (context, entries, PACKAGE_NAME);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		gchar *message;

		message = g_strdup_printf (_("Option parsing failed: %s"),
		                           error->message);
		g_printerr ("%s\n", message);
		g_free (message);

		g_error_free (error);

		retval = EXIT_INVALID_OPTIONS;
		goto done;
	}

	/* Load the JSON files. */
	json_files = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	for (i = 0;
	     option_json_filenames != NULL && option_json_filenames[i] != NULL;
	     i++) {
		JsonParser *parser = NULL;  /* owned */

		parser = json_parser_new ();
		json_parser_load_from_file (parser, option_json_filenames[i],
		                            &error);

		if (error != NULL) {
			if (!option_quiet) {
				gchar *message;

				/* The filename is specified in @error. */
				message = g_strdup_printf (_("Invalid JSON document: %s"),
				                           error->message);
				g_printerr ("%s\n", message);
				g_free (message);
			}

			if (retval == EXIT_SUCCESS) {
				retval = EXIT_INVALID_JSON;
			}
			g_error_free (error);

			if (!option_ignore_errors) {
				goto done;
			}
		}

		g_object_set_data (G_OBJECT (parser),
		                   "filename", option_json_filenames[i]);
		g_ptr_array_add (json_files, parser);  /* transfer */
	}

	/* Load the schemas. */
	schemas = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	for (i = 0;
	     option_schema_filenames != NULL && option_schema_filenames[i] != NULL;
	     i++) {
		WblSchema *schema = NULL;  /* owned */

		schema = wbl_schema_new ();
		wbl_schema_load_from_file (schema, option_schema_filenames[i],
		                           &error);

		if (error != NULL) {
			if (!option_quiet) {
				gchar *message;

				message = g_strdup_printf (_("Invalid JSON schema ‘%s’: %s"),
				                           option_schema_filenames[i],
				                           error->message);
				g_printerr ("%s\n", message);
				g_free (message);
			}

			if (retval == EXIT_SUCCESS) {
				retval = EXIT_INVALID_SCHEMA;
			}
			g_error_free (error);

			if (!option_ignore_errors) {
				goto done;
			}
		}

		g_object_set_data (G_OBJECT (schema),
		                   "filename", option_schema_filenames[i]);
		g_ptr_array_add (schemas, schema);  /* transfer */
	}

	/* Validate each of the JSON files against each of the schemas. */
	for (i = 0; i < json_files->len; i++) {
		for (j = 0; j < schemas->len; j++) {
			JsonParser *parser;  /* unowned */
			JsonNode *instance;  /* unowned */
			WblSchema *schema;  /* unowned */
			const gchar *json_filename, *schema_filename;

			parser = json_files->pdata[i];
			instance = json_parser_get_root (parser);
			schema = schemas->pdata[j];

			json_filename = g_object_get_data (G_OBJECT (parser),
			                                   "filename");
			schema_filename = g_object_get_data (G_OBJECT (schema),
			                                     "filename");

			if (!option_quiet) {
				gchar *message;

				message = g_strdup_printf (_("Validating ‘%s’ against ‘%s’…"),
				                           json_filename,
				                           schema_filename);
				g_print ("%s ", message);
				g_free (message);
			}

			/* Try applying the schema to the JSON. */
			wbl_schema_apply (schema, instance, &error);

			if (!option_quiet) {
				g_print ("%s",
				         (error == NULL) ? _("OK") : _("FAIL"));
				g_print ("\n");
			}

			if (error != NULL) {
				if (!option_quiet) {
					gchar *message;

					message = g_strdup_printf (_("Validation error for ‘%s’ against ‘%s’: %s"),
					                           json_filename,
					                           schema_filename,
					                           error->message);
					g_printerr ("%s\n", message);
					g_free (message);
				}

				if (retval == EXIT_SUCCESS) {
					retval = EXIT_SCHEMA_VALIDATION_FAILED;
				}
				g_error_free (error);

				if (!option_ignore_errors) {
					goto done;
				}
			}
		}
	}

done:
	if (context != NULL) {
		g_option_context_free (context);
	}
	if (json_files != NULL) {
		g_ptr_array_unref (json_files);
	}
	if (schemas != NULL) {
		g_ptr_array_unref (schemas);
	}

	return retval;
}
