/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright © Collabora Ltd. 2015
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

#include <glib.h>
#include <string.h>

#include "wbl-schema.h"
#include "utilities/wbl-utilities.h"

void
wbl_log (const gchar     *log_domain,
         GLogLevelFlags   log_level,
         const gchar     *message,
         gpointer         user_data)
{
	const gchar *domains;
	gboolean is_info;

	is_info = ((log_level & (G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG)) != 0);
	domains = g_getenv ("G_MESSAGES_DEBUG");

	/* Promote info and debug messages to normal messages so they go to
	 * stderr. */
	if (is_info &&
	    domains != NULL &&
	    ((strcmp (domains, "all") == 0 ||
	     (log_domain != NULL && strstr (domains, log_domain) != NULL)))) {
		log_level = G_LOG_LEVEL_MESSAGE;
	}

	g_log_default_handler (log_domain, log_level, message, user_data);
}

static const gchar *
validate_message_level_to_string (WblValidateMessageLevel level)
{
	switch (level) {
	case WBL_VALIDATE_MESSAGE_ERROR:
		return "error";
	default:
		g_assert_not_reached ();
	}
}

/* Reference: http://misc.flogisoft.com/bash/tip_colors_and_formatting */
static const gchar *
validate_message_level_to_colour (WblValidateMessageLevel level)
{
	switch (level) {
	case WBL_VALIDATE_MESSAGE_ERROR:
		/* Bold, red. */
		return "\033[1;31m";
	default:
		g_assert_not_reached ();
	}
}

static void
print_validate_messages (GPtrArray/*<owned WblValidateMessage>*/ *messages,
                         gboolean                                 use_colour,
                         const gchar                             *line_prefix)
{
	guint i;

	for (i = 0; i < messages->len; i++) {
		WblValidateMessage *message;
		gchar *specification_link = NULL;
		const gchar *m, *level, *node_path;
		GPtrArray/*<owned WblValidateMessage>*/ *sub_messages;
		gboolean prefix_is_empty = (*line_prefix == '\0');
		gboolean is_last = (i == messages->len - 1);
		gchar *prefix = NULL;
		const gchar *node_path_escape, *level_escape, *reset_escape;

		message = messages->pdata[i];

		node_path = wbl_validate_message_get_path (message);
		level = validate_message_level_to_string (wbl_validate_message_get_level (message));
		m = wbl_validate_message_get_message (message);
		specification_link = wbl_validate_message_build_specification_link (message);
		sub_messages = wbl_validate_message_get_sub_messages (message);

		/* node_path should be printed in bold. level should be printed
		 * in red (or another appropriate colour) and bold.
		 *
		 * We are trying to emulate gcc here.
		 *
		 * Reference: http://misc.flogisoft.com/bash/tip_colors_and_formatting */
		if (use_colour) {
			node_path_escape = "\033[1m";
			level_escape = validate_message_level_to_colour (wbl_validate_message_get_level (message));
			reset_escape = "\033[0m";
		} else {
			node_path_escape = "";
			level_escape = "";
			reset_escape = "";
		}

		if (specification_link != NULL) {
			g_printerr ("%s%s%s%s: %s%s%s: %s [%s]\n",
			            line_prefix,
			            node_path_escape, node_path, reset_escape,
			            level_escape, level, reset_escape,
			            m, specification_link);
		} else {
			g_printerr ("%s%s%s%s: %s%s%s: %s\n", line_prefix,
			            node_path_escape, node_path, reset_escape,
			            level_escape, level, reset_escape,
			            m);
		}

		g_free (specification_link);

		/* Print sub messages or the offending JSON. */
		if (prefix_is_empty && !is_last)
			prefix = g_strdup ("├─");
		else if (prefix_is_empty)
			prefix = g_strdup ("└─");
		else if (!is_last)
			prefix = g_strconcat ("│ ", line_prefix, NULL);
		else
			prefix = g_strconcat ("  ", line_prefix, NULL);

		if (sub_messages != NULL) {
			print_validate_messages (sub_messages, use_colour,
			                         prefix);
		} else {
			gchar *json = NULL;

			json = wbl_validate_message_build_json (message);

			if (json != NULL)
				g_printerr ("%s%s\n", prefix, json);

			g_free (json);
		}

		g_free (prefix);
	}
}

void
wbl_print_validate_messages (WblSchema *schema,
                             gboolean   use_colour)
{
	GPtrArray/*<owned WblValidateMessage>*/ *messages;

	messages = wbl_schema_get_validation_messages (schema);
	if (messages != NULL)
		print_validate_messages (messages, use_colour, "");
}
