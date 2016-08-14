/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2015, 2016 <philip@tecnocode.co.uk>
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
#include <stdio.h>

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
} ExitStatus;

/* Output formats. */
typedef enum {
	FORMAT_PLAIN = 0,
	FORMAT_C,
} OutputFormat;

/* Indexed by OutputFormat. */
static const gchar *output_formats[] = {
	"plain",
	"c",
};

G_STATIC_ASSERT (G_N_ELEMENTS (output_formats) == FORMAT_C + 1);

static gint
sort_schema_info_cb (gconstpointer a,
                     gconstpointer b)
{
	WblSchemaInfo *info_a = *((WblSchemaInfo **) a);
	WblSchemaInfo *info_b = *((WblSchemaInfo **) b);
	gint64 time_a, time_b;

	time_a = wbl_schema_info_get_generation_time (info_a);
	time_b = wbl_schema_info_get_generation_time (info_b);

	return time_b - time_a;
}

/* Command line parameters. */
static gboolean option_quiet = FALSE;
static gboolean option_valid_only = FALSE;
static gboolean option_invalid_only = FALSE;
static gboolean option_no_invalid_json = FALSE;
static gchar *option_format = NULL;
static gchar **option_schema_filenames = NULL;
static gchar *option_c_variable_name = NULL;
static gboolean option_show_timings = FALSE;

static const GOptionEntry entries[] = {
	{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &option_quiet,
	  N_("Quieten command line output"), NULL },
	{ "valid-only", 'v', 0, G_OPTION_ARG_NONE, &option_valid_only,
	  N_("Only output valid JSON instances"), NULL },
	{ "invalid-only", 'n', 0, G_OPTION_ARG_NONE, &option_invalid_only,
	  N_("Only output invalid JSON instances"), NULL },
	{ "no-invalid-json", 'j', 0, G_OPTION_ARG_NONE,
	  &option_no_invalid_json,
	  N_("Disable generation of invalid JSON vectors"), NULL },
	{ "format", 'f', 0, G_OPTION_ARG_STRING, &option_format,
	  N_("Output format (‘plain’ [default], ‘c’)"), NULL },
	{ "c-variable-name", 0, 0, G_OPTION_ARG_STRING, &option_c_variable_name,
	  N_("Vector array variable name (only with --format=c; default "
	     "‘json_instances’)"), NULL },
	{ "show-timings", 0, 0, G_OPTION_ARG_NONE, &option_show_timings,
	  N_("Print timing information to stderr after outputting generated "
	     "instances"), NULL },
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
	OutputFormat output_format;
	gboolean output_format_set = FALSE;
	GError *error = NULL;
	gboolean generated_any_valid_instances = FALSE;
	gboolean generated_any_invalid_instances = FALSE;
	gboolean use_colour_stderr;
	const gchar *bold_escape, *reset_escape;

#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif

	setlocale (LC_ALL, "");

	/* Can we use colour output? */
	use_colour_stderr = wbl_is_colour_supported (stderr);

	/* Redirect debug output to stderr so that stdout is purely generated
	 * test vectors. */
	g_log_set_default_handler (wbl_log, NULL);

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
		g_printerr ("%s: %s\n", argv[0], message);
		g_free (message);

		g_clear_error (&error);

		retval = EXIT_INVALID_OPTIONS;
		goto done;
	}

	/* Validate the options. */
	if (option_format == NULL) {
		/* Default. */
		output_format = FORMAT_PLAIN;
		output_format_set = TRUE;
	}

	for (i = 0;
	     i < G_N_ELEMENTS (output_formats) && !output_format_set; i++) {
		if (g_strcmp0 (output_formats[i], option_format) == 0) {
			output_format = i;
			output_format_set = TRUE;
			break;
		}
	}

	if (!output_format_set) {
		gchar *message1, *message2;

		message1 = g_strdup_printf (_("Invalid output format ‘%s’."),
		                            option_format);
		message2 = g_strdup_printf (_("Option parsing failed: %s"),
		                            message1);
		g_printerr ("%s: %s\n", argv[0], message2);
		g_free (message2);
		g_free (message1);

		g_clear_error (&error);

		retval = EXIT_INVALID_OPTIONS;
		goto done;
	}

	if (output_format == FORMAT_C) {
		/* Don’t bother validating @option_c_variable_name; the compiler
		 * will eventually do that for us. */
		if (option_c_variable_name == NULL ||
		    g_strcmp0 (option_c_variable_name, "") == 0) {
			g_free (option_c_variable_name);
			option_c_variable_name = g_strdup ("json_instances");
		}
	} else if (option_c_variable_name != NULL) {
		const gchar *message = NULL;

		message = _("Option --c-variable-name may only be specified "
		            "with --format=c.");
		g_printerr ("%s: %s\n", argv[0], message);

		retval = EXIT_INVALID_OPTIONS;
		goto done;
	}

	if (option_schema_filenames == NULL || option_schema_filenames[0] == NULL) {
		const gchar *message = NULL;

		message = _("At least one schema file must be specified.");
		g_printerr ("%s: %s\n", argv[0], message);

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
				g_printerr ("%s: %s\n", argv[0], message);
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
	if (!option_no_invalid_json) {
		flags |= WBL_GENERATE_INSTANCE_INVALID_JSON;
	}

	/* Initial output. This format is part of the json-schema-generate ABI
	 * and cannot be modified without a major version break. */
	if (output_format == FORMAT_C) {
		g_print ("/* Generated by %s. Do not modify. */\n\n", argv[0]);
		g_print ("#include <stddef.h>\n\n");
		g_print ("static const struct { const char *json; size_t size; unsigned int is_valid; } %s[] = {\n",
		         option_c_variable_name);
	}

	/* Generate from each of the schemas. */
	for (i = 0; i < schemas->len; i++) {
		WblSchema *schema;  /* unowned */
		GPtrArray/*<owned WblGeneratedInstance>*/ *instances = NULL;  /* owned */
		guint j;

		schema = schemas->pdata[i];
		instances = wbl_schema_generate_instances (schema, flags);

		/* Print out the instances. This format is part of the
		 * json-schema-generate ABI and cannot be modified without a
		 * major version break. */
		for (j = 0; j < instances->len; j++) {
			WblGeneratedInstance *instance;  /* unowned */
			const gchar *json;
			gboolean is_valid;

			instance = instances->pdata[j];
			json = wbl_generated_instance_get_json (instance);
			is_valid = wbl_generated_instance_is_valid (instance);
			generated_any_valid_instances |= is_valid;
			generated_any_invalid_instances |= !is_valid;

			switch (output_format) {
			case FORMAT_PLAIN:
				g_print ("%s\n", json);
				break;
			case FORMAT_C: {
				gchar *escaped = g_strescape (json, "");
				g_print ("\t{ \"%s\", %" G_GSIZE_FORMAT ", "
				         "%u },  /* %u */\n",
				         escaped, strlen (json),
				         is_valid ? 1 : 0, j);
				g_free (escaped);
				break;
			}
			default:
				g_assert_not_reached ();
			}
		}

		g_ptr_array_unref (instances);
	}

	/* Final output. */
	if (output_format == FORMAT_C) {
		g_print ("};\n");
	}

	/* Timing output for each of the schemas. */
	if (use_colour_stderr) {
		/* See: http://misc.flogisoft.com/bash/tip_colors_and_formatting */
		bold_escape = "\033[1m";
		reset_escape = "\033[0m";
	} else {
		bold_escape = "";
		reset_escape = "";
	}

	if (option_show_timings) {
		for (i = 0; i < schemas->len; i++) {
			WblSchema *schema;  /* unowned */
			GPtrArray/*<owned WblSchemaInfo>*/ *infos = NULL;  /* owned */
			guint j;

			schema = schemas->pdata[i];
			infos = wbl_schema_get_schema_info (schema);

			/* Sort the schema info by the generation time. */
			g_ptr_array_sort (infos, sort_schema_info_cb);

			/* Print timing information. */
			g_printerr ("%s%s%s timings:\n",
			            bold_escape, option_schema_filenames[i],
			            reset_escape);

			for (j = 0; j < infos->len; j++) {
				WblSchemaInfo *info = infos->pdata[j];
				guint n_instances;
				gint64 generation_time;
				gdouble time_per_instance;

				n_instances = wbl_schema_info_get_n_instances_generated (info);
				generation_time = wbl_schema_info_get_generation_time (info);

				if (n_instances > 0)
					time_per_instance = (gdouble) generation_time / n_instances;
				else
					time_per_instance = 0.0;

				g_printerr (" • %s%u%s generation took %"
				            G_GINT64_FORMAT "μs, %u times, "
				            "generating %u instances "
				            "(%.2fμs⋅instance⁻¹)\n",
				            bold_escape,
				            wbl_schema_info_get_id (info),
				            reset_escape,
				            generation_time,
				            wbl_schema_info_get_n_times_generated (info),
				            n_instances,
				            time_per_instance);
			}

			g_printerr ("%s%s%s schemas (total: %u):\n",
			            bold_escape, option_schema_filenames[i],
			            reset_escape, infos->len);

			for (j = 0; j < infos->len; j++) {
				WblSchemaInfo *info = infos->pdata[j];
				gchar *json = NULL;

				json = wbl_schema_info_build_json (info);
				g_printerr (" • %s%u%s:\n"
				            "      %s\n",
				            bold_escape,
				            wbl_schema_info_get_id (info),
				            reset_escape,
				            json);
				g_free (json);
			}

			g_ptr_array_unref (infos);
		}
	}

	/* Sanity check. */
	if (!option_invalid_only && !generated_any_valid_instances) {
		g_printerr ("%s: Warning: Failed to generate any valid "
		            "instances. Test coverage may be low. This may "
		            "indicate a bug in Walbottle; please report it.\n",
		            argv[0]);
	} else if (!option_valid_only && !generated_any_invalid_instances) {
		g_printerr ("%s: Warning: Failed to generate any invalid "
		            "instances. Test coverage may be low. This may "
		            "indicate a bug in Walbottle; please report it.\n",
		            argv[0]);
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
