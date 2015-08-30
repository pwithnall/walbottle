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
#include <locale.h>
#include <string.h>

#include "wbl-string-set.h"

/* Test the size of an empty set. */
static void
test_empty_set (void)
{
	WblStringSet *set = NULL;

	set = wbl_string_set_new_empty ();
	g_assert_cmpuint (wbl_string_set_get_size (set), ==, 0);

	wbl_string_set_unref (set);
}

/* Test uniqueness of members. */
static void
test_set_uniqueness (void)
{
	WblStringSet *set = NULL;

	set = wbl_string_set_union (wbl_string_set_new_singleton ("a"),
	                            wbl_string_set_new_singleton ("a"));
	g_assert_cmpuint (wbl_string_set_get_size (set), ==, 1);
	g_assert (wbl_string_set_contains (set, "a"));

	wbl_string_set_unref (set);
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

	/* #WblStringSet tests. */
	g_test_add_func ("/string-set/empty", test_empty_set);
	g_test_add_func ("/string-set/uniqueness", test_set_uniqueness);

	return g_test_run ();
}
