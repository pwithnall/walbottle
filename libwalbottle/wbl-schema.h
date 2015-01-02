/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2014 <philip@tecnocode.co.uk>
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

#ifndef WBL_SCHEMA_H
#define WBL_SCHEMA_H

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>  /* TODO: eliminate JSON public dep */

G_BEGIN_DECLS

/**
 * WblSchemaError:
 * @WBL_SCHEMA_ERROR_MESSAGE_MISMATCH: In comparison mode, a message received from the client did not match the next message in the current trace file.
 *
 * Error codes for #WblSchema operations.
 *
 * Since: UNRELEASED
 */
typedef enum {
	WBL_SCHEMA_ERROR_MESSAGE_MISMATCH = 1,
} WblSchemaError;

#define WBL_SCHEMA_ERROR		wbl_schema_error_quark ()

GQuark wbl_schema_error_quark (void) G_GNUC_CONST;

/**
 * WblSchemaNode:
 * @ref_count: reference count, must be accessed atomically
 * @node: JSON node at the root of the schema
 *
 * A reference counted structure which represents a single schema or subschema.
 *
 * Since: UNRELEASED
 */
typedef struct {
	gint ref_count;  /* atomic */
	JsonObject *node;  /* owned */
} WblSchemaNode;

GType wbl_schema_node_get_type (void) G_GNUC_CONST;

WblSchemaNode *wbl_schema_node_ref (WblSchemaNode *self);
void wbl_schema_node_unref (WblSchemaNode *self);

#define WBL_TYPE_SCHEMA			(wbl_schema_get_type ())
#define WBL_SCHEMA(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), WBL_TYPE_SCHEMA, WblSchema))
#define WBL_SCHEMA_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), WBL_TYPE_SCHEMA, WblSchemaClass))
#define WBL_IS_SCHEMA(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), WBL_TYPE_SCHEMA))
#define WBL_IS_SCHEMA_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), WBL_TYPE_SCHEMA))
#define WBL_SCHEMA_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), WBL_TYPE_SCHEMA, WblSchemaClass))

typedef struct _WblSchemaPrivate	WblSchemaPrivate;

/**
 * WblSchema:
 *
 * All the fields in the #WblSchema structure are private and should never
 * be accessed directly.
 *
 * Since: UNRELEASED
 */
typedef struct {
	/*< private >*/
	GObject parent;
} WblSchema;

/**
 * WblSchemaClass:
 * @validate_schema: Walk over a parsed schema and validate that it is a valid
 * schema. The default implementation checks against JSON Schema, but overriding
 * implementations could check extension properties. If %NULL, no validation
 * will be performed.
 *
 * Most of the fields in the #WblSchemaClass structure are private and should
 * never be accessed directly.
 *
 * Since: UNRELEASED
 */
typedef struct {
	/*< private >*/
	GObjectClass parent;

	/*< public >*/
	void (*validate_schema) (WblSchema *self,
	                         WblSchemaNode *root,
	                         GError **error);
} WblSchemaClass;

GType wbl_schema_get_type (void) G_GNUC_CONST;

WblSchema *wbl_schema_new (void) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

void wbl_schema_load_from_data (WblSchema *self,
                                const gchar *data,
                                gssize length,
                                GError **error);

void wbl_schema_load_from_file (WblSchema *self,
                                const gchar *filename,
                                GError **error);

void wbl_schema_load_from_stream (WblSchema *self,
                                  GInputStream *stream,
                                  GCancellable *cancellable,
                                  GError **error);
void wbl_schema_load_from_stream_async (WblSchema *self,
                                        GInputStream *stream,
                                        GCancellable *cancellable,
                                        GAsyncReadyCallback callback,
                                        gpointer user_data);
void wbl_schema_load_from_stream_finish (WblSchema *self,
                                         GAsyncResult *result,
                                         GError **error);

WblSchemaNode *
wbl_schema_get_root (WblSchema *self);

G_END_DECLS

#endif /* !WBL_SCHEMA_H */
