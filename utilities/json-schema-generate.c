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
#include "wbl-meta-schema.h"

/* Exit statuses. */
typedef enum {
	/* Success. */
	EXIT_SUCCESS = 0,
	/* Error parsing command line options. */
	EXIT_INVALID_OPTIONS = 1,
	/* JSON schema could not be parsed. */
	EXIT_INVALID_SCHEMA = 2,
} ExitStatus;

/* Command line parameters. */
static gboolean option_quiet = FALSE;
static gboolean option_valid_only = FALSE;
static gboolean option_invalid_only = FALSE;
static gchar **option_schema_filenames = NULL;

static const GOptionEntry entries[] = {
	{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &option_quiet,
	  N_("Quieten command line output"), NULL },
	{ "valid-only", 'v', 0, G_OPTION_ARG_NONE, &option_valid_only,
	  N_("Only output valid JSON instances"), NULL },
	{ "invalid-only", 'n', 0, G_OPTION_ARG_NONE, &option_invalid_only,
	  N_("Only output invalid JSON instances"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY,
	  &option_schema_filenames,
	  N_("JSON schema files to generate from"),
	  N_("JSON-SCHEMA [JSON-SCHEMA …]") },
	{ NULL, },
};

int
main (int argc, char *argv[])
{
	GOptionContext *context = NULL;  /* owned */
	ExitStatus retval = EXIT_SUCCESS;
	guint i;
	GPtrArray/*<owned WblSchema>*/ *schemas = NULL;  /* owned */
	WblGenerateInstanceFlags flags;
	GError *error = NULL;

#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif

	setlocale (LC_ALL, "");

	/* Command line parsing. */
	context = g_option_context_new (_("— generate test vectors from JSON "
	                                  "schemas"));
	g_option_context_set_summary (context,
	                              _("Generate valid and invalid instances "
	                                "of one or more JSON Schemas. These "
	                                "can be used as test vectors in unit "
	                                "tests for code which parses documents "
	                                "which should conform to all of these "
	                                "schemas. Schemas are outputted one "
	                                "per line.\n\nRead about JSON Schema "
	                                "here: http://json-schema.org/."));
	g_option_context_add_main_entries (context, entries, PACKAGE_NAME);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		gchar *message;

		message = g_strdup_printf (_("Option parsing failed: %s"),
		                           error->message);
		g_printerr ("%s\n", message);
		g_free (message);

		g_clear_error (&error);

		retval = EXIT_INVALID_OPTIONS;
		goto done;
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

			g_clear_error (&error);
			g_clear_object (&schema);

			retval = EXIT_INVALID_SCHEMA;
			goto done;
		}

		g_ptr_array_add (schemas, schema);  /* transfer */
	}

	/* Set up the generation flags. */
	flags = WBL_GENERATE_INSTANCE_NONE;

	if (option_valid_only) {
		flags |= WBL_GENERATE_INSTANCE_IGNORE_INVALID;
	}
	if (option_invalid_only) {
		flags |= WBL_GENERATE_INSTANCE_IGNORE_VALID;
	}

	/* Generate from each of the schemas. */
	for (i = 0; i < schemas->len; i++) {
		WblSchema *schema;  /* unowned */
		GPtrArray/*<owned WblGeneratedInstance>*/ *instances = NULL;  /* owned */
		guint j;

		schema = schemas->pdata[i];
		instances = wbl_schema_generate_instances (schema, flags);

		/* Print out the instances. */
		for (j = 0; j < instances->len; j++) {
			WblGeneratedInstance *instance;  /* unowned */

			instance = instances->pdata[j];

			g_print ("%s\n",
			         wbl_generated_instance_get_json (instance));
		}

		g_ptr_array_unref (instances);
	}

done:
	if (context != NULL) {
		g_option_context_free (context);
	}
	if (schemas != NULL) {
		g_ptr_array_unref (schemas);
	}

	return retval;
}