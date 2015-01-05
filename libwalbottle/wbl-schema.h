/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2014, 2015 <philip@tecnocode.co.uk>
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
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/**
 * WblSchemaError:
 * @WBL_SCHEMA_ERROR_MALFORMED: Loaded JSON Schema does not conform to the JSON
 *   Schema standard.
 * @WBL_SCHEMA_ERROR_INVALID: Instance does not conform to the given
 *   JSON Schema.
 *
 * Error codes for #WblSchema operations.
 *
 * Since: UNRELEASED
 */
typedef enum {
	WBL_SCHEMA_ERROR_MALFORMED = 1,
	WBL_SCHEMA_ERROR_INVALID,
} WblSchemaError;

#define WBL_SCHEMA_ERROR		wbl_schema_error_quark ()

GQuark wbl_schema_error_quark (void) G_GNUC_CONST;

/**
 * WblSchemaNode:
 *
 * A reference counted structure which represents a single schema or subschema.
 *
 * All the fields in the #WblSchemaNode structure are private and should never
 * be accessed directly.
 *
 * Since: UNRELEASED
 */
typedef struct _WblSchemaNode WblSchemaNode;

GType wbl_schema_node_get_type (void) G_GNUC_CONST;

WblSchemaNode *wbl_schema_node_ref (WblSchemaNode *self);
void wbl_schema_node_unref (WblSchemaNode *self);

const gchar *
wbl_schema_node_get_title (WblSchemaNode *self);
const gchar *
wbl_schema_node_get_description (WblSchemaNode *self);

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
 * implementations could check extension keywords. If %NULL, no validation
 * will be performed.
 * @apply_schema: Apply a parsed schema to a JSON instance, validating the
 *   instance against the schema. The default implementation applies standard
 *   JSON Schema keywords, but overriding implementations could implement extension
 *   keywords. If %NULL, no application will be performed.
 * @generate_instances: Generate a set of JSON instances which are valid and
 *   invalid for this JSON Schema. The default implementation generates for all
 *   standard JSON Schema keywords, but overriding implementations could generate
 *   for extension keywords. If %NULL, no instances will be generated.
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
	void (*apply_schema) (WblSchema *self,
	                      WblSchemaNode *root,
	                      JsonNode *instance,
	                      GError **error);
	void (*generate_instances) (WblSchema *self,
	                            WblSchemaNode *root,
	                            GPtrArray *output);
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

void
wbl_schema_apply (WblSchema *self,
                  JsonNode *instance,
                  GError **error);

/**
 * WblGenerateInstanceFlags:
 * @WBL_GENERATE_INSTANCE_NONE: No flags set.
 * @WBL_GENERATE_INSTANCE_IGNORE_VALID: Do not return valid instances.
 * @WBL_GENERATE_INSTANCE_IGNORE_INVALID: Do not return invalid instances.
 *
 * Flags affecting the generation of JSON instances for schemas using
 * wbl_schema_generate_instances().
 *
 * Since: UNRELEASED
 */
typedef enum {
	WBL_GENERATE_INSTANCE_NONE = 0,
	WBL_GENERATE_INSTANCE_IGNORE_VALID = (1 << 0),
	WBL_GENERATE_INSTANCE_IGNORE_INVALID = (1 << 1),
} WblGenerateInstanceFlags;

/**
 * WblGeneratedInstance:
 *
 * An allocated structure which represents a generated JSON instance which may
 * be valid for a given JSON Schema. Associated metadata is stored with the
 * instance, such as whether it is expected to be valid.
 *
 * All the fields in the #WblGeneratedInstance structure are private and should
 * never be accessed directly.
 *
 * Since: UNRELEASED
 */
typedef struct _WblGeneratedInstance WblGeneratedInstance;

GType
wbl_generated_instance_get_type (void) G_GNUC_CONST;

WblGeneratedInstance *
wbl_generated_instance_new_from_string (const gchar *json,
                                        gboolean valid);
WblGeneratedInstance *
wbl_generated_instance_copy (WblGeneratedInstance *self);
void
wbl_generated_instance_free (WblGeneratedInstance *self);

const gchar *
wbl_generated_instance_get_json (WblGeneratedInstance *self);

GPtrArray *
wbl_schema_generate_instances (WblSchema *self,
                               WblGenerateInstanceFlags flags);

G_END_DECLS

#endif /* !WBL_SCHEMA_H */
