/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Walbottle
 * Copyright (C) Philip Withnall 2014, 2015 <philip@tecnocode.co.uk>
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

/**
 * SECTION:wbl-schema
 * @short_description: JSON schema parsing and representation
 * @stability: Unstable
 * @include: libwalbottle/wbl-schema.h
 *
 * #WblSchema represents a single JSON schema, at the top level. This is a tree
 * of #WblSchemaNodes, with one guaranteed to exist at the top level
 * (retrievable using wbl_schema_get_root()) and others lower down representing
 * sub-schemas.
 *
 * When loading a schema, it is validated for well-formedness and adherence to
 * the JSON meta-schema (which defines the format used for schemas). Invalid
 * schemas will fail to load.
 *
 * Two main operations may be performed on a loaded schema: application of the
 * schema to a JSON instance, and generation of instances from the schema.
 * Applying a schema to an instance validates that instance against the schema,
 * returning success or a validation error. Generating instances from a schema
 * produces zero or more JSON instances which test various boundary conditions
 * of the schema. They are designed to be used in testing parser implementations
 * for that schema.
 *
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
 */
const gchar *
wbl_schema_node_get_title (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	if (!json_object_has_member (self->node, "title")) {
		return NULL;
	}

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
 * Since: 0.1.0
 */
const gchar *
wbl_schema_node_get_description (WblSchemaNode *self)
{
	g_return_val_if_fail (self != NULL, NULL);
	g_return_val_if_fail (self->ref_count > 0, NULL);

	if (!json_object_has_member (self->node, "description")) {
		return NULL;
	}

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
 * Since: 0.1.0
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

static WblGeneratedInstance *
_wbl_generated_instance_take_from_string (gchar     *json,
                                          gboolean   valid)
{
	WblGeneratedInstance *instance = NULL;  /* owned */

	g_return_val_if_fail (json != NULL, NULL);

	instance = g_slice_new0 (WblGeneratedInstance);
	instance->json = json;  /* transfer ownership */
	instance->valid = valid;

	return instance;
}

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
 * Since: 0.1.0
 */
WblGeneratedInstance *
wbl_generated_instance_new_from_string (const gchar *json,
                                        gboolean valid)
{
	return _wbl_generated_instance_take_from_string (g_strdup (json),
	                                                 valid);
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
 */
const gchar *
wbl_generated_instance_get_json (WblGeneratedInstance *self)
{
	g_return_val_if_fail (self != NULL, NULL);

	return self->json;
}

/**
 * wbl_generated_instance_is_valid:
 * @self: a #WblGeneratedInstance
 *
 * Get whether the generated JSON instance is valid with respect to the schema.
 *
 * Returns: %TRUE if valid, %FALSE otherwise
 *
 * Since: UNRELEASED
 */
gboolean
wbl_generated_instance_is_valid (WblGeneratedInstance *self)
{
	g_return_val_if_fail (self != NULL, FALSE);

	return self->valid;
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

static gchar *  /* transfer full */
json_double_to_string (gdouble i);

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
		return json_double_to_string (json_node_get_double (node));
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

/* Helper functions to validate, apply and generate subschemas. */
static void
subschema_validate (WblSchema *self,
                    JsonNode *subschema_node,
                    GError **error)
{
	WblSchemaClass *klass;

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (klass->validate_schema != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (subschema_node);
		klass->validate_schema (self, &node, error);
		json_object_unref (node.node);
	}
}

static void
subschema_apply (WblSchema *self,
                 JsonNode *subschema_node,
                 JsonNode *instance_node,
                 GError **error)
{
	WblSchemaClass *klass;

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (klass->apply_schema != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (subschema_node);
		klass->apply_schema (self, &node, instance_node, error);
		json_object_unref (node.node);
	}
}

static void
subschema_generate_instances (WblSchema *self,
                              JsonNode *subschema_node,
                              GHashTable/*<owned JsonNode>*/ *output)
{
	WblSchemaClass *klass;

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (klass->generate_instances != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = json_node_dup_object (subschema_node);

		klass->generate_instances (self, &node, output);

		json_object_unref (node.node);
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
                         GHashTable/*<owned JsonNode>*/ *output);

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
	JsonArray *schema_array;  /* unowned */
	guint i;

	if (!JSON_NODE_HOLDS_ARRAY (schema_node)) {
		return FALSE;
	}

	schema_array = json_node_get_array (schema_node);

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
		subschema_validate (self, child_node, &child_error);

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
	guint i, n_successes;

	n_successes = 0;

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */
		GError *child_error = NULL;

		child_node = json_array_get_element (schema_array, i);

		subschema_apply (self, child_node, instance_node, &child_error);

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
                       GHashTable/*<owned JsonNode>*/ *output)
{
	guint i, n_schemas;
	GHashTable/*<owned JsonNode>*/ *_output = NULL;  /* owned */
	GHashTableIter iter;
	JsonNode *instance;  /* unowned */

	n_schemas = json_array_get_length (schema_array);

	/* Generate instances for all schemas. */
	_output = g_hash_table_new_full (node_hash,
	                                 node_equal,
	                                 (GDestroyNotify) json_node_free,
	                                 NULL);

	for (i = 0; i < n_schemas; i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);
		subschema_generate_instances (self, child_node, _output);
	}

	/* Find any instances which match the number of schemas approved by the
	 * predicate. */
	g_hash_table_iter_init (&iter, _output);

	while (g_hash_table_iter_next (&iter, (gpointer *) &instance, NULL)) {
		guint n_matching_schemas;
		GError *child_error = NULL;

		n_matching_schemas = 0;

		/* Check it against each schema. */
		for (i = 0; i < n_schemas; i++) {
			JsonNode *child_node;  /* unowned */

			child_node = json_array_get_element (schema_array, i);

			subschema_apply (self, child_node, instance,
			                 &child_error);

			if (child_error == NULL) {
				n_matching_schemas++;
			}

			g_clear_error (&child_error);
		}

		/* Does it match the predicate? */
		if (predicate (n_matching_schemas, n_schemas)) {
			g_hash_table_add (output, json_node_copy (instance));
		}
	}

	g_hash_table_unref (_output);
}

/* A couple of utility functions for building #JsonNodes. */
static JsonNode *
node_new_int (gint64   value)
{
	JsonNode *node = json_node_new (JSON_NODE_VALUE);
	json_node_set_int (node, value);
	return node;
}

static JsonNode *
node_new_double (gdouble   value)
{
	JsonNode *node = json_node_new (JSON_NODE_VALUE);
	json_node_set_double (node, value);
	return node;
}

static JsonNode *
node_new_boolean (gboolean   value)
{
	JsonNode *node = json_node_new (JSON_NODE_VALUE);
	json_node_set_boolean (node, value);
	return node;
}

static JsonNode *
node_new_string (const gchar  *str)
{
	JsonNode *node = json_node_new (JSON_NODE_VALUE);
	json_node_set_string (node, str);
	return node;
}

/* Intended for debug use only. */
static gchar *
node_to_string (JsonNode  *node)
{
	JsonGenerator *generator = NULL;
	gchar *output = NULL;

	generator = json_generator_new ();
	json_generator_set_root (generator, node);
	output = json_generator_to_data (generator, NULL);
	g_object_unref (generator);

	return output;
}

/* A couple of utility functions for generation. */
static void
generate_take_node (GHashTable/*<owned JsonNode>*/  *output,
                    JsonNode                        *node)  /* transfer full */
{
	g_hash_table_add (output, node);  /* transfer */
}

static void
generate_take_builder (GHashTable/*<owned JsonNode>*/ *output,
                       JsonBuilder *builder,  /* transfer none */
                       gboolean valid)
{
	JsonNode *instance_root = NULL;  /* owned */

	instance_root = json_builder_get_root (builder);
	g_hash_table_add (output, instance_root);  /* transfer */
	instance_root = NULL;

	json_builder_reset (builder);
}

static void
generate_null_array (GHashTable/*<owned JsonNode>*/ *output,
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
generate_null_properties (GHashTable/*<owned JsonNode>*/ *output,
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
generate_filled_string (GHashTable/*<owned JsonNode>*/ *output,
                        gsize length,  /* in Unicode characters */
                        gunichar fill,
                        gboolean valid)
{
	JsonNode *node = NULL;
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

	/* Check there is enough room for a nul terminator. */
	if (G_MAXSIZE - 1 < length) {
		return;
	}

	/* Allocate a string. This might be huge. */
	str = g_malloc (length + 1  /* nul terminator */);

	/* Starting quotation mark. */
	i = 0;

	/* Fill with the unichar. Note: @length is divisible by @fill_len. */
	for (; i < length; i += fill_len) {
		memcpy (str + i, fill_utf8, fill_len);
	}

	/* Fill the rest with nul bytes. */
	for (; i < length + 1; i++) {
		str[i] = '\0';
	}

	/* Wrap in a #JsonNode. */
	node = node_new_string (str);
	generate_take_node (output, node);
	g_free (str);
}

/* Convert a double to a string in the C locale, guaranteeing to include a
 * decimal point in the output (so it can’t be re-parsed as a JSON integer). */
static gchar *  /* transfer full */
json_double_to_string (gdouble i)
{
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr (buf, sizeof (buf), i);

	if (strchr (buf, '.') == NULL) {
		return g_strdup_printf ("%s.0", buf);
	} else {
		return g_strdup (buf);
	}
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
                      GHashTable/*<owned JsonNode>*/ *output)
{
	GType schema_type;

	schema_type = json_node_get_value_type (schema_node);

	/* Standard outputs. */
	generate_take_node (output, node_new_int (0));

	if (schema_type == G_TYPE_INT64) {
		gint64 multiplicand;

		multiplicand = json_node_get_int (schema_node);

		generate_take_node (output, node_new_int (multiplicand));
		generate_take_node (output, node_new_int (multiplicand * 2));

		if (multiplicand != 1) {
			generate_take_node (output,
			                    node_new_int (multiplicand + 1));
		}
	} else if (schema_type == G_TYPE_DOUBLE) {
		gdouble multiplicand;

		multiplicand = json_node_get_double (schema_node);

		generate_take_node (output, node_new_double (multiplicand));
		generate_take_node (output,
		                    node_new_double (multiplicand * 2.0));

		if (multiplicand != 0.1) {
			generate_take_node (output,
			                    node_new_double (multiplicand + 0.1));
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
	if (!validate_value_type (schema_node, G_TYPE_INT64) &&
	    !validate_value_type (schema_node, G_TYPE_DOUBLE)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maximum must be a number. "
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
	gchar *maximum_str = NULL;  /* owned */
	gboolean exclusive_maximum = FALSE;  /* json-schema-validation§5.1.2.3 */
	JsonNode *node;  /* unowned */
	gint comparison;

	/* Check the instance is of the right type. */
	if (!validate_value_type (instance_node, G_TYPE_INT64) &&
	    !validate_value_type (instance_node, G_TYPE_DOUBLE)) {
		return;
	}

	/* Gather various parameters. */
	node = json_object_get_member (root, "exclusiveMaximum");
	if (node != NULL) {
		exclusive_maximum = json_node_get_boolean (node);
	}

	maximum_str = number_node_to_string (schema_node);

	/* Actually perform the validation. */
	comparison = number_node_comparison (instance_node, schema_node);

	if (!exclusive_maximum && comparison > 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be less than or equal to %s "
		               "due to the maximum schema "
		               "keyword. See json-schema-validation§5.1.2."),
		             maximum_str);
	} else if (exclusive_maximum && comparison >= 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be less than %s "
		               "due to the maximum and exclusiveMaximum schema "
		               "keywords. See json-schema-validation§5.1.2."),
		             maximum_str);
	}

	g_free (maximum_str);
}

static void
generate_maximum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GHashTable/*<owned JsonNode>*/ *output)
{
	gboolean exclusive_maximum = FALSE;  /* json-schema-validation§5.1.2.3 */
	JsonNode *node;  /* unowned */

	node = json_object_get_member (root, "exclusiveMaximum");
	if (node != NULL) {
		exclusive_maximum = json_node_get_boolean (node);
	}

	/* Generate the instances. */
	if (json_node_get_value_type (schema_node) == G_TYPE_INT64) {
		gint64 maximum;

		maximum = json_node_get_int (schema_node);

		if (maximum > G_MININT64 && exclusive_maximum) {
			generate_take_node (output, node_new_int (maximum - 1));
		}

		generate_take_node (output, node_new_int (maximum));
		generate_take_node (output,
		                    node_new_double ((gdouble) maximum));

		if (maximum < G_MAXINT64 && !exclusive_maximum) {
			generate_take_node (output, node_new_int (maximum + 1));
		}
	} else {
		gdouble maximum;
		gint64 rounded;

		maximum = json_node_get_double (schema_node);
		rounded = (gint64) maximum;  /* truncation towards 0 is fine */

		if (maximum > G_MINDOUBLE && exclusive_maximum) {
			generate_take_node (output,
			                    node_new_double (maximum - DBL_EPSILON));
		}

		generate_take_node (output, node_new_double (maximum));
		generate_take_node (output, node_new_int (rounded));

		if (maximum < G_MAXDOUBLE && !exclusive_maximum) {
			generate_take_node (output,
			                    node_new_double (maximum + DBL_EPSILON));
		}
	}
}

/* minimum and exclusiveMinimum. json-schema-validation§5.1.3. */
static void
validate_minimum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GError **error)
{
	if (!validate_value_type (schema_node, G_TYPE_INT64) &&
	    !validate_value_type (schema_node, G_TYPE_DOUBLE)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minimum must be a number. "
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
	gchar *minimum_str = NULL;  /* owned */
	gboolean exclusive_minimum = FALSE;  /* json-schema-validation§5.1.3.3 */
	JsonNode *node;  /* unowned */
	gint comparison;

	/* Check the instance is of the right type. */
	if (!validate_value_type (instance_node, G_TYPE_INT64) &&
	    !validate_value_type (instance_node, G_TYPE_DOUBLE)) {
		return;
	}

	/* Gather various parameters. */
	node = json_object_get_member (root, "exclusiveMinimum");
	if (node != NULL) {
		exclusive_minimum = json_node_get_boolean (node);
	}

	minimum_str = number_node_to_string (schema_node);

	/* Actually perform the validation. */
	comparison = number_node_comparison (instance_node, schema_node);

	if (!exclusive_minimum && comparison < 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be greater than or equal to %s "
		               "due to the minimum schema "
		               "keyword. See json-schema-validation§5.1.3."),
		             minimum_str);
	} else if (exclusive_minimum && comparison <= 0) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
		             _("Value must be greater than %s "
		               "due to the minimum and exclusiveMinimum schema "
		               "keywords. See json-schema-validation§5.1.3."),
		             minimum_str);
	}

	g_free (minimum_str);
}

static void
generate_minimum (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GHashTable/*<owned JsonNode>*/ *output)
{
	gboolean exclusive_minimum = FALSE;  /* json-schema-validation§5.1.3.3 */
	JsonNode *node;  /* unowned */

	/* Grab useful data. */
	node = json_object_get_member (root, "exclusiveMinimum");
	if (node != NULL) {
		exclusive_minimum = json_node_get_boolean (node);
	}

	/* Generate the instances. */
	if (json_node_get_value_type (schema_node) == G_TYPE_INT64) {
		gint64 minimum;

		minimum = json_node_get_int (schema_node);

		if (minimum > G_MININT64 && !exclusive_minimum) {
			generate_take_node (output, node_new_int (minimum - 1));
		}

		generate_take_node (output, node_new_int (minimum));
		generate_take_node (output,
		                    node_new_double ((gdouble) minimum));

		if (minimum < G_MAXINT64 && exclusive_minimum) {
			generate_take_node (output, node_new_int (minimum + 1));
		}
	} else {
		gdouble minimum;
		gint64 rounded;

		minimum = json_node_get_double (schema_node);
		rounded = (gint64) minimum;  /* truncation is fine */

		if (minimum > G_MINDOUBLE && !exclusive_minimum) {
			generate_take_node (output,
			                    node_new_double (minimum - DBL_EPSILON));
		}

		generate_take_node (output, node_new_double (minimum));
		generate_take_node (output, node_new_int (rounded));

		if (minimum < G_MAXDOUBLE && exclusive_minimum) {
			generate_take_node (output,
			                    node_new_double (minimum + DBL_EPSILON));
		}
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
                     GHashTable/*<owned JsonNode>*/ *output)
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
                     GHashTable/*<owned JsonNode>*/ *output)
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

	instance_str = json_node_get_string (instance_node);

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
                  GHashTable/*<owned JsonNode>*/ *output)
{
	/* FIXME: Generate two constant strings for the moment. In the future,
	 * we should generate strings which are guaranteed to pass and fail
	 * application of the schema to the instance; but that requires
	 * executing the regexp state machine, which is a bit more involved. */
	generate_take_node (output, node_new_string (""));
	generate_take_node (output, node_new_string ("non-empty"));
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
		GError *child_error = NULL;

		/* Validate the schema. */
		subschema_validate (self, schema_node, &child_error);

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
		GError *child_error = NULL;

		/* Validate the schema. */
		subschema_validate (self, schema_node, &child_error);

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
		JsonArray *array;
		guint i;

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
			subschema_validate (self, array_node, &child_error);

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

/* json-schema-validation§5.3.1.2. */
static void
apply_items_parent_schema (WblSchema *self,
                           JsonNode *items_schema_node,
                           JsonNode *additional_items_schema_node,
                           JsonArray *instance_array,
                           GError **error)
{
	gboolean additional_items_is_boolean;
	gboolean additional_items_boolean;

	if (JSON_NODE_HOLDS_OBJECT (items_schema_node)) {
		/* Validation of the parent node always succeeds if items is an
		 * object (json-schema-validation§5.3.1.2). */
		return;
	}

	additional_items_is_boolean = (additional_items_schema_node != NULL &&
	                               JSON_NODE_HOLDS_VALUE (additional_items_schema_node) &&
	                               json_node_get_value_type (additional_items_schema_node) ==
	                               G_TYPE_BOOLEAN);
	additional_items_boolean = additional_items_is_boolean ?
	                           json_node_get_boolean (additional_items_schema_node) : FALSE;

	if (additional_items_schema_node == NULL ||
	    JSON_NODE_HOLDS_OBJECT (additional_items_schema_node) ||
	    (additional_items_is_boolean && additional_items_boolean)) {
		/* Validation always succeeds if additionalItems is a boolean
		 * true or an object (or not present; as the default value is
		 * an empty schema, which is an object). */
		return;
	}

	if (additional_items_is_boolean && !additional_items_boolean &&
	    JSON_NODE_HOLDS_ARRAY (items_schema_node)) {
		JsonArray  *schema_array;  /* unowned */
		guint instance_size, schema_size;

		/* Validation succeeds if the instance size is less than or
		 * equal to the size of items
		 * (json-schema-validation§5.3.1.2). */
		schema_array = json_node_get_array (items_schema_node);

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

/* json-schema-validation§8.2.3. */
static void
apply_items_child_schema (WblSchema *self,
                          JsonNode *items_schema_node,
                          JsonNode *additional_items_schema_node,
                          JsonArray *instance_array,
                          GError **error)
{
	JsonArray *schema_array = NULL;  /* unowned */
	guint schema_size = 0, i;

	if (JSON_NODE_HOLDS_ARRAY (items_schema_node)) {
		schema_array = json_node_get_array (items_schema_node);
		schema_size = json_array_get_length (schema_array);
	}

	/* Validate each child instance. */
	for (i = 0; i < json_array_get_length (instance_array); i++) {
		JsonNode *child_node;  /* unowned */
		JsonNode *sub_schema_node;  /* unowned */
		const gchar *schema_keyword_name;
		GError *child_error = NULL;

		/* Work out which schema to apply. */
		if (JSON_NODE_HOLDS_OBJECT (items_schema_node)) {
			/* All child nodes of the instance must be valid against
			 * this schema (json-schema-validation§8.2.3.1). */
			sub_schema_node = items_schema_node;
			schema_keyword_name = "items";
		} else if (JSON_NODE_HOLDS_ARRAY (items_schema_node)) {
			/* …and if each child element validates against the
			 * corresponding items or additionalItems schema
			 * (json-schema-validation§8.2.3.2). */
			if (i < schema_size) {
				sub_schema_node = json_array_get_element (schema_array,
				                                          i);
				schema_keyword_name = "items";
			} else {
				sub_schema_node = additional_items_schema_node;
				schema_keyword_name = "additionalItems";
			}
		} else {
			g_assert_not_reached ();
		}

		/* Check the schema type. additionalItems may be a boolean,
		 * which we consider as an empty schema (which always applies
		 * successfully). (json-schema-validation§8.2.2.) */
		if (!JSON_NODE_HOLDS_OBJECT (sub_schema_node)) {
			continue;
		}

		/* Validate the child instance. */
		child_node = json_array_get_element (instance_array, i);
		subschema_apply (self, sub_schema_node, child_node,
		                 &child_error);

		if (child_error != NULL) {
			g_set_error (error,
			             WBL_SCHEMA_ERROR,
			             WBL_SCHEMA_ERROR_INVALID,
			             /* Translators: The parameter is the name
			              * of a schema keyword. */
			             _("Array element does not validate "
			               "against the schemas in the %s schema "
			               "keyword. "
			               "See json-schema-validation§8.2."),
			             schema_keyword_name);
			g_error_free (child_error);
			return;
		}
	}
}

static void
apply_items (WblSchema *self,
             JsonObject *root,
             JsonNode *schema_node,
             JsonNode *instance_node,
             GError **error)
{
	JsonNode *ai_schema_node;  /* unowned */
	JsonArray *instance_array;  /* unowned */
	GError *child_error = NULL;

	/* Check the instance type. */
	if (!JSON_NODE_HOLDS_ARRAY (instance_node)) {
		return;
	}

	ai_schema_node = json_object_get_member (root, "additionalItems");
	instance_array = json_node_get_array (instance_node);

	/* Validate the instance node. */
	apply_items_parent_schema (self, schema_node, ai_schema_node,
	                           instance_array, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return;
	}

	/* Validate its children. */
	apply_items_child_schema (self, schema_node, ai_schema_node,
	                          instance_array, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return;
	}
}

/* Generate a JSON array with null elements up to @subschema_position, and an
 * instance generated from @subschema_node at that position. Generate one of
 * these arrays for each instance generated from @subschema_node. */
static void
generate_array_with_subschema (WblSchema *self,
                               JsonNode *subschema_node,
                               guint subschema_position,
                               GHashTable/*<owned JsonNode>*/ *output)
{
	GHashTable/*<owned JsonNode>*/ *child_output = NULL;  /* owned */
	GHashTableIter iter;
	JsonParser *parser = NULL;  /* owned */
	JsonBuilder *builder = NULL;  /* owned */
	JsonNode *child_instance;  /* unowned */

	child_output = g_hash_table_new_full (node_hash,
	                                      node_equal,
	                                      (GDestroyNotify) json_node_free,
	                                      NULL);
	builder = json_builder_new ();
	parser = json_parser_new ();

	subschema_generate_instances (self, subschema_node, child_output);

	g_hash_table_iter_init (&iter, child_output);

	while (g_hash_table_iter_next (&iter, (gpointer *) &child_instance, NULL)) {
		guint k;

		json_builder_begin_array (builder);

		for (k = 0; k < subschema_position; k++) {
			json_builder_add_null_value (builder);
		}

		json_builder_add_value (builder, json_node_copy (child_instance));
		json_builder_end_array (builder);

		generate_take_builder (output, builder, TRUE);
	}

	g_object_unref (parser);
	g_object_unref (builder);
	g_hash_table_unref (child_output);
}

static void
generate_items (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                GHashTable/*<owned JsonNode>*/ *output)
{
	JsonBuilder *builder = NULL;
	guint items_length = 0;

	/* If items is an array, we care about its length. */
	if (JSON_NODE_HOLDS_ARRAY (schema_node)) {
		JsonArray *items_array;  /* unowned */

		items_array = json_node_get_array (schema_node);
		items_length = json_array_get_length (items_array);
	}

	/* Always have an empty array. */
	if (items_length > 0) {
		JsonNode *node = NULL;

		node = json_node_new (JSON_NODE_ARRAY);
		json_node_take_array (node, json_array_new ());
		generate_take_node (output, node);
	}

	/* Add arrays at @items_length and (@items_length + 1). */
	generate_null_array (output, items_length, TRUE);
	generate_null_array (output, items_length + 1, TRUE);
	/* FIXME: @valid should depend on additionalItems here. */

	builder = json_builder_new ();

	/* If items is an object, all child elements must validate against
	 * that subschema. */
	if (JSON_NODE_HOLDS_OBJECT (schema_node)) {
		GHashTable/*<owned JsonNode>*/ *child_output = NULL;  /* owned */
		GHashTableIter iter;
		JsonNode *child_instance;  /* unowned */

		child_output = g_hash_table_new_full (node_hash,
		                                      node_equal,
		                                      (GDestroyNotify) json_node_free,
		                                      NULL);

		subschema_generate_instances (self, schema_node, child_output);

		g_hash_table_iter_init (&iter, child_output);

		while (g_hash_table_iter_next (&iter, (gpointer *) &child_instance, NULL)) {
			json_builder_begin_array (builder);
			json_builder_add_value (builder,
			                        json_node_copy (child_instance));
			json_builder_end_array (builder);

			generate_take_builder (output, builder, TRUE);
		}

		g_hash_table_unref (child_output);
	}

	/* If items is an array, all child elements must validate against the
	 * subschema with the corresponding index.
	 *
	 * For each subschema, generate instances for it, and wrap them in an
	 * array with the generated instances at the index corresponding to the
	 * subschema. If additionalItems is set, generate an additional array
	 * with the generated instances for the additionalItems subschema at the
	 * final index. */
	if (JSON_NODE_HOLDS_ARRAY (schema_node)) {
		guint i;
		JsonArray *schema_array;  /* unowned */
		JsonNode *ai_schema_node;  /* unowned */

		ai_schema_node = json_object_get_member (root,
		                                         "additionalItems");
		schema_array = json_node_get_array (schema_node);

		for (i = 0; i < json_array_get_length (schema_array); i++) {
			JsonNode *subschema_node;  /* unowned */

			subschema_node = json_array_get_element (schema_array,
			                                         i);
			generate_array_with_subschema (self, subschema_node,
			                               i, output);
		}

		if (ai_schema_node != NULL &&
		    JSON_NODE_HOLDS_OBJECT (ai_schema_node)) {
			generate_array_with_subschema (self, ai_schema_node,
			                               items_length, output);
		}
	}

	g_object_unref (builder);
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
                    GHashTable/*<owned JsonNode>*/ *output)
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
                    GHashTable/*<owned JsonNode>*/ *output)
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
                       GHashTable/*<owned JsonNode>*/ *output)
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
                         GHashTable/*<owned JsonNode>*/ *output)
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
                         GHashTable/*<owned JsonNode>*/ *output)
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
	GPtrArray/*<unowned utf8>*/ *schema_member_names = NULL;  /* owned */
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

	schema_member_names = g_ptr_array_new ();
	set = g_hash_table_new (string_hash, string_equal);

	/* Put the required member names in the @schema_member_names. */
	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);
		g_ptr_array_add (schema_member_names,
		                 (gpointer) json_node_get_string (child_node));
	}

	/* Put the instance member names in the @set. */
	for (l = instance_member_names; l != NULL; l = l->next) {
		g_hash_table_add (set, l->data);
	}

	/* Check each of the @set against the @instance_member_names. */
	for (i = 0; i < schema_member_names->len; i++) {
		const gchar *member_name;

		member_name = schema_member_names->pdata[i];

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

	g_ptr_array_unref (schema_member_names);
	g_hash_table_unref (set);
	g_list_free (instance_member_names);
}

static void
generate_required (WblSchema *self,
                   JsonObject *root,
                   JsonNode *schema_node,
                   GHashTable/*<owned JsonNode>*/ *output)
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
	GError *child_error = NULL;

	if (validate_value_type (schema_node, G_TYPE_BOOLEAN)) {
		/* Valid. */
		return;
	}

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("additionalProperties must be a boolean or a "
		               "valid JSON Schema. "
		               "See json-schema-validation§5.4.4."));
		return;
	}

	/* Validate the child schema. */
	subschema_validate (self, schema_node, &child_error);

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
		subschema_validate (self, child_node, &child_error);

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
		subschema_validate (self, child_node, &child_error);

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

static gboolean
list_contains_string (GList *list, const gchar *data)
{
	GList *l;

	l = g_list_find_custom (list, data, (GCompareFunc) g_strcmp0);

	return (l != NULL);
}

/* json-schema-validation§5.4.4 */
static void
apply_properties_parent_schema (WblSchema *self,
                                JsonNode *ap_node,
                                JsonNode *p_node,
                                JsonNode *pp_node,
                                JsonObject *instance_object,
                                GError **error)
{
	gboolean ap_is_false;
	JsonObject *p_object;  /* unowned */
	JsonObject *pp_object = NULL;  /* unowned; nullable */
	GList/*<unowned utf8>*/ *set_s = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_p = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_pp = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */

	/* If additionalProperties is missing, its default value is an empty
	 * schema. json-schema-validation§5.4.4.3. */
	ap_is_false = (ap_node != NULL &&
	               validate_value_type (ap_node, G_TYPE_BOOLEAN) &&
	               !json_node_get_boolean (ap_node));

	/* As per json-schema-validation§5.4.4.2, validation always succeeds if
	 * additionalProperties is effectively true. */
	if (!ap_is_false) {
		return;
	}

	p_object = (p_node != NULL) ? json_node_get_object (p_node) : NULL;
	pp_object = (pp_node != NULL) ? json_node_get_object (pp_node) : NULL;

	/* Follow the algorithm in json-schema-validation§5.4.4.4. */
	set_s = json_object_get_members (instance_object);
	if (p_object != NULL) {
		set_p = json_object_get_members (p_object);
	}
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
		const gchar *regex_str;
		GRegex *regex = NULL;  /* owned */
		GList/*<unowned utf8>*/ *k = NULL;  /* unowned */
		GError *child_error = NULL;

		regex_str = l->data;

		/* Construct the regex. Should never fail due to being validated
		 * in validate_pattern_properties(). */
		regex = g_regex_new (regex_str, 0, 0, &child_error);
		g_assert_no_error (child_error);

		for (k = set_s; k != NULL;) {
			const gchar *member = k->data;

			if (g_regex_match (regex, member, 0, NULL)) {
				GList *next = k->next;

				set_s = g_list_delete_link (set_s, k);

				if (set_s == NULL) {
					break;
				}

				k = next;
			} else {
				k = k->next;
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

/* json-schema-validation§8.3.3 */
static void
apply_properties_child_schema (WblSchema *self,
                               JsonNode *ap_node,
                               JsonNode *p_node,
                               JsonNode *pp_node,
                               JsonObject *instance_object,
                               GError **error)
{
	JsonObject *p_object;  /* unowned */
	JsonObject *pp_object = NULL;  /* unowned; nullable */
	GList/*<unowned utf8>*/ *member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_p = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_pp = NULL;  /* owned */
	GList/*<unowned utf8>*/ *m = NULL, *l = NULL;  /* unowned */
	GHashTable/*<unowned JsonNode,
	             unowned JsonNode>*/ *set_s = NULL;  /* owned */

	p_object = (p_node != NULL) ? json_node_get_object (p_node) : NULL;
	pp_object = (pp_node != NULL) ? json_node_get_object (pp_node) : NULL;

	/* Follow the algorithm in json-schema-validation§8.3.3. */
	set_s = g_hash_table_new_full (g_direct_hash, g_direct_equal,
	                               NULL, NULL);

	if (p_object != NULL) {
		set_p = json_object_get_members (p_object);
	}
	if (pp_object != NULL) {
		set_pp = json_object_get_members (pp_object);
	}

	member_names = json_object_get_members (instance_object);

	for (m = member_names; m != NULL; m = m->next) {
		const gchar *member_name;
		GHashTableIter/*<unowned JsonNode, unowned JsonNode>*/ iter;
		JsonNode *child_node;  /* unowned */
		JsonNode *child_schema;  /* unowned */

		member_name = m->data;
		child_node = json_object_get_member (instance_object,
		                                     member_name);

		/* If @set_p contains @member_name, then the corresponding
		 * schema in ‘properties’ is added to @set_s.
		 * (json-schema-validation§8.3.3.2.) */
		if (list_contains_string (set_p, member_name)) {
			child_schema = json_object_get_member (p_object,
			                                       member_name);
			g_hash_table_add (set_s, child_schema);
		}

		/* For each regex in @set_pp, if it matches @member_name
		 * successfully, the corresponding schema in ‘patternProperties’
		 * is added to @set_s. (json-schema-validation§8.3.3.3.) */
		for (l = set_pp; l != NULL; l = l->next) {
			const gchar *regex_str;
			GRegex *regex = NULL;  /* owned */
			GError *child_error = NULL;

			regex_str = l->data;

			/* Construct the regex. Should never fail due to being
			 * validated in validate_pattern_properties(). */
			regex = g_regex_new (regex_str, 0, 0, &child_error);
			g_assert_no_error (child_error);

			if (g_regex_match (regex, member_name, 0, NULL)) {
				child_schema = json_object_get_member (pp_object,
				                                       regex_str);
				g_hash_table_add (set_s, child_schema);
			}

			g_regex_unref (regex);
		}

		/* The schema defined by ‘additionalProperties’ is added to
		 * @set_s if and only if, at this stage, @set_s is empty.
		 * (json-schema-validation§8.3.3.4.)
		 *
		 * Note that if @ap_node is %NULL or holds a boolean, it is
		 * considered equivalent to an empty schema, which is guaranteed
		 * to apply, so we don’t add that to @set_s.
		 * (json-schema-validation§8.3.2.) */
		if (g_hash_table_size (set_s) == 0 &&
		    ap_node != NULL && JSON_NODE_HOLDS_OBJECT (ap_node)) {
			g_hash_table_add (set_s, ap_node);
		}

		/* Now that we have a full set of the child schemas to apply to
		 * this property value, apply them. */
		g_hash_table_iter_init (&iter, set_s);

		while (g_hash_table_iter_next (&iter, NULL,
		                               (gpointer *) &child_schema)) {
			GError *child_error = NULL;

			subschema_apply (self, child_schema, child_node,
			                 &child_error);

			/* Report the first error. */
			if (child_error != NULL) {
				g_set_error (error,
				             WBL_SCHEMA_ERROR,
				             WBL_SCHEMA_ERROR_INVALID,
				             _("Object does not validate "
				               "against the schemas in the "
				               "‘%s’ child of the properties "
				               "schema keyword. See "
				               "json-schema-validation§8.3: "
				               "%s"),
				             member_name, child_error->message);
				g_clear_error (&child_error);

				goto done;
			}
		}

		g_hash_table_remove_all (set_s);
	}

done:
	g_list_free (set_pp);
	g_list_free (set_p);
	g_list_free (member_names);
	g_hash_table_unref (set_s);
}

static void
apply_properties_schemas (WblSchema *self,
                          JsonNode *ap_node,
                          JsonNode *p_node,
                          JsonNode *pp_node,
                          JsonNode *instance_node,
                          GError **error)
{
	JsonObject *instance_object;  /* unowned */
	GError *child_error = NULL;

	/* Validation succeeds if the instance is not an object. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	instance_object = json_node_get_object (instance_node);

	/* Validate the instance node. */
	apply_properties_parent_schema (self, ap_node, p_node, pp_node,
	                                instance_object, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return;
	}

	/* Validate its children. */
	apply_properties_child_schema (self, ap_node, p_node, pp_node,
	                               instance_object, &child_error);

	if (child_error != NULL) {
		g_propagate_error (error, child_error);
		return;
	}
}

static void
apply_properties (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  JsonNode *instance_node,
                  GError **error)
{
	JsonNode *ap_node, *p_node, *pp_node;  /* unowned */

	ap_node = json_object_get_member (root, "additionalProperties");
	p_node = schema_node;
	pp_node = json_object_get_member (root, "patternProperties");

	apply_properties_schemas (self, ap_node, p_node, pp_node,
	                          instance_node, error);
}

static void
apply_pattern_properties (WblSchema *self,
                          JsonObject *root,
                          JsonNode *schema_node,
                          JsonNode *instance_node,
                          GError **error)
{
	JsonNode *ap_node, *p_node, *pp_node;  /* unowned */

	ap_node = json_object_get_member (root, "additionalProperties");
	p_node = json_object_get_member (root, "properties");
	pp_node = schema_node;

	apply_properties_schemas (self, ap_node, p_node, pp_node,
	                          instance_node, error);
}

/* Generate a JSON object with an instance generated from @subschema_node as
 * @member_name. Generate one of these objects for each instance generated from
 * @subschema_node.
 *
 * @subschema_node may be %NULL to represent an empty schema. */
static void
generate_object_with_subschema (WblSchema *self,
                               JsonNode *subschema_node,
                               const gchar *member_name,
                               GHashTable/*<owned JsonNode>*/ *output)
{
	GHashTable/*<owned JsonNode>*/ *child_output = NULL;  /* owned */
	GHashTableIter iter;
	JsonNode *child_instance;  /* unowned */

	child_output = g_hash_table_new_full (node_hash,
	                                      node_equal,
	                                      (GDestroyNotify) json_node_free,
	                                      NULL);

	if (subschema_node != NULL) {
		subschema_generate_instances (self, subschema_node,
		                              child_output);
	} else {
		JsonNode *obj_node = NULL;

		obj_node = json_node_new (JSON_NODE_OBJECT);
		json_node_set_object (obj_node, json_object_new ());

		g_hash_table_add (output, obj_node);  /* transfer */
	}

	g_hash_table_iter_init (&iter, child_output);

	while (g_hash_table_iter_next (&iter, (gpointer *) &child_instance, NULL)) {
		JsonNode *obj_node = NULL;
		JsonObject *obj = NULL;

		obj = json_object_new ();
		json_object_set_member (obj, member_name, child_instance);
		g_hash_table_iter_steal (&iter);

		obj_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (obj_node, obj);

		g_hash_table_add (output, obj_node);  /* transfer */
	}

	g_hash_table_unref (child_output);
}

/* Generate a member name which does not match anything in @member_names. */
static gchar *  /* owned */
create_unique_member_name (const gchar *prefix,
                           GList/*<unowned utf8>*/ *member_names)
{
	gchar *member_name = NULL;  /* owned */
	guint i;

	for (i = 0;
	     member_name == NULL ||
	     list_contains_string (member_names, member_name);
	     i++) {
		g_free (member_name);
		member_name = g_strdup_printf ("%s%u", prefix, i);
	}

	return member_name;
}

static void
generate_properties (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GHashTable/*<owned JsonNode>*/ *output)
{
	JsonObject *schema_object;  /* unowned */
	GList/*<unowned utf8>*/ *member_names = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */
	JsonNode *ap_node;  /* unowned */
	JsonNode *obj_node = NULL;

	schema_object = json_node_get_object (schema_node);
	member_names = json_object_get_members (schema_object);

	/* Generate an instance for the empty object. */
	obj_node = json_node_new (JSON_NODE_OBJECT);
	json_node_take_object (obj_node, json_object_new ());
	g_hash_table_add (output, obj_node);  /* transfer */

	/* Generate instances for all property schemas and wrap them in
	 * objects. */
	for (l = member_names; l != NULL; l = l->next) {
		JsonNode *child_node;  /* unowned */
		const gchar *member_name;

		member_name = l->data;
		child_node = json_object_get_member (schema_object,
		                                     member_name);
		generate_object_with_subschema (self, child_node, member_name,
		                                output);
	}

	/* Generate an instance for the additionalProperties property. */
	ap_node = json_object_get_member (root, "additionalProperties");

	/* A missing additionalProperties is equivalent to an empty schema
	 * (json-schema-validation§8.3.2). */
	if (ap_node == NULL || JSON_NODE_HOLDS_OBJECT (ap_node)) {
		gchar *member_name = NULL;

		member_name = create_unique_member_name ("additionalProperties-test-",
		                                         member_names);
		generate_object_with_subschema (self, ap_node, member_name,
		                                output);
		g_free (member_name);
	} else if (ap_node != NULL) {
		gchar *member_name = NULL;
		JsonObject *obj = NULL;

		member_name = create_unique_member_name ("additionalProperties-test-",
		                                         member_names);

		obj = json_object_new ();
		json_object_set_member (obj, member_name,
		                        json_node_new (JSON_NODE_NULL));

		obj_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (obj_node, obj);

		g_hash_table_add (output, obj_node);  /* transfer */

		g_free (member_name);
	}

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
			GError *child_error = NULL;

			/* Must be a valid JSON Schema. */
			subschema_validate (self, child_node, &child_error);

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

			subschema_apply (self, child_node, instance_node,
			                 &child_error);

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
                       GHashTable/*<owned JsonNode>*/ *output)
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
               GHashTable/*<owned JsonNode>*/ *output)
{
	JsonArray *schema_array;  /* unowned */
	guint i;

	/* Output one instance of each of the child nodes. */
	schema_array = json_node_get_array (schema_node);

	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);
		generate_take_node (output, json_node_copy (child_node));
	}

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

	/* FIXME: The specification (json-schema-validation§5.5.2.1) does not
	 * require that the array is non-empty, as it does for some other
	 * arrays. However, it would make sense for this to be a requirement.
	 *
	 * For the moment, allow empty type arrays, and fail validation of all
	 * instances against them. This should be queried with the specification
	 * authors though. */

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

/* Convert a #JsonNode for the type keyword into a (potentially empty) array of
 * the #WblPrimitiveTypes it contains. If the keyword is a string, the array
 * will contain a single element. */
static GArray *  /* transfer full */
type_node_to_array (JsonNode *schema_node)
{
	WblPrimitiveType schema_node_type;
	const gchar *schema_str;
	GArray/*<WblPrimitiveType>*/ *schema_types = NULL;  /* owned */
	guint i;

	schema_types = g_array_new (FALSE, FALSE, sizeof (WblPrimitiveType));

	if (JSON_NODE_HOLDS_VALUE (schema_node)) {
		schema_str = json_node_get_string (schema_node);
		schema_node_type = primitive_type_from_string (schema_str);

		g_array_append_val (schema_types, schema_node_type);
	} else {
		JsonArray *schema_array;  /* unowned */

		schema_array = json_node_get_array (schema_node);

		for (i = 0; i < json_array_get_length (schema_array); i++) {
			JsonNode *child_node;  /* unowned */

			child_node = json_array_get_element (schema_array, i);

			schema_str = json_node_get_string (child_node);
			schema_node_type = primitive_type_from_string (schema_str);

			g_array_append_val (schema_types, schema_node_type);
		}
	}

	return schema_types;
}

static void
apply_type (WblSchema *self,
            JsonObject *root,
            JsonNode *schema_node,
            JsonNode *instance_node,
            GError **error)
{
	WblPrimitiveType instance_node_type;
	GArray/*<WblPrimitiveType>*/ *schema_types = NULL;  /* owned */
	guint i;

	/* Extract the type strings to an array of #WblPrimitiveTypes. */
	schema_types = type_node_to_array (schema_node);

	/* Validate the instance node type against the schema types. */
	instance_node_type = primitive_type_from_json_node (instance_node);

	for (i = 0; i < schema_types->len; i++) {
		WblPrimitiveType schema_node_type;

		schema_node_type = g_array_index (schema_types,
		                                  WblPrimitiveType, i);

		if (primitive_type_is_a (instance_node_type,
		                         schema_node_type)) {
			/* Success. */
			goto done;
		}
	}

	/* Type not found. Invalid. */
	g_set_error (error,
	             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID,
	             _("Instance type does not conform to type schema "
	               "keyword. See json-schema-validation§5.5.2."));

done:
	g_array_unref (schema_types);
}

static void
generate_type (WblSchema *self,
               JsonObject *root,
               JsonNode *schema_node,
               GHashTable/*<owned JsonNode>*/ *output)
{
	GArray/*<WblPrimitiveType>*/ *schema_types = NULL;  /* owned */
	guint i;

	/* Extract the type strings to an array of #WblPrimitiveTypes. */
	schema_types = type_node_to_array (schema_node);

	/* Generate instances for each of them. */
	for (i = 0; i < schema_types->len; i++) {
		WblPrimitiveType schema_node_type;
		JsonNode *node = NULL;

		schema_node_type = g_array_index (schema_types,
		                                  WblPrimitiveType, i);

		/* Add a valid instance. */
		switch (schema_node_type) {
		case WBL_PRIMITIVE_TYPE_ARRAY:
			node = json_node_new (JSON_NODE_ARRAY);
			json_node_take_array (node, json_array_new ());
			break;
		case WBL_PRIMITIVE_TYPE_BOOLEAN:
			node = node_new_boolean (TRUE);
			break;
		case WBL_PRIMITIVE_TYPE_INTEGER:
			node = node_new_int (1);
			break;
		case WBL_PRIMITIVE_TYPE_NUMBER:
			node = node_new_double (0.1);
			break;
		case WBL_PRIMITIVE_TYPE_NULL:
			node = json_node_new (JSON_NODE_NULL);
			break;
		case WBL_PRIMITIVE_TYPE_OBJECT:
			node = json_node_new (JSON_NODE_OBJECT);
			json_node_take_object (node, json_object_new ());
			break;
		case WBL_PRIMITIVE_TYPE_STRING:
			node = node_new_string ("");
			break;
		default:
			g_assert_not_reached ();
		}

		generate_take_node (output, node);

		/* And an invalid instance. */
		if (schema_node_type == WBL_PRIMITIVE_TYPE_NULL) {
			node = node_new_boolean (FALSE);
		} else {
			node = json_node_new (JSON_NODE_NULL);
		}

		generate_take_node (output, node);
	}

	g_array_unref (schema_types);
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
                 GHashTable/*<owned JsonNode>*/ *output)
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
                 GHashTable/*<owned JsonNode>*/ *output)
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
                 GHashTable/*<owned JsonNode>*/ *output)
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
	GError *child_error = NULL;

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("not must be a valid JSON Schema. See "
		               "json-schema-validation§5.5.6."));
		return;
	}

	/* Validate the child schema. */
	subschema_validate (self, schema_node, &child_error);

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
	GError *child_error = NULL;

	subschema_apply (self, schema_node, instance_node, &child_error);

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
              GHashTable/*<owned JsonNode>*/ *output)
{
	/* Generate instances for the schema. */
	subschema_generate_instances (self, schema_node, output);
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

/* default. json-schema-validation§6.2. */
static void
generate_default (WblSchema *self,
                  JsonObject *root,
                  JsonNode *schema_node,
                  GHashTable/*<owned JsonNode>*/ *output)
{
	/* Add the default value to the output. Technically, it could be invalid
	 * (json-schema-validation§6.2.2), but we can’t really know. */
	generate_take_node (output, json_node_copy (schema_node));
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
                        GHashTable/*<owned JsonNode, unowned JsonNode>*/ *output);

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
	{ "patternProperties", validate_pattern_properties, apply_pattern_properties, NULL },
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
	{ "default", NULL, NULL, generate_default },

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
                         GHashTable/*<owned JsonNode>*/ *output)
{
	guint i;

	/* Generate for each keyword in turn. */
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
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
 * Since: 0.1.0
 */
GPtrArray *
wbl_schema_generate_instances (WblSchema *self,
                               WblGenerateInstanceFlags flags)
{
	WblSchemaClass *klass;
	WblSchemaPrivate *priv;
	GHashTable/*<owned JsonNode>*/ *node_output = NULL;  /* owned */
	GHashTableIter iter;
	GPtrArray/*<owned WblGeneratedInstance>*/ *output = NULL;  /* owned */
	JsonParser *parser = NULL;  /* owned */
	JsonNode *node;  /* unowned */

	g_return_val_if_fail (WBL_IS_SCHEMA (self), NULL);

	klass = WBL_SCHEMA_GET_CLASS (self);
	priv = wbl_schema_get_instance_private (self);

	node_output = g_hash_table_new_full (node_hash,
	                                     node_equal,
	                                     (GDestroyNotify) json_node_free,
	                                     NULL);
	output = g_ptr_array_new_with_free_func ((GDestroyNotify) wbl_generated_instance_free);

	/* Generate schema instances. */
	if (klass->generate_instances != NULL) {
		klass->generate_instances (self, priv->schema, node_output);
	}

	/* See if they are valid. We cannot do this constructively because
	 * interactions between keywords change the validity of the overall
	 * JSON instance. */
	parser = json_parser_new ();

	g_hash_table_iter_init (&iter, node_output);

	while (g_hash_table_iter_next (&iter, (gpointer *) &node, NULL)) {
		gboolean valid;
		GError *error = NULL;

		/* Check the validity of this instance. */
		wbl_schema_apply (self, node, &error);
		valid = (error == NULL);

		g_clear_error (&error);

		/* Apply the filtering flags. */
		if ((!(flags & WBL_GENERATE_INSTANCE_IGNORE_VALID) || !valid) &&
		    (!(flags & WBL_GENERATE_INSTANCE_IGNORE_INVALID) || valid)) {
			WblGeneratedInstance *instance = NULL;
			gchar *json = NULL;

			/* Output the instance. */
			json = node_to_string (node);
			instance = _wbl_generated_instance_take_from_string (json, valid);
			g_ptr_array_add (output, instance);  /* transfer */
		}
	}

	g_object_unref (parser);
	g_hash_table_unref (node_output);

	/* Potentially add some invalid JSON. */
	if (flags & WBL_GENERATE_INSTANCE_INVALID_JSON) {
		g_ptr_array_add (output,
		                 wbl_generated_instance_new_from_string ("☠",
		                                                         FALSE));
	}

	return output;
}
