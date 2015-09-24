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
 * ## Build system integration # {#build-system-integration}
 *
 * The most common usage of Walbottle is to integrate it into the unit tests for
 * a parser in the software under test (SUT). This is typically done with the
 * `json-schema-generate` utility which comes with Walbottle.
 *
 * To do so is straightforward if the SUT is using autotools. Add the following
 * to the `configure.ac` file in the SUT:
 * |[
 * AC_PATH_PROG([JSON_SCHEMA_VALIDATE],[json-schema-validate])
 * AC_PATH_PROG([JSON_SCHEMA_GENERATE],[json-schema-generate])
 *
 * AS_IF([test "$JSON_SCHEMA_VALIDATE" == ""],
 *       [AC_MSG_ERROR([json-schema-validate not found])])
 * AS_IF([test "$JSON_SCHEMA_GENERATE" == ""],
 *       [AC_MSG_ERROR([json-schema-generate not found])])
 * ]|
 *
 * Then add the following in the `Makefile.am` for the SUT’s parser unit tests
 * and adjust `json_schemas` to point to the schemas for the format (or formats)
 * accepted by the SUT’s parser:
 * |[
 * json_schemas = \
 * 	my-format.schema.json \
 * 	my-other-format.schema.json \
 * 	$(NULL)
 *
 * EXTRA_DIST += $(json_schemas)
 *
 * check-json-schema: $(json_schemas)
 * 	$(AM_V_GEN)$(JSON_SCHEMA_VALIDATE) $^
 * check-local: check-json-schema
 * .PHONY: check-json-schema
 *
 * json_schemas_c = $(json_schemas:.schema.json=.schema.c)
 *
 * CLEANFILES += $(json_schemas_c)
 *
 * %.schema.c: %.schema.json
 * 	$(AM_V_GEN)$(JSON_SCHEMA_GENERATE) \
 * 		--c-variable-name=$(subst -,_,$(notdir $*))_json_instances \
 * 		--format c $^ > $@
 * ]|
 *
 * If using the GLib test framework, a typical way of then using the generated
 * test vectors in a test suite is to include the generated C file and add a
 * unit test for each vector. In `Makefile.am:`
 * |[
 * my_test_suite_SOURCES = my-test-suite.c my-format.schema.c
 * ]|
 *
 * And in the test suite code itself (`my-test-suite.c`):
 * |[
 * #include "my-format.schema.c"
 *
 * …
 *
 * // Test the parser with each generated test vector from the JSON schema.
 * static void
 * test_parser_generated (gconstpointer user_data)
 * {
 *   guint i;
 *   GObject *parsed = NULL;
 *   GError *error = NULL;
 *
 *   i = GPOINTER_TO_UINT (user_data);
 *
 *   parsed = try_parsing_string (my_format_json_instances[i].json,
 *                                my_format_json_instances[i].size, &error);
 *
 *   if (my_format_json_instances[i].is_valid)
 *     {
 *       // Assert @parsed is valid.
 *       g_assert_no_error (error);
 *       g_assert (G_IS_OBJECT (parser));
 *     }
 *   else
 *     {
 *       // Assert parsing failed.
 *       g_assert_error (error, SOME_ERROR_DOMAIN, SOME_ERROR_CODE);
 *       g_assert (parsed == NULL);
 *     }
 *
 *   g_clear_error (&error);
 *   g_clear_object (&parsed);
 * }
 *
 * …
 *
 * int
 * main (int argc, char *argv[])
 * {
 *   guint i;
 *
 *   …
 *
 *   for (i = 0; i < G_N_ELEMENTS (my_format_json_instances); i++)
 *     {
 *       gchar *test_name = NULL;
 *
 *       test_name = g_strdup_printf ("/parser/generated/%u", i);
 *       g_test_add_data_func (test_name, GUINT_TO_POINTER (i),
 *                             test_parser_generated);
 *       g_free (test_name);
 *     }
 *
 *   …
 * }
 * ]|
 *
 * Since: 0.1.0
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "wbl-json-node.h"
#include "wbl-schema.h"
#include "wbl-string-set.h"

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

		/* We know that an empty schema (‘{}’) is always valid, so avoid
		 * recursing on this. This is important — the default values of
		 * several schema properties is an empty subschema, which can
		 * cause infinite recursion on this path.
		 *
		 * For example, json-schema-validation§5.3.1.4. */
		if (json_object_get_size (node.node) > 0) {
			klass->validate_schema (self, &node, error);
		}

		json_object_unref (node.node);
	}
}

static void
subschema_apply (WblSchema *self,
                 JsonObject *subschema_object,
                 JsonNode *instance_node,
                 GError **error)
{
	WblSchemaClass *klass;

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (klass->apply_schema != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = subschema_object;
		klass->apply_schema (self, &node, instance_node, error);
	}
}

static void
subschema_generate_instances (WblSchema *self,
                              JsonObject *subschema_object,
                              GHashTable/*<owned JsonNode>*/ *output)
{
	WblSchemaClass *klass;

	klass = WBL_SCHEMA_GET_CLASS (self);

	if (klass->generate_instance_nodes != NULL) {
		WblSchemaNode node;

		node.ref_count = 1;
		node.node = subschema_object;

		/* Avoid infinite recursion; see the rationale in
		 * subschema_validate(). Generate a null value instead. */
		if (json_object_get_size (node.node) > 0) {
			klass->generate_instance_nodes (self, &node, output);
		} else {
			g_hash_table_add (output,
			                  json_node_new (JSON_NODE_NULL));
		}
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
real_generate_instance_nodes (WblSchema *self,
                              WblSchemaNode *schema,
                              GHashTable/*<owned JsonNode>*/ *output);

static void
generate_all_properties (WblSchema                       *self,
                         JsonArray                       *_required,
                         gint64                           min_properties,
                         gint64                           max_properties,
                         JsonObject                      *properties,
                         JsonObject                      *pattern_properties,
                         JsonNode                        *additional_properties,
                         JsonObject                      *dependencies,
                         GHashTable/*<owned JsonNode>*/  *output);

struct _WblSchemaPrivate {
	JsonParser *parser;  /* owned */
	WblSchemaNode *schema;  /* owned; NULL when not loading */
	gboolean debug;
};

G_DEFINE_TYPE_WITH_PRIVATE (WblSchema, wbl_schema, G_TYPE_OBJECT)

static void
wbl_schema_class_init (WblSchemaClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = wbl_schema_dispose;

	klass->validate_schema = real_validate_schema;
	klass->apply_schema = real_apply_schema;
	klass->generate_instance_nodes = real_generate_instance_nodes;
}

static void
wbl_schema_init (WblSchema *self)
{
	WblSchemaPrivate *priv;
	const gchar *messages_debug;

	priv = wbl_schema_get_instance_private (self);

	priv->parser = json_parser_new ();

	/* Check whether to enable debug output. */
	messages_debug = g_getenv ("G_MESSAGES_DEBUG");
	priv->debug = FALSE;

	if (messages_debug != NULL) {
		gchar **domains = NULL;
		const gchar * const *m;

		domains = g_strsplit (messages_debug, ",", -1);

		for (m = (const gchar * const *) domains; *m != NULL; m++) {
			if (g_str_equal (*m, "all") ||
			    g_str_equal (*m, G_LOG_DOMAIN)) {
				priv->debug = TRUE;
			} else if (g_str_equal (*m, "none")) {
				priv->debug = FALSE;
			}
		}

		g_strfreev (domains);
	}
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

	set = g_hash_table_new (wbl_json_string_hash, wbl_json_string_equal);

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
		JsonObject *child_object = NULL;
		GError *child_error = NULL;

		child_object = json_array_get_object_element (schema_array, i);

		subschema_apply (self, child_object, instance_node,
		                 &child_error);

		/* Count successes. */
		if (child_error == NULL) {
			n_successes++;
		}

		g_clear_error (&child_error);
	}

	return n_successes;
}

/* Generate instances for all of the schemas in a schema array. */
static void
generate_schema_array (WblSchema *self,
                       JsonArray *schema_array,
                       GHashTable/*<owned JsonNode>*/ *output)
{
	guint i, n_schemas;

	n_schemas = json_array_get_length (schema_array);

	/* Generate instances for all schemas. */
	for (i = 0; i < n_schemas; i++) {
		JsonObject *child_object;  /* unowned */

		child_object = json_array_get_object_element (schema_array, i);
		subschema_generate_instances (self, child_object, output);
	}
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

		str = wbl_json_number_node_to_string (schema_node);
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
		return;
	}

	if (!json_object_has_member (root, "maximum")) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("maximum must be present if exclusiveMaximum is "
		               "present. See json-schema-validation§5.1.2."));
		return;
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

	maximum_str = wbl_json_number_node_to_string (schema_node);

	/* Actually perform the validation. */
	comparison = wbl_json_number_node_comparison (instance_node,
	                                              schema_node);

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
		return;
	}

	if (!json_object_has_member (root, "minimum")) {
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("minimum must be present if exclusiveMinimum is "
		               "present. See json-schema-validation§5.1.3."));
		return;
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

	minimum_str = wbl_json_number_node_to_string (schema_node);

	/* Actually perform the validation. */
	comparison = wbl_json_number_node_comparison (instance_node,
	                                              schema_node);

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

/**
 * array_copy_n:
 * @items: a JSON array instance
 * @length: number of items to copy
 *
 * Build an array containing a copy of the first @length elements of @items.
 * @length must be less than or equal to the length of @items.
 *
 * Returns: (transfer full): a copy of the @length prefix of @items
 *
 * Since: UNRELEASED
 */
static JsonArray *
array_copy_n (JsonArray  *items,
              guint       length)
{
	guint i;
	JsonArray *new_array = NULL;

	g_return_val_if_fail (length <= json_array_get_length (items), NULL);

	new_array = json_array_sized_new (length);
	for (i = 0; i < length; i++) {
		json_array_add_element (new_array,
		                        json_node_copy (json_array_get_element (items, i)));
	}

	return new_array;
}

/**
 * array_copy_append_n:
 * @items: a JSON array instance
 * @n_copies: number of elements to append to the array
 * @additional_items_subschema: subschema to copy
 *
 * Build an array containing a complete copy of @items, with @n_copies of
 * @additional_items_subschema appended.
 *
 * Returns: (transfer full): a copy of @items with @n_copies items appended
 *
 * Since: UNRELEASED
 */
static JsonArray *
array_copy_append_n (JsonArray   *items,
                     guint        n_copies,
                     JsonObject  *additional_items_subschema)
{
	guint i;
	JsonArray *new_array = NULL;

	g_return_val_if_fail (items != NULL, NULL);
	g_return_val_if_fail (additional_items_subschema != NULL, NULL);

	new_array = json_array_sized_new (json_array_get_length (items) + n_copies);
	for (i = 0; i < json_array_get_length (items); i++) {
		json_array_add_element (new_array,
		                        json_node_copy (json_array_get_element (items, i)));
	}

	/* Append @n_copies of @additional_items_subschema. */
	for (i = 0; i < n_copies; i++) {
		JsonNode *node = NULL;

		node = json_node_new (JSON_NODE_OBJECT);
		json_node_set_object (node, additional_items_subschema);
		json_array_add_element (new_array, node);  /* transfer */
	}

	return new_array;
}

/**
 * generate_subschema_arrays:
 * @items_node: `items` schema keyword
 * @additional_items_boolean: `additionalItems` schema keyword if it’s a
 *    boolean; %TRUE if it’s a subschema
 * @additional_items_subschema: (nullable): `additionalItems` schema keyword if
 *    it’s a subschema; %NULL otherwise
 * @min_items: `minItems` schema keyword
 * @max_items: `maxItems` schema keyword
 *
 * Generate a collection of arrays of subschemas which represent the formats of
 * array we are interested in generating as test vectors. All the returned
 * subschema arrays should be valid against the schema (for example, they should
 * all respect @max_items).
 *
 * See inline comments for formal descriptions of the code.
 *
 * Returns: (transfer full) (element-type JsonArray): array of subschema
 *    instances
 *
 * Since: UNRELEASED
 */
static GPtrArray/*<owned JsonArray>*/ *
generate_subschema_arrays (JsonNode    *items_node,
                           gboolean     additional_items_boolean,
                           JsonObject  *additional_items_subschema,
                           gint64       min_items,
                           gint64       max_items)
{
	GPtrArray/*<owned JsonArray>*/ *output = NULL;
	guint64 i, limit;

	g_return_val_if_fail (!additional_items_boolean ||
	                      additional_items_subschema != NULL, NULL);

	g_debug ("%s: additionalItems = %s, minItems = %"
	         G_GINT64_FORMAT ", maxItems = %" G_GINT64_FORMAT, G_STRFUNC,
	         additional_items_boolean ? "subschema" : "false", min_items,
	         max_items);

	/* TODO: Add formal documentation inline. */
	output = g_ptr_array_new_with_free_func ((GDestroyNotify) json_array_unref);

	if (JSON_NODE_HOLDS_ARRAY (items_node)) {
		JsonArray *items_array;

		items_array = json_node_get_array (items_node);

		/* Output sub-arrays from @items. */
		limit = MIN (json_array_get_length (items_array), max_items);

		for (i = min_items; i <= limit; i++) {
			JsonArray *generated = NULL;

			generated = array_copy_n (items_array, i);
			g_ptr_array_add (output, generated);  /* transfer */
		}

		/* And generate arrays of @additional_items_subschema. */
		if (!additional_items_boolean) {
			return output;
		}

		if (max_items == G_MAXINT64) {
			limit = json_array_get_length (items_array) + 1;
		} else {
			limit = max_items;
		}

		for (i = json_array_get_length (items_array) + 1; i <= limit; i++) {
			JsonArray *generated = NULL;

			generated = array_copy_append_n (items_array,
			                                 i - json_array_get_length (items_array),
			                                 additional_items_subschema);
			g_ptr_array_add (output, generated);
		}
	} else {
		JsonObject *items_subschema;

		items_subschema = json_node_get_object (items_node);

		/* Output a fixed repetition of @items_subschema and ignore
		 * @additional_items_subschema.
		 * json-schema-validation§8.2.3.1. */
		if (max_items != G_MAXINT64) {
			limit = max_items;
		} else if (additional_items_boolean) {
			/* Push this up to (arbitrarily) 2 to check the parser
			 * can handle multiple elements. */
			limit = MAX (min_items, 1) + 1;
		} else {
			limit = min_items;
		}

		for (i = min_items; i <= limit; i++) {
			JsonArray *generated = NULL;
			guint j;

			generated = json_array_sized_new (i);
			for (j = 0; j < i; j++) {
				JsonNode *node = NULL;

				node = json_node_new (JSON_NODE_OBJECT);
				json_node_set_object (node, items_subschema);
				json_array_add_element (generated, node);  /* transfer */
			}

			g_ptr_array_add (output, generated);  /* transfer */
		}
	}

	return output;
}

/**
 * instance_drop_n_elements:
 * @array: a JSON array instance
 * @n: number of elements to drop
 *
 * Build a copy of @array with the last @n of its items removed. @n must be
 * less than or equal to the length of @array.
 *
 * Returns: (transfer full): a new JSON instance with fewer items
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_drop_n_elements (JsonArray  *array,
                          guint       n)
{
	JsonNode *output = NULL;

	g_return_val_if_fail (n <= json_array_get_length (array), NULL);

	output = json_node_new (JSON_NODE_ARRAY);
	json_node_take_array (output,
	                      array_copy_n (array, json_array_get_length (array) - n));

	return output;
}

/**
 * instance_add_n_elements:
 * @array: a JSON array instance
 * @n: number of elements to add
 * @items_node: `items` schema keyword
 * @additional_items_node: `additionalItems` schema keyword
 *
 * Build a copy of @array with @n items appended. The new items will be
 * generated to match @items_node or @additional_items_node; if @items_node is
 * an array, as many items will be generated as there are indices of that array
 * more than the current length of @array. Any further items will be generated
 * to match @additional_items_node. If @items_node is a subschema, all new
 * items will be generated to match it.
 *
 * Returns: (transfer full): a new JSON instance with @n extra items
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_add_n_elements (JsonArray  *array,
                         guint       n,
                         JsonNode   *items_node,
                         JsonNode   *additional_items_node)
{
	JsonNode *output = NULL;
	JsonArray *new_array = NULL;
	guint i;

	output = json_node_new (JSON_NODE_ARRAY);
	new_array = array_copy_n (array, json_array_get_length (array));

	/* TODO: This really should generate from
	 * @items_node and @additional_items_node. */
	for (i = 0; i < n; i++) {
		json_array_add_null_element (new_array);
	}

	json_node_take_array (output, new_array);

	return output;
}

/**
 * instance_add_null_element:
 * @array: a JSON array instance
 *
 * Build a copy of @array with one `null` JSON instance appended.
 *
 * Returns: (transfer full): a new JSON instance with one extra item
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_add_null_element (JsonArray  *array)
{
	JsonNode *output = NULL;
	JsonArray *new_array = NULL;

	output = json_node_new (JSON_NODE_ARRAY);
	new_array = array_copy_n (array, json_array_get_length (array));
	json_array_add_null_element (new_array);
	json_node_take_array (output, new_array);

	return output;
}

/**
 * instance_clone_final_element
 * @array: a JSON array instance
 *
 * Build a copy of @array with its final element cloned and appended. If @array
 * is empty, a new empty array will be returned.
 *
 * Returns: (transfer full): a new JSON instance with one extra item
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_clone_final_element (JsonArray  *array)
{
	JsonNode *output = NULL;
	JsonArray *new_array = NULL;
	guint len;

	len = json_array_get_length (array);
	output = json_node_new (JSON_NODE_ARRAY);
	new_array = array_copy_n (array, len);

	if (len > 0) {
		JsonNode *final_element;

		final_element = json_array_get_element (new_array, len - 1);
		json_array_add_element (new_array,
		                        json_node_copy (final_element));
	}

	json_node_take_array (output, new_array);

	return output;
}

/**
 * generate_boolean_array:
 * @n_elements: number of elements to generate
 * @first_false_element: index of the first element to be %FALSE
 *
 * Generate an array of boolean values which are all %TRUE until index
 * @first_false_element, after (and including) which they are all %FALSE.
 *
 * Returns: (transfer full): array of @n_elements
 * Since: UNRELEASED
 */
static GArray/*<boolean>*/ *
generate_boolean_array (guint   n_elements,
                        guint   first_false_element)
{
	GArray/*<boolean>*/ *output = NULL;
	guint i;

	/* @first_false_element represents the first index to be false. */
	output = g_array_sized_new (FALSE, TRUE, sizeof (gboolean), n_elements);

	for (i = 0; i < n_elements; i++) {
		gboolean val = (i < first_false_element);
		g_array_append_val (output, val);
	}

	return output;
}

/**
 * generate_validity_arrays:
 * @n_elements: number of elements to generate in each array
 * @items_node: `items` schema keyword
 * @max_n_valid_instances: minimum number of arrays to generate containing %TRUE
 *    for each index (inclusive)
 * @max_n_invalid_instances: minimum number of arrays to generate containing
 *    %FALSE for each index (inclusive)
 *
 * Generate a collection of arrays controlling the validity of child instances
 * in test vectors generated for an items schema (@items_node). The idea is that
 * generated test vectors should test all the control flow paths in the code
 * under test, and since most JSON vectors will be parsed by either a sequence
 * of code or a loop which exits on the first invalid child instance, the way to
 * test such code is to generate test vectors which transition from valid
 * children to invalid children at different points in the array.
 *
 * Each validity array is an array of booleans of length @n_elements which
 * contains %TRUE elements in the indexes corresponding to child instances which
 * should be valid, and %FALSE elements for invalid ones. When an array instance
 * is generated matching a validity array, these booleans are respected.
 *
 * It is important that enough array instances are generated such that each
 * possible child instance is included in at least one array instance — this is
 * controlled by @max_n_valid_instances and @max_n_invalid_instances.
 *
 * Returns: (transfer full): collection of boolean validity arrays
 * Since: UNRELEASED
 */
static GPtrArray/*<owned GArray<boolean>>*/ *
generate_validity_arrays (guint      n_elements,
                          JsonNode  *items_node,
                          guint      max_n_valid_instances,
                          guint      max_n_invalid_instances)
{
	GPtrArray/*<owned GArray<boolean>>*/ *output = NULL;
	guint i;

	g_debug ("%s: n_elements: %u, items_node: %s, max_n_valid_instances: "
	         "%u, max_n_invalid_instances: %u", G_STRFUNC, n_elements,
	         JSON_NODE_HOLDS_OBJECT (items_node) ? "subschema" : "subschema array",
	         max_n_valid_instances, max_n_invalid_instances);

	output = g_ptr_array_new_with_free_func ((GDestroyNotify) g_array_unref);

	if (!JSON_NODE_HOLDS_OBJECT (items_node)) {
		/* @items_node is an array of subschemas, which items must
		 * conform to based on matching indexes. Therefore, we need to
		 * worry about index-dependent control paths in the code under
		 * test. One approach for this is to generate validity arrays
		 * which test [ …, true, true, false, false, … ] step changes
		 * for each index in @n_elements. The [ false, … ] and
		 * [ …, true ] base cases are handled below. */
		for (i = 1; i < n_elements; i++) {
			/* @i represents the first index to be false. */
			g_ptr_array_add (output,
			                 generate_boolean_array (n_elements, i));
		}
	}

	/* Now add arrays of the form:
	 *    [ false, …, false ]
	 *    [ true, …, true ]
	 *
	 * which handle index-independent control paths. The most important ones
	 * here are the all-valid arrays, as the more valid instances we can
	 * generate, the better. If these instances are used as child nodes in
	 * a larger instance, validation of that instance can’t proceed without
	 * valid child nodes.
	 *
	 * Overall, we need to be sure to generate enough instances that the
	 * *minimum* number of true values for a given index is at least
	 * @max_n_valid_instances; and similarly for the number of false values
	 * and @max_n_invalid_instances. This ensures that each of the generated
	 * instances is used at least once. */
	for (i = 0; i < max_n_valid_instances; i++) {
		g_ptr_array_add (output,
		                 generate_boolean_array (n_elements,
		                                         n_elements));
	}

	for (i = 0; i < max_n_invalid_instances; i++) {
		g_ptr_array_add (output,
		                 generate_boolean_array (n_elements, 0));
	}

	/* Fallback output to test the empty array case. */
	if (n_elements == 0) {
		g_ptr_array_add (output, generate_boolean_array (0, 0));
	}

	return output;
}

static gchar *
validity_array_to_string (GArray/*<boolean>*/  *arr)
{
	guint i;
	GString *out = NULL;

	out = g_string_new ("[");

	for (i = 0; i < arr->len; i++) {
		if (i > 0) {
			g_string_append (out, ",");
		}

		g_string_append (out,
		                 g_array_index (arr, gboolean, i) ? "1" : "0");
	}

	g_string_append (out, (i > 0) ? " ]" : "]");

	return g_string_free (out, FALSE);
}

/**
 * generate_all_items:
 * @self: a #WblSchema
 * @items_node: `items` schema keyword
 * @additional_items_node: `additionalItems` schema keyword
 * @min_items: minimum number of items required (inclusive)
 * @max_items: maximum number of items allowed (inclusive)
 * @unique_items: %TRUE if all items have to be unique, %FALSE otherwise
 * @output: caller-allocated hash table for output to be appended to
 *
 * Generic implementation for the generate function for all item-related
 * schema keywords (minItems`, `maxItems`, `items`, `additionalItems`,
 * `uniqueItems`).
 *
 * The overall approach taken by this function is to:
 *  # Generate a set of arrays of subschemas which match the schema’s
 *    constraints.
 *  # For each schema array, generate a set of array instances which all have
 *    that number of items, each matching the corresponding subschema.
 *  # For each item schema keyword, mutate all the array instances in some way
 *    to explore validation of the boundary conditions for validity of that
 *    keyword.
 *  # Add all the mutated and non-mutated array instances to @output.
 *
 * Since: UNRELEASED
 */
static void
generate_all_items (WblSchema                       *self,
                    JsonNode                        *items_node,
                    JsonNode                        *additional_items_node,
                    gint64                           min_items,
                    gint64                           max_items,
                    gboolean                         unique_items,
                    GHashTable/*<owned JsonNode>*/  *output)
{
	WblSchemaPrivate *priv;
	GPtrArray/*<owned JsonArray>*/ *subschema_arrays = NULL;
	gboolean additional_items_boolean;
	JsonObject *additional_items_subschema = NULL;  /* nullable; owned */
	GHashTable/*<owned JsonNode>*/ *instance_set = NULL;
	GHashTable/*<owned JsonNode>*/ *mutation_set = NULL;
	guint i, j, k;
	JsonBuilder *builder = NULL;
	GHashTableIter iter;
	JsonNode *node;
	GHashTable/*<unowned JsonObject,
	             owned GHashTable<owned JsonNode>>*/ *valid_instances_map = NULL;
	GHashTable/*<unowned JsonObject,
	             owned GHashTable<owned JsonNode>>*/ *invalid_instances_map = NULL;

	priv = wbl_schema_get_instance_private (self);

	/* Massage the input to remove some irregularities. */
	if (validate_value_type (additional_items_node, G_TYPE_BOOLEAN)) {
		additional_items_boolean = json_node_get_boolean (additional_items_node);
		if (additional_items_boolean) {
			additional_items_subschema = json_object_new ();
		} else {
			additional_items_subschema = NULL;
		}
	} else {
		additional_items_boolean = TRUE;
		additional_items_subschema = json_node_dup_object (additional_items_node);
	}

	/* Generate a set of arrays of subschemas. Note that the output must
	 * differ if @items_node is a subschema vs being a singleton array of
	 * subschemas — the former is index-independent whereas the latter is
	 * not. */
	subschema_arrays = generate_subschema_arrays (items_node,
	                                              additional_items_boolean,
	                                              additional_items_subschema,
	                                              min_items, max_items);

	/* Debug. */
	for (i = 0; i < subschema_arrays->len && priv->debug; i++) {
		JsonNode *debug_node = NULL;
		gchar *json = NULL;

		debug_node = json_node_new (JSON_NODE_ARRAY);
		json_node_set_array (debug_node, subschema_arrays->pdata[i]);
		json = node_to_string (debug_node);
		g_debug ("%s: Subschema array: %s", G_STRFUNC, json);
		g_free (json);
		json_node_free (debug_node);
	}

	/* Since we encounter the same subschemas many times, and will always
	 * generate the same set of subinstances for each, store those
	 * subinstances to avoid repetition. */
	valid_instances_map = g_hash_table_new_full (g_direct_hash,
	                                             g_direct_equal,
	                                             NULL,
	                                             (GDestroyNotify) g_hash_table_unref);
	invalid_instances_map = g_hash_table_new_full (g_direct_hash,
	                                               g_direct_equal,
	                                               NULL,
	                                               (GDestroyNotify) g_hash_table_unref);

	/* Generate a set of array instances from the set of arrays of
	 * subschemas. */
	instance_set = g_hash_table_new_full (wbl_json_node_hash,
	                                      wbl_json_node_equal,
	                                      (GDestroyNotify) json_node_free,
	                                      NULL);
	builder = json_builder_new ();

	for (i = 0; i < subschema_arrays->len; i++) {
		JsonArray *subschema_array;
		GPtrArray/*<owned GArray<boolean>>*/ *validity_arrays = NULL;
		GPtrArray/*<owned GHashTable<owned JsonNode>>*/ *valid_instances_array;
		GPtrArray/*<owned GHashTable<owned JsonNode>>*/ *invalid_instances_array;
		GHashTableIter *valid_iters = NULL, *invalid_iters = NULL;
		guint n_iterations_remaining;
		guint max_n_valid_instances, max_n_invalid_instances;

		subschema_array = subschema_arrays->pdata[i];
		valid_instances_array = g_ptr_array_new_full (json_array_get_length (subschema_array),
		                                              (GDestroyNotify) g_hash_table_unref);
		invalid_instances_array = g_ptr_array_new_full (json_array_get_length (subschema_array),
		                                                (GDestroyNotify) g_hash_table_unref);
		max_n_valid_instances = 0;
		max_n_invalid_instances = 0;

		for (j = 0; j < json_array_get_length (subschema_array); j++) {
			GHashTable/*<owned JsonNode>*/ *valid_instances = NULL;
			GHashTable/*<owned JsonNode>*/ *invalid_instances = NULL;
			GHashTableIter valid_iter;
			JsonObject *subschema;

			subschema = json_array_get_object_element (subschema_array, j);

			valid_instances = g_hash_table_lookup (valid_instances_map,
			                                       subschema);
			invalid_instances = g_hash_table_lookup (invalid_instances_map,
			                                         subschema);

			if (valid_instances == NULL ||
			    invalid_instances == NULL) {
				valid_instances = g_hash_table_new_full (wbl_json_node_hash,
				                                         wbl_json_node_equal,
				                                         (GDestroyNotify) json_node_free,
				                                         NULL);
				invalid_instances = g_hash_table_new_full (wbl_json_node_hash,
				                                           wbl_json_node_equal,
				                                           (GDestroyNotify) json_node_free,
				                                           NULL);

				subschema_generate_instances (self, subschema,
				                              valid_instances);

				/* Split the instances into valid and invalid. */
				g_hash_table_iter_init (&valid_iter, valid_instances);

				while (g_hash_table_iter_next (&valid_iter,
				                               (gpointer *) &node, NULL)) {
					GError *child_error = NULL;

					subschema_apply (self, subschema, node,
					                 &child_error);

					if (child_error != NULL) {
						g_hash_table_iter_steal (&valid_iter);
						g_hash_table_add (invalid_instances,
						                  node);

						g_error_free (child_error);
					}

					/* Debug output. */
					if (priv->debug) {
						gchar *debug_output = NULL;

						debug_output = node_to_string (node);
						g_debug ("%s: Subinstance %u (%s): %s",
						         G_STRFUNC, j,
						         (child_error == NULL) ? "valid" : "invalid",
						         debug_output);
						g_free (debug_output);
					}
				}

				/* Store for later re-use. Transfer
				 * ownership. */
				g_hash_table_insert (valid_instances_map,
				                     subschema,
				                     valid_instances);
				g_hash_table_insert (invalid_instances_map,
				                     subschema,
				                     invalid_instances);
			}

			g_ptr_array_add (valid_instances_array,
			                 g_hash_table_ref (valid_instances));
			g_ptr_array_add (invalid_instances_array,
			                 g_hash_table_ref (invalid_instances));

			max_n_valid_instances = MAX (max_n_valid_instances,
			                             g_hash_table_size (valid_instances));
			max_n_invalid_instances = MAX (max_n_invalid_instances,
			                               g_hash_table_size (invalid_instances));
		}

		validity_arrays = generate_validity_arrays (json_array_get_length (subschema_array),
		                                            items_node,
		                                            max_n_valid_instances,
		                                            max_n_invalid_instances);

		/* Debug. */
		for (j = 0; j < validity_arrays->len && priv->debug; j++) {
			gchar *arr;

			arr = validity_array_to_string (validity_arrays->pdata[j]);
			g_debug ("%s: Validity array: %s", G_STRFUNC, arr);
			g_free (arr);
		}

		/* Now combine all the instances for each array position to
		 * produce a set of output instances. There are various
		 * heuristics we could choose for this: completeness is
		 * guaranteed by taking the N-fold cross-product between all the
		 * instances over all N positions in @subschema_array. That
		 * produces a combinatorial explosion of output instances,
		 * however.
		 *
		 * Instead, we assume that the code under test is going to
		 * validate each of the instances in order, and hence we only
		 * need to care about true → false transitions. We need to
		 * ensure we generate at least one valid array instance (valid
		 * instance in each index) and one invalid instance (invalid
		 * instance in at least one index); we also want to ensure we
		 * include each valid child instance in at least one valid
		 * array, and each invalid child instance in at least one
		 * invalid array.
		 *
		 * Hence, iterate over all the members of the sets in
		 * @instances_array in parallel, iterating for each validity
		 * array in @validity_arrays as a template.
		 */
		valid_iters = g_new0 (GHashTableIter, valid_instances_array->len);
		invalid_iters = g_new0 (GHashTableIter, invalid_instances_array->len);
		n_iterations_remaining = 0;

		for (j = 0; j < valid_instances_array->len; j++) {
			GHashTable/*<owned JsonNode>*/ *instances;

			instances = valid_instances_array->pdata[j];
			g_hash_table_iter_init (&valid_iters[j], instances);
			n_iterations_remaining = MAX (n_iterations_remaining,
			                              g_hash_table_size (instances));
		}

		for (j = 0; j < invalid_instances_array->len; j++) {
			GHashTable/*<owned JsonNode>*/ *instances;

			instances = invalid_instances_array->pdata[j];
			g_hash_table_iter_init (&invalid_iters[j], instances);
			n_iterations_remaining = MAX (n_iterations_remaining,
			                              g_hash_table_size (instances));
		}

		for (j = 0; j < validity_arrays->len; j++) {
			GArray/*<boolean>*/ *validity_array;
			JsonNode *instance = NULL;
			gchar *debug_output = NULL;

			validity_array = validity_arrays->pdata[j];

			json_builder_begin_array (builder);

			for (k = 0; k < validity_array->len; k++) {
				GHashTable/*<owned JsonNode>*/ *instances;
				GHashTableIter *instances_iter;
				JsonNode *generated_instance;

				if (g_array_index (validity_array, gboolean, k)) {
					instances = valid_instances_array->pdata[k];
					instances_iter = &valid_iters[k];
				} else {
					instances = invalid_instances_array->pdata[k];
					instances_iter = &invalid_iters[k];
				}

				if (g_hash_table_size (instances) == 0) {
					generated_instance = NULL;
				} else if (!g_hash_table_iter_next (instances_iter,
				                                    (gpointer *) &generated_instance,
				                                    NULL)) {
					/* Arbitrarily loop round and start
					 * again. */
					g_hash_table_iter_init (instances_iter,
					                        instances);
					if (!g_hash_table_iter_next (instances_iter,
					                             (gpointer *) &generated_instance,
					                             NULL)) {
						generated_instance = NULL;
					}
				}

				if (generated_instance != NULL) {
					json_builder_add_value (builder,
					                        json_node_copy (generated_instance));
				} else {
					json_builder_add_null_value (builder);
				}
			}

			json_builder_end_array (builder);

			instance = json_builder_get_root (builder);
			g_hash_table_add (instance_set, instance);  /* transfer */

			/* Debug output. */
			if (priv->debug) {
				debug_output = node_to_string (instance);
				g_debug ("%s: Instance: %s", G_STRFUNC, debug_output);
				g_free (debug_output);
			}

			json_builder_reset (builder);
		}

		g_free (valid_iters);
		g_free (invalid_iters);
		g_ptr_array_unref (valid_instances_array);
		g_ptr_array_unref (invalid_instances_array);
		g_ptr_array_unref (validity_arrays);
	}

	g_hash_table_unref (invalid_instances_map);
	g_hash_table_unref (valid_instances_map);

	/* The final step is to take each of the generated instances, and
	 * mutate it for each of the relevant schema properties. */
	mutation_set = g_hash_table_new_full (wbl_json_node_hash,
	                                      wbl_json_node_equal,
	                                      (GDestroyNotify) json_node_free,
	                                      NULL);

	/* Mutate on minItems, maxItems, additionalItems, items and
	 * uniqueItems. */
	g_hash_table_iter_init (&iter, instance_set);

	while (g_hash_table_iter_next (&iter, (gpointer *) &node, NULL)) {
		JsonNode *mutated_instance = NULL;
		JsonArray *array;

		array = json_node_get_array (node);

		/* minItems. */
		if (min_items > 0) {
			g_assert (json_array_get_length (array) >= min_items);

			mutated_instance = instance_drop_n_elements (array,
			                                             json_array_get_length (array) - min_items + 1);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* maxItems. */
		if (max_items < G_MAXINT64) {
			g_assert (json_array_get_length (array) <= max_items);

			mutated_instance = instance_add_n_elements (array,
			                                            max_items - json_array_get_length (array) + 1,
			                                            items_node,
			                                            additional_items_node);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* items and additionalItems. Add an extra element to the array
		 * if additionalItems is false. */
		if (validate_value_type (additional_items_node, G_TYPE_BOOLEAN) &&
		    !json_node_get_boolean (additional_items_node) &&
		    (JSON_NODE_HOLDS_OBJECT (items_node) ||
		     json_array_get_length (array) == json_array_get_length (json_node_get_array (items_node)))) {
			mutated_instance = instance_add_null_element (array);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* TODO: How to do uniqueItems? */
		if (unique_items && json_array_get_length (array) > 0) {
			mutated_instance = instance_clone_final_element (array);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		} else if (unique_items) {
			JsonArray *new_array = NULL;

			mutated_instance = json_node_new (JSON_NODE_ARRAY);
			new_array = json_array_new ();
			json_array_add_null_element (new_array);
			json_array_add_null_element (new_array);

			json_node_take_array (mutated_instance, new_array);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}
	}

	/* Throw the output over the fence. */
	g_hash_table_iter_init (&iter, instance_set);

	while (g_hash_table_iter_next (&iter, (gpointer *) &node, NULL)) {
		generate_take_node (output, json_node_copy (node));
	}

	g_hash_table_iter_init (&iter, mutation_set);

	while (g_hash_table_iter_next (&iter, (gpointer *) &node, NULL)) {
		generate_take_node (output, json_node_copy (node));
	}

	g_hash_table_unref (mutation_set);
	g_hash_table_unref (instance_set);
	g_ptr_array_unref (subschema_arrays);
	g_clear_pointer (&additional_items_subschema,
	                 (GDestroyNotify) json_object_unref);
	g_object_unref (builder);
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
generate_additional_items (WblSchema *self,
                           JsonObject *root,
                           JsonNode *schema_node,
                           GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *items_node, *additional_items_node = NULL;
	gint64 min_items, max_items;
	gboolean unique_items;

	if (json_object_has_member (root, "items")) {
		items_node = json_object_dup_member (root, "items");
	} else {
		/* json-schema-validation§5.3.1.4. */
		items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (items_node, json_object_new ());
	}

	additional_items_node = schema_node;

	if (json_object_has_member (root, "minItems")) {
		min_items = json_object_get_int_member (root, "minItems");
	} else {
		/* json-schema-validation§5.3.3.3. */
		min_items = 0;
	}

	if (json_object_has_member (root, "maxItems")) {
		max_items = json_object_get_int_member (root, "maxItems");
	} else {
		max_items = G_MAXINT64;
	}

	if (json_object_has_member (root, "uniqueItems")) {
		unique_items = json_object_get_boolean_member (root,
		                                               "uniqueItems");
	} else {
		/* json-schema-validation§5.3.4.3. */
		unique_items = FALSE;
	}

	/* TODO: this is called multiple times; probably could reduce that */
	generate_all_items (self, items_node, additional_items_node, min_items,
	                    max_items, unique_items, output);

	json_node_free (items_node);
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
		if (sub_schema_node == NULL ||
		    !JSON_NODE_HOLDS_OBJECT (sub_schema_node)) {
			continue;
		}

		/* Validate the child instance. */
		child_node = json_array_get_element (instance_array, i);
		subschema_apply (self, json_node_get_object (sub_schema_node),
		                 child_node, &child_error);

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

static void
generate_items (WblSchema *self,
                JsonObject *root,
                JsonNode *schema_node,
                GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *items_node, *additional_items_node = NULL;
	gint64 min_items, max_items;
	gboolean unique_items;

	items_node = schema_node;

	if (json_object_has_member (root, "additionalItems")) {
		additional_items_node = json_object_dup_member (root,
		                                                "additionalItems");
	} else {
		/* json-schema-validation§5.3.1.4. */
		additional_items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_items_node,
		                       json_object_new ());
	}

	if (json_object_has_member (root, "minItems")) {
		min_items = json_object_get_int_member (root, "minItems");
	} else {
		/* json-schema-validation§5.3.3.3. */
		min_items = 0;
	}

	if (json_object_has_member (root, "maxItems")) {
		max_items = json_object_get_int_member (root, "maxItems");
	} else {
		max_items = G_MAXINT64;
	}

	if (json_object_has_member (root, "uniqueItems")) {
		unique_items = json_object_get_boolean_member (root,
		                                               "uniqueItems");
	} else {
		/* json-schema-validation§5.3.4.3. */
		unique_items = FALSE;
	}

	/* TODO: this is called multiple times; probably could reduce that */
	generate_all_items (self, items_node, additional_items_node, min_items,
	                    max_items, unique_items, output);

	json_node_free (additional_items_node);
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
	JsonNode *items_node = NULL, *additional_items_node = NULL;
	gint64 min_items, max_items;
	gboolean unique_items;

	if (json_object_has_member (root, "items")) {
		items_node = json_object_dup_member (root, "items");
	} else {
		/* json-schema-validation§5.3.1.4. */
		items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (items_node, json_object_new ());
	}

	if (json_object_has_member (root, "additionalItems")) {
		additional_items_node = json_object_dup_member (root,
		                                                "additionalItems");
	} else {
		/* json-schema-validation§5.3.1.4. */
		additional_items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_items_node,
		                       json_object_new ());
	}

	if (json_object_has_member (root, "minItems")) {
		min_items = json_object_get_int_member (root, "minItems");
	} else {
		/* json-schema-validation§5.3.3.3. */
		min_items = 0;
	}

	max_items = json_node_get_int (schema_node);

	if (json_object_has_member (root, "uniqueItems")) {
		unique_items = json_object_get_boolean_member (root,
		                                               "uniqueItems");
	} else {
		/* json-schema-validation§5.3.4.3. */
		unique_items = FALSE;
	}

	/* TODO: this is called multiple times; probably could reduce that */
	generate_all_items (self, items_node, additional_items_node, min_items,
	                    max_items, unique_items, output);

	json_node_free (additional_items_node);
	json_node_free (items_node);
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
	JsonNode *items_node = NULL, *additional_items_node = NULL;
	gint64 min_items, max_items;
	gboolean unique_items;

	if (json_object_has_member (root, "items")) {
		items_node = json_object_dup_member (root, "items");
	} else {
		/* json-schema-validation§5.3.1.4. */
		items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (items_node, json_object_new ());
	}

	if (json_object_has_member (root, "additionalItems")) {
		additional_items_node = json_object_dup_member (root,
		                                                "additionalItems");
	} else {
		/* json-schema-validation§5.3.1.4. */
		additional_items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_items_node,
		                       json_object_new ());
	}

	min_items = json_node_get_int (schema_node);

	if (json_object_has_member (root, "maxItems")) {
		max_items = json_object_get_int_member (root, "maxItems");
	} else {
		max_items = G_MAXINT64;
	}

	if (json_object_has_member (root, "uniqueItems")) {
		unique_items = json_object_get_boolean_member (root,
		                                               "uniqueItems");
	} else {
		/* json-schema-validation§5.3.4.3. */
		unique_items = FALSE;
	}

	/* TODO: this is called multiple times; probably could reduce that */
	generate_all_items (self, items_node, additional_items_node, min_items,
	                    max_items, unique_items, output);

	json_node_free (additional_items_node);
	json_node_free (items_node);
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
	unique_hash = g_hash_table_new (wbl_json_node_hash,
	                                wbl_json_node_equal);

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
	JsonNode *items_node = NULL, *additional_items_node = NULL;
	gint64 min_items, max_items;
	gboolean unique_items;

	if (json_object_has_member (root, "items")) {
		items_node = json_object_dup_member (root, "items");
	} else {
		/* json-schema-validation§5.3.1.4. */
		items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (items_node, json_object_new ());
	}

	if (json_object_has_member (root, "additionalItems")) {
		additional_items_node = json_object_dup_member (root,
		                                                "additionalItems");
	} else {
		/* json-schema-validation§5.3.1.4. */
		additional_items_node = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_items_node,
		                       json_object_new ());
	}

	if (json_object_has_member (root, "minItems")) {
		min_items = json_object_get_int_member (root, "minItems");
	} else {
		/* json-schema-validation§5.3.3.3. */
		min_items = 0;
	}

	if (json_object_has_member (root, "maxItems")) {
		max_items = json_object_get_int_member (root, "maxItems");
	} else {
		max_items = G_MAXINT64;
	}

	unique_items = json_node_get_boolean (schema_node);

	/* TODO: this is called multiple times; probably could reduce that */
	generate_all_items (self, items_node, additional_items_node, min_items,
	                    max_items, unique_items, output);

	json_node_free (additional_items_node);
	json_node_free (items_node);
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
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = json_object_get_member (root, "required");
	min_properties_node = json_object_get_member (root, "minProperties");
	max_properties_node = schema_node;
	properties_node = json_object_get_member (root, "properties");
	pattern_properties_node = json_object_get_member (root, "patternProperties");
	additional_properties_node = json_object_get_member (root, "additionalProperties");
	dependencies_node = json_object_get_member (root, "dependencies");

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
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
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = json_object_get_member (root, "required");
	min_properties_node = schema_node;
	max_properties_node = json_object_get_member (root, "maxProperties");
	properties_node = json_object_get_member (root, "properties");
	pattern_properties_node = json_object_get_member (root, "patternProperties");
	additional_properties_node = json_object_get_member (root, "additionalProperties");
	dependencies_node = json_object_get_member (root, "dependencies");

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
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
	JsonObjectIter instance_member_names_iter;
	const gchar *member_name;
	guint i;

	/* Quick type check. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	schema_array = json_node_get_array (schema_node);
	instance_object = json_node_get_object (instance_node);
	json_object_iter_init (&instance_member_names_iter, instance_object);

	schema_member_names = g_ptr_array_new ();
	set = g_hash_table_new (wbl_json_string_hash, wbl_json_string_equal);

	/* Put the required member names in the @schema_member_names. */
	for (i = 0; i < json_array_get_length (schema_array); i++) {
		JsonNode *child_node;  /* unowned */

		child_node = json_array_get_element (schema_array, i);
		g_ptr_array_add (schema_member_names,
		                 (gpointer) json_node_get_string (child_node));
	}

	/* Put the instance member names in the @set. */
	while (json_object_iter_next (&instance_member_names_iter,
	                               &member_name, NULL)) {
		g_hash_table_add (set, (gpointer) member_name);
	}

	/* Check each of the @set against the @schema_member_names. */
	for (i = 0; i < schema_member_names->len; i++) {
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
}

static void
generate_required (WblSchema *self,
                   JsonObject *root,
                   JsonNode *schema_node,
                   GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = schema_node;
	min_properties_node = json_object_get_member (root, "minProperties");
	max_properties_node = json_object_get_member (root, "maxProperties");
	properties_node = json_object_get_member (root, "properties");
	pattern_properties_node = json_object_get_member (root, "patternProperties");
	additional_properties_node = json_object_get_member (root, "additionalProperties");
	dependencies_node = json_object_get_member (root, "dependencies");

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
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
	JsonObjectIter iter;
	JsonNode *child_node;  /* unowned */

	if (!JSON_NODE_HOLDS_OBJECT (schema_node)) {
		/* Invalid. */
		g_set_error (error,
		             WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_MALFORMED,
		             _("properties must be an object of valid JSON "
		               "Schemas. See json-schema-validation§5.4.4."));

		return;
	}

	schema_object = json_node_get_object (schema_node);
	json_object_iter_init (&iter, schema_object);

	while (json_object_iter_next (&iter, NULL, &child_node)) {
		GError *child_error = NULL;

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
}

static void
validate_pattern_properties (WblSchema *self,
                             JsonObject *root,
                             JsonNode *schema_node,
                             GError **error)
{
	JsonObject *schema_object;  /* unowned */
	JsonObjectIter iter;
	const gchar *member_name;
	JsonNode *child_node;  /* unowned */

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
	json_object_iter_init (&iter, schema_object);

	while (json_object_iter_next (&iter, &member_name, &child_node)) {
		GError *child_error = NULL;

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
	JsonObjectIter instance_object_iter;
	const gchar *member_name;
	JsonNode *child_node;  /* unowned */
	GList/*<unowned utf8>*/ *set_p = NULL;  /* owned */
	GList/*<unowned utf8>*/ *set_pp = NULL;  /* owned */
	GList/*<unowned utf8>*/ *l = NULL;  /* unowned */
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

	json_object_iter_init (&instance_object_iter, instance_object);

	while (json_object_iter_next (&instance_object_iter,
	                              &member_name, &child_node)) {
		GHashTableIter/*<unowned JsonNode, unowned JsonNode>*/ iter;
		JsonNode *child_schema;  /* unowned */

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

			subschema_apply (self,
			                 json_node_get_object (child_schema),
			                 child_node, &child_error);

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

/* Find the subschema of the first pattern to match @property. If no patterns
 * match, return %NULL. */
static JsonObject *
pattern_properties_find_match (JsonObject   *pattern_properties,
                               const gchar  *property)
{
	JsonObject *subschema = NULL;  /* unowned */
	JsonObjectIter iter;
	const gchar *member_name;
	JsonNode *child_node;

	json_object_iter_init (&iter, pattern_properties);

	while (subschema == NULL &&
	       json_object_iter_next (&iter, &member_name, &child_node)) {
		GRegex *regex = NULL;

		regex = g_regex_new (member_name, 0, 0, NULL);
		g_assert (regex != NULL);

		if (g_regex_match (regex, property, 0, NULL)) {
			subschema = json_node_get_object (child_node);
		}

		g_regex_unref (regex);
	}

	return subschema;
}

/**
 * generate_n_additional_properties:
 * @num_additional_properties: minimum number of additional properties to
 *    generate
 * @known_properties: set of existing property names which must not be in the
 *    output set
 * @pattern_properties: set of existing property name patterns which must not
 *    match any members of the output set
 *
 * Generate at least @num_additional_properties which aren’t in
 * @known_properties and which don’t match any of @pattern_properties’
 * member patterns.
 *
 * It is possible that fewer than @num_additional_properties will be generated
 * if the patterns in @pattern_properties are sufficiently general. For example,
 * if @pattern_properties contains `.*`, the return value will be an empty set.
 *
 * Formally, this function returns:
 *    { a | a ∉ @known_properties ∧ ∀ p ∈ @pattern_properties. ¬match(a, p) }
 * such that (if possible):
 *    |a| ≥ @num_additional_properties
 *
 * Returns: (transfer floating): a set of newly generated property names
 *
 * Since: UNRELEASED
 */
static WblStringSet *
generate_n_additional_properties (gint64         num_additional_properties,
                                  WblStringSet  *known_properties,
                                  JsonObject    *pattern_properties)
{
	WblStringSet *output = NULL;
	gint64 i;

	output = wbl_string_set_new_empty ();

	/* FIXME: Reverse-engineer the regexes. */
	for (i = 0; num_additional_properties > 0; i++) {
		gchar *new_property = NULL;

		new_property = g_strdup_printf ("%" G_GINT64_FORMAT, i);

		if (!wbl_string_set_contains (known_properties, new_property) &&
		    pattern_properties_find_match (pattern_properties,
		                                   new_property) == NULL) {
			output = wbl_string_set_union (output,
			                               wbl_string_set_new_singleton (new_property));
			num_additional_properties--;
		}

		g_free (new_property);
	}

	return output;
}

/**
 * pattern_properties_generate_instances:
 * @pattern_properties: set of existing property name patterns to generate
 *    instances of
 * @properties: set of existing property names which must not be in the
 *    output set
 *
 * Generate at least one instance string for each regex pattern in
 * @pattern_properties, ensuring that none of the generated strings are members
 * of @properties.
 *
 * Note that @properties is not the `knownProperties` formal variable; it is
 * the set of property names from the `properties` schema keyword.
 *
 * Returns: (transfer floating): a set of newly generated property names
 *
 * Since: UNRELEASED
 */
static WblStringSet *
pattern_properties_generate_instances (JsonObject    *pattern_properties,
                                       WblStringSet  *properties)
{
	WblStringSet *output = NULL;
	JsonObjectIter iter;
	const gchar *member_name;

	output = wbl_string_set_new_empty ();
	json_object_iter_init (&iter, pattern_properties);

	while (json_object_iter_next (&iter, &member_name, NULL)) {
		GRegex *regex = NULL;
		guint i;
		const gchar *candidate_properties[] = {
			"a",
			"A",
			"0",
			"aaa",
			"000",
			"!",
			NULL
		};

		/* FIXME: This is a horrendous hack and should instead be
		 * handled by exploring the regex’s FSM. */
		regex = g_regex_new (member_name, 0, 0, NULL);
		g_assert (regex != NULL);

		for (i = 0; i < G_N_ELEMENTS (candidate_properties); i++) {
			if (g_regex_match (regex, candidate_properties[i], 0,
			                   NULL) &&
			    !wbl_string_set_contains (properties,
			                              candidate_properties[i])) {
				break;
			}
		}

		if (i >= G_N_ELEMENTS (candidate_properties)) {
			g_critical ("%s: Could not generate an instance "
			            "matching regex ‘%s’. Fix this code.",
			            G_STRLOC, member_name);
			g_assert_not_reached ();
		}

		output = wbl_string_set_union (output,
		                               wbl_string_set_new_singleton (candidate_properties[i]));

		g_regex_unref (regex);
	}

	return output;
}

/**
 * generate_valid_property_sets:
 * @required: set of required property names
 * @min_properties: minimum number of properties required (inclusive)
 * @max_properties: maximum number of properties allowed (inclusive)
 * @properties: object mapping known property names to subschemas
 * @pattern_properties: object mapping known property name patterns to
 *    subschemas
 * @additional_properties_allowed: %TRUE if properties not matching @properties
 *    or @pattern_properties are allowed, %FALSE otherwise
 * @dependencies: object mapping property names to arrays of property names
 *    they depend on
 *
 * Generate a family of valid property sets which satisfy all the schema
 * keywords taken as input. This family is not necessarily complete; it is
 * designed to generate interesting sets of property names which (when coupled
 * with appropriate instances for those properties) should explore a large
 * proportion of the code paths in a parser for the JSON format described by
 * this schema.
 *
 * See the inline comments for a formal specification.
 *
 * Returns: (transfer full): a family of property sets, which may be empty
 *
 * Since: UNRELEASED
 */
static GHashTable/*<floating WblStringSet>*/ *
generate_valid_property_sets (WblStringSet  *required,
                              gint64         min_properties,
                              gint64         max_properties,
                              JsonObject    *properties,
                              JsonObject    *pattern_properties,
                              gboolean       additional_properties_allowed,
                              JsonObject    *dependencies,
                              gboolean       debug)
{
	WblStringSet *initial = NULL;
	WblStringSet *known_properties = NULL;
	WblStringSet *additional_properties = NULL;
	GHashTable/*<owned WblStringSet>*/ *set_family = NULL;
	GHashTable/*<floating WblStringSet>*/ *property_sets = NULL;
	WblStringSetIter iter;
	const gchar *element;
	GHashTableIter hash_iter;
	WblStringSet *property_set;

	/* Add all properties from @required and transitively satisfy
	 * dependencies.
	 *
	 * Formally:
	 *    @initial = dependency_closure(@required ∪ domain(@dependencies))
	 */
	initial = wbl_string_set_union_dependencies (required, dependencies);
	wbl_string_set_ref_sink (initial);

	/* Debug. */
	if (debug) {
		gchar *debug_str = NULL;

		debug_str = wbl_string_set_to_string (initial);
		g_debug ("%s: initial = %s", G_STRFUNC, debug_str);
		g_free (debug_str);
	}

	/* Work out the known properties.
	 *
	 * Formally:
	 *    @known_properties = domain(@properties) ∪
	 *                        generate(domain(@pattern_properties)) ∪
	 *                        domain(dependencies)
	 * where
	 *    generate: regex set ↦ property name set
	 * is a function to generate matching instances of a regex.
	 */
	known_properties = wbl_string_set_new_from_object_members (properties);
	known_properties = wbl_string_set_union (known_properties,
	                                         pattern_properties_generate_instances (pattern_properties,
	                                                                                known_properties));
	known_properties = wbl_string_set_union (known_properties,
	                                         wbl_string_set_new_from_object_members (dependencies));

	wbl_string_set_ref_sink (known_properties);

	/* Debug. */
	if (debug) {
		gchar *debug_str = NULL;

		debug_str = wbl_string_set_to_string (known_properties);
		g_debug ("%s: knownProperties = %s", G_STRFUNC, debug_str);
		g_free (debug_str);
	}

	/* Work out the additional properties. */
	if (additional_properties_allowed) {
		gint64 num_additional_properties;

		num_additional_properties = MAX (1, min_properties - wbl_string_set_get_size (initial));

		if (max_properties < G_MAXINT64) {
			num_additional_properties = MAX (num_additional_properties,
			                                 max_properties - wbl_string_set_get_size (initial));
		}

		additional_properties = generate_n_additional_properties (num_additional_properties,
		                                                          known_properties,
		                                                          pattern_properties);
	} else {
		additional_properties = wbl_string_set_new_empty ();
	}

	wbl_string_set_ref_sink (additional_properties);

	/* Debug. */
	if (debug) {
		gchar *debug_str = NULL;

		debug_str = wbl_string_set_to_string (additional_properties);
		g_debug ("%s: additionalProperties = %s", G_STRFUNC, debug_str);
		g_free (debug_str);
	}

	/* Calculate results. We can’t assume that any significant proportion of
	 *  the results will be filtered out by the @minProperties and
	 * @maxProperties restrictions — meta-schema.json, for example, does not
	 * restrict in that manner at all. Hence going for total coverage by
	 * generating the power set of the known and additional properties is
	 * infeasible.
	 *
	 * The heuristic in use at the moment is to return the empty set, the
	 * set of known properties, the set of known and additional properties,
	 * and singleton sets for each member of those.
	 *
	 * Note that a later step in the process is to add the initial property
	 * set (including required properties) and calculate the transitive
	 * dependency set, so using singleton sets is fine.
	 *
	 * Formally, we calculate:
	 *    @set_family = {
	 *       ∅,
	 *       @known_properties,
	 *       @known_properties ∪ @additional_properties,
	 *    } ∪
	 *    { {k} | k ∈ @known_properties } ∪
	 *    { {a} | a ∈ @additional_properties }
	 */
	set_family = g_hash_table_new_full ((GHashFunc) wbl_string_set_hash,
	                                    (GEqualFunc) wbl_string_set_equal,
	                                    (GDestroyNotify) wbl_string_set_unref,
	                                    NULL);
	property_sets = g_hash_table_new_full ((GHashFunc) wbl_string_set_hash,
	                                       (GEqualFunc) wbl_string_set_equal,
	                                       (GDestroyNotify) wbl_string_set_unref,
	                                       NULL);

	g_hash_table_add (set_family,
	                  wbl_string_set_ref_sink (wbl_string_set_new_empty ()));
	g_hash_table_add (set_family, wbl_string_set_ref (known_properties));
	g_hash_table_add (set_family,
	                  wbl_string_set_ref_sink (wbl_string_set_union (known_properties,
	                                                                 additional_properties)));

	wbl_string_set_iter_init (&iter, known_properties);
	while (wbl_string_set_iter_next (&iter, &element)) {
		WblStringSet *singleton = NULL;

		singleton = wbl_string_set_new_singleton (element);
		g_hash_table_add (set_family, wbl_string_set_ref_sink (singleton));
	}

	wbl_string_set_iter_init (&iter, additional_properties);
	while (wbl_string_set_iter_next (&iter, &element)) {
		WblStringSet *singleton = NULL;

		singleton = wbl_string_set_new_singleton (element);
		g_hash_table_add (set_family, wbl_string_set_ref_sink (singleton));
	}

	/* Add in the initial set and calculate the transitive dependency set
	 * for each set in @set_family.
	 *
	 * Formally:
	 *    dependency_closure(s) = s ∪ dependency_closure(⋃_{m ∈ s} dependencies{m})
	 *    @property_sets = { dependency_closure(@initial ∪ s) | s ∈ @set_family }
	 */
	g_hash_table_iter_init (&hash_iter, set_family);

	while (g_hash_table_iter_next (&hash_iter, (gpointer *) &property_set, NULL)) {
		WblStringSet *candidate = NULL;
		guint candidate_size;

		candidate = wbl_string_set_union (initial,
		                                  wbl_string_set_union_dependencies (property_set,
		                                                                     dependencies));
		candidate_size = wbl_string_set_get_size (candidate);

		/* Debug output. */
		if (debug) {
			gchar *debug_str = NULL;

			debug_str = wbl_string_set_to_string (candidate);
			g_debug ("%s: Candidate: %s", G_STRFUNC, debug_str);
			g_free (debug_str);
		}

		if (min_properties <= candidate_size &&
		    candidate_size <= max_properties) {
			g_hash_table_add (property_sets,
			                  wbl_string_set_ref (candidate));
		}

		wbl_string_set_unref (candidate);
	}

	g_hash_table_unref (set_family);
	wbl_string_set_unref (additional_properties);
	wbl_string_set_unref (known_properties);
	wbl_string_set_unref (initial);

	return property_sets;
}

/**
 * get_subschemas_for_property:
 * @properties: object mapping known property names to subschemas
 * @pattern_properties: object mapping known property name patterns to
 *    subschemas
 * @additional_properties: `additionalProperties` keyword from the JSON schema,
 *    which may either be a subschema JSON object, or a JSON boolean value
 * @property: name of the property to find subschemas for
 *
 * Retrieve the set of subschemas which must apply to the given @property. These
 * come from matching entries in @properties and @pattern_properties; or from
 * @additional_properties if it defines a subschema and no other subschemas
 * match.
 *
 * Reference: json-schema-validation§8.3.3
 *
 * Returns: (transfer full) (element-type JsonObject): a collection of
 *    subschemas which must apply to @property; the collection may be empty
 *
 * Since: UNRELEASED
 */
static GPtrArray/*<owned JsonObject>*/ *
get_subschemas_for_property (JsonObject   *properties,
                             JsonObject   *pattern_properties,
                             JsonNode     *additional_properties,
                             const gchar  *property,
                             gboolean      debug)
{
	GPtrArray/*<owned JsonObject>*/ *output = NULL;
	JsonObject *subschema;
	JsonObjectIter iter;
	const gchar *member_name;
	JsonNode *child_node;

	output = g_ptr_array_new_with_free_func ((GDestroyNotify) json_object_unref);

	/* @properties is easy. */
	if (json_object_has_member (properties, property)) {
		subschema = json_object_get_object_member (properties, property);
		if (subschema != NULL) {
			g_ptr_array_add (output, json_object_ref (subschema));
		}
	}

	/* @pattern_properties needs to check for matches.
	 *
	 * Any errors in the regex should have been caught in
	 * validate_pattern(). */
	json_object_iter_init (&iter, pattern_properties);

	while (json_object_iter_next (&iter, &member_name, &child_node)) {
		GRegex *regex = NULL;

		regex = g_regex_new (member_name, 0, 0, NULL);
		g_assert (regex != NULL);

		if (g_regex_match (regex, property, 0, NULL)) {
			subschema = json_node_get_object (child_node);
			g_ptr_array_add (output, json_object_ref (subschema));
		}

		g_regex_unref (regex);
	}

	/* @additionalProperties should be a subschema if it’s non-%FALSE.
	 *
	 * Note that by json-schema-validation§8.3.3.4, this is only added if
	 * no other subschemas have been added. */
	if (output->len == 0) {
		if (JSON_NODE_HOLDS_OBJECT (additional_properties)) {
			subschema = json_node_get_object (additional_properties);
			g_ptr_array_add (output, json_object_ref (subschema));
		} else if (validate_value_type (additional_properties,
		                                G_TYPE_BOOLEAN) &&
		           json_node_get_boolean (additional_properties)) {
			/* The default value for @additionalProperties is an
			 * empty subschema. */
			g_ptr_array_add (output, json_object_new ());
		}
	}

	/* Debug output. */
	if (debug) {
		JsonGenerator *generator = NULL;
		guint i;

		generator = json_generator_new ();

		for (i = 0; i < output->len; i++) {
			JsonNode *node = NULL;
			gchar *subschema_string = NULL;

			node = json_node_new (JSON_NODE_OBJECT);
			json_node_set_object (node, output->pdata[i]);
			json_generator_set_root (generator, node);
			json_node_free (node);

			subschema_string = json_generator_to_data (generator,
			                                           NULL);
			g_debug ("%s: Subschema for ‘%s’: %s",
			         G_STRFUNC, property, subschema_string);
			g_free (subschema_string);
		}

		g_object_unref (generator);
	}

	return output;
}

/**
 * instance_drop_n_properties:
 * @instance: a JSON object instance
 * @n: number of properties to drop
 * @required: names of the properties required by the schema
 *
 * Build a copy of @instance with @n of its properties removed, if possible.
 * Properties which are not in @required will be dropped by preference; but if
 * there are fewer than @n of these, properties from @required will also be
 * dropped.
 *
 * Returns: (transfer full): a new JSON instance with fewer properties
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_drop_n_properties (JsonObject    *instance,
                            guint          n,
                            WblStringSet  *required)
{
	JsonBuilder *builder = NULL;
	guint n_properties_remaining;
	JsonObjectIter instance_iter;
	JsonNode *output = NULL;
	WblStringSetIter iter;
	const gchar *property;
	JsonNode *property_node;

	/* Build a copy of @instance with @n fewer properties; ideally taking
	 * those properties from the complement of @required. */
	builder = json_builder_new ();
	json_builder_begin_object (builder);

	n_properties_remaining = json_object_get_size (instance) - n;
	json_object_iter_init (&instance_iter, instance);

	while (n_properties_remaining > 0 &&
	       json_object_iter_next (&instance_iter, &property, &property_node)) {
		if (!wbl_string_set_contains (required, property)) {
			json_builder_set_member_name (builder, property);
			json_builder_add_value (builder, json_node_copy (property_node));

			n_properties_remaining--;
		}
	}

	/* Fill in the gaps with required properties. */
	wbl_string_set_iter_init (&iter, required);

	while (n_properties_remaining > 0 &&
	       wbl_string_set_iter_next (&iter, &property)) {
		if (json_object_has_member (instance, property)) {
			json_builder_set_member_name (builder, property);
			json_builder_add_value (builder, json_node_copy (json_object_get_member (instance, property)));

			n_properties_remaining--;
		}
	}

	g_assert_cmpuint (n_properties_remaining, ==, 0);

	json_builder_end_object (builder);
	output = json_builder_get_root (builder);

	g_object_unref (builder);

	return output;
}

/**
 * instance_drop_property:
 * @instance: a JSON object instance
 * @property: name of the property to remove
 *
 * Build a copy of @instance with the named @property removed, if possible. If
 * @property does not exist in @instance, a clone of @instance will be returned.
 *
 * Returns: (transfer full): a new JSON instance with @property removed
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_drop_property (JsonObject   *instance,
                        const gchar  *property)
{
	JsonBuilder *builder = NULL;
	JsonObjectIter iter;
	JsonNode *output = NULL;
	const gchar *member_name;
	JsonNode *child_node;

	/* Build a copy of @instance without @property. */
	builder = json_builder_new ();
	json_builder_begin_object (builder);

	json_object_iter_init (&iter, instance);

	while (json_object_iter_next (&iter, &member_name, &child_node)) {
		if (g_strcmp0 (property, member_name) != 0) {
			json_builder_set_member_name (builder, member_name);
			json_builder_add_value (builder, json_node_copy (child_node));
		}
	}

	json_builder_end_object (builder);
	output = json_builder_get_root (builder);

	g_object_unref (builder);

	return output;
}

/**
 * instance_add_n_properties:
 * @instance: a JSON object instance
 * @n: number of properties to add
 * @properties: object mapping known property names to subschemas
 * @pattern_properties: object mapping known property name patterns to
 *    subschemas
 * @additional_properties: `additionalProperties` keyword from the JSON schema,
 *    which may either be a subschema JSON object, or a JSON boolean value
 *
 * Build a copy of @instance with @n new properties added, choosing the new
 * properties to match the given constraints (@properties, @pattern_properties,
 * @additional_properties) as much as possible.
 *
 * Returns: (transfer full): a new JSON instance with @n new properties
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_add_n_properties (JsonObject  *instance,
                           guint        n,
                           JsonObject  *properties,
                           JsonObject  *pattern_properties,
                           JsonNode    *additional_properties)
{
	JsonBuilder *builder = NULL;
	JsonNode *output = NULL;
	JsonObjectIter iter;
	const gchar *property;
	JsonNode *property_node;
	guint i;

	/* Add @n properties to a copy of @instance, which match the given
	 * constraints as much as possible. */
	builder = json_builder_new ();
	json_builder_begin_object (builder);

	json_object_iter_init (&iter, instance);

	while (json_object_iter_next (&iter, &property, &property_node)) {
		json_builder_set_member_name (builder, property);
		json_builder_add_value (builder, json_node_copy (property_node));
	}

	/* FIXME: Make sure the new property matches one of the constraints,
	 * including its subschema if present. */
	for (i = 0; i < n; i++) {
		gchar *member_name = NULL;

		member_name = g_strdup_printf ("additionalProperties-test-%u", i);

		json_builder_set_member_name (builder, member_name);
		json_builder_add_null_value (builder);

		g_free (member_name);
	}

	json_builder_end_object (builder);
	output = json_builder_get_root (builder);

	g_object_unref (builder);

	return output;
}

/**
 * instance_add_non_matching_property:
 * @instance: a JSON object instance
 * @properties: object mapping known property names to subschemas
 * @pattern_properties: object mapping known property name patterns to
 *    subschemas
 * @additional_properties: `additionalProperties` keyword from the JSON schema,
 *    which may either be a subschema JSON object, or a JSON boolean value
 *
 * Build a copy of @instance with one new property added, choosing the new
 * property to not match any of the given constraints (@properties,
 * @pattern_properties, @additional_properties).
 *
 * Returns: (transfer full): a new JSON instance with one new property
 *
 * Since: UNRELEASED
 */
static JsonNode *
instance_add_non_matching_property (JsonObject  *instance,
                                    JsonObject  *properties,
                                    JsonObject  *pattern_properties,
                                    JsonNode    *additional_properties)
{
	JsonBuilder *builder = NULL;
	JsonNode *output = NULL;
	JsonObjectIter iter;
	const gchar *property;
	JsonNode *property_node;

	/* Add 1 property to a copy of @instance, which matches none of the
	 * given constraints. */
	builder = json_builder_new ();
	json_builder_begin_object (builder);

	json_object_iter_init (&iter, instance);

	while (json_object_iter_next (&iter, &property, &property_node)) {
		json_builder_set_member_name (builder, property);
		json_builder_add_value (builder, json_node_copy (property_node));
	}

	/* FIXME: Make sure the new property matches none of the constraints. */
	json_builder_set_member_name (builder, "additionalProperties-test-unique");
	json_builder_add_null_value (builder);

	json_builder_end_object (builder);
	output = json_builder_get_root (builder);

	g_object_unref (builder);

	return output;
}

/**
 * generate_all_properties:
 * @self: a #WblSchema
 * @_required: JSON array of required property names
 * @min_properties: minimum number of properties required (inclusive)
 * @max_properties: maximum number of properties allowed (inclusive)
 * @properties: JSON object mapping known property names to subschemas
 * @pattern_properties: JSON object mapping regexes for property names to
 *    subschemas
 * @additional_properties: `additionalProperties` keyword from the JSON schema,
 *    which may either be a subschema JSON object, or a JSON boolean value
 * @dependencies: JSON object mapping property names to arrays of properties
 *    they depend on
 * @output: caller-allocated hash table for output to be appended to
 *
 * Generic implementation for the generate function for all property-related
 * schema keywords (`required`, `minProperties`, `maxProperties`, `properties`,
 * `patternProperties`, `additionalProperties`, `dependencies`).
 *
 * The overall approach taken by this function is to:
 *  # Generate a family of property name sets which match the schema’s
 *    constraints.
 *  # For each property name set, generate a set of object instances which all
 *    have that set of properties, paired with values generated from their
 *    subschemas.
 *  # For each property schema keyword, mutate all the object instances in some
 *    way to explore validation of the boundary conditions for validity of that
 *    keyword.
 *  # Add all the mutated and non-mutated object instances to @output.
 *
 * Since: UNRELEASED
 */
static void
generate_all_properties (WblSchema                       *self,
                         JsonArray                       *_required,
                         gint64                           min_properties,
                         gint64                           max_properties,
                         JsonObject                      *properties,
                         JsonObject                      *pattern_properties,
                         JsonNode                        *additional_properties,
                         JsonObject                      *dependencies,
                         GHashTable/*<owned JsonNode>*/  *output)
{
	WblSchemaPrivate *priv;
	GHashTable/*<floating WblStringSet>*/ *valid_property_sets = NULL;
	gboolean additional_properties_allowed;
	GHashTable/*<owned JsonNode>*/ *instance_set = NULL;
	GHashTable/*<owned JsonNode>*/ *mutation_set = NULL;
	GHashTableIter property_sets_iter;
	WblStringSet *valid_property_set;
	JsonBuilder *builder = NULL;
	GHashTableIter instance_set_iter, mutation_set_iter;
	JsonNode *node;
	guint i;
	WblStringSet *required = NULL;
	GHashTable/*<owned utf8, GHashTable<owned JsonNode>>*/ *instance_map = NULL;

	priv = wbl_schema_get_instance_private (self);
	builder = json_builder_new ();

	/* Copy @required so we can handle it as a set. */
	required = wbl_string_set_new_from_array_elements (_required);
	wbl_string_set_ref_sink (required);

	/* Work out the type of @additional_properties. */
	additional_properties_allowed = (JSON_NODE_HOLDS_OBJECT (additional_properties) ||
	                                 json_node_get_boolean (additional_properties));

	/* Generate a set of all valid property sets, given the constraints. */
	valid_property_sets = generate_valid_property_sets (required,
	                                                    min_properties,
	                                                    max_properties,
	                                                    properties,
	                                                    pattern_properties,
	                                                    additional_properties_allowed,
	                                                    dependencies,
	                                                    priv->debug);

	/* Map of property name to a set of possible subinstances for
	 * it. The @instance_map is not unique to any @valid_property_set. */
	instance_map = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                      g_free,
	                                      (GDestroyNotify) g_hash_table_unref);

	/* Generate a set of probably-valid instances by recursing on the
	 * property subschemas. */
	instance_set = g_hash_table_new_full (wbl_json_node_hash,
	                                      wbl_json_node_equal,
	                                      (GDestroyNotify) json_node_free,
	                                      NULL);
	g_hash_table_iter_init (&property_sets_iter, valid_property_sets);

	while (g_hash_table_iter_next (&property_sets_iter,
	                               (gpointer *) &valid_property_set,
	                               NULL)) {
		WblStringSetIter string_iter;
		const gchar *property_name;
		GHashTable/*<owned JsonNode>*/ *property_instances = NULL;  /* owned */
		GHashTableIter *iters = NULL;
		guint n_iterations_remaining;

		wbl_string_set_iter_init (&string_iter, valid_property_set);

		while (wbl_string_set_iter_next (&string_iter, &property_name)) {
			/* Work out the subschemas which are applicable for this
			 * property and generate instances from each of them.
			 *
			 * If we’re already examined this @property_name from
			 * a previous @valid_property_set, re-use the
			 * instances. */
			GPtrArray/*<owned JsonObject>*/ *subschemas = NULL;

			if (g_hash_table_contains (instance_map,
			                           property_name)) {
				continue;
			}

			property_instances = g_hash_table_new_full (wbl_json_node_hash,
			                                            wbl_json_node_equal,
			                                            (GDestroyNotify) json_node_free,
			                                            NULL);

			subschemas = get_subschemas_for_property (properties,
			                                          pattern_properties,
			                                          additional_properties,
			                                          property_name,
			                                          priv->debug);

			for (i = 0; i < subschemas->len; i++) {
				subschema_generate_instances (self,
				                              subschemas->pdata[i],
				                              property_instances);
			}

			/* Debug. */
			if (priv->debug) {
				GHashTableIter debug_iter;
				JsonNode *instance;

				g_hash_table_iter_init (&debug_iter,
				                        property_instances);

				while (g_hash_table_iter_next (&debug_iter,
				                               (gpointer *) &instance,
				                               NULL)) {
					gchar *json = node_to_string (instance);
					g_debug ("%s: Subinstance for ‘%s’: %s",
					         G_STRFUNC, property_name,
					         json);
					g_free (json);
				}
			}

			/* Add to the instance map. Transfer ownership. */
			g_hash_table_insert (instance_map,
			                     g_strdup (property_name),
			                     property_instances);
			property_instances = NULL;

			g_ptr_array_unref (subschemas);
		}

		/* Convert the @instance_map function of type
		 *    property ↦ { subinstance }
		 * to a set of functions of overall type:
		 *    { property ↦ subinstance }
		 * And add them all to @output (since they’re actually each a
		 * JSON instance.
		 *
		 * In order to get full coverage, this should really be an
		 * N-fold cross-product between all the subinstance sets, where
		 * N is wbl_string_set_get_size(valid_property_set). However,
		 * that would be massive.
		 *
		 * In order to get reasonable coverage, we can assume that the
		 * values of the different subinstances are probably
		 * independent. Hence we just need to generate enough instances
		 * to include each of the subinstances at least once.
		 *
		 * So, build one instance with its properties set to the first
		 * subinstance from each subinstance set; the next instance with
		 * its properties set to the second subinstances, etc. If a
		 * subinstance set is too small, keep re-using arbitrary
		 * subinstances from it.
		 */
		iters = g_new0 (GHashTableIter,
		                wbl_string_set_get_size (valid_property_set));
		n_iterations_remaining = 0;

		for (i = 0, wbl_string_set_iter_init (&string_iter,
		                                      valid_property_set);
		     wbl_string_set_iter_next (&string_iter, &property_name);
		     i++) {
			property_instances = g_hash_table_lookup (instance_map,
			                                          property_name);
			g_assert (property_instances != NULL);

			g_hash_table_iter_init (&iters[i], property_instances);
			n_iterations_remaining = MAX (n_iterations_remaining,
			                              g_hash_table_size (property_instances));
		}

		g_debug ("%s: n_iterations_remaining = %u",
		         G_STRFUNC, n_iterations_remaining);

		/* FIXME: This is an enormous hack to ensure we get at least one
		 * iteration. */
		n_iterations_remaining = MAX (n_iterations_remaining, 1);

		for (; n_iterations_remaining > 0; n_iterations_remaining--) {
			JsonNode *instance = NULL;

			json_builder_begin_object (builder);

			for (i = 0, wbl_string_set_iter_init (&string_iter,
			                                      valid_property_set);
			     wbl_string_set_iter_next (&string_iter, &property_name);
			     i++) {
				JsonNode *generated_instance;

				property_instances = g_hash_table_lookup (instance_map,
				                                          property_name);
				g_assert (property_instances != NULL);

				if (g_hash_table_size (property_instances) == 0) {
					generated_instance = NULL;
				} else if (!g_hash_table_iter_next (&iters[i],
				                                    (gpointer *) &generated_instance,
				                                    NULL)) {
					/* Arbitrarily loop round and start
					 * again. */
					g_hash_table_iter_init (&iters[i],
					                        property_instances);
					if (!g_hash_table_iter_next (&iters[i],
					                             (gpointer *) &generated_instance,
					                             NULL)) {
						generated_instance = NULL;
					}
				}

				json_builder_set_member_name (builder,
				                              property_name);

				if (generated_instance != NULL) {
					json_builder_add_value (builder,
					                        json_node_copy (generated_instance));
				} else {
					json_builder_add_null_value (builder);
				}
			}

			json_builder_end_object (builder);

			instance = json_builder_get_root (builder);
			g_hash_table_add (instance_set, instance);

			/* Debug output. */
			if (priv->debug) {
				gchar *debug_output = NULL;

				debug_output = node_to_string (instance);
				g_debug ("%s: Instance: %s", G_STRFUNC, debug_output);
				g_free (debug_output);
			}

			json_builder_reset (builder);
		}

		g_free (iters);
	}

	g_hash_table_unref (instance_map);

	/* The final step is to take each of the generated instances, and
	 * mutate it for each of the relevant schema properties. */
	mutation_set = g_hash_table_new_full (wbl_json_node_hash,
	                                      wbl_json_node_equal,
	                                      (GDestroyNotify) json_node_free,
	                                      NULL);

	/* Mutate on minProperties, maxProperties, additionalProperties,
	 * patternProperties and properties. */
	g_hash_table_iter_init (&instance_set_iter, instance_set);

	while (g_hash_table_iter_next (&instance_set_iter,
	                               (gpointer *) &node, NULL)) {
		JsonNode *mutated_instance = NULL;
		JsonObject *obj;
		JsonObjectIter dependencies_iter;
		const gchar *dependency_key;
		JsonNode *dependency_node;

		obj = json_node_get_object (node);

		/* minProperties. */
		if (min_properties > 0) {
			g_assert (json_object_get_size (obj) >= min_properties);

			mutated_instance = instance_drop_n_properties (obj,
			                                               json_object_get_size (obj) - min_properties + 1,
			                                               required);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* maxProperties. */
		if (max_properties < G_MAXINT64) {
			g_assert (json_object_get_size (obj) <= max_properties);

			mutated_instance = instance_add_n_properties (obj,
			                                              max_properties - json_object_get_size (obj) + 1,
			                                              properties,
			                                              pattern_properties,
			                                              additional_properties);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* properties, patternProperties and additionalProperties. */
		if (json_object_get_size (properties) > 0 ||
		    json_object_get_size (pattern_properties) > 0 ||
		    (validate_value_type (additional_properties, G_TYPE_BOOLEAN) &&
		     !json_node_get_boolean (additional_properties))) {
			mutated_instance = instance_add_non_matching_property (obj,
			                                                       properties,
			                                                       pattern_properties,
			                                                       additional_properties);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* required. */
		for (i = 0; i < json_array_get_length (_required); i++) {
			const gchar *required_property;

			required_property = json_array_get_string_element (_required, i);

			mutated_instance = instance_drop_property (obj,
			                                           required_property);
			g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
		}

		/* dependencies. */
		json_object_iter_init (&dependencies_iter, dependencies);

		while (json_object_iter_next (&dependencies_iter,
		                              &dependency_key,
		                              &dependency_node)) {
			JsonArray *dependency_array;

			/* We only care about property dependencies. */
			if (!JSON_NODE_HOLDS_ARRAY (dependency_node)) {
				continue;
			}

			dependency_array = json_node_get_array (dependency_node);

			for (i = 0;
			     i < json_array_get_length (dependency_array);
			     i++) {
				const gchar *required_property;

				required_property = json_array_get_string_element (dependency_array, i);

				mutated_instance = instance_drop_property (obj,
				                                           required_property);
				g_hash_table_add (mutation_set, mutated_instance);  /* transfer */
			}
		}
	}

	/* Throw the output over the fence. */
	g_hash_table_iter_init (&instance_set_iter, instance_set);

	while (g_hash_table_iter_next (&instance_set_iter,
	                               (gpointer *) &node, NULL)) {
		generate_take_node (output, json_node_copy (node));
	}

	g_hash_table_iter_init (&mutation_set_iter, mutation_set);

	while (g_hash_table_iter_next (&mutation_set_iter,
	                               (gpointer *) &node, NULL)) {
		generate_take_node (output, json_node_copy (node));
	}

	g_hash_table_unref (mutation_set);
	g_hash_table_unref (instance_set);
	g_hash_table_unref (valid_property_sets);
	wbl_string_set_unref (required);
	g_object_unref (builder);
}

static void
generate_properties (WblSchema *self,
                     JsonObject *root,
                     JsonNode *schema_node,
                     GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = json_object_get_member (root, "required");
	min_properties_node = json_object_get_member (root, "minProperties");
	max_properties_node = json_object_get_member (root, "maxProperties");
	properties_node = schema_node;
	pattern_properties_node = json_object_get_member (root, "patternProperties");
	additional_properties_node = json_object_get_member (root, "additionalProperties");
	dependencies_node = json_object_get_member (root, "dependencies");

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
}

static void
generate_additional_properties (WblSchema *self,
                                JsonObject *root,
                                JsonNode *schema_node,
                                GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = json_object_get_member (root, "required");
	min_properties_node = json_object_get_member (root, "minProperties");
	max_properties_node = json_object_get_member (root, "maxProperties");
	properties_node = json_object_get_member (root, "properties");
	pattern_properties_node = json_object_get_member (root, "patternProperties");
	additional_properties_node = schema_node;
	dependencies_node = json_object_get_member (root, "dependencies");

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
}

static void
generate_pattern_properties (WblSchema *self,
                             JsonObject *root,
                             JsonNode *schema_node,
                             GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = json_object_get_member (root, "required");
	min_properties_node = json_object_get_member (root, "minProperties");
	max_properties_node = json_object_get_member (root, "maxProperties");
	properties_node = json_object_get_member (root, "properties");
	pattern_properties_node = schema_node;
	additional_properties_node = json_object_get_member (root, "additionalProperties");
	dependencies_node = json_object_get_member (root, "dependencies");

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
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
	JsonObjectIter iter;
	const gchar *member_name;
	JsonNode *child_node;  /* unowned */

	/* Check type. */
	if (!JSON_NODE_HOLDS_OBJECT (instance_node)) {
		return;
	}

	instance_object = json_node_get_object (instance_node);
	schema_object = json_node_get_object (schema_node);
	json_object_iter_init (&iter, schema_object);

	while (json_object_iter_next (&iter, &member_name, &child_node)) {
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

			subschema_apply (self,
			                 json_node_get_object (child_node),
			                 instance_node, &child_error);

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
}

static void
generate_dependencies (WblSchema *self,
                       JsonObject *root,
                       JsonNode *schema_node,
                       GHashTable/*<owned JsonNode>*/ *output)
{
	JsonNode *required_node, *min_properties_node, *max_properties_node;
	JsonNode *properties_node, *pattern_properties_node;
	JsonNode *additional_properties_node, *dependencies_node;
	JsonArray *required = NULL;
	gint64 min_properties, max_properties;
	JsonObject *properties = NULL;
	JsonObject *pattern_properties = NULL;
	JsonObject *dependencies = NULL;
	JsonNode *additional_properties = NULL;

	/* Arrange default values for all the relevant schema properties. */
	required_node = json_object_get_member (root, "required");
	min_properties_node = json_object_get_member (root, "minProperties");
	max_properties_node = json_object_get_member (root, "maxProperties");
	properties_node = json_object_get_member (root, "properties");
	pattern_properties_node = json_object_get_member (root, "patternProperties");
	additional_properties_node = json_object_get_member (root, "additionalProperties");
	dependencies_node = schema_node;

	if (required_node != NULL) {
		required = json_node_dup_array (required_node);
	} else {
		required = json_array_new ();
	}

	if (min_properties_node != NULL) {
		min_properties = json_node_get_int (min_properties_node);
	} else {
		min_properties = 0;
	}

	if (max_properties_node != NULL) {
		max_properties = json_node_get_int (max_properties_node);
	} else {
		max_properties = G_MAXINT64;
	}

	if (properties_node != NULL) {
		properties = json_node_dup_object (properties_node);
	} else {
		properties = json_object_new ();
	}

	if (pattern_properties_node != NULL) {
		pattern_properties = json_node_dup_object (pattern_properties_node);
	} else {
		pattern_properties = json_object_new ();
	}

	if (additional_properties_node != NULL) {
		additional_properties = json_node_copy (additional_properties_node);
	} else {
		additional_properties = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (additional_properties,
		                       json_object_new ());
	}

	if (dependencies_node != NULL) {
		dependencies = json_node_dup_object (dependencies_node);
	} else {
		dependencies = json_object_new ();
	}

	/* TODO: ideally we would prevent this being called multiple times, but
	 * this will do for now */
	generate_all_properties (self, required, min_properties, max_properties,
	                         properties, pattern_properties,
	                         additional_properties, dependencies, output);

	json_object_unref (dependencies);
	json_node_free (additional_properties);
	json_object_unref (pattern_properties);
	json_object_unref (properties);
	json_array_unref (required);
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
	set = g_hash_table_new (wbl_json_node_hash, wbl_json_node_equal);

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

		if (wbl_json_node_equal (instance_node, child_node)) {
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
	    wbl_primitive_type_validate (json_node_get_string (schema_node))) {
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
		    !wbl_primitive_type_validate (json_node_get_string (child_node))) {
			goto invalid;
		}

		child_str = json_node_get_string (child_node);
		child_type = wbl_primitive_type_from_string (child_str);

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
		schema_node_type = wbl_primitive_type_from_string (schema_str);

		g_array_append_val (schema_types, schema_node_type);
	} else {
		JsonArray *schema_array;  /* unowned */

		schema_array = json_node_get_array (schema_node);

		for (i = 0; i < json_array_get_length (schema_array); i++) {
			JsonNode *child_node;  /* unowned */

			child_node = json_array_get_element (schema_array, i);

			schema_str = json_node_get_string (child_node);
			schema_node_type = wbl_primitive_type_from_string (schema_str);

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
	instance_node_type = wbl_primitive_type_from_json_node (instance_node);

	for (i = 0; i < schema_types->len; i++) {
		WblPrimitiveType schema_node_type;

		schema_node_type = g_array_index (schema_types,
		                                  WblPrimitiveType, i);

		if (wbl_primitive_type_is_a (instance_node_type,
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

static void
generate_all_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GHashTable/*<owned JsonNode>*/ *output)
{
	JsonArray *schema_array;  /* unowned */

	schema_array = json_node_get_array (schema_node);

	generate_schema_array (self, schema_array, output);
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

static void
generate_any_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GHashTable/*<owned JsonNode>*/ *output)
{
	JsonArray *schema_array;  /* unowned */

	schema_array = json_node_get_array (schema_node);

	generate_schema_array (self, schema_array, output);
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

static void
generate_one_of (WblSchema *self,
                 JsonObject *root,
                 JsonNode *schema_node,
                 GHashTable/*<owned JsonNode>*/ *output)
{
	JsonArray *schema_array;  /* unowned */

	schema_array = json_node_get_array (schema_node);

	generate_schema_array (self, schema_array, output);
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

	subschema_apply (self, json_node_get_object (schema_node),
	                 instance_node, &child_error);

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
	subschema_generate_instances (self, json_node_get_object (schema_node),
	                              output);
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
 * in json-schema-core§3.2. Default keywords are described in
 * json-schema-validation§4.3. */
typedef struct {
	const gchar *name;
	const gchar *default_value;  /* NULL if there is no default */
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
	{ "multipleOf", NULL, validate_multiple_of, apply_multiple_of, generate_multiple_of },
	/* json-schema-validation§5.1.2 */
	{ "maximum", NULL, validate_maximum, apply_maximum, generate_maximum },
	{ "exclusiveMaximum", NULL, validate_exclusive_maximum, NULL, NULL },
	/* json-schema-validation§5.1.3. */
	{ "minimum", NULL, validate_minimum, apply_minimum, generate_minimum },
	{ "exclusiveMinimum", NULL, validate_exclusive_minimum, NULL, NULL },
	/* json-schema-validation§5.2.1 */
	{ "maxLength", NULL, validate_max_length, apply_max_length, generate_max_length },
	/* json-schema-validation§5.2.2 */
	{ "minLength", "0", validate_min_length, apply_min_length, generate_min_length },
	/* json-schema-validation§5.2.3 */
	{ "pattern", NULL, validate_pattern, apply_pattern, generate_pattern },
	/* json-schema-validation§5.3.1 */
	{ "additionalItems", "{}", validate_additional_items, NULL, generate_additional_items },
	{ "items", "{}", validate_items, apply_items, generate_items },
	/* json-schema-validation§5.3.2 */
	{ "maxItems", NULL, validate_max_items, apply_max_items, generate_max_items },
	/* json-schema-validation§5.3.3 */
	{ "minItems", "0", validate_min_items, apply_min_items, generate_min_items },
	/* json-schema-validation§5.3.3 */
	{ "uniqueItems", "false", validate_unique_items, apply_unique_items, generate_unique_items },
	/* json-schema-validation§5.4.1 */
	{ "maxProperties", NULL, validate_max_properties, apply_max_properties, generate_max_properties },
	/* json-schema-validation§5.4.2 */
	{ "minProperties", "0", validate_min_properties, apply_min_properties, generate_min_properties },
	/* json-schema-validation§5.4.3 */
	{ "required", NULL, validate_required, apply_required, generate_required },
	/* json-schema-validation§5.4.4 */
	{ "additionalProperties", "{}", validate_additional_properties, NULL, generate_additional_properties },
	{ "properties", "{}", validate_properties, apply_properties, generate_properties },
	{ "patternProperties", "{}", validate_pattern_properties, apply_pattern_properties, generate_pattern_properties },
	/* json-schema-validation§5.4.5 */
	{ "dependencies", NULL, validate_dependencies, apply_dependencies, generate_dependencies },
	/* json-schema-validation§5.5.1 */
	{ "enum", NULL, validate_enum, apply_enum, generate_enum },
	/* json-schema-validation§5.5.2 */
	{ "type", NULL, validate_type, apply_type, generate_type },
	/* json-schema-validation§5.5.3 */
	{ "allOf", NULL, validate_all_of, apply_all_of, generate_all_of },
	/* json-schema-validation§5.5.4 */
	{ "anyOf", NULL, validate_any_of, apply_any_of, generate_any_of },
	/* json-schema-validation§5.5.5 */
	{ "oneOf", NULL, validate_one_of, apply_one_of, generate_one_of },
	/* json-schema-validation§5.5.6 */
	{ "not", NULL, validate_not, apply_not, generate_not },
	/* json-schema-validation§6.1 */
	{ "title", NULL, validate_title, NULL, NULL },
	{ "description", NULL, validate_description, NULL, NULL },
	/* json-schema-validation§6.2 */
	{ "default", NULL, NULL, NULL, generate_default },

	/* TODO:
	 *  • definitions (json-schema-validation§5.5.7)
	 *  • format (json-schema-validation§7.1)
	 *  • json-schema-core
	 *  • json-schema-hypermedia
	 */
};

/**
 * parse_default_value:
 * @json_string: a JSON string to parse
 *
 * Parse the default value for a JSON schema keyword. This is an internal
 * utility method which takes advantage of the limited set of default values and
 * uses direct string comparison to instantiate a #JsonNode for the
 * @json_string, rather than setting up a full JSON parser.
 *
 * If new default values are added to @json_schema_keywords, this method must
 * be updated to handle them.
 *
 * Returns: (transfer full): a new #JsonNode representing the parsed string
 *
 * Since: UNRELEASED
 */
static JsonNode *
parse_default_value (const gchar *json_string)
{
	JsonNode *output = NULL;

	g_return_val_if_fail (json_string != NULL, NULL);

	if (g_strcmp0 (json_string, "{}") == 0) {
		output = json_node_new (JSON_NODE_OBJECT);
		json_node_take_object (output, json_object_new ());
	} else if (g_strcmp0 (json_string, "false") == 0) {
		output = json_node_new (JSON_NODE_VALUE);
		json_node_set_boolean (output, FALSE);
	} else if (g_strcmp0 (json_string, "0") == 0) {
		output = json_node_new (JSON_NODE_VALUE);
		json_node_set_int (output, 0);
	} else {
		g_assert_not_reached ();
	}

	return output;
}

static void
real_validate_schema (WblSchema *self,
                      WblSchemaNode *schema,
                      GError **error)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (json_schema_keywords); i++) {
		const KeywordData *keyword = &json_schema_keywords[i];
		JsonNode *schema_node, *default_schema_node = NULL;
		GError *child_error = NULL;

		schema_node = json_object_get_member (schema->node,
		                                      keyword->name);

		/* Default. */
		if (schema_node == NULL && keyword->default_value != NULL) {
			default_schema_node = parse_default_value (keyword->default_value);
			schema_node = default_schema_node;
		}

		if (schema_node != NULL && keyword->validate != NULL) {
			keyword->validate (self, schema->node,
			                   schema_node, &child_error);
		}

		g_clear_pointer (&default_schema_node, json_node_free);

		if (child_error != NULL) {
			g_propagate_error (error, child_error);
			return;
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
		JsonNode *schema_node, *default_schema_node = NULL;
		GError *child_error = NULL;

		schema_node = json_object_get_member (schema->node,
		                                      keyword->name);

		/* Default. */
		if (schema_node == NULL && keyword->default_value != NULL) {
			default_schema_node = parse_default_value (keyword->default_value);
			schema_node = default_schema_node;
		}

		if (schema_node != NULL && keyword->apply != NULL) {
			keyword->apply (self, schema->node,
			                schema_node, instance, &child_error);
		}

		g_clear_pointer (&default_schema_node, json_node_free);

		if (child_error != NULL) {
			g_propagate_error (error, child_error);
			return;
		}
	}
}

static void
real_generate_instance_nodes (WblSchema *self,
                              WblSchemaNode *schema,
                              GHashTable/*<owned JsonNode>*/ *output)
{
	guint i;

	/* Generate for each keyword in turn. */
	for (i = 0; i < G_N_ELEMENTS (json_schema_keywords); i++) {
		const KeywordData *keyword = &json_schema_keywords[i];
		JsonNode *schema_node, *default_schema_node = NULL;

		schema_node = json_object_get_member (schema->node,
		                                      keyword->name);

		/* Default. */
		if (schema_node == NULL && keyword->default_value != NULL) {
			default_schema_node = parse_default_value (keyword->default_value);
			schema_node = default_schema_node;
		}

		if (schema_node != NULL && keyword->generate != NULL) {
			keyword->generate (self, schema->node,
			                   schema_node, output);
		}

		g_clear_pointer (&default_schema_node, json_node_free);
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

	node_output = g_hash_table_new_full (wbl_json_node_hash,
	                                     wbl_json_node_equal,
	                                     (GDestroyNotify) json_node_free,
	                                     NULL);
	output = g_ptr_array_new_with_free_func ((GDestroyNotify) wbl_generated_instance_free);

	/* Generate schema instances. */
	if (klass->generate_instance_nodes != NULL) {
		klass->generate_instance_nodes (self, priv->schema, node_output);
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
