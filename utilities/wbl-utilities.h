/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright © Collabora Ltd. 2015
 * Copyright © Philip Withnall 2016
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

#ifndef WBL_UTILITIES_H
#define WBL_UTILITIES_H

#include <glib.h>
#include <stdio.h>

G_BEGIN_DECLS

gboolean wbl_is_colour_supported (FILE *file);

void wbl_log (const gchar     *log_domain,
              GLogLevelFlags   log_level,
              const gchar     *message,
              gpointer         user_data);

void wbl_print_validate_messages (WblSchema *schema,
                                  gboolean   use_colour);

G_END_DECLS

#endif /* !WBL_UTILITIES_H */
