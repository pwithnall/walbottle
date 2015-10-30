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

#ifndef WBL_STRING_SET_H
#define WBL_STRING_SET_H

#include <glib.h>
#include <glib-object.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * WblStringSet:
 *
 * A reference counted structure which represents a mathematical set of UTF-8
 * encoded strings.
 *
 * All the fields in the #WblStringSet structure are private and should never
 * be accessed directly.
 *
 * Since: 0.2.0
 */
typedef struct _WblStringSet WblStringSet;

GType         wbl_string_set_get_type                (void) G_GNUC_CONST;
#define       WBL_TYPE_STRING_SET                    (wbl_string_set_get_type ())

WblStringSet *wbl_string_set_ref_sink                (WblStringSet                *self);
WblStringSet *wbl_string_set_ref                     (WblStringSet                *self);
void          wbl_string_set_unref                   (WblStringSet                *self);

WblStringSet *wbl_string_set_new_empty               (void);
WblStringSet *wbl_string_set_new_singleton           (const gchar                 *element);
WblStringSet *wbl_string_set_new_from_object_members (JsonObject                  *obj);
WblStringSet *wbl_string_set_new_from_array_elements (JsonArray/*<owned utf8>*/   *array);

WblStringSet *wbl_string_set_dup                     (WblStringSet                *set);

WblStringSet *wbl_string_set_union                   (WblStringSet                *a,
                                                      WblStringSet                *b);
WblStringSet *wbl_string_set_union_dependencies      (WblStringSet                *set,
                                                      JsonObject                  *dependencies);

gboolean      wbl_string_set_contains                (WblStringSet                *set,
                                                      const gchar                 *member);
guint         wbl_string_set_get_size                (WblStringSet                *set);

guint         wbl_string_set_hash                    (WblStringSet                *set);
gboolean      wbl_string_set_equal                   (WblStringSet                *a,
                                                      WblStringSet                *b);

gchar        *wbl_string_set_to_string               (WblStringSet                *set);

/**
 * WblStringSetIter:
 *
 * An iterator used to iterate over the elements of a #WblStringSet. This must
 * be allocated on the stack and initialised using wbl_string_set_iter_init().
 *
 * All the fields in the #WblStringSetIter structure are private and should
 * never be accessed directly.
 *
 * Since: 0.2.0
 */
typedef struct {
	/*< private >*/
	GHashTableIter   iter;
	WblStringSet    *set;  /* unowned */
} WblStringSetIter;

void          wbl_string_set_iter_init               (WblStringSetIter            *iter,
                                                      WblStringSet                *set);
gboolean      wbl_string_set_iter_next               (WblStringSetIter            *iter,
                                                      const gchar                **member);

G_END_DECLS

#endif /* !WBL_STRING_SET_H */
