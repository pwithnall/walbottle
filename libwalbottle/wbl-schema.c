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

/**
 * SECTION:wbl-schema
 * @short_description: JSON schema parsing and representation
 * @stability: Unstable
 * @include: libwalbottle/wbl-schema.h
 *
 * TODO:
 *  - Schema parsing
 *  - Online validation of JSON values against a schema
 *  - Test case generation from a schema
 *
 * Since: UNRELEASED
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "wbl-schema.h"

/* Internal definition of a #WblSchemaNode. */
struct _WblSchemaNode {
	gint ref_count;  /* atomic */
	JsonObject *node;  /* owned */
};

GQuark
wbl_schema_error_quark (void)
{
	return g_quark_from_static_string ("wbl-schema-error-quark");
}

G_DEFINE_BOXED_TYPE (WblSchemaNode, wbl_schema_node, 
                     wbl_schema_node_ref, wbl_schema_node_unref);

/**
 * wbl_schema_node_ref:
 * @self: (transfer none): a #WblSchemaNode
 *
 * Increment the reference count of the schema node.
 *
 * Returns: (transfer full): the original schema node
 *
 * Since: UNRELEASED
 */
WblSchemaNode *
wbl_schema_node_ref (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	g_atomic_int_inc (&self->ref_count);

	return self;
}

/**
 * wbl_schema_node_unref:
 * @self: (transfer full): a #WblSchemaNode
 *
 * Decrement the reference count of the schema node.
 *
 * Since: UNRELEASED
 */
void
wbl_schema_node_unref (WblSchemaNode *self)
{
	g_return_if_fail (self != NULL);
	g_return_if_fail (self->ref_count > 0);

	if (g_atomic_int_dec_and_test (&self->ref_count)) {
		json_object_unref (self->node);
		g_slice_free (WblSchemaNode, self);
	}
}

static void
wbl_schema_dispose (GObject *object);

static void
real_validate_schema (WblSchema *self,
                      WblSchemaNode *schema,
                      GError **error);
static void
real_apply_schema (WblSchema *self,
                   WblSchemaNode *schema,
                   JsonNode *instance,
                   GError **error);

struct _WblSchemaPrivate {
	JsonParser *parser;  /* owned */
	WblSchemaNode *schema;  /* owned; NULL when not loading */
};

G_DEFINE_TYPE_WITH_PRIVATE (WblSchema, wbl_schema, G_TYPE_OBJECT)

static void
wbl_schema_class_init (WblSchemaClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = wbl_schema_dispose;

	klass->validate_schema = real_validate_schema;
	klass->apply_schema = real_apply_schema;
}

static void
wbl_schema_init (WblSchema *self)
{
	WblSchemaPrivate *priv = wbl_schema_get_instance_private (self);

	priv->parser = json_parser_new ();
}

static void
wbl_schema_dispose (GObject *object)
{
	WblSchema *self = WBL_SCHEMA (object);
	WblSchemaPrivate *priv = wbl_schema_get_instance_private (self);

	g_clear_object (&priv->parser);
	if (priv->schema != NULL) {
		wbl_schema_node_unref (priv->schema);
		priv->schema = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS (wbl_schema_parent_class)->dispose (object);
}

/* A couple of utility functions for validation. */
static gboolean
validate_regex (const gchar *regex)
{
	GRegex *r = NULL;  /* owned */
	GError *error = NULL;

	r = g_regex_new (regex, 0, 0, &error);
	if (error != NULL) {
		g_error_free (error);
		return FALSE;
	} else {
		g_regex_unref (r);
		return TRUE;
	}
}

static gboolean
validate_value_type (JsonNode *node, GType value_type)
{
	return (JSON_NODE_HOLDS_VALUE (node) &&
	        json_node_get_value_type (node) == value_type);
}

/* multipleOf. json-schema-validation§5.1.1. */
static void
validate_multiple_of (WblSchema *self,
                      JsonObject *root,
                      JsonNode *schema_node,
                      GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) <= 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("multipleOf must be a positive integer. "
		               "See json-schema-validation§5.1.1."));
	}
}

static void
apply_multiple_of (WblSchema *self,
                   JsonObject *root,
                   JsonNode *schema_node,
                   JsonNode *instance_node,
                   GError **error)
{
	if (JSON_NODE_HOLDS_VALUE (instance_node) &&
	    json_node_get_value_type (instance_node) == G_TYPE_INT64 &&
	    (json_node_get_int (instance_node) %
	     json_node_get_int (schema_node)) != 0) {
		/* TODO: error */
	}
}

/* maximum and exclusiveMaximum. json-schema-validation§5.1.2. */
static void
validate_maximum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maximum must be an integer. "
		               "See json-schema-validation§5.1.2."));
	}
}

static void
validate_exclusive_maximum (WblSchema *self,
                            JsonObject *root,
                            JsonNode *schema_node,
                            GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_BOOLEAN)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("exclusiveMaximum must be a boolean. "
		               "See json-schema-validation§5.1.2."));
	}

	if (!json_object_has_member (root, "maximum")) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maximum must be present if exclusiveMaximum is "
		               "present. See json-schema-validation§5.1.2."));
	}
}

static void
apply_maximum (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               JsonNode *instance_node,
               GError **error)
{
	gint64 maximum, instance;
	gboolean exclusive_maximum = FALSE;  /* json-schema-validation§5.1.2.3 */
	JsonNode *node;  /* unowned */

	/* Check the instance is of the right type. */
	if (!JSON_NODE_HOLDS_VALUE (instance_node) ||
	    json_node_get_value_type (instance_node) != G_TYPE_INT64) {
		return;
	}

	/* Gather various parameters. */
	maximum = json_node_get_int (schema_node);
	instance = json_node_get_int (instance_node);

	node = json_object_get_member (root, "exclusiveMaximum");
	if (node != NULL) {
		exclusive_maximum = json_node_get_boolean (node);
	}

	/* Actually perform the validation. */
	if ((!exclusive_maximum && instance > maximum) ||
	    (exclusive_maximum && instance >= maximum)) {
		/* TODO: invalid */
	}
}

/* minimum and exclusiveMinimum. json-schema-validation§5.1.3. */
static void
validate_minimum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minimum must be an integer. "
		               "See json-schema-validation§5.1.3."));
	}
}

static void
validate_exclusive_minimum (WblSchema *self,
                            JsonObject *root,
                            JsonNode *schema_node,
                            GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_BOOLEAN)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("exclusiveMinimum must be a boolean. "
		               "See json-schema-validation§5.1.3."));
	}

	if (!json_object_has_member (root, "minimum")) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minimum must be present if exclusiveMinimum is "
		               "present. See json-schema-validation§5.1.3."));
	}
}

static void
apply_minimum (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               JsonNode *instance_node,
               GError **error)
{
	gint64 minimum, instance;
	gboolean exclusive_minimum = FALSE;  /* json-schema-validation§5.1.3.3 */
	JsonNode *node;  /* unowned */

	/* Check the instance is of the right type. */
	if (!JSON_NODE_HOLDS_VALUE (instance_node) ||
	    json_node_get_value_type (instance_node) != G_TYPE_INT64) {
		return;
	}

	/* Gather various parameters. */
	minimum = json_node_get_int (schema_node);
	instance = json_node_get_int (instance_node);

	node = json_object_get_member (root, "exclusiveMinimum");
	if (node != NULL) {
		exclusive_minimum = json_node_get_boolean (node);
	}

	/* Actually perform the validation. */
	if ((!exclusive_minimum && instance < minimum) ||
	    (exclusive_minimum && instance <= minimum)) {
		/* TODO: invalid */
	}
}

/* maxLength. json-schema-validation§5.2.1. */
static void
validate_max_length (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maxLength must be a non-negative integer. "
		               "See json-schema-validation§5.2.1."));
	}
}

static void
apply_max_length (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  JsonNode *instance_node,
                  GError **error)
{
	/* TODO: Double-check that g_utf8_strlen() matches
	 * section 5.2.1.2; i.e. RFC 4627. */
	if (JSON_NODE_HOLDS_VALUE (instance_node) &&
	    json_node_get_value_type (instance_node) == G_TYPE_STRING &&
	    g_utf8_strlen (json_node_get_string (instance_node), -1) >
	    json_node_get_int (schema_node)) {
		/* TODO: invalid */
	}
}

/* minLength. json-schema-validation§5.2.2. */
static void
validate_min_length (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minLength must be a non-negative integer. "
		               "See json-schema-validation§5.2.2."));
	}
}

static void
apply_min_length (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  JsonNode *instance_node,
                  GError **error)
{
	/* TODO: Double-check that g_utf8_strlen() matches
	 * section 5.2.1.2; i.e. RFC 4627. */
	if (JSON_NODE_HOLDS_VALUE (instance_node) &&
	    json_node_get_value_type (instance_node) == G_TYPE_STRING &&
	    g_utf8_strlen (json_node_get_string (instance_node), -1) <
	    json_node_get_int (schema_node)) {
		/* TODO: invalid */
	}
}

/* pattern. json-schema-validation§5.2.3. */
static void
validate_pattern (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_STRING) ||
	    !validate_regex (json_node_get_string (schema_node))) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("pattern must be a valid regular expression. "
		               "See json-schema-validation§5.2.3."));
	}
}

static void
apply_pattern (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               JsonNode *instance_node,
               GError **error)
{
	/* TODO */
}

/* additionalItems and items. json-schema-validation§5.3.1. */
static void
validate_additional_items (WblSchema *self,
                           JsonObject *root,
                           JsonNode *schema_node,
                           GError **error)
{
	if (JSON_NODE_HOLDS_VALUE (schema_node) &&
	    json_node_get_value_type (schema_node) == G_TYPE_BOOLEAN) {
		/* Valid. */
		return;
	}

	if (JSON_NODE_HOLDS_OBJECT (schema_node)) {
		WblSchemaClass *klass;
		GError *child_error = NULL;

		klass = WBL_SCHEMA_GET_CLASS (self);

		/* Validate the schema. */
		if (klass->validate_schema != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (schema_node);

			klass->validate_schema (self, &node, &child_error);

			json_object_unref (node.node);
		}

		if (child_error == NULL) {
			/* Valid. */
			return;
		}

		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             /* Translators: The parameter is another error
		              * message. */
		             _("additionalItems must be a boolean or a valid "
		               "JSON Schema. See json-schema-validation§5.3.1: "
		               "%s"), child_error->message);
		g_error_free (child_error);

		return;
	}

	/* Invalid type. */
	g_set_error (error,
	             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
	             _("additionalItems must be a boolean or a valid JSON "
	               "Schema. See json-schema-validation§5.3.1."));
}

static void
validate_items (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                GError **error)
{
	if (JSON_NODE_HOLDS_OBJECT (schema_node)) {
		WblSchemaClass *klass;
		GError *child_error = NULL;

		klass = WBL_SCHEMA_GET_CLASS (self);

		/* Validate the schema. */
		if (klass->validate_schema != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (schema_node);

			klass->validate_schema (self, &node, &child_error);

			json_object_unref (node.node);
		}

		if (child_error == NULL) {
			/* Valid. */
			return;
		}

		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             /* Translators: The parameter is another error
		              * message. */
		             _("items must be a valid JSON Schema or an array "
		               "of valid JSON Schemas. See "
		               "json-schema-validation§5.3.1: %s"),
		             child_error->message);
		g_error_free (child_error);

		return;
	}

	if (JSON_NODE_HOLDS_ARRAY (schema_node)) {
		WblSchemaClass *klass;
		JsonArray *array;
		guint i;

		klass = WBL_SCHEMA_GET_CLASS (self);

		array = json_node_get_array (schema_node);

		for (i = 0; i < json_array_get_length (array); i++) {
			GError *child_error = NULL;
			JsonNode *array_node;

			/* Get the node. */
			array_node = json_array_get_element (array, i);

			if (!JSON_NODE_HOLDS_OBJECT (array_node)) {
				g_set_error (error,
				             WBL_SCHEMA_ERROR,
				             WBL_SCHEMA_ERROR_MALFORMED,
				             _("items must be a valid JSON "
				               "Schema or an array of valid "
				               "JSON Schemas. See "
				               "json-schema-validation§5.3.1."));

				return;
			}

			/* Validate the schema. */
			if (klass->validate_schema != NULL) {
				WblSchemaNode node;

				node.ref_count = 1;
				node.node = json_node_dup_object (array_node);

				klass->validate_schema (self, &node,
				                        &child_error);

				json_object_unref (node.node);
			}

			if (child_error == NULL) {
				/* Valid. */
				continue;
			}

			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("items must be a valid JSON Schema or "
			               "an array of valid JSON Schemas. See "
			               "json-schema-validation§5.3.1: %s"),
			             child_error->message);
			g_error_free (child_error);

			return;
		}

		/* Valid. */
		return;
	}

	/* Invalid type. */
	g_set_error (error,
	             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
	             _("items must be a valid JSON Schema or an array of valid "
	               "JSON Schemas. See json-schema-validation§5.3.1."));
}

static void
apply_items (WblSchema *self,
             JsonObject *root,
             JsonNode *schema_node,
             JsonNode *instance_node,
             GError **error)
{
	JsonNode *node;  /* unowned */
	gboolean additional_items_is_boolean;
	gboolean additional_items_boolean;

	/* Check the instance type. */
	if (!JSON_NODE_HOLDS_ARRAY (instance_node)) {
		return;
	}

	if (JSON_NODE_HOLDS_OBJECT (schema_node)) {
		/* Validation always succeeds if items is an object. */
		return;
	}

	node = json_object_get_member (root, "additionalItems");
	additional_items_is_boolean = (node != NULL &&
	                               JSON_NODE_HOLDS_VALUE (node) &&
	                               json_node_get_value_type (node) ==
	                               G_TYPE_BOOLEAN);
	additional_items_boolean = additional_items_is_boolean ?
	                           json_node_get_boolean (node) : FALSE;

	if (node == NULL ||
	    JSON_NODE_HOLDS_OBJECT (node) ||
	    (additional_items_is_boolean && additional_items_boolean)) {
		/* Validation always succeeds if additionalItems is a boolean
		 * true or an object (or not present; as the default value is
		 * an empty schema, which is an object). */
		return;
	}

	if (additional_items_is_boolean && !additional_items_boolean &&
	    JSON_NODE_HOLDS_ARRAY (schema_node)) {
		JsonArray *instance_array, *schema_array;  /* unowned */
		guint instance_size, schema_size;

		/* Validation succeeds if the instance size is less than or
		 * equal to the size of items. */
		instance_array = json_node_get_array (instance_node);
		schema_array = json_node_get_array (schema_node);

		instance_size = json_array_get_length (instance_array);
		schema_size = json_array_get_length (schema_array);

		if (instance_size > schema_size) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
			             _("Array elements do not conform to items "
			               "and additionalItems schema keywords. "
			               "See json-schema-validation§5.3.1."));
			return;
		}
	}
}

/* maxItems. json-schema-validation§5.3.2. */
static void
validate_max_items (WblSchema *self,
                    JsonObject *root,
                    JsonNode *schema_node,
                    GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maxItems must be a non-negative integer. "
		               "See json-schema-validation§5.3.2."));
	}
}

static void
apply_max_items (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 JsonNode *instance_node,
                 GError **error)
{
	/* TODO */
}

/* minItems. json-schema-validation§5.3.3. */
static void
validate_min_items (WblSchema *self,
                    JsonObject *root,
                    JsonNode *schema_node,
                    GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minItems must be a non-negative integer. "
		               "See json-schema-validation§5.3.3."));
	}
}

static void
apply_min_items (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 JsonNode *instance_node,
                 GError **error)
{
	/* TODO */
}

/* uniqueItems. json-schema-validation§5.3.3. */
static void
validate_unique_items (WblSchema *self,
                       JsonObject *root,
                       JsonNode *schema_node,
                       GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_BOOLEAN)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("uniqueItems must be a boolean. "
		               "See json-schema-validation§5.3.3."));
	}
}

static void
apply_unique_items (WblSchema *self,
                    JsonObject *root,
                    JsonNode *schema_node,
                    JsonNode *instance_node,
                    GError **error)
{
	/* TODO */
}

/* maxProperties. json-schema-validation§5.4.1. */
static void
validate_max_properties (WblSchema *self,
                         JsonObject *root,
                         JsonNode *schema_node,
                         GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maxProperties must be a non-negative integer. "
		               "See json-schema-validation§5.4.1."));
	}
}

static void
apply_max_properties (WblSchema *self,
                      JsonObject *root,
                      JsonNode *schema_node,
                      JsonNode *instance_node,
                      GError **error)
{
	/* TODO */
}

/* minProperties. json-schema-validation§5.4.2. */
static void
validate_min_properties (WblSchema *self,
                         JsonObject *root,
                         JsonNode *schema_node,
                         GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) ||
	    json_node_get_int (schema_node) < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minProperties must be a non-negative integer. "
		               "See json-schema-validation§5.4.2."));
	}
}

static void
apply_min_properties (WblSchema *self,
                      JsonObject *root,
                      JsonNode *schema_node,
                      JsonNode *instance_node,
                      GError **error)
{
	/* TODO */
}

/* required. json-schema-validation§5.4.3. */
static void
validate_required (WblSchema *self,
                   JsonObject *root,
                   JsonNode *schema_node,
                   GError **error)
{
#if 0
	if (!JSON_NODE_HOLDS_ARRAY (schema_node) ||
	    !validate_required (schema_node)) {
		/* TODO: Invalid */
	}
#endif
}

static void
apply_required (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                JsonNode *instance_node,
                GError **error)
{
	/* TODO */
}

typedef void
(*KeywordValidateFunc) (WblSchema *self,
                        JsonObject *root,
                        JsonNode *schema_node,
                        GError **error);
typedef void
(*KeywordApplyFunc) (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     JsonNode *instance_node,
                     GError **error);

/* TODO: Docs */
typedef struct {
	const gchar *name;
	KeywordValidateFunc validate;  /* NULL if validation always succeeds */
	KeywordApplyFunc apply;  /* NULL if application always succeeds */
} KeywordData;

/* TODO: Docs */
static const KeywordData json_schema_keywords[] = {
	/* json-schema-validation§5.1.1 */
	{ "multipleOf", validate_multiple_of, apply_multiple_of },
	/* json-schema-validation§5.1.2 */
	{ "maximum", validate_maximum, apply_maximum },
	{ "exclusiveMaximum", validate_exclusive_maximum, NULL },
	/* json-schema-validation§5.1.3. */
	{ "minimum", validate_minimum, apply_minimum },
	{ "exclusiveMinimum", validate_exclusive_minimum, NULL },
	/* json-schema-validation§5.2.1 */
	{ "maxLength", validate_max_length, apply_max_length },
	/* json-schema-validation§5.2.2 */
	{ "minLength", validate_min_length, apply_min_length },
	/* json-schema-validation§5.2.3 */
	{ "pattern", validate_pattern, apply_pattern },
	/* json-schema-validation§5.3.1 */
	{ "additionalItems", validate_additional_items, NULL },
	{ "items", validate_items, apply_items },
	/* json-schema-validation§5.3.2 */
	{ "maxItems", validate_max_items, apply_max_items },
	/* json-schema-validation§5.3.3 */
	{ "minItems", validate_min_items, apply_min_items },
	/* json-schema-validation§5.3.3 */
	{ "uniqueItems", validate_unique_items, apply_unique_items },
	/* json-schema-validation§5.4.1 */
	{ "maxProperties", validate_max_properties, apply_max_properties },
	/* json-schema-validation§5.4.2 */
	{ "minProperties", validate_min_properties, apply_min_properties },
	/* json-schema-validation§5.4.3 */
	{ "required", validate_required, apply_required },

	/* TODO:
	 *  • additionalProperties (json-schema-validation§5.4.4)
	 *  • properties (json-schema-validation§5.4.4)
	 *  • patternProperties (json-schema-validation§5.4.4)
	 *  • dependencies (json-schema-validation§5.4.5)
	 *  • enum (json-schema-validation§5.5.1)
	 *  • type (json-schema-validation§5.5.2)
	 *  • allOf (json-schema-validation§5.5.3)
	 *  • anyOf (json-schema-validation§5.5.4)
	 *  • oneOf (json-schema-validation§5.5.5)
	 *  • not (json-schema-validation§5.5.6)
	 *  • definitions (json-schema-validation§5.5.7)
	 *  • title (json-schema-validation§6.1)
	 *  • description (json-schema-validation§6.1)
	 *  • default (json-schema-validation§6.2)
	 *  • format (json-schema-validation§7.1)
	 */
};

static void
real_validate_schema (WblSchema *self,
                      WblSchemaNode *schema,
                      GError **error)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (json_schema_keywords); i++) {
		const KeywordData *keyword = &json_schema_keywords[i];
		JsonNode *schema_node;  /* unowned */
		GError *child_error = NULL;

		schema_node = json_object_get_member (schema->node,
		                                      keyword->name);

		if (schema_node != NULL && keyword->validate != NULL) {
			keyword->validate (self, schema->node,
			                   schema_node, &child_error);

			if (child_error != NULL) {
				g_propagate_error (error, child_error);
				return;
			}
		}
	}
}

static void
real_apply_schema (WblSchema *self,
                   WblSchemaNode *schema,
                   JsonNode *instance,
                   GError **error)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (json_schema_keywords); i++) {
		const KeywordData *keyword = &json_schema_keywords[i];
		JsonNode *schema_node;  /* unowned */
		GError *child_error = NULL;

		schema_node = json_object_get_member (schema->node,
		                                      keyword->name);

		if (schema_node != NULL && keyword->apply != NULL) {
			keyword->apply (self, schema->node,
			                schema_node, instance, &child_error);

			if (child_error != NULL) {
				g_propagate_error (error, child_error);
				return;
			}
		}
	}
}

/**
 * wbl_schema_new:
 *
 * Creates a new #WblSchema with default properties.
 *
 * Return value: (transfer full): a new #WblSchema; unref with g_object_unref()
 *
 * Since: UNRELEASED
 */
WblSchema *
wbl_schema_new (void)
{
	return g_object_new (WBL_TYPE_SCHEMA, NULL);
}

/**
 * wbl_schema_load_from_data:
 * TODO
 *
 * Since: UNRELEASED
 */
void
wbl_schema_load_from_data (WblSchema *self,
                           const gchar *data,
                           gssize length,
                           GError **error)
{
	GInputStream *stream = NULL;  /* owned */

	g_return_if_fail (WBL_IS_SCHEMA (self));
	g_return_if_fail (data != NULL);
	g_return_if_fail (length >= -1);
	g_return_if_fail (error == NULL || *error == NULL);

	if (length < 0) {
		length = strlen (data);
	}

	stream = g_memory_input_stream_new_from_data (g_memdup (data, length),
	                                              length, g_free);
	wbl_schema_load_from_stream (self, stream, NULL, error);
	g_object_unref (stream);
}

/**
 * wbl_schema_load_from_file:
 * TODO
 *
 * Since: UNRELEASED
 */
void
wbl_schema_load_from_file (WblSchema *self,
                           const gchar *filename,
                           GError **error)
{
	GFile *file = NULL;  /* owned */
	GFileInputStream *stream = NULL;  /* owned */

	g_return_if_fail (WBL_IS_SCHEMA (self));
	g_return_if_fail (filename != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	file = g_file_new_for_path (filename);
	stream = g_file_read (file, NULL, error);
	g_object_unref (file);

	if (stream == NULL) {
		return;
	}

	wbl_schema_load_from_stream (self, G_INPUT_STREAM (stream),
	                             NULL, error);
	g_object_unref (stream);
}

static void
start_loading (WblSchema *self)
{
	WblSchemaPrivate *priv;

	priv = wbl_schema_get_instance_private (self);

	/* Clear the old wrapper node first. */
	if (priv->schema != NULL) {
		wbl_schema_node_unref (priv->schema);
		priv->schema = NULL;
	}
}

static void
finish_loading (WblSchema *self, GError **error)
{
	WblSchemaClass *klass;
	WblSchemaPrivate *priv;
	JsonNode *root;  /* unowned */

	klass = WBL_SCHEMA_GET_CLASS (self);
	priv = wbl_schema_get_instance_private (self);

	/* A schema must be a JSON object. json-schema-core§3.2. */
	root = json_parser_get_root (priv->parser);

	if (root == NULL || !JSON_NODE_HOLDS_OBJECT (root)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("Root node of schema is not an object. "
		               "See json-schema-core§3.2."));
		return;
	}

	/* Schema. */
	priv->schema = g_slice_new0 (WblSchemaNode);
	priv->schema->ref_count = 1;
	priv->schema->node = json_node_dup_object (root);

	/* Validate the schema. */
	if (klass->validate_schema != NULL) {
		klass->validate_schema (self, priv->schema, error);
	}
}

/**
 * wbl_schema_load_from_stream:
 * TODO
 *
 * Since: UNRELEASED
 */
void
wbl_schema_load_from_stream (WblSchema *self,
                             GInputStream *stream,
                             GCancellable *cancellable,
                             GError **error)
{
	GError *child_error = NULL;

	WblSchemaPrivate *priv;

	g_return_if_fail (WBL_IS_SCHEMA (self));
	g_return_if_fail (G_IS_INPUT_STREAM (stream));
	g_return_if_fail (cancellable == NULL ||
	                  G_IS_CANCELLABLE (cancellable));
	g_return_if_fail (error == NULL || *error == NULL);

	priv = wbl_schema_get_instance_private (self);

	start_loading (self);
	json_parser_load_from_stream (priv->parser, stream,
	                              cancellable, &child_error);

	if (child_error != NULL) {
		goto done;
	}

	finish_loading (self, &child_error);

done:
	if (child_error != NULL) {
		g_propagate_error (error, child_error);
	}
}

static void
load_from_stream_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	WblSchema *self;
	WblSchemaPrivate *priv;
	GTask *task = NULL;  /* owned */
	GError *child_error = NULL;

	self = WBL_SCHEMA (source_object);
	priv = wbl_schema_get_instance_private (self);
	task = G_TASK (user_data);

	/* Clear any previous loads. */
	start_loading (self);

	/* Finish loading from the stream. */
	json_parser_load_from_stream_finish (priv->parser,
	                                     result, &child_error);

	if (child_error != NULL) {
		goto done;
	}

	/* Finish loading and validate. */
	finish_loading (self, &child_error);

done:
	if (child_error != NULL) {
		g_task_return_error (task, child_error);
	} else {
		g_task_return_boolean (task, TRUE);
	}

	g_object_unref (task);
}

/**
 * wbl_schema_load_from_stream_async:
 * TODO
 *
 * Since: UNRELEASED
 */
void
wbl_schema_load_from_stream_async (WblSchema *self,
                                   GInputStream *stream,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback,
                                   gpointer user_data)
{
	WblSchemaPrivate *priv;
	GTask *task = NULL;  /* owned */

	g_return_if_fail (WBL_IS_SCHEMA (self));
	g_return_if_fail (G_IS_INPUT_STREAM (stream));
	g_return_if_fail (cancellable == NULL ||
	                  G_IS_CANCELLABLE (cancellable));

	priv = wbl_schema_get_instance_private (self);

	task = g_task_new (self, cancellable, callback, user_data);
	json_parser_load_from_stream_async (priv->parser, stream, cancellable,
	                                    load_from_stream_cb,
	                                    g_object_ref (task));
	g_object_unref (task);
}

/**
 * wbl_schema_load_from_stream_finish:
 * TODO
 *
 * Since: UNRELEASED
 */
void
wbl_schema_load_from_stream_finish (WblSchema *self,
                                    GAsyncResult *result,
                                    GError **error)
{
	GTask *task;  /* unowned */

	g_return_if_fail (WBL_IS_SCHEMA (self));
	g_return_if_fail (g_task_is_valid (result, self));
	g_return_if_fail (error == NULL || *error == NULL);

	task = G_TASK (result);

	g_task_propagate_boolean (task, error);
}

/**
 * wbl_schema_get_root:
 * @self: a #WblSchema
 *
 * Get the root schema from the parsed schema document. If no document has been
 * parsed using wbl_schema_load_from_stream_async() yet, or if the parsed
 * document was invalid, %NULL is returned.
 *
 * The returned object is valid as long as the #WblSchema is alive and has not
 * started parsing another document.
 *
 * Returns: (transfer none) (nullable): a JSON schema root object, or %NULL
 *
 * Since: UNRELEASED
 */
WblSchemaNode *
wbl_schema_get_root (WblSchema *self)
{
	WblSchemaPrivate *priv;

	g_return_val_if_fail (WBL_IS_SCHEMA (self), NULL);

	priv = wbl_schema_get_instance_private (self);

	return priv->schema;
}

/**
 * wbl_schema_apply:
 * @self: a #WblSchema
 * @instance: the JSON instance to validate against the schema
 * @error: return location for a #GError, or %NULL
 *
 * Apply a JSON Schema to a JSON instance, validating whether the instance
 * conforms to the schema. The instance may be any kind of JSON node, and does
 * not necessarily have to be a JSON object.
 *
 * Since: UNRELEASED
 */
void
wbl_schema_apply (WblSchema *self,
                  JsonNode *instance,
                  GError **error)
{
	WblSchemaClass *klass;
	WblSchemaPrivate *priv;

	g_return_if_fail (WBL_IS_SCHEMA (self));
	g_return_if_fail (instance != NULL);
	g_return_if_fail (error == NULL || *error == NULL);

	klass = WBL_SCHEMA_GET_CLASS (self);
	priv = wbl_schema_get_instance_private (self);

	/* Apply the schema to the instance. */
	if (klass->apply_schema != NULL) {
		klass->apply_schema (self, priv->schema, instance, error);
	}
}
