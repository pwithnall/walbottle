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

#ifndef WBL_TEST_UTILS_H
#define WBL_TEST_UTILS_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

gchar **
wbl_test_load_json_instances_from_file (const gchar  *filename);
gchar *
wbl_test_dump_json_instances_to_file (GPtrArray/*<owned WblGeneratedInstance>*/  *actual);
void
wbl_test_assert_generated_instances_match (GPtrArray/*<owned WblGeneratedInstance>*/  *actual,
                                           const gchar                               **expected  /* NULL terminated */,
                                           const gchar                                *expected_filename);
void
wbl_test_assert_generated_instances_match_file (GPtrArray/*<owned WblGeneratedInstance>*/  *actual,
                                                const gchar                                *expected_filename);

G_END_DECLS

#endif /* !WBL_TEST_UTILS_H */
