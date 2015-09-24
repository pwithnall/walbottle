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
 * SECTION:wbl-json-node
 * @short_description: JSON node utilities
 * @stability: Private
 * @include: libwalbottle/wbl-json-node.h
 *
 * A collection of utility functions for handling #JsonNode objects within the
 * context of JSON Schema. For example, this implements node hashing and
 * comparison, and a structured form of the JSON Schema type system.
 *
 * Since: UNRELEASED
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "wbl-json-node.h"

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

/**
 * wbl_primitive_type_from_string:
 * @str: a string form of a #WblPrimitiveType
 *
 * Parse @str to a #WblPrimitiveType. If the type is not recognised, abort.
 *
 * Use wbl_primitive_type_validate() to validate strings without aborting.
 *
 * Returns: the type represented by @str
 * Since: UNRELEASED
 */
WblPrimitiveType
wbl_primitive_type_from_string (const gchar *str)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (wbl_primitive_type_names); i++) {
		if (g_strcmp0 (str, wbl_primitive_type_names[i]) == 0) {
			return (WblPrimitiveType) i;
		}
	}

	g_assert_not_reached ();
}

/**
 * wbl_primitive_type_from_json_node:
 * @node: a #JsonNode
 *
 * Get the #WblPrimitiveType of the value in the given #JsonNode. This is a
 * superset of json_node_get_value_type(), as it handles objects, arrays and
 * null values as well.
 *
 * Returns: the #WblPrimitiveType of @node
 * Since: UNRELEASED
 */
WblPrimitiveType
wbl_primitive_type_from_json_node (JsonNode *node)
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

/**
 * wbl_primitive_type_validate:
 * @str: a string form of a #WblPrimitiveType
 *
 * Validate @str to see if it is a valid #WblPrimitiveType which could be
 * understood by wbl_primitive_type_from_string().
 *
 * Returns: %TRUE if @str is a valid #WblPrimitiveType; %FALSE otherwise
 * Since: UNRELEASED
 */
gboolean
wbl_primitive_type_validate (const gchar *str)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (wbl_primitive_type_names); i++) {
		if (g_strcmp0 (str, wbl_primitive_type_names[i]) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * wbl_primitive_type_is_a:
 * @sub: sub-type
 * @super: super-type
 *
 * Check whether @sub is a sub-type of, or equal to, @super. The only sub-type
 * relationship in the JSON Schema type system is that
 * %WBL_PRIMITIVE_TYPE_INTEGER is a sub-type of %WBL_PRIMITIVE_TYPE_NUMBER.
 *
 * Formally, this function calculates: `@sub <: @super`.
 *
 * Reference: http://json-schema.org/latest/json-schema-core.html#rfc.section.3.5
 *
 * Returns: %TRUE if @sub is a sub-type of, or equal to, @super; %FALSE
 *    otherwise
 * Since: UNRELEASED
 */
gboolean
wbl_primitive_type_is_a (WblPrimitiveType sub,
                         WblPrimitiveType super)
{
	return (super == sub ||
	        (super == WBL_PRIMITIVE_TYPE_NUMBER &&
	         sub == WBL_PRIMITIVE_TYPE_INTEGER));
}

/**
 * wbl_json_number_node_comparison:
 * @a: a #JsonNode
 * @b: another #JsonNode
 *
 * Compare the numbers stored in two nodes, returning a strcmp()-like value.
 * The nodes must each contain an integer or a double, but do not have to
 * contain the same type.
 *
 * It is an error to call this function with #JsonNodes which do not have type
 * #WBL_PRIMITIVE_TYPE_NUMBER or #WBL_PRIMITIVE_TYPE_INTEGER.
 *
 * Returns: an integer less than zero if @a < @b, equal to zero if @a == @b, and
 *    greater than zero if @a > @b
 * Since: UNRELEASED
 */
gint
wbl_json_number_node_comparison (JsonNode *a,
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

/* Convert a double to a string in the C locale, guaranteeing to include a
 * decimal point in the output (so it can’t be re-parsed as a JSON integer). */
static gchar *  /* transfer full */
double_to_string (gdouble i)
{
	gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr (buf, sizeof (buf), i);

	if (strchr (buf, '.') == NULL) {
		return g_strdup_printf ("%s.0", buf);
	} else {
		return g_strdup (buf);
	}
}

/**
 * wbl_json_number_node_to_string:
 * @node: a #JsonNode
 *
 * Convert the number stored in a node to a string. The output is
 * locale-independent.
 *
 * It is an error to call this function with a #JsonNode which does not have
 * type #WBL_PRIMITIVE_TYPE_NUMBER or #WBL_PRIMITIVE_TYPE_INTEGER.
 *
 * Returns: (transfer full): string form of @node
 * Since: UNRELEASED
 */
gchar *
wbl_json_number_node_to_string (JsonNode *node)
{
	GType node_type;

	node_type = json_node_get_value_type (node);

	if (node_type == G_TYPE_INT64) {
		return g_strdup_printf ("%" G_GINT64_FORMAT,
		                        json_node_get_int (node));
	} else if (node_type == G_TYPE_DOUBLE) {
		return double_to_string (json_node_get_double (node));
	} else {
		g_assert_not_reached ();
	}
}

/**
 * wbl_json_string_hash:
 * @key: (type utf8): a JSON string to hash
 *
 * Calculate a hash value for the given @key (a UTF-8 JSON string).
 *
 * Note: Member names are compared byte-wise, without applying any Unicode
 * decomposition or normalisation. This is not explicitly mentioned in the JSON
 * standard (ECMA-404), but is assumed.
 *
 * Returns: hash value for @key
 * Since: UNRELEASED
 */
guint
wbl_json_string_hash (gconstpointer key)
{
	return g_str_hash (key);
}

/**
 * wbl_json_string_equal:
 * @a: (type utf8): a JSON string
 * @b: (type utf8): another JSON string
 *
 * Check whether @a and @b are equal UTF-8 JSON strings.
 *
 * Returns: %TRUE if @a and @b are equal; %FALSE otherwise
 * Since: UNRELEASED
 */
gboolean
wbl_json_string_equal (gconstpointer a,
                       gconstpointer b)
{
	return g_str_equal (a, b);
}

/**
 * wbl_json_string_compare:
 * @a: (type utf8): a JSON string
 * @b: (type utf8): another JSON string
 *
 * Check whether @a and @b are equal UTF-8 JSON strings and return an ordering
 * over them in strcmp() style.
 *
 * Returns: an integer less than zero if @a < @b, equal to zero if @a == @b, and
 *    greater than zero if @a > @b
 * Since: UNRELEASED
 */
gint
wbl_json_string_compare (gconstpointer a,
                         gconstpointer b)
{
	return g_strcmp0 (a, b);
}

/**
 * wbl_json_node_hash:
 * @key: (type JsonNode): a #JsonNode to hash
 *
 * Calculate a hash value for the given @key (a #JsonNode).
 *
 * Reference: http://json-schema.org/latest/json-schema-core.html#rfc.section.3.6
 *
 * Returns: hash value for @key
 * Since: UNRELEASED
 */
guint
wbl_json_node_hash (gconstpointer key)
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
	type = wbl_primitive_type_from_json_node (node);

	switch (type) {
	case WBL_PRIMITIVE_TYPE_BOOLEAN:
		return json_node_get_boolean (node) ? true_hash : false_hash;
	case WBL_PRIMITIVE_TYPE_NULL:
		return null_hash;
	case WBL_PRIMITIVE_TYPE_STRING:
		return wbl_json_string_hash (json_node_get_string (node));
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

		return (len | wbl_json_node_hash (first_element));
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

/* Reference: json-schema-core§3.6. */
/**
 * wbl_json_node_equal:
 * @a: (type JsonNode): a #JsonNode
 * @b: (type JsonNode): another #JsonNode
 *
 * Check whether @a and @b are equal #JsonNodes, in the sense defined by JSON
 * Schema.
 *
 * Reference: http://json-schema.org/latest/json-schema-core.html#rfc.section.3.6
 *
 * Returns: %TRUE if @a and @b are equal; %FALSE otherwise
 * Since: UNRELEASED
 */
gboolean
wbl_json_node_equal (gconstpointer a,
                     gconstpointer b)
{
	JsonNode *node_a, *node_b;  /* unowned */
	WblPrimitiveType type_a, type_b;

	node_a = (JsonNode *) a;
	node_b = (JsonNode *) b;

	/* Identity comparison. */
	if (node_a == node_b) {
		return TRUE;
	}

	type_a = wbl_primitive_type_from_json_node (node_a);
	type_b = wbl_primitive_type_from_json_node (node_b);

	/* Eliminate mismatched types rapidly. */
	if (!wbl_primitive_type_is_a (type_a, type_b) &&
	    !wbl_primitive_type_is_a (type_b, type_a)) {
		return FALSE;
	}

	switch (type_a) {
	case WBL_PRIMITIVE_TYPE_NULL:
		return TRUE;
	case WBL_PRIMITIVE_TYPE_BOOLEAN:
		return (json_node_get_boolean (node_a) ==
		        json_node_get_boolean (node_b));
	case WBL_PRIMITIVE_TYPE_STRING:
		return wbl_json_string_equal (json_node_get_string (node_a),
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

		/* Identity comparison. */
		if (array_a == array_b) {
			return TRUE;
		}

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

			if (!wbl_json_node_equal (child_a, child_b)) {
				return FALSE;
			}
		}

		return TRUE;
	}
	case WBL_PRIMITIVE_TYPE_OBJECT: {
		JsonObject *object_a, *object_b;
		guint size_a, size_b;
		JsonObjectIter iter_a;
		JsonNode *child_a, *child_b;  /* unowned */
		const gchar *member_name;

		object_a = json_node_get_object (node_a);
		object_b = json_node_get_object (node_b);

		/* Identity comparison. */
		if (object_a == object_b) {
			return TRUE;
		}

		/* Check sizes. */
		size_a = json_object_get_size (object_a);
		size_b = json_object_get_size (object_b);

		if (size_a != size_b) {
			return FALSE;
		}

		/* Check member names and values. Check the member names first
		 * to avoid expensive recursive value comparisons which might
		 * be unnecessary. */
		json_object_iter_init (&iter_a, object_a);

		while (json_object_iter_next (&iter_a, &member_name, NULL)) {
			if (!json_object_has_member (object_b, member_name)) {
				return FALSE;
			}
		}

		json_object_iter_init (&iter_a, object_a);

		while (json_object_iter_next (&iter_a, &member_name, &child_a)) {
			child_b = json_object_get_member (object_b, member_name);

			if (!wbl_json_node_equal (child_a, child_b)) {
				return FALSE;
			}
		}

		return TRUE;
	}
	default:
		g_assert_not_reached ();
	}
}
