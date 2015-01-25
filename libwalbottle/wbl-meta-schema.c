/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2015 <philip@tecnocode.co.uk>
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

/**
 * SECTION:wbl-meta-schema
 * @short_description: JSON meta-schema loading
 * @stability: Unstable
 * @include: libwalbottle/wbl-meta-schema.h
 *
 * JSON meta-schemas are schemas which describe the JSON schema format.
 * They are used to validate JSON schemas.
 *
 * There are currently two meta-schemas: %WBL_META_SCHEMA_META_SCHEMA and
 * %WBL_META_SCHEMA_HYPER_META_SCHEMA. The latter is a superset of the former,
 * and is designed for validating hypertext in addition to the core schema
 * keywords.
 *
 * Since: 0.1.0
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include "wbl-meta-schema.h"
#include "wbl-schema.h"

/* From the generated wbl-meta-schemas.c. */
extern GResource *wbl_meta_schemas_get_resource (void);

/* Paths within the resource file for the meta-schema files.
 *
 * This must be kept in sync with WblMetaSchemaType, which indexes it. */
static const gchar *meta_schema_paths[] = {
	"/org/walbottle/meta-schemas/meta-schema.json",
	"/org/walbottle/meta-schemas/hyper-meta-schema.json",
};

G_STATIC_ASSERT (G_N_ELEMENTS (meta_schema_paths) ==
                 WBL_META_SCHEMA_HYPER_META_SCHEMA + 1);

/**
 * wbl_meta_schema_load:
 * @meta_schema_type: type of meta-schema to load
 * @error: return location for a #GError, or %NULL
 *
 * Load a meta-schema and return an input stream for its serialised JSON data.
 *
 * The meta-schemas are stored as resources within the library, so can safely
 * be read synchronously without blocking.
 *
 * Returns: (transfer full): input stream for the meta-schema file
 *
 * Since: 0.1.0
 */
GInputStream *
wbl_meta_schema_load (WblMetaSchemaType meta_schema_type,
                      GError **error)
{
	const gchar *path;
	GResource *resource;  /* unowned */

	g_return_val_if_fail (meta_schema_type >= WBL_META_SCHEMA_META_SCHEMA &&
	                      meta_schema_type <= WBL_META_SCHEMA_HYPER_META_SCHEMA,
	                      NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Load the resource. */
	path = meta_schema_paths[meta_schema_type];
	resource = wbl_meta_schemas_get_resource ();

	return g_resource_open_stream (resource, path,
	                               G_RESOURCE_LOOKUP_FLAGS_NONE, error);
}

/**
 * wbl_meta_schema_load_schema:
 * @meta_schema_type: type of meta-schema to load
 * @error: return location for a #GError, or %NULL
 *
 * Load and parse a meta-schema, returning a #WblSchema containing it. On
 * error, @error will be set, and %NULL will be returned (rather than an empty
 * #WblSchema instance).
 *
 * Returns: (transfer full): #WblSchema instance holding the parsed and loaded
 *   meta-schema
 *
 * Since: 0.1.0
 */
WblSchema *
wbl_meta_schema_load_schema (WblMetaSchemaType meta_schema_type,
                             GError **error)
{
	WblSchema *schema = NULL;  /* owned */
	GInputStream *stream = NULL;  /* owned */
	GError *child_error = NULL;

	g_return_val_if_fail (meta_schema_type >= WBL_META_SCHEMA_META_SCHEMA &&
	                      meta_schema_type <= WBL_META_SCHEMA_HYPER_META_SCHEMA,
	                      NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* Open the resource stream. */
	stream = wbl_meta_schema_load (meta_schema_type, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return NULL;
	}

	/* Load it into a schema. */
	schema = wbl_schema_new ();
	wbl_schema_load_from_stream (schema, stream, NULL, &child_error);

	g_object_unref (stream);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		g_clear_object (&schema);
	}

	return schema;
}
