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

GQuark
wbl_schema_error_quark (void)
{
	return g_quark_from_static_string ("wbl-schema-error-quark");
}

/* Internal definition of a #WblSchemaNode. */
struct _WblSchemaNode {
	gint ref_count;  /* atomic */
	JsonObject *node;  /* owned */
};

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

/**
 * wbl_schema_node_get_root:
 * @self: a #WblSchemaNode
 *
 * Get the JSON object forming the root node of this schema or subschema.
 *
 * Returns: (transfer none): schema’s root node
 *
 * Since: UNRELEASED
 */
JsonObject *
wbl_schema_node_get_root (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	return self->node;
}

/**
 * wbl_schema_node_get_title:
 * @self: a #WblSchemaNode
 *
 * Get the title metadata of this schema or subschema, if set. This should
 * briefly state the purpose of the instance produced by this schema.
 *
 * Returns: (nullable): schema’s title, or %NULL if unset
 *
 * Since: UNRELEASED
 */
const gchar *
wbl_schema_node_get_title (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	return json_object_get_string_member (self->node, "title");
}

/**
 * wbl_schema_node_get_description:
 * @self: a #WblSchemaNode
 *
 * Get the description metadata of this schema or subschema, if set. This should
 * explain in depth the purpose of the instance produced by this schema.
 *
 * Returns: (nullable): schema’s description, or %NULL if unset
 *
 * Since: UNRELEASED
 */
const gchar *
wbl_schema_node_get_description (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	return json_object_get_string_member (self->node, "description");
}

/**
 * wbl_schema_node_get_default:
 * @self: a #WblSchemaNode
 *
 * Get the default value for instances of this schema or subschema, if set. This
 * may not validate against the schema.
 *
 * Returns: (nullable): schema’s default value, or %NULL if unset
 *
 * Since: UNRELEASED
 */
JsonNode *
wbl_schema_node_get_default (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	return json_object_get_member (self->node, "default");
}

/* Internal definition of a #WblGeneratedInstance. */
struct _WblGeneratedInstance {
	gchar *json;  /* owned */
	gboolean valid;
};

G_DEFINE_BOXED_TYPE (WblGeneratedInstance, wbl_generated_instance,
                     wbl_generated_instance_copy, wbl_generated_instance_free);

/**
 * wbl_generated_instance_new_from_string:
 * @json: serialised JSON to use for the instance
 * @valid: whether the instance is expected to validate against the schema
 *
 * Create a new #WblGeneratedInstance from the given serialised JSON instance
 * and associated metadata.
 *
 * Returns: (transfer full): newly allocated #WblGeneratedInstance
 *
 * Since: UNRELEASED
 */
WblGeneratedInstance *
wbl_generated_instance_new_from_string (const gchar *json,
                                        gboolean valid)
{
	WblGeneratedInstance *instance = NULL;  /* owned */

	g_return_val_if_fail (json != NULL, NULL);

	instance = g_slice_new0 (WblGeneratedInstance);
	instance->json = g_strdup (json);
	instance->valid = valid;

	return instance;
}

/**
 * wbl_generated_instance_copy:
 * @self: (transfer none): a #WblGeneratedInstance
 *
 * Copy a #WblGeneratedInstance into a newly allocated region of memory. This is
 * a deep copy.
 *
 * Returns: (transfer full): newly allocated #WblGeneratedInstance
 *
 * Since: UNRELEASED
 */
WblGeneratedInstance *
wbl_generated_instance_copy (WblGeneratedInstance *self)
{
	WblGeneratedInstance *instance = NULL;  /* owned */

	instance = g_slice_new0 (WblGeneratedInstance);
	instance->json = g_strdup (self->json);
	instance->valid = self->valid;

	return instance;
}

/**
 * wbl_generated_instance_free:
 * @self: (transfer full): a #WblGeneratedInstance
 *
 * Free an allocated #WblGeneratedInstance.
 *
 * Since: UNRELEASED
 */
void
wbl_generated_instance_free (WblGeneratedInstance *self)
{
	g_free (self->json);

	g_slice_free (WblGeneratedInstance, self);
}

/**
 * wbl_generated_instance_get_json:
 * @self: a #WblGeneratedInstance
 *
 * Get the string form of the generated JSON instance.
 *
 * Returns: string form of the generated instance
 *
 * Since: UNRELEASED
 */
const gchar *
wbl_generated_instance_get_json (WblGeneratedInstance *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->json;
}

/* Primitive type handling. Reference: json-schema-core§3.5. */
typedef enum {
	WBL_PRIMITIVE_TYPE_ARRAY,
	WBL_PRIMITIVE_TYPE_BOOLEAN,
	WBL_PRIMITIVE_TYPE_INTEGER,
	WBL_PRIMITIVE_TYPE_NUMBER,
	WBL_PRIMITIVE_TYPE_NULL,
	WBL_PRIMITIVE_TYPE_OBJECT,
	WBL_PRIMITIVE_TYPE_STRING,
} WblPrimitiveType;

/* Indexed by #WblPrimitiveType. */
static const gchar *wbl_primitive_type_names[] = {
	"array",
	"boolean",
	"integer",
	"number",
	"null",
	"object",
	"string",
};

G_STATIC_ASSERT (G_N_ELEMENTS (wbl_primitive_type_names) ==
                 WBL_PRIMITIVE_TYPE_STRING + 1);

static WblPrimitiveType
primitive_type_from_string (const gchar *str)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (wbl_primitive_type_names); i++) {
		if (g_strcmp0 (str, wbl_primitive_type_names[i]) == 0) {
			return (WblPrimitiveType) i;
		}
	}

	g_assert_not_reached ();
}

static WblPrimitiveType
primitive_type_from_json_node (JsonNode *node)
{
	switch (json_node_get_node_type (node)) {
	case JSON_NODE_OBJECT:
		return WBL_PRIMITIVE_TYPE_OBJECT;
	case JSON_NODE_ARRAY:
		return WBL_PRIMITIVE_TYPE_ARRAY;
	case JSON_NODE_NULL:
		return WBL_PRIMITIVE_TYPE_NULL;
	case JSON_NODE_VALUE: {
		GType value_type = json_node_get_value_type (node);

		if (value_type == G_TYPE_BOOLEAN) {
			return WBL_PRIMITIVE_TYPE_BOOLEAN;
		} else if (value_type == G_TYPE_STRING) {
			return WBL_PRIMITIVE_TYPE_STRING;
		} else if (value_type == G_TYPE_DOUBLE) {
			return WBL_PRIMITIVE_TYPE_NUMBER;
		} else if (value_type == G_TYPE_INT64) {
			return WBL_PRIMITIVE_TYPE_INTEGER;
		} else {
			g_assert_not_reached ();
		}
	}
	default:
		g_assert_not_reached ();
	}
}

static gboolean
validate_primitive_type (const gchar *str)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (wbl_primitive_type_names); i++) {
		if (g_strcmp0 (str, wbl_primitive_type_names[i]) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
primitive_type_is_a (WblPrimitiveType sub,
                     WblPrimitiveType super)
{
	return (super == sub ||
	        (super == WBL_PRIMITIVE_TYPE_NUMBER &&
	         sub == WBL_PRIMITIVE_TYPE_INTEGER));
}

/* JSON Schema number (integer or float) comparison.
 *
 * Compare the numbers stored in two nodes, returning a strcmp()-like value.
 * The nodes must each contain an integer or a double, but do not have to
 * contain the same type. */
static gint
number_node_comparison (JsonNode *a,
                        JsonNode *b)
{
	GType a_type, b_type;
	gint retval;

	a_type = json_node_get_value_type (a);
	b_type = json_node_get_value_type (b);

	/* Note: These comparisons cannot use arithmetic, both because that
	 * would introduce floating point errors; and also because it could
	 * cause overflow or underflow. */
	if (a_type == G_TYPE_INT64 && b_type == G_TYPE_INT64) {
		gint64 a_value, b_value;

		/* Integer comparison is possible. */
		a_value = json_node_get_int (a);
		b_value = json_node_get_int (b);

		if (a_value < b_value) {
			retval = -1;
		} else if (a_value > b_value) {
			retval = 1;
		} else {
			retval = 0;
		}
	} else {
		gdouble a_value, b_value;

		/* Have to compare by doubles. */
		a_value = json_node_get_double (a);
		b_value = json_node_get_double (b);

		if (a_value < b_value) {
			retval = -1;
		} else if (a_value > b_value) {
			retval = 1;
		} else {
			retval = 0;
		}
	}

	return retval;
}

/* Convert a JSON Schema number node (float or integer) to a string. */
static gchar *  /* transfer full */
number_node_to_string (JsonNode *node)
{
	GType node_type;

	node_type = json_node_get_value_type (node);

	if (node_type == G_TYPE_INT64) {
		return g_strdup_printf ("%" G_GINT64_FORMAT,
		                        json_node_get_int (node));
	} else if (node_type == G_TYPE_DOUBLE) {
		return g_strdup_printf ("%f", json_node_get_double (node));
	} else {
		g_assert_not_reached ();
	}
}

/* JSON string equality via hash tables.
 *
 * Note: Member names are compared byte-wise, without applying any Unicode
 * decomposition or normalisation. This is not explicitly mentioned in the JSON
 * standard (ECMA-404), but is assumed. */
static guint
string_hash (gconstpointer key)
{
	return g_str_hash (key);
}

static gboolean
string_equal (gconstpointer a,
              gconstpointer b)
{
	return g_str_equal (a, b);
}

/* JSON instance equality via hash tables, json-schema-core§3.6. */
static guint
node_hash (gconstpointer key)
{
	JsonNode *node;  /* unowned */
	WblPrimitiveType type;

	/* Arbitrary magic values, chosen to hopefully not collide when combined
	 * with other hash values (e.g. for %WBL_PRIMITIVE_TYPE_ARRAY). */
	const guint true_hash = 175;
	const guint false_hash = 8823;
	const guint null_hash = 33866;
	const guint empty_array_hash = 7735;
	const guint empty_object_hash = 23545;

	node = (JsonNode *) key;
	type = primitive_type_from_json_node (node);

	switch (type) {
	case WBL_PRIMITIVE_TYPE_BOOLEAN:
		return json_node_get_boolean (node) ? true_hash : false_hash;
	case WBL_PRIMITIVE_TYPE_NULL:
		return null_hash;
	case WBL_PRIMITIVE_TYPE_STRING:
		return string_hash (json_node_get_string (node));
	case WBL_PRIMITIVE_TYPE_INTEGER: {
		gint64 v = json_node_get_int (node);
		return g_int64_hash (&v);
	}
	case WBL_PRIMITIVE_TYPE_NUMBER: {
		gdouble v = json_node_get_double (node);
		return g_double_hash (&v);
	}
	case WBL_PRIMITIVE_TYPE_ARRAY: {
		JsonArray *a;  /* unowned */
		guint len;
		JsonNode *first_element;  /* unowned */

		/* FIXME: Don’t know how effective this is. We shouldn’t just
		 * use the array length, as single-element arrays are common.
		 * Combine it with the hash of the first element to get
		 * something suitably well distributed. */
		a = json_node_get_array (node);
		len = json_array_get_length (a);

		if (len == 0) {
			return empty_array_hash;
		}

		first_element = json_array_get_element (a, 0);

		return (len | node_hash (first_element));
	}
	case WBL_PRIMITIVE_TYPE_OBJECT: {
		JsonObject *o;  /* unowned */
		guint size;

		/* FIXME: Objects are a bit more complex: their members are
		 * unordered, so we can’t pluck out the first one without (e.g.)
		 * retrieving all member names and ordering them alphabetically
		 * first. That’s too expensive, so just stick with size */
		o = json_node_get_object (node);
		size = json_object_get_size (o);

		/* Try and reduce collisions with low-valued integer values. */
		size += empty_object_hash;

		return g_int_hash (&size);
	}
	default:
		g_assert_not_reached ();
	}
}

/* Check whether two sets of JSON object member names are equal. The comparison
 * is unordered. */
static gboolean
member_names_equal (GList/*<unowned utf8>*/ *member_names_a  /* unowned */,
                    GList/*<unowned utf8>*/ *member_names_b  /* unowned */)
{
	GHashTable/*<unowned utf8, unowned utf8>*/ *set = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */
	gboolean retval = TRUE;

	/* Special case for empty objects. */
	if (member_names_a == NULL || member_names_b == NULL) {
		return (member_names_a == member_names_b);
	}

	/* Add all the names from @member_names_a to a set, then remove all the
	 * names from @member_names_b and throw an error if any of them don’t
	 * exist. */
	set = g_hash_table_new (string_hash, string_equal);

	for (l = member_names_a; l != NULL; l = l->next) {
		g_hash_table_add (set, l->data);
	}

	for (l = member_names_b; l != NULL; l = l->next) {
		if (!g_hash_table_remove (set, l->data)) {
			retval = FALSE;
			break;
		}
	}

	/* Check the sizes: as we’ve removed all the matching elements, the set
	 * should now be empty. */
	if (g_hash_table_size (set) != 0) {
		retval = FALSE;
	}

	g_hash_table_unref (set);

	return retval;
}

/* Reference: json-schema-core§3.6. */
static gboolean
node_equal (gconstpointer a,
            gconstpointer b)
{
	JsonNode *node_a, *node_b;  /* unowned */
	WblPrimitiveType type_a, type_b;

	node_a = (JsonNode *) a;
	node_b = (JsonNode *) b;

	type_a = primitive_type_from_json_node (node_a);
	type_b = primitive_type_from_json_node (node_b);

	/* Eliminate mismatched types rapidly. */
	if (!primitive_type_is_a (type_a, type_b) &&
	    !primitive_type_is_a (type_b, type_a)) {
		return FALSE;
	}

	switch (type_a) {
	case WBL_PRIMITIVE_TYPE_NULL:
		return TRUE;
	case WBL_PRIMITIVE_TYPE_BOOLEAN:
		return (json_node_get_boolean (node_a) ==
		        json_node_get_boolean (node_b));
	case WBL_PRIMITIVE_TYPE_STRING:
		return string_equal (json_node_get_string (node_a),
		                     json_node_get_string (node_b));
	case WBL_PRIMITIVE_TYPE_NUMBER:
	case WBL_PRIMITIVE_TYPE_INTEGER: {
		gdouble val_a, val_b;

		/* Integer comparison doesn’t need to involve doubles. */
		if (type_a == WBL_PRIMITIVE_TYPE_INTEGER &&
		    type_b == WBL_PRIMITIVE_TYPE_INTEGER) {
			return (json_node_get_int (node_a) ==
			        json_node_get_int (node_b));
		}

		/* Everything else does. We can use bitwise double equality
		 * here, since we’re not doing any calculations which could
		 * introduce floating point error. We expect that the doubles
		 * in the JSON nodes come directly from strtod() or similar,
		 * so should be bitwise equal for equal string
		 * representations.
		 *
		 * Interesting background reading:
		 * http://randomascii.wordpress.com/2012/06/26/\
		 *   doubles-are-not-floats-so-dont-compare-them/
		 */
		if (type_a == WBL_PRIMITIVE_TYPE_INTEGER) {
			val_a = json_node_get_int (node_a);
		} else {
			val_a = json_node_get_double (node_a);
		}

		if (type_b == WBL_PRIMITIVE_TYPE_INTEGER) {
			val_b = json_node_get_int (node_b);
		} else {
			val_b = json_node_get_double (node_b);
		}

		return (val_a == val_b);
	}
	case WBL_PRIMITIVE_TYPE_ARRAY: {
		JsonArray *array_a, *array_b;  /* unowned */
		guint length_a, length_b, i;

		array_a = json_node_get_array (node_a);
		array_b = json_node_get_array (node_b);

		/* Check lengths. */
		length_a = json_array_get_length (array_a);
		length_b = json_array_get_length (array_b);

		if (length_a != length_b) {
			return FALSE;
		}

		/* Check elements. */
		for (i = 0; i < length_a; i++) {
			JsonNode *child_a, *child_b;  /* unowned */

			child_a = json_array_get_element (array_a, i);
			child_b = json_array_get_element (array_b, i);

			if (!node_equal (child_a, child_b)) {
				return FALSE;
			}
		}

		return TRUE;
	}
	case WBL_PRIMITIVE_TYPE_OBJECT: {
		JsonObject *object_a, *object_b;
		guint size_a, size_b;
		GList/*<unowned utf8>*/ *member_names_a = NULL;  /* owned */
		GList/*<unowned utf8>*/ *member_names_b = NULL;  /* owned */
		GList/*<unowned utf8>*/ *l = NULL;  /* unowned */
		gboolean retval = TRUE;

		object_a = json_node_get_object (node_a);
		object_b = json_node_get_object (node_b);

		/* Check sizes. */
		size_a = json_object_get_size (object_a);
		size_b = json_object_get_size (object_b);

		if (size_a != size_b) {
			return FALSE;
		}

		/* Check member names. */
		member_names_a = json_object_get_members (object_a);
		member_names_b = json_object_get_members (object_b);

		if (!member_names_equal (member_names_a, member_names_b)) {
			retval = FALSE;
		}

		/* Check member values. */
		for (l = member_names_a; l != NULL && retval; l = l->next) {
			JsonNode *child_a, *child_b;  /* unowned */

			child_a = json_object_get_member (object_a, l->data);
			child_b = json_object_get_member (object_b, l->data);

			if (!node_equal (child_a, child_b)) {
				retval = FALSE;
				break;
			}
		}

		g_list_free (member_names_b);
		g_list_free (member_names_a);

		return retval;
	}
	default:
		g_assert_not_reached ();
	}
}

/* Schemas. */
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
static void
real_generate_instances (WblSchema *self,
                         WblSchemaNode *schema,
                         GPtrArray *output);

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
	klass->generate_instances = real_generate_instances;
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

/* Validate a node is a non-empty array of unique strings. */
static gboolean
validate_non_empty_unique_string_array (JsonNode *schema_node)
{
	JsonArray *schema_array;  /* unowned */
	guint i;
	GHashTable/*<unowned utf8, unowned utf8>*/ *set = NULL;  /* owned */
	gboolean valid = TRUE;

	if (!JSON_NODE_HOLDS_ARRAY (schema_node)) {
		return FALSE;
	}

	schema_array = json_node_get_array (schema_node);

	if (json_array_get_length (schema_array) == 0) {
		return FALSE;
	}

	set = g_hash_table_new (string_hash, string_equal);

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */
		const gchar *child_str;

		child_node = json_array_get_element (schema_array, i);

		/* Check it’s a string. */
		if (!validate_value_type (child_node, G_TYPE_STRING)) {
			valid = FALSE;
			break;
		}

		/* Check for uniqueness. */
		child_str = json_node_get_string (child_node);

		if (g_hash_table_contains (set, child_str)) {
			valid = FALSE;
			break;
		}

		g_hash_table_add (set, (gpointer) child_str);
	}

	g_hash_table_unref (set);

	return valid;
}

/* Validate a node is a non-empty array of valid JSON schemas.
 * If any of the schemas is invalid, return the first validation error as
 * @schema_error. */
static gboolean
validate_schema_array (WblSchema *self,
                       JsonNode *schema_node,
                       GError **schema_error)
{
	WblSchemaClass *klass;
	JsonArray *schema_array;  /* unowned */
	guint i;

	if (!JSON_NODE_HOLDS_ARRAY (schema_node)) {
		return FALSE;
	}

	schema_array = json_node_get_array (schema_node);
	klass = WBL_SCHEMA_GET_CLASS (self);

	if (json_array_get_length (schema_array) == 0) {
		return FALSE;
	}

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */
		GError *child_error = NULL;

		child_node = json_array_get_element (schema_array, i);

		if (!JSON_NODE_HOLDS_OBJECT (child_node)) {
			return FALSE;
		}

		/* Validate the child schema. */
		if (klass->validate_schema != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (child_node);

			klass->validate_schema (self, &node, &child_error);

			json_object_unref (node.node);
		}

		if (child_error != NULL) {
			/* Invalid. */
			g_set_error (schema_error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("allOf must be a non-empty array of "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.5.3: "
			               "%s"), child_error->message);
			g_error_free (child_error);

			return FALSE;
		}
	}

	/* Valid. */
	return TRUE;
}

/* Apply an array of schemas to an instance and count how many succeed. */
static guint
apply_schema_array (WblSchema *self,
                    JsonArray *schema_array,
                    JsonNode *instance_node)
{
	WblSchemaClass *klass;
	guint i, n_successes;

	klass = WBL_SCHEMA_GET_CLASS (self);
	n_successes = 0;

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */
		GError *child_error = NULL;

		child_node = json_array_get_element (schema_array, i);

		if (klass->apply_schema != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (child_node);

			klass->apply_schema (self, &node, instance_node,
			                     &child_error);

			json_object_unref (node.node);
		}

		/* Count successes. */
		if (child_error == NULL) {
			n_successes++;
		}

		g_clear_error (&child_error);
	}

	return n_successes;
}

/* Generate instances which match zero or more of the schemas in a schema array,
 * according to the #WblMatchPredicate, which selects instances based on the
 * number of schemas they match. */
typedef gboolean
(*WblMatchPredicate) (guint n_matching_schemas,
                      guint n_schemas);

static void
generate_schema_array (WblSchema *self,
                       JsonArray *schema_array,
                       WblMatchPredicate predicate,
                       GPtrArray *output)
{
	WblSchemaClass *klass;
	guint i, j, n_schemas;
	GPtrArray/*<owned WblGeneratedInstance>*/ *_output = NULL;  /* owned */
	JsonParser *parser = NULL;  /* owned */

	n_schemas = json_array_get_length (schema_array);
	klass = WBL_SCHEMA_GET_CLASS (self);

	/* Generate instances for all schemas. */
	_output = g_ptr_array_new_with_free_func ((GDestroyNotify) wbl_generated_instance_free);

	for (i = 0; i < n_schemas; i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);

		if (klass->generate_instances != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (child_node);

			klass->generate_instances (self, &node, _output);

			json_object_unref (node.node);
		}
	}

	/* Find any instances which match the number of schemas approved by the
	 * predicate. */
	parser = json_parser_new ();

	for (j = 0; j < _output->len; j++) {
		WblGeneratedInstance *instance;  /* unowned */
		const gchar *instance_str;
		JsonNode *instance_node;  /* unowned */
		guint n_matching_schemas;
		GError *child_error = NULL;

		/* Get the instance. */
		instance = _output->pdata[j];
		instance_str = wbl_generated_instance_get_json (instance);
		json_parser_load_from_data (parser, instance_str, -1,
		                            &child_error);
		g_assert_no_error (child_error);

		instance_node = json_parser_get_root (parser);
		n_matching_schemas = 0;

		/* Check it against each schema. */
		for (i = 0; i < n_schemas; i++) {
			JsonNode *child_node;  /* unowned */

			child_node = json_array_get_element (schema_array, i);

			if (klass->apply_schema != NULL) {
				WblSchemaNode node;

				node.ref_count = 1;
				node.node = json_node_dup_object (child_node);

				klass->apply_schema (self, &node, instance_node,
				                     &child_error);

				if (child_error == NULL) {
					n_matching_schemas++;
				}

				g_clear_error (&child_error);
				json_object_unref (node.node);
			}
		}

		/* Does it match the predicate? */
		if (predicate (n_matching_schemas, n_schemas)) {
			g_ptr_array_add (output,
			                 wbl_generated_instance_copy (instance));
		}
	}

	g_object_unref (parser);
	g_ptr_array_unref (_output);
}

/* A couple of utility functions for generation. */
static void
generate_set_string (GPtrArray *output,
                     const gchar *str,
                     gboolean valid)
{
	g_ptr_array_add (output,
	                 wbl_generated_instance_new_from_string (str, valid));
}

static void
generate_take_string (GPtrArray *output,
                      gchar *str,  /* transfer full */
                      gboolean valid)
{
	generate_set_string (output, str, valid);
	g_free (str);
}

static void
generate_take_builder (GPtrArray *output,
                       JsonBuilder *builder,  /* transfer none */
                       gboolean valid)
{
	JsonNode *instance_root = NULL;  /* owned */
	JsonGenerator *generator = NULL;  /* owned */

	generator = json_generator_new ();

	instance_root = json_builder_get_root (builder);
	json_generator_set_root (generator, instance_root);
	json_node_free (instance_root);

	generate_take_string (output,
	                      json_generator_to_data (generator, NULL),
	                      valid);

	json_builder_reset (builder);

	g_object_unref (generator);
}

static void
generate_null_array (GPtrArray *output,
                     guint n_elements,
                     gboolean valid)
{
	JsonBuilder *builder = NULL;  /* owned */
	guint i;

	builder = json_builder_new ();

	json_builder_begin_array (builder);

	for (i = 0; i < n_elements; i++) {
		json_builder_add_null_value (builder);
	}

	json_builder_end_array (builder);

	generate_take_builder (output, builder, valid);

	g_object_unref (builder);
}

static void
generate_null_properties (GPtrArray *output,
                          guint n_properties,
                          gboolean valid)
{
	JsonBuilder *builder = NULL;  /* owned */
	guint i;

	builder = json_builder_new ();

	json_builder_begin_object (builder);

	for (i = 0; i < n_properties; i++) {
		gchar *member_name;

		member_name = g_strdup_printf ("%u", i);

		json_builder_set_member_name (builder, member_name);
		json_builder_add_null_value (builder);

		g_free (member_name);
	}

	json_builder_end_object (builder);

	generate_take_builder (output, builder, valid);

	g_object_unref (builder);
}

static void
generate_filled_string (GPtrArray *output,
                        gsize length,  /* in Unicode characters */
                        gunichar fill,
                        gboolean valid)
{
	gchar *str = NULL;  /* owned */
	gchar fill_utf8[6];
	gint fill_len;  /* in bytes */
	gsize i;  /* in bytes */

	/* Convert the character to UTF-8 and convert the string length to
	 * bytes. */
	fill_len = g_unichar_to_utf8 (fill, fill_utf8);

	if (G_MAXSIZE / fill_len <= length) {
		return;
	}

	length *= fill_len;

	/* Check there is enough room for a nul terminator and
	 * quotation marks. */
	if (G_MAXSIZE - 3 < length) {
		return;
	}

	/* Allocate a string. This might be huge. */
	str = g_malloc (length +
	                2 +  /* quotation marks */
	                1  /* nul terminator */);

	/* Starting quotation mark. */
	i = 0;
	str[i++] = '"';

	/* Fill with the unichar. Note: @length is divisible by @fill_len. */
	for (; i < length + 1; i += fill_len) {
		memcpy (str + i, fill_utf8, fill_len);
	}

	/* Terminating quotation mark. */
	str[i++] = '"';

	/* Fill the rest with nul bytes. */
	for (; i < length + 2 + 1; i++) {
		str[i] = '\0';
	}

	generate_take_string (output, str, valid);
}

static gchar *  /* transfer full */
json_int_to_string (gint64 i)
{
	return g_strdup_printf ("%" G_GINT64_FORMAT, i);
}

static gchar *  /* transfer full */
json_double_to_string (gdouble i)
{
	return g_strdup_printf ("%f", i);
}

/* Check whether @obj has a property named after each string in @property_array,
 * which must be a non-empty array of unique strings. */
static gboolean
object_has_properties (JsonObject *obj,
                       JsonArray *property_array)
{
	guint i, property_array_length;

	property_array_length = json_array_get_length (property_array);

	for (i = 0; i < property_array_length; i++) {
		JsonNode *property;  /* unowned */
		const gchar *property_str;

		property = json_array_get_element (property_array, i);
		property_str = json_node_get_string (property);

		if (!json_object_has_member (obj, property_str)) {
			return FALSE;
		}
	}

	return TRUE;
}

/* multipleOf. json-schema-validation§5.1.1. */
static void
validate_multiple_of (WblSchema *self,
                      JsonObject *root,
                      JsonNode *schema_node,
                      GError **error)
{
	if ((!validate_value_type (schema_node, G_TYPE_INT64) ||
	     json_node_get_int (schema_node) <= 0) &&
	    (!validate_value_type (schema_node, G_TYPE_DOUBLE) ||
	     json_node_get_double (schema_node) <= 0.0)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("multipleOf must be a positive number. "
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
	gboolean retval;
	GType instance_type, schema_type;

	if (!JSON_NODE_HOLDS_VALUE (instance_node)) {
		return;
	}

	instance_type = json_node_get_value_type (instance_node);
	schema_type = json_node_get_value_type (schema_node);

	/* Type check. */
	if (instance_type != G_TYPE_INT64 &&
	    instance_type != G_TYPE_DOUBLE) {
		return;
	}

	if (instance_type == G_TYPE_INT64 &&
	    schema_type == G_TYPE_INT64) {
		/* Integer comparison. */
		retval = ((json_node_get_int (instance_node) %
		          json_node_get_int (schema_node)) == 0);
	} else {
		gdouble instance_value, schema_value;
		gdouble factor;

		/* Double comparison. */
		if (instance_type == G_TYPE_INT64) {
			instance_value = json_node_get_int (instance_node);
		} else {
			instance_value = json_node_get_double (instance_node);
		}

		if (schema_type == G_TYPE_INT64) {
			schema_value = json_node_get_int (schema_node);
		} else {
			schema_value = json_node_get_double (schema_node);
		}

		factor = instance_value / schema_value;
		retval = (((gint64) factor) == factor);
	}

	if (!retval) {
		gchar *str = NULL;  /* owned */

		str = number_node_to_string (schema_node);
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be a multiple of %s "
		               "due to the multipleOf schema keyword. "
		               "See json-schema-validation§5.1.1."), str);
		g_free (str);
	}
}

static void
generate_multiple_of (WblSchema *self,
                      JsonObject *root,
                      JsonNode *schema_node,
                      GPtrArray *output)
{
	GType schema_type;

	schema_type = json_node_get_value_type (schema_node);

	/* Standard outputs. */
	generate_set_string (output, "0", TRUE);
	generate_set_string (output, "0.0", TRUE);

	if (schema_type == G_TYPE_INT64) {
		gint64 multiplicand;

		multiplicand = json_node_get_int (schema_node);

		generate_take_string (output,
		                      json_int_to_string (multiplicand), TRUE);
		generate_take_string (output,
		                      json_int_to_string (multiplicand * 2),
		                      TRUE);

		if (multiplicand != 1) {
			generate_take_string (output,
			                      json_int_to_string (multiplicand + 1),
			                      FALSE);
		}
	} else if (schema_type == G_TYPE_DOUBLE) {
		gdouble multiplicand;

		multiplicand = json_node_get_double (schema_node);

		generate_take_string (output,
		                      json_double_to_string (multiplicand), TRUE);
		generate_take_string (output,
		                      json_double_to_string (multiplicand * 2.0),
		                      TRUE);

		if (multiplicand != 0.1) {
			generate_take_string (output,
			                      json_double_to_string (multiplicand + 0.1),
			                      FALSE);
		}
	} else {
		g_assert_not_reached ();
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
	if (!exclusive_maximum && instance > maximum) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be less than or equal to %"
		               G_GINT64_FORMAT " due to the maximum schema "
		               "keyword. See json-schema-validation§5.1.2."),
		             maximum);
	} else if (exclusive_maximum && instance >= maximum) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be less than %" G_GINT64_FORMAT " "
		               "due to the maximum and exclusiveMaximum schema "
		               "keywords. See json-schema-validation§5.1.2."),
		             maximum);
	}
}

static void
generate_maximum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GPtrArray *output)
{
	gint64 maximum;
	gboolean exclusive_maximum = FALSE;  /* json-schema-validation§5.1.2.3 */
	JsonNode *node;  /* unowned */

	maximum = json_node_get_int (schema_node);

	node = json_object_get_member (root, "exclusiveMaximum");
	if (node != NULL) {
		exclusive_maximum = json_node_get_boolean (node);
	}

	if (maximum > G_MININT64) {
		generate_take_string (output, json_int_to_string (maximum - 1),
		                      TRUE);
	}

	generate_take_string (output, json_int_to_string (maximum),
	                      !exclusive_maximum);

	if (maximum < G_MAXINT64) {
		generate_take_string (output, json_int_to_string (maximum + 1),
		                      FALSE);
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
	if (!exclusive_minimum && instance < minimum) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be greater than or equal to %"
		               G_GINT64_FORMAT " due to the minimum schema "
		               "keyword. See json-schema-validation§5.1.3."),
		             minimum);
	} else if (exclusive_minimum && instance <= minimum) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be greater than %" G_GINT64_FORMAT
		               " due to the minimum and exclusiveMinimum schema"
		               " keywords. See json-schema-validation§5.1.3."),
		             minimum);
	}
}

static void
generate_minimum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GPtrArray *output)
{
	gint64 minimum;
	gboolean exclusive_minimum = FALSE;  /* json-schema-validation§5.1.3.3 */
	JsonNode *node;  /* unowned */

	minimum = json_node_get_int (schema_node);

	node = json_object_get_member (root, "exclusiveMinimum");
	if (node != NULL) {
		exclusive_minimum = json_node_get_boolean (node);
	}

	if (minimum > G_MININT64) {
		generate_take_string (output, json_int_to_string (minimum - 1),
		                      FALSE);
	}

	generate_take_string (output, json_int_to_string (minimum),
	                      !exclusive_minimum);

	if (minimum < G_MAXINT64) {
		generate_take_string (output, json_int_to_string (minimum + 1),
		                      TRUE);
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
	if (JSON_NODE_HOLDS_VALUE (instance_node) &&
	    json_node_get_value_type (instance_node) == G_TYPE_STRING &&
	    g_utf8_strlen (json_node_get_string (instance_node), -1) >
	    json_node_get_int (schema_node)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be at most %" G_GINT64_FORMAT " "
		               "characters long due to the maxLength schema "
		               "keyword. See json-schema-validation§5.2.1."),
		             json_node_get_int (schema_node));
	}
}

static void
generate_max_length (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GPtrArray *output)
{
	gint64 max_length;

	max_length = json_node_get_int (schema_node);

	/* Generate strings which are @max_length and (@max_length + 1) ASCII
	 * characters long. */
	generate_filled_string (output, max_length, '0', TRUE);

	if (max_length < G_MAXINT64) {
		generate_filled_string (output, max_length + 1, '0', FALSE);
	}

	/* Generate strings which are @max_length and (@max_length + 1)
	 * non-ASCII (multi-byte UTF-8) characters long. */
	generate_filled_string (output, max_length, 0x1F435  /* 🐵 */, TRUE);

	if (max_length < G_MAXINT64) {
		generate_filled_string (output, max_length + 1,
		                        0x1F435  /* 🐵 */, FALSE);
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
	if (JSON_NODE_HOLDS_VALUE (instance_node) &&
	    json_node_get_value_type (instance_node) == G_TYPE_STRING &&
	    g_utf8_strlen (json_node_get_string (instance_node), -1) <
	    json_node_get_int (schema_node)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be at least %" G_GINT64_FORMAT " "
		               "characters long due to the maxLength schema "
		               "keyword. See json-schema-validation§5.2.2."),
		             json_node_get_int (schema_node));
	}
}

static void
generate_min_length (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GPtrArray *output)
{
	gint64 min_length;

	min_length = json_node_get_int (schema_node);

	/* Generate strings which are @min_length and (@min_length - 1) ASCII
	 * characters long. */
	generate_filled_string (output, min_length, '0', TRUE);

	if (min_length > 0) {
		generate_filled_string (output, min_length - 1, '0', FALSE);
	}

	/* Generate strings which are @min_length and (@min_length - 1)
	 * non-ASCII (multi-byte UTF-8) characters long. */
	generate_filled_string (output, min_length, 0x1F435  /* 🐵 */, TRUE);

	if (min_length > 0) {
		generate_filled_string (output, min_length - 1,
		                        0x1F435  /* 🐵 */, FALSE);
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
	const gchar *regex_str;
	const gchar *instance_str;
	GRegex *regex = NULL;  /* owned */
	GError *child_error = NULL;

	/* Validation succeeds if the instance is the wrong type. */
	if (!validate_value_type (instance_node, G_TYPE_STRING)) {
		return;
	}

	instance_str = json_node_get_string (schema_node);

	/* Any errors in the regex should have been caught in
	 * validate_pattern() */
	regex_str = json_node_get_string (schema_node);
	regex = g_regex_new (regex_str, 0, 0, &child_error);
	g_assert_no_error (child_error);

	if (!g_regex_match (regex, instance_str, 0, NULL)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             /* Translators: The parameter is a regular
		              * expression. */
		             _("Value must match the regular expression ‘%s’ "
		               "from the pattern schema keyword. "
		               "See json-schema-validation§5.2.3."),
		             regex_str);
	}

	g_regex_unref (regex);
}

static void
generate_pattern (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GPtrArray *output)
{
	/* FIXME: Generate two constant strings for the moment. In the future,
	 * we should generate strings which are guaranteed to pass and fail
	 * application of the schema to the instance; but that requires
	 * executing the regexp state machine, which is a bit more involved. */
	generate_set_string (output, "\"\"", FALSE);
	generate_set_string (output, "\"non-empty\"", FALSE);
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

static void
generate_items (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                GPtrArray *output)
{
	guint items_length = 0;

	/* If items is an array, we care about its length. */
	if (JSON_NODE_HOLDS_ARRAY (schema_node)) {
		JsonArray *items_array;  /* unowned */

		items_array = json_node_get_array (schema_node);
		items_length = json_array_get_length (items_array);
	}

	/* Always have an empty array. */
	if (items_length > 0) {
		generate_set_string (output, "[]", TRUE);
	}

	/* Add arrays at @items_length and (@items_length + 1). */
	generate_null_array (output, items_length, TRUE);
	generate_null_array (output, items_length + 1, TRUE);
	/* FIXME: @valid should depend on additionalItems here. */
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
	JsonArray *instance_array;  /* unowned */
	gint64 max_items, instance_items;

	/* Check the type first. */
	if (!JSON_NODE_HOLDS_ARRAY (instance_node)) {
		return;
	}

	max_items = json_node_get_int (schema_node);
	instance_array = json_node_get_array (instance_node);
	instance_items = json_array_get_length (instance_array);

	if (instance_items > max_items) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Array must have at most %" G_GINT64_FORMAT " "
		               "elements due to the maxItems schema keyword. "
		               "See json-schema-validation§5.3.2."),
		             max_items);
	}
}

static void
generate_max_items (WblSchema *self,
                    JsonObject *root,
                    JsonNode *schema_node,
                    GPtrArray *output)
{
	gint64 max_items;

	max_items = json_node_get_int (schema_node);

	generate_null_array (output, max_items, TRUE);

	if (max_items < G_MAXINT64) {
		generate_null_array (output, max_items + 1, FALSE);
	}
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
	JsonArray *instance_array;  /* unowned */
	gint64 min_items, instance_items;

	/* Check the type first. */
	if (!JSON_NODE_HOLDS_ARRAY (instance_node)) {
		return;
	}

	min_items = json_node_get_int (schema_node);
	instance_array = json_node_get_array (instance_node);
	instance_items = json_array_get_length (instance_array);

	if (instance_items < min_items) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Array must have at least %" G_GINT64_FORMAT " "
		               "elements due to the minItems schema keyword. "
		               "See json-schema-validation§5.3.3."),
		             min_items);
	}
}

static void
generate_min_items (WblSchema *self,
                    JsonObject *root,
                    JsonNode *schema_node,
                    GPtrArray *output)
{
	gint64 min_items;

	min_items = json_node_get_int (schema_node);

	generate_null_array (output, min_items, TRUE);

	if (min_items > 0) {
		generate_null_array (output, min_items - 1, FALSE);
	}
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
	gboolean unique_items;
	JsonArray *instance_array;  /* unowned */
	GHashTable/*<unowned JsonNode,
	             unowned JsonNode>*/ *unique_hash = NULL;  /* owned */
	guint i;

	/* Check type. */
	if (!JSON_NODE_HOLDS_ARRAY (instance_node)) {
		return;
	}

	unique_items = json_node_get_boolean (schema_node);

	if (!unique_items) {
		return;
	}

	/* Check the array elements are unique. */
	instance_array = json_node_get_array (instance_node);
	unique_hash = g_hash_table_new (node_hash, node_equal);

	for (i = 0; i < json_array_get_length (instance_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (instance_array, i);

		if (g_hash_table_contains (unique_hash, child_node)) {
			g_set_error (error,
			             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
			             _("Array must have unique elements due to "
			               "the uniqueItems schema keyword. "
			               "See json-schema-validation§5.3.4."));
			break;
		}

		g_hash_table_add (unique_hash, child_node);
	}

	g_hash_table_unref (unique_hash);
}

static void
generate_unique_items (WblSchema *self,
                       JsonObject *root,
                       JsonNode *schema_node,
                       GPtrArray *output)
{
	if (json_node_get_boolean (schema_node)) {
		/* Assume generate_null_array() uses non-unique elements. */
		generate_null_array (output, 1, TRUE);
		generate_null_array (output, 2, FALSE);
	}
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
	JsonObject *instance_object;  /* unowned */
	gint64 max_properties, instance_properties;

	/* Check the type first. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	max_properties = json_node_get_int (schema_node);
	instance_object = json_node_get_object (instance_node);
	instance_properties = json_object_get_size (instance_object);

	if (instance_properties > max_properties) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Object must have at most %" G_GINT64_FORMAT " "
		               "properties due to the maxProperties schema "
		               "keyword. See json-schema-validation§5.4.1."),
		             max_properties);
	}
}

static void
generate_max_properties (WblSchema *self,
                         JsonObject *root,
                         JsonNode *schema_node,
                         GPtrArray *output)
{
	gint64 max_properties;

	max_properties = json_node_get_int (schema_node);

	/* Generate objects with @max_properties properties and
	 * (@max_properties + 1) properties. */
	generate_null_properties (output, max_properties, TRUE);

	if (max_properties < G_MAXINT64) {
		generate_null_properties (output, max_properties + 1, FALSE);
	}
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
	JsonObject *instance_object;  /* unowned */
	gint64 min_properties, instance_properties;

	/* Check the type first. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	min_properties = json_node_get_int (schema_node);
	instance_object = json_node_get_object (instance_node);
	instance_properties = json_object_get_size (instance_object);

	if (instance_properties < min_properties) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Object must have at least %" G_GINT64_FORMAT " "
		               "properties due to the minProperties schema "
		               "keyword. See json-schema-validation§5.4.2."),
		             min_properties);
	}
}

static void
generate_min_properties (WblSchema *self,
                         JsonObject *root,
                         JsonNode *schema_node,
                         GPtrArray *output)
{
	gint64 min_properties;

	min_properties = json_node_get_int (schema_node);

	/* Generate objects with @min_properties properties and
	 * (@min_properties - 1) properties. */
	generate_null_properties (output, min_properties, TRUE);

	if (min_properties > 0) {
		generate_null_properties (output, min_properties - 1, FALSE);
	}
}

/* required. json-schema-validation§5.4.3. */
static void
validate_required (WblSchema *self,
                   JsonObject *root,
                   JsonNode *schema_node,
                   GError **error)
{
	if (!validate_non_empty_unique_string_array (schema_node)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("required must be a non-empty array of unique "
		               "strings. See json-schema-validation§5.4.3."));
	}
}

static void
apply_required (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                JsonNode *instance_node,
                GError **error)
{
	JsonArray *schema_array;  /* unowned */
	JsonObject *instance_object;  /* unowned */
	GHashTable/*<unowned utf8, unowned utf8>*/ *set = NULL;  /* owned */
	GList/*<unowned utf8>*/ *instance_member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */
	guint i;

	/* Quick type check. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	schema_array = json_node_get_array (schema_node);
	instance_object = json_node_get_object (instance_node);
	instance_member_names = json_object_get_members (instance_object);

	set = g_hash_table_new (string_hash, string_equal);

	/* Put the required member names in the @set. */
	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);
		g_hash_table_add (set,
		                  (gpointer) json_node_get_string (child_node));
	}

	/* Check each of the member names against the @set. */
	for (l = instance_member_names; l != NULL; l = l->next) {
		const gchar *member_name;

		member_name = l->data;

		if (!g_hash_table_contains (set, member_name)) {
			g_set_error (error,
			             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
			             /* Translators: The parameter is a JSON
			              * property name. */
			             _("Object must have property ‘%s’ due to "
			               "the required schema keyword. "
			               "See json-schema-validation§5.4.3."),
			             member_name);
			break;
		}
	}

	g_hash_table_unref (set);
	g_list_free (instance_member_names);
}

static void
generate_required (WblSchema *self,
                   JsonObject *root,
                   JsonNode *schema_node,
                   GPtrArray *output)
{
	JsonBuilder *builder = NULL;  /* owned */
	JsonArray *schema_array;  /* unowned */
	guint i;

	schema_array = json_node_get_array (schema_node);

	builder = json_builder_new ();

	/* Missing each required property. */
	for (i = 0; i < json_array_get_length (schema_array); i++) {
		guint j;

		json_builder_begin_object (builder);

		for (j = 0; j < json_array_get_length (schema_array); j++) {
			const gchar *member_name;

			/* Add element j unless (i == j). */
			if (i == j) {
				continue;
			}

			member_name = json_array_get_string_element (schema_array, j);
			json_builder_set_member_name (builder, member_name);
			json_builder_add_null_value (builder);
		}

		json_builder_end_object (builder);

		generate_take_builder (output, builder, FALSE);
	}

	/* With each required property. */
	json_builder_begin_object (builder);

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		const gchar *member_name;

		member_name = json_array_get_string_element (schema_array, i);
		json_builder_set_member_name (builder, member_name);
		json_builder_add_null_value (builder);
	}

	json_builder_end_object (builder);

	generate_take_builder (output, builder, FALSE);

	g_object_unref (builder);
}

/* additionalProperties, properties, patternProperties.
 * json-schema-validation§5.4.4. */
static void
validate_additional_properties (WblSchema *self,
                                JsonObject *root,
                                JsonNode *schema_node,
                                GError **error)
{
	WblSchemaClass *klass;
	GError *child_error = NULL;

	if (validate_value_type (schema_node, G_TYPE_BOOLEAN)) {
		/* Valid. */
		return;
	}

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("additionalProperties must be a boolean or a "
		               "valid JSON Schema. "
		               "See json-schema-validation§5.4.4."));
		return;
	}

	/* Validate the child schema. */
	if (klass->validate_schema != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (schema_node);

		klass->validate_schema (self, &node, &child_error);

		json_object_unref (node.node);
	}

	if (child_error != NULL) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             /* Translators: The parameter is another error
		              * message. */
		             _("additionalProperties must be a boolean or a "
		               "valid JSON Schema. "
		               "See json-schema-validation§5.4.4: %s"),
		             child_error->message);
		g_error_free (child_error);

		return;
	}
}

static void
validate_properties (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GError **error)
{
	WblSchemaClass *klass;
	JsonObject *schema_object;  /* unowned */
	GList/*<unowned utf8>*/ *member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l;  /* unowned */

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("properties must be an object of valid JSON "
		               "Schemas. See json-schema-validation§5.4.4."));

		return;
	}

	schema_object = json_node_get_object (schema_node);
	member_names = json_object_get_members (schema_object);
	klass = WBL_SCHEMA_GET_CLASS (self);

	for (l = member_names; l != NULL; l = l->next) {
		JsonNode *child_node;  /* unowned */
		GError *child_error = NULL;

		child_node = json_object_get_member (schema_object, l->data);

		if (!JSON_NODE_HOLDS_OBJECT (child_node)) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             _("properties must be an object of "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.4.4."));

			break;
		}

		/* Validate the child schema. */
		if (klass->validate_schema != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (child_node);

			klass->validate_schema (self, &node, &child_error);

			json_object_unref (node.node);
		}

		if (child_error != NULL) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("properties must be an object of "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.4.4: "
			               "%s"), child_error->message);
			g_error_free (child_error);

			break;
		}
	}

	g_list_free (member_names);
}

static void
validate_pattern_properties (WblSchema *self,
                             JsonObject *root,
                             JsonNode *schema_node,
                             GError **error)
{
	WblSchemaClass *klass;
	JsonObject *schema_object;  /* unowned */
	GList/*<unowned utf8>*/ *member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l;  /* unowned */

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("patternProperties must be an object of valid "
		               "JSON Schemas. See "
		               "json-schema-validation§5.4.4."));

		return;
	}

	schema_object = json_node_get_object (schema_node);
	member_names = json_object_get_members (schema_object);
	klass = WBL_SCHEMA_GET_CLASS (self);

	for (l = member_names; l != NULL; l = l->next) {
		JsonNode *child_node;  /* unowned */
		const gchar *member_name;
		GError *child_error = NULL;

		member_name = l->data;
		child_node = json_object_get_member (schema_object,
		                                     member_name);

		/* Validate the member name is a valid regex and the value is
		 * a valid JSON Schema. */
		if (!validate_regex (member_name) ||
		    !JSON_NODE_HOLDS_OBJECT (child_node)) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             _("patternProperties must be an object of "
			               "valid regular expressions mapping to "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.4.4."));

			break;
		}

		/* Validate the child schema. */
		if (klass->validate_schema != NULL) {
			WblSchemaNode node;

			node.ref_count = 1;
			node.node = json_node_dup_object (child_node);

			klass->validate_schema (self, &node, &child_error);

			json_object_unref (node.node);
		}

		if (child_error != NULL) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("patternProperties must be an object of "
			               "valid regular expressions mapping to "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.4.4: "
			               "%s"), child_error->message);
			g_error_free (child_error);

			break;
		}
	}

	g_list_free (member_names);
}

/* Version of g_list_remove() which does string comparison of @data, rather
 * than pointer comparison. */
static GList *
list_remove_string (GList *list, const gchar *data)
{
	GList *l;

	l = g_list_find_custom (list, data, (GCompareFunc) g_strcmp0);
	if (l != NULL) {
		return g_list_delete_link (list, l);
	}

	return list;
}

static void
apply_properties (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  JsonNode *instance_node,
                  GError **error)
{
	JsonNode *ap_node;  /* unowned */
	JsonNode *pp_node;  /* unowned */
	gboolean ap_is_false;
	JsonObject *instance_object;  /* unowned */
	JsonObject *properties_object;  /* unowned */
	JsonObject *pp_object = NULL;  /* unowned; nullable */
	GList/*<unowned utf8>*/ *set_s = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_p = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_pp = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */

	/* If additionalProperties is missing, its default value is an empty
	 * schema. json-schema-validation§5.4.4.3. */
	ap_node = json_object_get_member (root, "additionalProperties");
	ap_is_false = (ap_node != NULL &&
	               validate_value_type (ap_node, G_TYPE_BOOLEAN) &&
	               !json_node_get_boolean (ap_node));

	/* As per json-schema-validation§5.4.4.2, validation always succeeds if
	 * additionalProperties is effectively true. */
	if (!ap_is_false) {
		return;
	}

	/* Validation succeeds if the instance is not an object. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	instance_object = json_node_get_object (instance_node);
	properties_object = json_node_get_object (schema_node);
	pp_node = json_object_get_member (root, "patternProperties");
	pp_object = (pp_node != NULL) ? json_node_get_object (pp_node) : NULL;

	/* Follow the algorithm in json-schema-validation§5.4.4.4. */
	set_s = json_object_get_members (instance_object);
	set_p = json_object_get_members (properties_object);
	if (pp_object != NULL) {
		set_pp = json_object_get_members (pp_object);
	}

	/* Remove from @set_s all elements of @set_p, if any. */
	for (l = set_p; l != NULL; l = l->next) {
		const gchar *member = l->data;

		set_s = list_remove_string (set_s, member);
	}

	/* For each regex in @set_pp, remove all elements of @set_s which this
	 * regex matches. */
	for (l = set_pp; l != NULL; l = l->next) {
		const gchar *regex_str = l->data;
		GRegex *regex = NULL;  /* owned */
		GList/*<unowned utf8>*/ *k = NULL;  /* unowned */
		GError *child_error = NULL;

		/* Construct the regex. Should never fail due to being validated
		 * in validate_pattern_properties(). */
		regex = g_regex_new (regex_str, 0, 0, &child_error);
		g_assert_no_error (child_error);

		for (k = set_s; k != NULL; k = k->next) {
			const gchar *member = k->data;

			if (g_regex_match (regex, member, 0, NULL)) {
				GList *prev = k->prev;
				set_s = g_list_delete_link (set_s, k);
				k = prev;
			}
		}

		g_regex_unref (regex);
	}

	/* Validation of the instance succeeds if @set_s is empty. */
	if (set_s != NULL) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Object properties do not conform to "
		               "additionalProperties, properties and "
		               "patternProperties schema keywords. "
		               "See json-schema-validation§5.4.4."));
	}

	g_list_free (set_pp);
	g_list_free (set_p);
	g_list_free (set_s);
}

static void
generate_properties (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GPtrArray *output)
{
	WblSchemaClass *klass;
	JsonObject *schema_object;  /* unowned */
	GList/*<unowned utf8>*/ *member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */
	GPtrArray/*<owned WblGeneratedInstance>*/ *_output = NULL;  /* owned */

	schema_object = json_node_get_object (schema_node);
	member_names = json_object_get_members (schema_object);
	_output = g_ptr_array_new_with_free_func ((GDestroyNotify) wbl_generated_instance_free);
	klass = WBL_SCHEMA_GET_CLASS (self);

	/* Generate instances for all property schemas and wrap them in
	 * objects. */
	for (l = member_names; l != NULL; l = l->next) {
		JsonNode *child_node;  /* unowned */
		const gchar *member_name;

		member_name = l->data;
		child_node = json_object_get_member (schema_object,
		                                     member_name);

		if (klass->generate_instances != NULL) {
			WblSchemaNode node;
			guint i;

			/* Generate the child instances. */
			node.ref_count = 1;
			node.node = json_node_dup_object (child_node);

			klass->generate_instances (self, &node, _output);

			json_object_unref (node.node);

			/* Wrap them in objects. */
			for (i = 0; i < _output->len; i++) {
				gchar *json = NULL;  /* owned */
				const gchar *instance_json;
				WblGeneratedInstance *instance;  /* unowned */

				instance = _output->pdata[i];
				instance_json = wbl_generated_instance_get_json (instance);
				json = g_strdup_printf ("{\"%s\":%s}",
				                        member_name,
				                        instance_json);

				generate_take_string (output, json, TRUE);
			}

			g_ptr_array_set_size (_output, 0);
		}
	}

	g_ptr_array_unref (_output);
	g_list_free (member_names);
}

/* dependencies. json-schema-validation§5.4.5. */
static void
validate_dependencies (WblSchema *self,
                       JsonObject *root,
                       JsonNode *schema_node,
                       GError **error)
{
	JsonObject *schema_object;  /* unowned */
	GList/*<unowned JsonNode>*/ *schema_values = NULL;  /* owned */
	GList/*<unowned JsonNode>*/ *l = NULL;  /* unowned */
	gboolean valid = TRUE;

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		valid = FALSE;
		goto done;
	}

	schema_object = json_node_get_object (schema_node);
	schema_values = json_object_get_values (schema_object);

	for (l = schema_values; l != NULL; l = l->next) {
		JsonNode *child_node;  /* unowned */

		child_node = l->data;

		if (JSON_NODE_HOLDS_OBJECT (child_node)) {
			WblSchemaClass *klass;
			GError *child_error = NULL;

			klass = WBL_SCHEMA_GET_CLASS (self);

			/* Must be a valid JSON Schema. */
			if (klass->validate_schema != NULL) {
				WblSchemaNode node;

				node.ref_count = 1;
				node.node = json_node_dup_object (child_node);

				klass->validate_schema (self, &node,
				                        &child_error);

				json_object_unref (node.node);
			}

			if (child_error != NULL) {
				g_error_free (child_error);
				valid = FALSE;
				break;
			}
		} else if (JSON_NODE_HOLDS_ARRAY (child_node)) {
			/* Must be a non-empty array of unique strings. */
			if (!validate_non_empty_unique_string_array (child_node)) {
				valid = FALSE;
				break;
			}
		} else {
			valid = FALSE;
			break;
		}
	}

	g_list_free (schema_values);

done:
	if (!valid) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("dependencies must be an object of valid JSON "
		               "Schemas or non-empty arrays of unique strings. "
		               "See json-schema-validation§5.4.5."));
	}
}

static void
apply_dependencies (WblSchema *self,
                    JsonObject *root,
                    JsonNode *schema_node,
                    JsonNode *instance_node,
                    GError **error)
{
	WblSchemaClass *klass;
	JsonObject *schema_object;  /* unowned */
	JsonObject *instance_object;  /* unowned */
	GList/*<unowned utf8>*/ *member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */

	/* Check type. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	instance_object = json_node_get_object (instance_node);
	schema_object = json_node_get_object (schema_node);
	member_names = json_object_get_members (schema_object);
	klass = WBL_SCHEMA_GET_CLASS (self);

	for (l = member_names; l != NULL; l = l->next) {
		const gchar *member_name;
		JsonNode *child_node;  /* unowned */

		member_name = l->data;
		child_node = json_object_get_member (schema_object,
		                                     member_name);

		/* Does the instance have a property by this name? */
		if (!json_object_has_member (instance_object, member_name)) {
			continue;
		}

		if (JSON_NODE_HOLDS_OBJECT (child_node)) {
			/* Schema dependency.
			 * json-schema-validation§5.4.5.2.1.
			 *
			 * Instance must validate successfully against this
			 * schema. */
			GError *child_error = NULL;

			if (klass->apply_schema != NULL) {
				WblSchemaNode node;

				node.ref_count = 1;
				node.node = json_node_dup_object (child_node);

				klass->apply_schema (self, &node, instance_node,
				                     &child_error);

				json_object_unref (node.node);
			}

			if (child_error != NULL) {
				g_set_error (error,
				             WBL_SCHEMA_ERROR,
				             WBL_SCHEMA_ERROR_INVALID,
				             _("Object does not validate "
				               "against the schemas in the "
				               "dependencies schema keyword. "
				               "See "
				               "json-schema-validation§5.4.5."));
				break;
			}
		} else if (JSON_NODE_HOLDS_ARRAY (child_node)) {
			JsonArray *child_array;  /* unowned */

			/* Property dependency.
			 * json-schema-validation§5.4.5.2.2.
			 *
			 * Instance must have all properties listed in the
			 * @child_node array. */
			child_array = json_node_get_array (child_node);

			if (!object_has_properties (instance_object,
			                            child_array)) {
				g_set_error (error,
				             WBL_SCHEMA_ERROR,
				             WBL_SCHEMA_ERROR_INVALID,
				             /* Translators: The parameter is a
				              * JSON property name. */
				             _("Object does not have "
				               "properties for all elements in "
				               "the ‘%s’ dependencies array in "
				               "the dependencies schema "
				               "keyword. See "
				               "json-schema-validation§5.4.5."),
				             member_name);
				break;
			}
		}
	}

	g_list_free (member_names);
}

static void
generate_dependencies (WblSchema *self,
                       JsonObject *root,
                       JsonNode *schema_node,
                       GPtrArray *output)
{
	/* FIXME: Tricky to implement without a combinatorial explosion of
	 * test vectors. Need to think about it. */
}

/* enum. json-schema-validation§5.5.1. */
static void
validate_enum (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               GError **error)
{
	JsonArray *schema_array;  /* unowned */
	GHashTable/*<unowned JsonNode,
	             unowned JsonNode>*/ *set = NULL;  /* owned */
	guint i;
	gboolean valid = TRUE;

	if (!JSON_NODE_HOLDS_ARRAY (schema_node)) {
		valid = FALSE;
		goto done;
	}

	/* Check length. */
	schema_array = json_node_get_array (schema_node);

	if (json_array_get_length (schema_array) == 0) {
		valid = FALSE;
		goto done;
	}

	/* Check elements for uniqueness. */
	set = g_hash_table_new (node_hash, node_equal);

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);

		if (g_hash_table_contains (set, child_node)) {
			valid = FALSE;
			break;
		}

		g_hash_table_add (set, child_node);
	}

	g_hash_table_unref (set);

done:
	if (!valid) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("enum must be a non-empty array of unique "
		               "elements. See json-schema-validation§5.5.1."));
	}
}

static void
apply_enum (WblSchema *self,
            JsonObject *root,
            JsonNode *schema_node,
            JsonNode *instance_node,
            GError **error)
{
	JsonArray *schema_array;  /* unowned */
	guint i;

	schema_array = json_node_get_array (schema_node);

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);

		if (node_equal (instance_node, child_node)) {
			/* Success: the instance matches this enum element. */
			return;
		}
	}

	/* Failure: the instance matched none of the enum elements. */
	g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
	             _("Instance does not equal any of the elements of the "
	               "enum schema keyword. "
	               "See json-schema-validation§5.5.1."));
}

static void
generate_enum (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               GPtrArray *output)
{
	JsonArray *schema_array;  /* unowned */
	JsonGenerator *generator = NULL;  /* owned */
	guint i;

	/* Output one instance of each of the child nodes. */
	schema_array = json_node_get_array (schema_node);
	generator = json_generator_new ();

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);

		json_generator_set_root (generator, child_node);
		generate_take_string (output,
		                      json_generator_to_data (generator, NULL),
		                      TRUE);
	}

	g_object_unref (generator);

	/* FIXME: Also output an instance which matches none of the enum
	 * members? How would we generate one of those? */
}

/* type. json-schema-validation§5.5.2. */
static void
validate_type (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               GError **error)
{
	guint seen_types;
	guint i;
	JsonArray *schema_array;  /* unowned */

	if (validate_value_type (schema_node, G_TYPE_STRING) &&
	    validate_primitive_type (json_node_get_string (schema_node))) {
		/* Valid. */
		return;
	}

	if (!JSON_NODE_HOLDS_ARRAY (schema_node)) {
		goto invalid;
	}

	/* Check uniqueness of the array elements using @seen_types as a
	 * bitmask indexed by #WblPrimitiveType. */
	schema_array = json_node_get_array (schema_node);
	seen_types = 0;

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		WblPrimitiveType child_type;
		JsonNode *child_node;  /* unowned */
		const gchar *child_str;

		child_node = json_array_get_element (schema_array, i);

		if (!validate_value_type (child_node, G_TYPE_STRING) ||
		    !validate_primitive_type (json_node_get_string (child_node))) {
			goto invalid;
		}

		child_str = json_node_get_string (child_node);
		child_type = primitive_type_from_string (child_str);

		if (seen_types & (1 << child_type)) {
			goto invalid;
		}

		seen_types |= (1 << child_type);
	}

	/* Valid. */
	return;

invalid:
	g_set_error (error,
	             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
	             _("type must be a string or array of unique strings, each "
	               "a valid primitive type. "
	               "See json-schema-validation§5.5.2."));
}

static void
apply_type (WblSchema *self,
            JsonObject *root,
            JsonNode *schema_node,
            JsonNode *instance_node,
            GError **error)
{
	WblPrimitiveType schema_node_type, instance_node_type;
	const gchar *schema_str;

	schema_str = json_node_get_string (schema_node);
	schema_node_type = primitive_type_from_string (schema_str);
	instance_node_type = primitive_type_from_json_node (instance_node);

	if (!primitive_type_is_a (instance_node_type, schema_node_type)) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Instance type does not conform to type schema "
		               "keyword. See json-schema-validation§5.5.2."));
		return;
	}
}

static void
generate_type (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               GPtrArray *output)
{
	WblPrimitiveType schema_node_type;
	const gchar *schema_str, *valid, *invalid;

	schema_str = json_node_get_string (schema_node);
	schema_node_type = primitive_type_from_string (schema_str);

	/* Add a valid instance. */
	switch (schema_node_type) {
	case WBL_PRIMITIVE_TYPE_ARRAY: valid = "[]"; break;
	case WBL_PRIMITIVE_TYPE_BOOLEAN: valid = "true"; break;
	case WBL_PRIMITIVE_TYPE_INTEGER: valid = "1"; break;
	case WBL_PRIMITIVE_TYPE_NUMBER: valid = "0.1"; break;
	case WBL_PRIMITIVE_TYPE_NULL: valid = "null"; break;
	case WBL_PRIMITIVE_TYPE_OBJECT: valid = "{}"; break;
	case WBL_PRIMITIVE_TYPE_STRING: valid = "''"; break;
	default: g_assert_not_reached ();
	}

	generate_set_string (output, valid, TRUE);

	/* And an invalid instance. */
	if (schema_node_type == WBL_PRIMITIVE_TYPE_NULL) {
		invalid = "false";
	} else {
		invalid = "null";
	}

	generate_set_string (output, invalid, FALSE);
}

/* allOf. json-schema-validation§5.5.3. */
static void
validate_all_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GError **error)
{
	GError *child_error = NULL;

	if (!validate_schema_array (self, schema_node, &child_error)) {
		if (child_error != NULL) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("allOf must be a non-empty array of "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.5.3: "
			               "%s"), child_error->message);
			g_error_free (child_error);
		} else {
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             _("allOf must be a non-empty array of "
			               "valid JSON Schemas. "
			               "See json-schema-validation§5.5.3."));
		}
	}
}

static void
apply_all_of (WblSchema *self,
              JsonObject *root,
              JsonNode *schema_node,
              JsonNode *instance_node,
              GError **error)
{
	JsonArray *schema_array;  /* unowned */
	guint n_successes;

	schema_array = json_node_get_array (schema_node);
	n_successes = apply_schema_array (self, schema_array, instance_node);

	if (n_successes < json_array_get_length (schema_array)) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Instance does not validate against one of the "
		               "schemas in the allOf schema keyword. See "
		               "json-schema-validation§5.5.3."));
	}
}

/* Select instances which match zero or all schemas.
 *
 * FIXME: Is this the best strategy to get maximum coverage? */
static gboolean
predicate_all_or_none (guint n_matching_schemas,
                       guint n_schemas)
{
	return (n_matching_schemas == 0 ||
	        n_matching_schemas == n_schemas);
}

static void
generate_all_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GPtrArray *output)
{
	JsonArray *schema_array;  /* unowned */

	schema_array = json_node_get_array (schema_node);

	generate_schema_array (self, schema_array,
	                       predicate_all_or_none, output);
}

/* anyOf. json-schema-validation§5.5.4. */
static void
validate_any_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GError **error)
{
	GError *child_error = NULL;

	if (!validate_schema_array (self, schema_node, &child_error)) {
		if (child_error != NULL) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("anyOf must be a non-empty array of "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.5.4: "
			               "%s"), child_error->message);
			g_error_free (child_error);
		} else {
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             _("anyOf must be a non-empty array of "
			               "valid JSON Schemas. "
			               "See json-schema-validation§5.5.4."));
		}
	}
}

static void
apply_any_of (WblSchema *self,
              JsonObject *root,
              JsonNode *schema_node,
              JsonNode *instance_node,
              GError **error)
{
	JsonArray *schema_array;  /* unowned */
	guint n_successes;

	schema_array = json_node_get_array (schema_node);
	n_successes = apply_schema_array (self, schema_array, instance_node);

	if (n_successes == 0) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Instance does not validate against any of the "
		               "schemas in the anyOf schema keyword. "
		               "See json-schema-validation§5.5.4."));
	}
}

/* Select instances which match zero or one schemas.
 *
 * FIXME: Is this the best strategy to get maximum coverage? */
static gboolean
predicate_none_or_one (guint n_matching_schemas,
                       guint n_schemas)
{
	return (n_matching_schemas == 0 ||
	        n_matching_schemas == 1);
}

static void
generate_any_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GPtrArray *output)
{
	JsonArray *schema_array;  /* unowned */

	schema_array = json_node_get_array (schema_node);

	generate_schema_array (self, schema_array,
	                       predicate_none_or_one, output);
}

/* oneOf. json-schema-validation§5.5.5. */
static void
validate_one_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GError **error)
{
	GError *child_error = NULL;

	if (!validate_schema_array (self, schema_node, &child_error)) {
		if (child_error != NULL) {
			/* Invalid. */
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             /* Translators: The parameter is another
			              * error message. */
			             _("oneOf must be a non-empty array of "
			               "valid JSON Schemas. See "
			               "json-schema-validation§5.5.4: "
			               "%s"), child_error->message);
			g_error_free (child_error);
		} else {
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_MALFORMED,
			             _("oneOf must be a non-empty array of "
			               "valid JSON Schemas. "
			               "See json-schema-validation§5.5.4."));
		}
	}
}

static void
apply_one_of (WblSchema *self,
              JsonObject *root,
              JsonNode *schema_node,
              JsonNode *instance_node,
              GError **error)
{
	JsonArray *schema_array;  /* unowned */
	guint n_successes;

	schema_array = json_node_get_array (schema_node);
	n_successes = apply_schema_array (self, schema_array, instance_node);

	if (n_successes != 1) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Instance does not validate against exactly one "
		               "of the schemas in the oneOf schema keyword. "
		               "See json-schema-validation§5.5.5."));
	}
}

/* Select instances which match all except one schema.
 *
 * FIXME: Is this the best strategy to get maximum coverage? */
static gboolean
predicate_all_except_one (guint n_matching_schemas,
                          guint n_schemas)
{
	return (n_matching_schemas == n_schemas - 1);
}

static void
generate_one_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GPtrArray *output)
{
	JsonArray *schema_array;  /* unowned */

	schema_array = json_node_get_array (schema_node);

	generate_schema_array (self, schema_array,
	                       predicate_all_except_one, output);
}

/* not. json-schema-validation§5.5.6. */
static void
validate_not (WblSchema *self,
              JsonObject *root,
              JsonNode *schema_node,
              GError **error)
{
	WblSchemaClass *klass;
	GError *child_error = NULL;

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("not must be a valid JSON Schema. See "
		               "json-schema-validation§5.5.6."));
		return;
	}

	klass = WBL_SCHEMA_GET_CLASS (self);

	/* Validate the child schema. */
	if (klass->validate_schema != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (schema_node);

		klass->validate_schema (self, &node, &child_error);

		json_object_unref (node.node);
	}

	if (child_error != NULL) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             /* Translators: The parameter is another error
		              * message. */
		             _("not must be a valid JSON Schema. See "
		               "json-schema-validation§5.5.6: "
		               "%s"), child_error->message);
		g_error_free (child_error);

		return;
	}
}

static void
apply_not (WblSchema *self,
           JsonObject *root,
           JsonNode *schema_node,
           JsonNode *instance_node,
           GError **error)
{
	WblSchemaClass *klass;
	GError *child_error = NULL;

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (klass->apply_schema != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (schema_node);

		klass->apply_schema (self, &node, instance_node, &child_error);

		json_object_unref (node.node);
	}

	/* Fail if application succeeded. */
	if (child_error == NULL) {
		g_set_error (error, WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Instance validates against the schemas in the "
		               "not schema keyword. "
		               "See json-schema-validation§5.5.6."));
	}

	g_clear_error (&child_error);
}

static void
generate_not (WblSchema *self,
              JsonObject *root,
              JsonNode *schema_node,
              GPtrArray *output)
{
	WblSchemaClass *klass;

	klass = WBL_SCHEMA_GET_CLASS (self);

	/* Generate instances for the schema. */
	if (klass->generate_instances != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (schema_node);

		klass->generate_instances (self, &node, output);

		json_object_unref (node.node);
	}
}

/* title. json-schema-validation§6.1. */
static void
validate_title (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_STRING)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("title must be a string. "
		               "See json-schema-validation§6.1."));
	}
}

/* description. json-schema-validation§6.1. */
static void
validate_description (WblSchema *self,
                      JsonObject *root,
                      JsonNode *schema_node,
                      GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_STRING)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("description must be a string. "
		               "See json-schema-validation§6.1."));
	}
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
typedef void
(*KeywordGenerateFunc) (WblSchema *self,
                        JsonObject *root,
                        JsonNode *schema_node,
                        GPtrArray *output);

/* Structure holding information about a single JSON Schema keyword, as defined
 * in json-schema-core§3.2. */
typedef struct {
	const gchar *name;
	KeywordValidateFunc validate;  /* NULL if validation always succeeds */
	KeywordApplyFunc apply;  /* NULL if application always succeeds */
	KeywordGenerateFunc generate;  /* NULL if generation produces nothing */
} KeywordData;

/* Information about all JSON Schema keywords defined in
 *  - json-schema-core
 *  - json-schema-validation
 *  - json-schema-hypermedia
 */
static const KeywordData json_schema_keywords[] = {
	/* json-schema-validation§5.1.1 */
	{ "multipleOf", validate_multiple_of, apply_multiple_of, generate_multiple_of },
	/* json-schema-validation§5.1.2 */
	{ "maximum", validate_maximum, apply_maximum, generate_maximum },
	{ "exclusiveMaximum", validate_exclusive_maximum, NULL, NULL },
	/* json-schema-validation§5.1.3. */
	{ "minimum", validate_minimum, apply_minimum, generate_minimum },
	{ "exclusiveMinimum", validate_exclusive_minimum, NULL, NULL },
	/* json-schema-validation§5.2.1 */
	{ "maxLength", validate_max_length, apply_max_length, generate_max_length },
	/* json-schema-validation§5.2.2 */
	{ "minLength", validate_min_length, apply_min_length, generate_min_length },
	/* json-schema-validation§5.2.3 */
	{ "pattern", validate_pattern, apply_pattern, generate_pattern },
	/* json-schema-validation§5.3.1 */
	{ "additionalItems", validate_additional_items, NULL, NULL },
	{ "items", validate_items, apply_items, generate_items },
	/* json-schema-validation§5.3.2 */
	{ "maxItems", validate_max_items, apply_max_items, generate_max_items },
	/* json-schema-validation§5.3.3 */
	{ "minItems", validate_min_items, apply_min_items, generate_min_items },
	/* json-schema-validation§5.3.3 */
	{ "uniqueItems", validate_unique_items, apply_unique_items, generate_unique_items },
	/* json-schema-validation§5.4.1 */
	{ "maxProperties", validate_max_properties, apply_max_properties, generate_max_properties },
	/* json-schema-validation§5.4.2 */
	{ "minProperties", validate_min_properties, apply_min_properties, generate_min_properties },
	/* json-schema-validation§5.4.3 */
	{ "required", validate_required, apply_required, generate_required },
	/* json-schema-validation§5.4.4 */
	{ "additionalProperties", validate_additional_properties, NULL, NULL },
	{ "properties", validate_properties, apply_properties, generate_properties },
	{ "patternProperties", validate_pattern_properties, NULL, NULL },
	/* json-schema-validation§5.4.5 */
	{ "dependencies", validate_dependencies, apply_dependencies, generate_dependencies },
	/* json-schema-validation§5.5.1 */
	{ "enum", validate_enum, apply_enum, generate_enum },
	/* json-schema-validation§5.5.2 */
	{ "type", validate_type, apply_type, generate_type },
	/* json-schema-validation§5.5.3 */
	{ "allOf", validate_all_of, apply_all_of, generate_all_of },
	/* json-schema-validation§5.5.4 */
	{ "anyOf", validate_any_of, apply_any_of, generate_any_of },
	/* json-schema-validation§5.5.5 */
	{ "oneOf", validate_one_of, apply_one_of, generate_one_of },
	/* json-schema-validation§5.5.6 */
	{ "not", validate_not, apply_not, generate_not },
	/* json-schema-validation§6.1 */
	{ "title", validate_title, NULL, NULL },
	{ "description", validate_description, NULL, NULL },
	/* json-schema-validation§6.2 */
	{ "default", NULL, NULL, NULL },

	/* TODO:
	 *  • definitions (json-schema-validation§5.5.7)
	 *  • format (json-schema-validation§7.1)
	 *  • json-schema-core
	 *  • json-schema-hypermedia
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

static void
real_generate_instances (WblSchema *self,
                         WblSchemaNode *schema,
                         GPtrArray *output)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (json_schema_keywords); i++) {
		const KeywordData *keyword = &json_schema_keywords[i];
		JsonNode *schema_node;  /* unowned */

		schema_node = json_object_get_member (schema->node,
		                                      keyword->name);

		if (schema_node != NULL && keyword->generate != NULL) {
			keyword->generate (self, schema->node,
			                   schema_node, output);
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
 * @self: a #WblSchema
 * @data: (array length=length): serialised JSON data to load
 * @length: length of @data, or -1 if @data is nul-terminated
 * @error: return location for a #GError, or %NULL
 *
 * Load and parse a JSON schema from the given serialised JSON @data.
 *
 * See wbl_schema_load_from_stream_async() for more details.
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
 * @self: a #WblSchema
 * @filename: (type filename): path to a local JSON file to load
 * @error: return location for a #GError, or %NULL
 *
 * Load and parse a JSON schema from the given local file containing serialised
 * JSON data. To load a non-local file, or to use a URI, use
 * wbl_schema_load_from_stream_async().
 *
 * See wbl_schema_load_from_stream_async() for more details.
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
 * @self: a #WblSchema
 * @stream: stream of serialised JSON data to load
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Load and parse a JSON schema from an input stream providing serialised JSON
 * data. This is the synchronous version of wbl_schema_load_from_stream_async(),
 * which is preferred for production code.
 *
 * See wbl_schema_load_from_stream_async() for more details.
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
 * @self: a #WblSchema
 * @stream: stream of serialised JSON data to load
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async) (nullable): callback to invoke when parsing is
 *   complete, or %NULL
 * @user_data: (closure callback): user data to pass to @callback
 *
 * Load and parse a JSON schema from an input stream providing serialised JSON
 * data. The loading and parsing is done asynchronously and, once complete,
 * @callback is invoked in the main context which was thread default at the time
 * of calling this method.
 *
 * Call wbl_schema_load_from_stream_finish() from @callback to retrieve
 * information about any parsing errors.
 *
 * If a schema is loaded successfully, it is guaranteed to be valid.
 *
 * Any previously loaded schemas will be unloaded when starting to load a new
 * one, even if the new load operation fails (e.g. due to the new schema being
 * invalid).
 *
 * This is the preferred method for loading a schema due to the potential
 * blocking involved in the I/O.
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
 * @self: a #WblSchema
 * @result: result from the asynchronous operation
 * @error: return location for a #GError, or %NULL
 *
 * Finish an asynchronous schema loading operation started with
 * wbl_schema_load_from_stream_async().
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

/**
 * wbl_schema_generate_instances:
 * @self: a #WblSchema
 * @flags: flags affecting how instances are generated
 *
 * Generate JSON instances for the given JSON Schema. These instances are
 * designed to be used as test vectors for code which parses and handles JSON
 * following this schema. By default, instances which are both valid and invalid
 * are generated, with the invalid schemas designed to test boundary conditions
 * of validity, and common parsing problems.
 *
 * All generated instances are correctly formed JSON, and all will parse
 * successfully. By design, however, some of the instances will not validate
 * according to the given #WblSchema.
 *
 * Returns: (transfer full) (element-type WblGeneratedInstance): newly allocated
 *   array of #WblGeneratedInstances
 *
 * Since: UNRELEASED
 */
GPtrArray *
wbl_schema_generate_instances (WblSchema *self,
                               WblGenerateInstanceFlags flags)
{
	WblSchemaClass *klass;
	WblSchemaPrivate *priv;
	GPtrArray/*<owned WblGeneratedInstance>*/ *output = NULL;  /* owned */
	JsonParser *parser = NULL;  /* owned */
	guint i;
	gboolean removed = FALSE;

	g_return_val_if_fail (WBL_IS_SCHEMA (self), NULL);

	klass = WBL_SCHEMA_GET_CLASS (self);
	priv = wbl_schema_get_instance_private (self);

	output = g_ptr_array_new_with_free_func ((GDestroyNotify) wbl_generated_instance_free);

	/* Generate schema instances. */
	if (klass->generate_instances != NULL) {
		klass->generate_instances (self, priv->schema, output);
	}

	/* See if they are valid. We cannot do this constructively because
	 * interactions between keywords change the validity of the overall
	 * JSON instance. */
	parser = json_parser_new ();

	for (i = 0; i < output->len; i += (removed ? 0 : 1)) {
		WblGeneratedInstance *instance;
		const gchar *json;
		GError *error = NULL;

		instance = output->pdata[i];
		json = wbl_generated_instance_get_json (instance);

		json_parser_load_from_data (parser, json, -1, &error);
		g_assert_no_error (error);

		wbl_schema_apply (self, json_parser_get_root (parser), &error);
		instance->valid = (error == NULL);

		g_clear_error (&error);

		/* Apply the filtering flags. */
		if (((flags & WBL_GENERATE_INSTANCE_IGNORE_VALID) &&
		     instance->valid) ||
		    ((flags & WBL_GENERATE_INSTANCE_IGNORE_INVALID) &&
		     !instance->valid)) {
			removed = TRUE;
			g_ptr_array_remove_index_fast (output, i);
		} else {
			removed = FALSE;
		}
	}

	g_object_unref (parser);

	return output;
}
