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

/*
 * SECTION:wbl-string-set
 * @short_description: Storage of mathematical sets of UTF-8 strings
 * @stability: Private
 * @include: libwalbottle/wbl-string-set.h
 *
 * #WblStringSet is a data structure for representing sets of strings, in the
 * mathematical sense of a set. Each string is UTF-8.
 *
 * The API of #WblStringSet is designed to closely match the underlying
 * mathematical operations; this was prioritised as a design goal over
 * performance of the data structure. If performance is found to be a problem,
 * the structure can be improved to use copy-on-write internally, and to
 * implement its own hash table structure to avoid the allocations necessary for
 * using #GHashTable.
 *
 * Each #WblStringSet is immutable after creation, and all operations produce
 * new immutable instances, rather than modifying their inputs.
 *
 * Floating references are supported, and all newly constructed #WblStringSets
 * have a floating reference. Each operation sinks and unrefs, so operations can
 * be chained together easily without worrying about memory management.
 *
 * #WblStringSet is unsorted, and iteration over it may happen in any order.
 *
 * Since: 0.2.0
 */

#include "config.h"

#include <glib.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "wbl-string-set.h"


typedef enum {
	STATE_NONE = 0,
	STATE_FLOATING = (1 << 0),
	STATE_IMMUTABLE = (1 << 1),
} WblStringSetState;

struct _WblStringSet {
	volatile gint ref_count;
	gint state;
	guint hash;  /* XOR of g_str_hash() of all members of @set;
	              * 0 for the empty set */
	GHashTable/*<owned utf8, unowned utf8>*/ *set;
};

G_DEFINE_BOXED_TYPE (WblStringSet, wbl_string_set,
                     wbl_string_set_ref, wbl_string_set_unref);

/**
 * _wbl_string_set_add:
 * @set: a #WblStringSet
 * @member: new member
 *
 * Add a member to the string set. @set must not be immutable; it is modified
 * in place. This is an internal method.
 *
 * Since: 0.2.0
 */
static void
_wbl_string_set_add (WblStringSet  *set,
                     const gchar   *member)
{
	g_return_if_fail ((set->state & STATE_IMMUTABLE) == 0);

	if (g_hash_table_add (set->set, g_strdup (member))) {
		set->hash ^= g_str_hash (member);
	}
}

/**
 * _wbl_string_set_is_valid:
 * @set: a #WblStringSet
 *
 * Check that @set is non-%NULL and has a valid reference count.
 *
 * Returns: %TRUE if @set is valid, %FALSE otherwise
 *
 * Since: 0.2.0
 */
static gboolean
_wbl_string_set_is_valid (WblStringSet  *set)
{
	return (set != NULL && set->ref_count > 0);
}

/**
 * _wbl_string_set_new:
 *
 * Create a new #WblStringSet with a floating reference and no members. It is
 * not yet marked as immutable; the caller must initialise the set’s members
 * then mark it as immutable.
 *
 * Returns: (transfer full): a new #WblStringSet
 *
 * Since: 0.2.0
 */
static WblStringSet *
_wbl_string_set_new (void)
{
	WblStringSet *set = NULL;

	set = g_new0 (WblStringSet, 1);
	set->ref_count = 1;
	set->state = STATE_FLOATING;
	set->hash = 0;
	set->set = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                  (GDestroyNotify) g_free, NULL);

	return set;
}

/**
 * wbl_string_set_new_empty:
 *
 * Create a new empty #WblStringSet. The set is returned with a floating
 * reference.
 *
 * Returns: (transfer full): a new #WblStringSet
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_new_empty (void)
{
	WblStringSet *output = NULL;

	output = _wbl_string_set_new ();
	output->state |= STATE_IMMUTABLE;

	return output;
}

/**
 * wbl_string_set_new_singleton:
 * @element: the element to be in the set
 *
 * Create a new single-element #WblStringSet. The set is returned with a
 * floating reference.
 *
 * Returns: (transfer full): a new #WblStringSet
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_new_singleton (const gchar  *element)
{
	WblStringSet *output = NULL;

	output = _wbl_string_set_new ();
	_wbl_string_set_add (output, element);
	output->state |= STATE_IMMUTABLE;

	return output;
}

/**
 * wbl_string_set_new_from_object_members:
 * @obj: a JSON object
 *
 * Create a new #WblStringSet containing the set of names for the properties in
 * @obj, which may be an empty set.
 *
 * Returns: (transfer full): a new #WblStringSet
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_new_from_object_members (JsonObject  *obj)
{
	JsonObjectIter iter;
	WblStringSet *set = NULL;
	const gchar *member_name;

	set = _wbl_string_set_new ();
	json_object_iter_init (&iter, obj);

	while (json_object_iter_next (&iter, &member_name, NULL)) {
		_wbl_string_set_add (set, member_name);
	}

	set->state |= STATE_IMMUTABLE;

	return set;
}

/**
 * wbl_string_set_new_from_array_elements:
 * @array: a JSON array of UTF-8 strings
 *
 * Create a new #WblStringSet containing the values of all the elements in
 * @array, which may be empty. Duplicate elements are ignored.
 *
 * It is a programmer error to call this with a #JsonArray which contains
 * non-UTF-8 elements.
 *
 * Returns: (transfer full): a new #WblStringSet
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_new_from_array_elements (JsonArray  *array)
{
	WblStringSet *set = NULL;
	guint i, len;

	set = _wbl_string_set_new ();

	for (i = 0, len = json_array_get_length (array); i < len; i++) {
		_wbl_string_set_add (set,
		                     json_array_get_string_element (array, i));
	}

	set->state |= STATE_IMMUTABLE;

	return set;
}

/**
 * wbl_string_set_dup:
 * @set: existing set to copy
 *
 * Create a new string set which contains all the elements of @set, which may
 * be empty.
 *
 * Returns: (transfer full): a new #WblStringSet
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_dup (WblStringSet  *set)
{
	return wbl_string_set_union (wbl_string_set_new_empty (), set);
}

/**
 * wbl_string_set_ref_sink:
 * @set: a #WblStringSet
 *
 * If @set has a floating reference, mark it as no longer floating. Otherwise,
 * increase the reference count of @set.
 *
 * Returns: (transfer full): pass through of @set
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_ref_sink (WblStringSet  *set)
{
	g_return_val_if_fail (_wbl_string_set_is_valid (set), NULL);

	if (set->state & STATE_FLOATING) {
		set->state &= ~STATE_FLOATING;
	} else {
		wbl_string_set_ref (set);
	}

	return set;
}

/**
 * wbl_string_set_ref:
 * @set: a #WblStringSet
 *
 * Increase the reference count of @set. If @set is floating, its reference
 * count will still be increased, and it will remain floating.
 *
 * Returns: (transfer full): pass through of @set
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_ref (WblStringSet  *set)
{
	g_return_val_if_fail (_wbl_string_set_is_valid (set), NULL);

	g_atomic_int_inc (&set->ref_count);

	return set;
}

/**
 * wbl_string_set_unref:
 * @set: a #WblStringSet
 *
 * Decrease the reference count of @set. If this reaches zero, free the object.
 * If @set is floating, its reference count will still be decreased, and it
 * will remain floating.
 *
 * Since: 0.2.0
 */
void
wbl_string_set_unref (WblStringSet  *set)
{
	g_return_if_fail (_wbl_string_set_is_valid (set));

	if (g_atomic_int_dec_and_test (&set->ref_count)) {
		g_hash_table_unref (set->set);
		g_free (set);
	}
}

/**
 * wbl_string_set_union:
 * @a: (transfer floating): a #WblStringSet
 * @b: (transfer floating): another #WblStringSet
 *
 * Create the union of the two sets, @a and @b. The result set will be returned
 * as a new instance with a floating reference. Neither @a or @b will be
 * modified, though floating references on them will be sunk.
 *
 * Returns: (transfer full): the union of @a and @b
 *
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_union (WblStringSet  *a,
                      WblStringSet  *b)
{
	WblStringSet *output = NULL;
	GHashTableIter iter;
	const gchar *key;

	output = _wbl_string_set_new ();

	wbl_string_set_ref_sink (a);
	wbl_string_set_ref_sink (b);

	/* Add all from @a. */
	g_hash_table_iter_init (&iter, a->set);

	while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL)) {
		_wbl_string_set_add (output, key);
	}

	/* Add all from @b. */
	g_hash_table_iter_init (&iter, b->set);

	while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL)) {
		_wbl_string_set_add (output, key);
	}

	wbl_string_set_unref (b);
	wbl_string_set_unref (a);

	output->state |= STATE_IMMUTABLE;

	return output;
}

/**
 * wbl_string_set_union_dependencies:
 * @set: (transfer floating): a #WblStringSet
 * @dependencies: a JSON object mapping property names to arrays of property
 *    names (or to object instances)
 *
 * Calculate the transitive union of the dependencies of the elements of an
 * initial @set and return it as a new #WblStringSet. @dependencies is treated
 * as a map of property names to dependencies; the function essentially
 * calculates
 *    output = set ∪ ⋃_{d ϵ output} dependencies(d)
 *
 * Complexity: O(S * D) in the size S of @set and number D of @dependencies keys
 * Returns: the transitive dependency set of @set
 * Since: 0.2.0
 */
WblStringSet *
wbl_string_set_union_dependencies (WblStringSet  *set,
                                   JsonObject    *dependencies)
{
	WblStringSet *output = NULL;
	guint old_output_size;

	g_return_val_if_fail (_wbl_string_set_is_valid (set), NULL);
	g_return_val_if_fail (dependencies != NULL, NULL);

	output = _wbl_string_set_new ();

	wbl_string_set_ref_sink (set);

	do {
		GHashTableIter iter;
		const gchar *property_name;

		old_output_size = g_hash_table_size (output->set);

		g_hash_table_iter_init (&iter, set->set);

		while (g_hash_table_iter_next (&iter, (gpointer *) &property_name, NULL)) {
			JsonNode *dependency_value;
			guint i, len;

			/* Add the dependency key. */
			_wbl_string_set_add (output, property_name);

			/* See if there are any values associated with the
			 * dependency key. */
			dependency_value = json_object_get_member (dependencies,
			                                           property_name);

			/* Ignore schema dependencies; we only care about
			 * property dependencies, for which we add all
			 * dependent properties. */
			if (dependency_value != NULL &&
			    JSON_NODE_HOLDS_ARRAY (dependency_value)) {
				JsonArray *array;

				array = json_node_get_array (dependency_value);

				for (i = 0, len = json_array_get_length (array);
				     i < len;
				     i++) {
					_wbl_string_set_add (output,
					                     json_array_get_string_element (array, i));
				}
			}
		}
	} while (old_output_size != g_hash_table_size (output->set));

	wbl_string_set_unref (set);

	output->state |= STATE_IMMUTABLE;

	return output;
}

/**
 * wbl_string_set_contains:
 * @set: a #WblStringSet
 * @member: member to test
 *
 * Check whether @set contains @member. Formally:
 *    @member ϵ @set
 *
 * Returns: %TRUE if @member is in @set, %FALSE otherwise
 *
 * Since: 0.2.0
 */
gboolean
wbl_string_set_contains (WblStringSet  *set,
                         const gchar   *member)
{
	g_return_val_if_fail (_wbl_string_set_is_valid (set), FALSE);
	g_return_val_if_fail (member != NULL, FALSE);

	return g_hash_table_contains (set->set, member);
}

/**
 * wbl_string_set_get_size:
 * @set: a #WblStringSet
 *
 * Get the number of elements in @set (its cardinality). Formally:
 *    |@set|
 *
 * Returns: cardinality of @set
 *
 * Since: 0.2.0
 */
guint
wbl_string_set_get_size (WblStringSet  *set)
{
	g_return_val_if_fail (_wbl_string_set_is_valid (set), 0);

	return g_hash_table_size (set->set);
}

/**
 * wbl_string_set_hash:
 * @set: a #WblStringSet
 *
 * Calculate a hash value for the @set. This is guaranteed not to change for
 * immutable sets.
 *
 * Returns: hash value for the set
 *
 * Since: 0.2.0
 */
guint
wbl_string_set_hash (WblStringSet  *set)
{
	g_return_val_if_fail (_wbl_string_set_is_valid (set), 0);

	/* String sets must be immutable to be hashed, or their hash value could
	 * end up changing while they’re in the container. */
	g_return_val_if_fail (set->state & STATE_IMMUTABLE, 0);

	return set->hash;
}

/**
 * wbl_string_set_equal:
 * @a: a #WblStringSet
 * @b: another #WblStringSet
 *
 * Check whether two string sets are equal — whether they contain exactly the
 * same members. Formally:
 *    @a = @b ≡ @a ⊆ @b ∧ @b ⊆ @a
 *
 * Returns: %TRUE if @a and @b are equal, %FALSE otherwise
 *
 * Since: 0.2.0
 */
gboolean
wbl_string_set_equal (WblStringSet  *a,
                      WblStringSet  *b)
{
	GHashTableIter iter;
	const gchar *key;

	g_return_val_if_fail (_wbl_string_set_is_valid (a), FALSE);
	g_return_val_if_fail (_wbl_string_set_is_valid (b), FALSE);

	/* Quick checks. */
	if (a == b)
		return TRUE;
	if (a->hash != b->hash)
		return FALSE;
	if (wbl_string_set_get_size (a) != wbl_string_set_get_size (b))
		return FALSE;

	/* Compare elements. */
	g_hash_table_iter_init (&iter, a->set);

	while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL)) {
		if (!g_hash_table_contains (b->set, key)) {
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * wbl_string_set_to_string:
 * @set: a #WblStringSet
 *
 * Convert @set to a human-readable string. This is intended for debugging
 * purposes, and the format may change in future.
 *
 * Returns: (transfer full): a string representation of @set
 *
 * Since: 0.2.0
 */
gchar *
wbl_string_set_to_string (WblStringSet  *set)
{
	GString *output = NULL;
	WblStringSetIter iter;
	const gchar *element;
	gboolean first = TRUE;

	g_return_val_if_fail (_wbl_string_set_is_valid (set), NULL);

	output = g_string_new ("{");
	wbl_string_set_iter_init (&iter, set);

	while (wbl_string_set_iter_next (&iter, &element)) {
		if (first) {
			g_string_append_printf (output, " ‘%s’", element);
		} else {
			g_string_append_printf (output ,", ‘%s’", element);
		}

		first = FALSE;
	}

	g_string_append (output, first ? "}" : " }");

	return g_string_free (output, FALSE);
}

/**
 * wbl_string_set_iter_init:
 * @iter: an uninitialised #WblStringSetIter
 * @set: the #WblStringSet to iterate over
 *
 * Initialise the @iter and associate it with @set. The @set must be immutable
 * in order to be iterated over.
 *
 * |[<!-- language="C" -->
 * WblStringSetIter iter;
 * const gchar *element;
 *
 * wbl_string_set_iter_init (&iter, some_set);
 * while (wbl_string_set_iter_next (&iter, &element))
 *   {
 *     // Do something with @element.
 *   }
 * ]|
 *
 * Since: 0.2.0
 */
void
wbl_string_set_iter_init (WblStringSetIter  *iter,
                          WblStringSet      *set)
{
	g_return_if_fail (iter != NULL);
	g_return_if_fail (_wbl_string_set_is_valid (set));
	g_return_if_fail (set->state & STATE_IMMUTABLE);

	iter->set = set;
	g_hash_table_iter_init (&iter->iter, set->set);
}

/**
 * wbl_string_set_iter_next:
 * @iter: a #WblStringSetIter
 * @member: (out callee-allocates) (transfer none) (optional): return location
 *    for a set element, or %NULL
 *
 * Advance @iter and retrieve the next @element in the set. If the end of the
 * set is reached, %FALSE is returned and @member is set to an invalid value.
 * After that point, the @iter is invalid.
 *
 * Returns: %TRUE if @member is valid; %FALSE if the end of the set has been
 *    reached
 *
 * Since: 0.2.0
 */
gboolean
wbl_string_set_iter_next (WblStringSetIter  *iter,
                          const gchar      **member)
{
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (_wbl_string_set_is_valid (iter->set), FALSE);
	g_return_val_if_fail (iter->set->state & STATE_IMMUTABLE, FALSE);

	return g_hash_table_iter_next (&iter->iter, (gpointer *) member, NULL);
}
