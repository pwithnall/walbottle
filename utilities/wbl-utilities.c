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
