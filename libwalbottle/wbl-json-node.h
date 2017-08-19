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

#ifndef WBL_JSON_NODE_H
#define WBL_JSON_NODE_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/* Primitive type handling. Reference: draft-zyp-json-schema-04§3.5. */
/**
 * WblPrimitiveType:
 * @WBL_PRIMITIVE_TYPE_ARRAY: an array node
 * @WBL_PRIMITIVE_TYPE_BOOLEAN: a boolean value
 * @WBL_PRIMITIVE_TYPE_INTEGER: an integer value
 * @WBL_PRIMITIVE_TYPE_NUMBER: a numeric (integer or double) value
 * @WBL_PRIMITIVE_TYPE_NULL: a null value
 * @WBL_PRIMITIVE_TYPE_OBJECT: an object node
 * @WBL_PRIMITIVE_TYPE_STRING: a string value
 *
 * Reference: https://tools.ietf.org/html/draft-zyp-json-schema-04#section-3.5
 *
 * Since: 0.2.0
 */
typedef enum {
	WBL_PRIMITIVE_TYPE_ARRAY,
	WBL_PRIMITIVE_TYPE_BOOLEAN,
	WBL_PRIMITIVE_TYPE_INTEGER,
	WBL_PRIMITIVE_TYPE_NUMBER,
	WBL_PRIMITIVE_TYPE_NULL,
	WBL_PRIMITIVE_TYPE_OBJECT,
	WBL_PRIMITIVE_TYPE_STRING,
} WblPrimitiveType;

WblPrimitiveType
wbl_primitive_type_from_string    (const gchar       *str);
WblPrimitiveType
wbl_primitive_type_from_json_node (JsonNode          *node);
gboolean
wbl_primitive_type_validate       (const gchar       *str);
gboolean
wbl_primitive_type_is_a           (WblPrimitiveType   sub,
                                   WblPrimitiveType   super);

gchar *
wbl_json_number_node_to_string    (JsonNode          *node);
gint
wbl_json_number_node_comparison   (JsonNode          *a,
                                   JsonNode          *b);

guint
wbl_json_string_hash              (gconstpointer      key);
gboolean
wbl_json_string_equal             (gconstpointer      a,
                                   gconstpointer      b);
gint
wbl_json_string_compare           (gconstpointer      a,
                                   gconstpointer      b);

guint
wbl_json_node_hash                (gconstpointer      key);
gboolean
wbl_json_node_equal               (gconstpointer      a,
                                   gconstpointer      b);

G_END_DECLS

#endif /* !WBL_JSON_NODE_H */
