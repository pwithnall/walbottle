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
#include "utilities/wbl-utilities.h"

/* Exit statuses. */
typedef enum {
	/* Success. */
	EXIT_SUCCESS = 0,
	/* Error parsing command line options. */
	EXIT_INVALID_OPTIONS = 1,
	/* JSON schema could not be parsed. */
	EXIT_INVALID_SCHEMA = 2,
	/* JSON file does not validate against the meta-schema. */
	EXIT_SCHEMA_VALIDATION_FAILED = 3,
} ExitStatus;

/* Command line parameters. */
static gboolean option_quiet = FALSE;
static gboolean option_no_hyper = FALSE;
static gboolean option_ignore_errors = FALSE;
static gchar **option_schema_filenames = NULL;

static const GOptionEntry entries[] = {
	{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &option_quiet,
	  N_("Quieten command line output"), NULL },
	{ "no-hyper", 0, 0, G_OPTION_ARG_NONE, &option_no_hyper,
	  N_("Validate against the meta-schema rather than the "
	     "hyper-meta-schema"), NULL },
	{ "ignore-errors", 'i', 0, G_OPTION_ARG_NONE, &option_ignore_errors,
	  N_("Continue validating after errors are encountered, rather than "
	     "stopping at the first error"), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY,
	  &option_schema_filenames,
	  N_("JSON schema files to validate"),
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
	WblSchema *meta_schema = NULL;  /* owned */
	WblMetaSchemaType meta_schema_type;
	const gchar *meta_schema_name;
	GError *error = NULL;

#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif

	setlocale (LC_ALL, "");

	/* Redirect debug output to stderr so that stdout is purely useful. */
	g_log_set_default_handler (wbl_log, NULL);

	/* Command line parsing. */
	context = g_option_context_new (_("— validate JSON schemas"));
	g_option_context_set_summary (context,
	                              _("Validate one or more JSON Schemas, "
	                                "checking for well-formedness, and "
	                                "validating against a JSON "
	                                "meta-schema.\n\nThere are two "
	                                "meta-schemas:\n • meta-schema: Used "
	                                "for schemas written for pure "
	                                "validation.\n • hyper-meta-schema: "
	                                "Used for schemas written for "
	                                "validation and hyper-linking.\n\nThe "
	                                "hyper-meta-schema is used by default, "
	                                "and is a superset of meta-schema.\n\n"
	                                "Read about JSON Schema here: "
	                                "http://json-schema.org/."));
	g_option_context_add_main_entries (context, entries, PACKAGE_NAME);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		gchar *message;

		message = g_strdup_printf (_("Option parsing failed: %s"),
		                           error->message);
		g_printerr ("%s: %s\n", argv[0], message);
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

		/* Print error or other messages from validation. */
		wbl_print_validate_messages (schema,
		                             TRUE  /* use colour */);

		if (error != NULL) {
			if (!option_quiet) {
				gchar *message;

				message = g_strdup_printf (_("Invalid JSON schema ‘%s’: %s"),
				                           option_schema_filenames[i],
				                           error->message);
				g_printerr ("%s: %s\n", argv[0], message);
				g_free (message);
			}

			if (retval == EXIT_SUCCESS) {
				retval = EXIT_INVALID_SCHEMA;
			}
			g_clear_error (&error);

			if (!option_ignore_errors) {
				g_object_unref (schema);
				goto done;
			} else {
				g_object_unref (schema);
				continue;
			}
		}

		g_object_set_data (G_OBJECT (schema),
		                   "filename", option_schema_filenames[i]);
		g_ptr_array_add (schemas, schema);  /* transfer */
	}

	/* Choose which meta-schema to use. */
	if (option_no_hyper) {
		meta_schema_type = WBL_META_SCHEMA_META_SCHEMA;
		meta_schema_name = "meta-schema";
	} else {
		meta_schema_type = WBL_META_SCHEMA_HYPER_META_SCHEMA;
		meta_schema_name = "hyper-meta-schema";
	}

	meta_schema = wbl_meta_schema_load_schema (meta_schema_type, &error);
	g_assert_no_error (error);

	/* Validate each of the JSON schemas against the meta-schema. */
	for (i = 0; i < schemas->len; i++) {
		WblSchema *schema;  /* unowned */
		const gchar *schema_filename;
		WblSchemaNode *schema_root;  /* unowned */
		JsonNode *schema_root_node = NULL;  /* owned */
		JsonObject *schema_root_object;  /* unowned */

		schema = schemas->pdata[i];
		schema_filename = g_object_get_data (G_OBJECT (schema),
		                                     "filename");

		if (!option_quiet) {
			gchar *message;

			message = g_strdup_printf (_("Validating ‘%s’ against %s…"),
			                           schema_filename,
			                           meta_schema_name);
			g_print ("%s ", message);
			g_free (message);
		}

		/* Try applying the meta-schema to the schema. */
		schema_root = wbl_schema_get_root (schema);
		schema_root_object = wbl_schema_node_get_root (schema_root);

		schema_root_node = json_node_alloc ();
		json_node_init_object (schema_root_node, schema_root_object);
		wbl_schema_apply (meta_schema, schema_root_node, &error);
		json_node_free (schema_root_node);

		if (!option_quiet) {
			g_print ("%s",
			         (error == NULL) ? _("OK") : _("FAIL"));
			g_print ("\n");
		}

		if (error != NULL) {
			if (!option_quiet) {
				gchar *message;

				message = g_strdup_printf (_("Validation error for ‘%s’ against %s: %s"),
				                           schema_filename,
				                           meta_schema_name,
				                           error->message);
				g_printerr ("%s: %s\n", argv[0], message);
				g_free (message);
			}

			if (retval == EXIT_SUCCESS) {
				retval = EXIT_SCHEMA_VALIDATION_FAILED;
			}
			g_clear_error (&error);

			if (!option_ignore_errors) {
				goto done;
			}
		}
	}

done:
	if (context != NULL) {
		g_option_context_free (context);
	}
	if (schemas != NULL) {
		g_ptr_array_unref (schemas);
	}
	g_clear_object (&meta_schema);

	return retval;
}
