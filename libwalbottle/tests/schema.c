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

#include <glib.h>
#include <locale.h>
#include <string.h>

#include "utils.h"
#include "wbl-schema.h"
#include "wbl-meta-schema.h"

/* Test that construction and finalisation of a WblSchema works. */
static void
test_schema_construction (void)
{
	WblSchema *schema = NULL;  /* owned */

	schema = wbl_schema_new ();
	g_assert (wbl_schema_get_root (schema) == NULL);
	g_object_unref (schema);
}

/* Test simple parsing of a JSON Schema.
 * Example taken from http://json-schema.org/examples.html. */
static void
test_schema_parsing_simple (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"title\": \"Example Schema\","
			"\"type\": \"object\","
			"\"properties\": {"
				"\"firstName\": {"
					"\"type\": \"string\""
				"},"
				"\"lastName\": {"
					"\"type\": \"string\""
				"},"
				"\"age\": {"
					"\"description\": \"Age in years\","
					"\"type\": \"integer\","
					"\"minimum\": 0"
				"}"
			"},"
			"\"required\": [\"firstName\", \"lastName\"]"
		"}", -1, &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test complex parsing of a JSON Schema.
 * Example taken from http://json-schema.org/example1.html. */
static void
test_schema_parsing_complex (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"$schema\": \"http://json-schema.org/draft-04/schema#\","
			"\"title\": \"Product set\","
			"\"type\": \"array\","
			"\"items\": {"
				"\"title\": \"Product\","
				"\"type\": \"object\","
				"\"properties\": {"
					"\"id\": {"
						"\"description\": \"The unique identifier for a product\","
						"\"type\": \"number\""
					"},"
					"\"name\": {"
						"\"type\": \"string\""
					"},"
					"\"price\": {"
						"\"type\": \"number\","
						"\"minimum\": 0,"
						"\"exclusiveMinimum\": true"
					"},"
					"\"tags\": {"
						"\"type\": \"array\","
						"\"items\": {"
							"\"type\": \"string\""
						"},"
						"\"minItems\": 1,"
						"\"uniqueItems\": true"
					"},"
					"\"dimensions\": {"
						"\"type\": \"object\","
						"\"properties\": {"
							"\"length\": {\"type\": \"number\"},"
							"\"width\": {\"type\": \"number\"},"
							"\"height\": {\"type\": \"number\"}"
						"},"
						"\"required\": [\"length\", \"width\", \"height\"]"
					"},"
					"\"warehouseLocation\": {"
						"\"description\": \"Coordinates of the warehouse with the product\","
						"\"$ref\": \"http://json-schema.org/geo\""
					"}"
				"},"
				"\"required\": [\"id\", \"name\", \"price\"]"
			"}"
		"}", -1, &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test parsing the JSON Schema meta-schema.
 * http://json-schema.org/schema */
static void
test_schema_parsing_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test parsing the JSON Hyper Schema meta-schema.
 * http://json-schema.org/hyper-schema */
static void
test_schema_parsing_hyper_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_HYPER_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* From the ‘discoveryRestUrl’ properties of
 *    https://www.googleapis.com/discovery/v1/apis/
 * Each one contains zero or more schemas in its ‘schemas’ property, which we
 * extract and parse.
 */
static const gchar *google_schemas[] = {
	"google-adexchangebuyer-v1.2.json",
	"google-adexchangebuyer-v1.3.json",
	"google-adexchangebuyer-v1.4.json",
	"google-adexchangeseller-v1.1.json",
	"google-adexchangeseller-v1.json",
	"google-adexchangeseller-v2.0.json",
	"google-admin-datatransfer_v1.json",
	"google-admin-directory_v1.json",
	"google-admin-email_migration_v2.json",
	"google-admin-reports_v1.json",
	"google-adsensehost-v4.1.json",
	"google-adsense-v1.2.json",
	"google-adsense-v1.3.json",
	"google-adsense-v1.4.json",
	"google-analytics-v2.4.json",
	"google-analytics-v3.json",
	"google-androidenterprise-v1.json",
	"google-androidpublisher-v1.1.json",
	"google-androidpublisher-v1.json",
	"google-androidpublisher-v2.json",
	"google-appengine-v1beta4.json",
	"google-appsactivity-v1.json",
	"google-appstate-v1.json",
	"google-autoscaler-v1beta2.json",
	"google-bigquery-v2.json",
	"google-blogger-v2.json",
	"google-blogger-v3.json",
	"google-books-v1.json",
	"google-calendar-v3.json",
	"google-civicinfo-v2.json",
	"google-classroom-v1.json",
	"google-cloudbilling-v1.json",
	"google-clouddebugger-v2.json",
	"google-cloudlatencytest-v2.json",
	"google-cloudmonitoring-v2beta2.json",
	"google-cloudresourcemanager-v1beta1.json",
	"google-cloudtrace-v1.json",
	"google-clouduseraccounts-alpha.json",
	"google-clouduseraccounts-beta.json",
	"google-clouduseraccounts-vm_alpha.json",
	"google-clouduseraccounts-vm_beta.json",
	"google-compute-v1.json",
	"google-container-v1.json",
	"google-content-v2sandbox.json",
	"google-content-v2.json",
	"google-coordinate-v1.json",
	"google-customsearch-v1.json",
	"google-dataflow-v1b3.json",
	"google-dataproc-v1alpha1.json",
	"google-dataproc-v1beta1.json",
	"google-dataproc-v1.json",
	"google-datastore-v1beta1.json",
	"google-datastore-v1beta2.json",
	"google-deploymentmanager-v2beta1.json",
	"google-deploymentmanager-v2beta2.json",
	"google-deploymentmanager-v2.json",
	"google-dfareporting-v1.1.json",
	"google-dfareporting-v1.2.json",
	"google-dfareporting-v1.3.json",
	"google-dfareporting-v1.json",
	"google-dfareporting-v2.0.json",
	"google-dfareporting-v2.1.json",
	"google-dfareporting-v2.2.json",
	"google-dfareporting-v2.3.json",
	"google-discovery-v1.json",
	"google-dns-v1.json",
	"google-doubleclickbidmanager-v1.json",
	"google-doubleclicksearch-v2.json",
	"google-drive-v1.json",
	"google-drive-v2.json",
	"google-drive-v3.json",
	"google-fitness-v1.json",
	"google-freebase-v1.json",
	"google-fusiontables-v1.json",
	"google-fusiontables-v2.json",
	"google-gamesConfiguration-v1configuration.json",
	"google-gamesManagement-v1management.json",
	"google-games-v1.json",
	"google-gan-v1beta1.json",
	"google-genomics-v1beta2.json",
	"google-genomics-v1.json",
	"google-gmail-v1.json",
	"google-groupsmigration-v1.json",
	"google-groupssettings-v1.json",
	"google-identitytoolkit-v3.json",
	"google-kgsearch-v1.json",
	"google-licensing-v1.json",
	"google-logging-v2beta1.json",
	"google-manager-v1beta2.json",
	"google-mapsengine-exp2.json",
	"google-mapsengine-v1.json",
	"google-mirror-v1.json",
	"google-oauth2-v1.json",
	"google-oauth2-v2.json",
	"google-pagespeedonline-v1.json",
	"google-pagespeedonline-v2.json",
	"google-partners-v2.json",
	"google-playmoviespartner-v1.json",
	"google-plusDomains-v1.json",
	"google-plus-v1.json",
	"google-prediction-v1.2.json",
	"google-prediction-v1.3.json",
	"google-prediction-v1.4.json",
	"google-prediction-v1.5.json",
	"google-prediction-v1.6.json",
	"google-proximitybeacon-v1beta1.json",
	"google-pubsub-v1beta1a.json",
	"google-pubsub-v1beta2.json",
	"google-pubsub-v1.json",
	"google-qpxExpress-v1.json",
	"google-replicapoolupdater-v1beta1.json",
	"google-replicapool-v1beta1.json",
	"google-replicapool-v1beta2.json",
	"google-reseller-v1sandbox.json",
	"google-reseller-v1.json",
	"google-resourceviews-v1beta1.json",
	"google-resourceviews-v1beta2.json",
	"google-script-v1.json",
	"google-siteVerification-v1.json",
	"google-spectrum-v1explorer.json",
	"google-sqladmin-v1beta3.json",
	"google-sqladmin-v1beta4.json",
	"google-storagetransfer-v1.json",
	"google-storage-v1beta1.json",
	"google-storage-v1beta2.json",
	"google-storage-v1.json",
	"google-tagmanager-v1.json",
	"google-taskqueue-v1beta1.json",
	"google-taskqueue-v1beta2.json",
	"google-tasks-v1.json",
	"google-translate-v2.json",
	"google-urlshortener-v1.json",
	"google-webfonts-v1.json",
	"google-webmasters-v3.json",
	"google-youtubeAnalytics-v1beta1.json",
	"google-youtubeAnalytics-v1.json",
	"google-youtubereporting-v1.json",
	"google-youtube-v3.json",
};

/* Schemas from the above list which are known to be invalid. */
static const struct {
	const gchar *schema_filename;
	const gchar *schema_name;
} known_invalid_google_schemas[] = {
	/* Uses ‘"type": "any"’. */
	{ "google-adexchangebuyer-v1.3.json", "PerformanceReport" },
	{ "google-adexchangebuyer-v1.4.json", "PerformanceReport" },
	{ "google-admin-directory_v1.json", "User" },
	{ "google-admin-directory_v1.json", "UserCustomProperties" },
	{ "google-admin-reports_v1.json", "UsageReport" },
	{ "google-appengine-v1beta4.json", "Operation" },
	{ "google-appengine-v1beta4.json", "Status" },
	{ "google-bigquery-v2.json", "JsonValue" },
	{ "google-bigquery-v2.json", "TableCell" },
	{ "google-books-v1.json", "Volume" },
	{ "google-books-v1.json", "Annotationdata" },
	{ "google-customsearch-v1.json", "Result" },
	{ "google-dataflow-v1b3.json", "MetricUpdate" },
	{ "google-dataflow-v1b3.json", "Sink" },
	{ "google-dataflow-v1b3.json", "Step" },
	{ "google-dataflow-v1b3.json", "Source" },
	{ "google-dataflow-v1b3.json", "SideInputInfo" },
	{ "google-dataflow-v1b3.json", "Environment" },
	{ "google-dataflow-v1b3.json", "WorkItemServiceState" },
	{ "google-dataflow-v1b3.json", "Status" },
	{ "google-dataflow-v1b3.json", "InstructionOutput" },
	{ "google-dataflow-v1b3.json", "ParDoInstruction" },
	{ "google-dataflow-v1b3.json", "SeqMapTask" },
	{ "google-dataflow-v1b3.json", "PartialGroupByKeyInstruction" },
	{ "google-dataflow-v1b3.json", "WorkerPool" },
	{ "google-dataproc-v1alpha1.json", "Operation" },
	{ "google-dataproc-v1alpha1.json", "Status" },
	{ "google-dataproc-v1beta1.json", "Operation" },
	{ "google-dataproc-v1beta1.json", "Status" },
	{ "google-deploymentmanager-v2beta1.json", "Operation" },
	{ "google-doubleclicksearch-v2.json", "ReportRow" },
	/* Has some numbers presented as strings: */
	{ "google-doubleclicksearch-v2.json", "ReportRequest" },
	/* And we are back to ‘"type": "any"’: */
	{ "google-fusiontables-v1.json", "Geometry" },
	{ "google-fusiontables-v1.json", "Sqlresponse" },
	{ "google-fusiontables-v2.json", "Geometry" },
	{ "google-fusiontables-v2.json", "Sqlresponse" },
	{ "google-gan-v1beta1.json", "Report" },
	{ "google-genomics-v1.json", "ReadGroup" },
	{ "google-genomics-v1.json", "Operation" },
	{ "google-genomics-v1.json", "OperationMetadata" },
	{ "google-genomics-v1.json", "Variant" },
	{ "google-genomics-v1.json", "Read" },
	{ "google-genomics-v1.json", "ReadGroupSet" },
	{ "google-genomics-v1.json", "VariantCall" },
	{ "google-genomics-v1.json", "CallSet" },
	{ "google-genomics-v1.json", "Status" },
	{ "google-genomics-v1.json", "VariantSetMetadata" },
	{ "google-kgsearch-v1.json", "SearchResponse" },
	{ "google-logging-v2beta1.json", "LogEntry" },
	{ "google-mapsengine-exp2.json", "Filter" },
	{ "google-mapsengine-exp2.json", "GeoJsonProperties" },
	{ "google-mapsengine-v1.json", "Filter" },
	{ "google-mapsengine-v1.json", "GeoJsonProperties" },
	{ "google-prediction-v1.2.json", "Update" },
	{ "google-prediction-v1.2.json", "Input" },
	{ "google-prediction-v1.3.json", "Update" },
	{ "google-prediction-v1.3.json", "Input" },
	{ "google-prediction-v1.4.json", "Update" },
	{ "google-prediction-v1.4.json", "Input" },
	{ "google-prediction-v1.5.json", "Input" },
	{ "google-prediction-v1.5.json", "Training" },
	{ "google-prediction-v1.5.json", "Update" },
	{ "google-prediction-v1.6.json", "Insert" },
	{ "google-prediction-v1.6.json", "Input" },
	{ "google-prediction-v1.6.json", "Update" },
	{ "google-script-v1.json", "Operation" },
	{ "google-script-v1.json", "Status" },
	{ "google-script-v1.json", "ExecutionRequest" },
	{ "google-script-v1.json", "ExecutionResponse" },
	{ "google-storagetransfer-v1.json", "Status" },
	{ "google-storagetransfer-v1.json", "Operation" },
	{ "google-storage-v1beta2.json", "ObjectAccessControls" },
	{ "google-storage-v1.json", "ObjectAccessControls" },
	{ "google-youtubeAnalytics-v1beta1.json", "ResultTable" },
	{ "google-youtubeAnalytics-v1.json", "ResultTable" },
};

/* Test parsing various Google services schemas. */
static void
test_schema_parsing_google (gconstpointer user_data)
{
	const gchar *schema_filename = user_data;
	gchar *path = NULL;
	JsonParser *parser = NULL;
	JsonNode *root;
	JsonObject *root_object;
	JsonObject *schemas;
	JsonObjectIter iter;
	const gchar *schema_name;
	JsonNode *schema_node;
	GError *error = NULL;

	/* Load the JSON file and extract the schemas — they’re the elements
	 * beneath the ‘schemas’ property. */
	parser = json_parser_new ();
	path = g_test_build_filename (G_TEST_DIST, schema_filename, NULL);
	json_parser_load_from_file (parser, path, &error);
	g_assert_no_error (error);
	g_free (path);

	root = json_parser_get_root (parser);
	g_assert (JSON_NODE_HOLDS_OBJECT (root));
	root_object = json_node_get_object (root);

	if (!json_object_has_member (root_object, "schemas")) {
		g_object_unref (parser);
		return;
	}

	schemas = json_object_get_object_member (root_object, "schemas");

	json_object_iter_init (&iter, schemas);

	while (json_object_iter_next (&iter, &schema_name, &schema_node)) {
		WblSchema *schema = NULL;
		gboolean known_invalid = FALSE;
		guint i;
		GPtrArray/*<owned WblValidateMessage>*/ *messages;

		/* Do we expect parsing to fail? */
		for (i = 0; i < G_N_ELEMENTS (known_invalid_google_schemas); i++) {
			if (g_strcmp0 (known_invalid_google_schemas[i].schema_filename,
			               schema_filename) == 0 &&
			    g_strcmp0 (known_invalid_google_schemas[i].schema_name,
			               schema_name) == 0) {
				known_invalid = TRUE;
				break;
			}
		}

		g_test_message ("Parsing schema ‘%s’ from file ‘%s’. "
		                "Expecting parsing to %s.",
		                schema_name, schema_filename,
		                known_invalid ? "fail" : "succeed");

		schema = wbl_schema_new ();

		wbl_schema_load_from_json (schema, schema_node, NULL, &error);
		messages = wbl_schema_get_validation_messages (schema);

		if (!known_invalid) {
			wbl_print_validation_messages (messages);
			g_assert_no_error (error);
			g_assert_nonnull (wbl_schema_get_root (schema));
		} else {
			wbl_print_validation_messages (messages);
			g_assert_error (error, WBL_SCHEMA_ERROR,
			                WBL_SCHEMA_ERROR_MALFORMED);
			g_assert_null (wbl_schema_get_root (schema));
			g_assert_cmpuint (messages->len, >, 0);

			g_clear_error (&error);
		}

		g_object_unref (schema);
	}

	g_object_unref (parser);
}

static const gchar *varied_schemas[] = {
	/* From http://json-schema.org/example2.html */
	"example2.schema.json",
	/* From http://jsonapi.org/schema */
	"json-api.schema.json",
};

/* Test parsing various schemas from places around the internet. */
static void
test_schema_parsing_varied (gconstpointer user_data)
{
	const gchar *schema_filename = user_data;
	gchar *path = NULL;
	WblSchema *schema = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	path = g_test_build_filename (G_TEST_DIST, schema_filename, NULL);
	wbl_schema_load_from_file (schema, path, &error);
	g_assert_no_error (error);
	g_free (path);

	g_assert (wbl_schema_get_root (schema) != NULL);

	g_object_unref (schema);
}

/* Test applying a schema to an instance using items and additionalItems.
 * Taken from draft-fge-json-schema-validation-00§5.3.1.3. */
static void
test_schema_application (void)
{
	WblSchema *schema = NULL;  /* owned */
	JsonParser *parser = NULL;  /* owned */
	guint i;
	GError *error = NULL;

	const gchar *valid_cases[] = {
		"[]",
		"[ [ 1, 2, 3, 4 ], [ 5, 6, 7, 8 ] ]",
		"[ 1, 2, 3 ]",
	};

	const gchar *invalid_cases[] = {
		"[ 1, 2, 3, 4 ]",
		"[ null, { \"a\": \"b\" }, true, 31.000002020013 ]",
	};

	schema = wbl_schema_new ();
	parser = json_parser_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"items\": [ {}, {}, {} ],"
			"\"additionalItems\": false"
		"}", -1, &error);
	g_assert_no_error (error);

	/* Valid cases. */
	for (i = 0; i < G_N_ELEMENTS (valid_cases); i++) {
		json_parser_load_from_data (parser, valid_cases[i], -1, &error);
		g_assert_no_error (error);

		wbl_schema_apply (schema,
		                  json_parser_get_root (parser), &error);
		g_assert_no_error (error);
	}

	/* Invalid cases. */
	for (i = 0; i < G_N_ELEMENTS (invalid_cases); i++) {
		json_parser_load_from_data (parser,
		                            invalid_cases[i], -1, &error);
		g_assert_no_error (error);

		wbl_schema_apply (schema,
		                  json_parser_get_root (parser), &error);
		g_assert_error (error,
		                WBL_SCHEMA_ERROR, WBL_SCHEMA_ERROR_INVALID);
		g_clear_error (&error);
	}

	g_object_unref (parser);
	g_object_unref (schema);
}

/* Test generating instances for a simple schema. */
static void
test_schema_instance_generation_simple (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"title\": \"Example Schema\","
			"\"type\": \"object\","
			"\"properties\": {"
				"\"firstName\": {"
					"\"type\": \"string\""
				"},"
				"\"lastName\": {"
					"\"type\": \"string\""
				"},"
				"\"age\": {"
					"\"description\": \"Age in years\","
					"\"type\": \"integer\","
					"\"minimum\": 0"
				"}"
			"},"
			"\"required\": [\"firstName\", \"lastName\"]"
		"}", -1, &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	wbl_test_assert_generated_instances_match_file (instances,
	                                                "schema-instance-generation-simple.json");
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test generating instances for a complex schema. */
static void
test_schema_instance_generation_complex (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	schema = wbl_schema_new ();

	wbl_schema_load_from_data (schema,
		"{"
			"\"$schema\": \"http://json-schema.org/draft-04/schema#\","
			"\"title\": \"Product set\","
			"\"type\": \"array\","
			"\"items\": {"
				"\"title\": \"Product\","
				"\"type\": \"object\","
				"\"properties\": {"
					"\"id\": {"
						"\"description\": \"The unique identifier for a product\","
						"\"type\": \"number\""
					"},"
					"\"name\": {"
						"\"type\": \"string\""
					"},"
					"\"price\": {"
						"\"type\": \"number\","
						"\"minimum\": 0,"
						"\"exclusiveMinimum\": true"
					"},"
					"\"tags\": {"
						"\"type\": \"array\","
						"\"items\": {"
							"\"type\": \"string\""
						"},"
						"\"minItems\": 1,"
						"\"uniqueItems\": true"
					"},"
					"\"dimensions\": {"
						"\"type\": \"object\","
						"\"properties\": {"
							"\"length\": {\"type\": \"number\"},"
							"\"width\": {\"type\": \"number\"},"
							"\"height\": {\"type\": \"number\"}"
						"},"
						"\"required\": [\"length\", \"width\", \"height\"]"
					"},"
					"\"warehouseLocation\": {"
						"\"description\": \"Coordinates of the warehouse with the product\","
						"\"$ref\": \"http://json-schema.org/geo\""
					"}"
				"},"
				"\"required\": [\"id\", \"name\", \"price\"]"
			"}"
		"}", -1, &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	wbl_test_assert_generated_instances_match_file (instances,
	                                                "schema-instance-generation-complex.json");
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test generating instances for the JSON Schema meta-schema.
 * http://json-schema.org/schema */
static void
test_schema_instance_generation_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	/* FIXME: This needs a lot of mollycoddling to get it doing the right
	 * thing. */
	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	wbl_test_assert_generated_instances_match_file (instances,
	                                                "schema-instance-generation-schema.json");
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test generating instances for the JSON Hyper Schema meta-schema.
 * http://json-schema.org/hyper-schema */
static void
test_schema_instance_generation_hyper_schema (void)
{
	WblSchema *schema = NULL;  /* owned */
	GPtrArray/*<owned WblGeneratedInstace>*/ *instances = NULL;  /* owned */
	GError *error = NULL;

	/* FIXME: This needs a lot of mollycoddling to get it doing the right
	 * thing. */
	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_HYPER_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	instances = wbl_schema_generate_instances (schema,
	                                           WBL_GENERATE_INSTANCE_NONE);
	wbl_test_assert_generated_instances_match_file (instances,
	                                                "schema-instance-generation-hyper-schema.json");
	g_ptr_array_unref (instances);

	g_object_unref (schema);
}

/* Test reference counting of #WblSchemaNode. */
static void
test_schema_node_refs (void)
{
	WblSchema *schema = NULL;  /* owned */
	WblSchemaNode *node = NULL;  /* owned */
	JsonObject *json_object;  /* unowned */
	GError *error = NULL;

	schema = wbl_meta_schema_load_schema (WBL_META_SCHEMA_META_SCHEMA,
	                                      &error);
	g_assert_no_error (error);

	/* Grab the root schema node. */
	node = wbl_schema_get_root (schema);
	wbl_schema_node_ref (node);

	g_object_unref (schema);

	/* Check the node; the #WblSchemaNode should still be alive. */
	json_object = wbl_schema_node_get_root (node);
	g_assert (json_object_has_member (json_object, "properties"));

	wbl_schema_node_unref (node);
}

/* Test the title property of #WblSchemaNode. */
static void
test_schema_node_title (void)
{
	guint i;
	const struct {
		const gchar *expected_title;
		const gchar *schema_json;
	} vectors[] = {
		{ NULL, "{}" },
		{ "Title", "{ \"title\" : \"Title\" }" },
		{ "\"Title\"", "{ \"title\" : \"\\\"Title\\\"\" }" },
	};

	for (i = 0; i < G_N_ELEMENTS (vectors); i++) {
		WblSchema *schema = NULL;  /* owned */
		WblSchemaNode *node;  /* unowned */
		GError *error = NULL;

		schema = wbl_schema_new ();

		wbl_schema_load_from_data (schema,
		                           vectors[i].schema_json, -1, &error);
		g_assert_no_error (error);

		/* Grab the root schema node. */
		node = wbl_schema_get_root (schema);

		/* Check the title. */
		g_assert_cmpstr (wbl_schema_node_get_title (node), ==,
		                 vectors[i].expected_title);

		g_object_unref (schema);
	}
}

/* Test the description property of #WblSchemaNode. */
static void
test_schema_node_description (void)
{
	guint i;
	const struct {
		const gchar *expected_description;
		const gchar *schema_json;
	} vectors[] = {
		{ NULL, "{}" },
		{ "Description!", "{ \"description\" : \"Description!\" }" },
		{ "\"Description\"", "{ \"description\" : \"\\\"Description\\\"\" }" },
	};

	for (i = 0; i < G_N_ELEMENTS (vectors); i++) {
		WblSchema *schema = NULL;  /* owned */
		WblSchemaNode *node;  /* unowned */
		GError *error = NULL;

		schema = wbl_schema_new ();

		wbl_schema_load_from_data (schema,
		                           vectors[i].schema_json, -1, &error);
		g_assert_no_error (error);

		/* Grab the root schema node. */
		node = wbl_schema_get_root (schema);

		/* Check the description. */
		g_assert_cmpstr (wbl_schema_node_get_description (node), ==,
		                 vectors[i].expected_description);

		g_object_unref (schema);
	}
}

/* Test the default property of #WblSchemaNode. */
static void
test_schema_node_default (void)
{
	guint i;
	const struct {
		const gchar *expected_default_json;
		const gchar *schema_json;
	} vectors[] = {
		{ NULL, "{}" },
		{ "\"Default\"", "{ \"default\" : \"Default\" }" },
		{ "12", "{ \"default\" : 12 }" },
		{ "null", "{ \"default\": null }" },
	};

	for (i = 0; i < G_N_ELEMENTS (vectors); i++) {
		WblSchema *schema = NULL;  /* owned */
		WblSchemaNode *node;  /* unowned */
		JsonNode *default_node;  /* unowned */
		JsonGenerator *generator = NULL;  /* owned */
		gchar *json = NULL;  /* owned */
		GError *error = NULL;

		schema = wbl_schema_new ();

		wbl_schema_load_from_data (schema,
		                           vectors[i].schema_json, -1, &error);
		g_assert_no_error (error);

		/* Grab the root schema node. */
		node = wbl_schema_get_root (schema);

		/* Check the default. */
		default_node = wbl_schema_node_get_default (node);

		generator = json_generator_new ();
		json_generator_set_root (generator, default_node);
		json = json_generator_to_data (generator, NULL);
		g_assert_cmpstr (json, ==, vectors[i].expected_default_json);
		g_free (json);
		g_object_unref (generator);

		g_object_unref (schema);
	}
}

int
main (int argc, char *argv[])
{
	guint i;

#if !GLIB_CHECK_VERSION (2, 35, 0)
	g_type_init ();
#endif

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	/* Schema tests. */
	g_test_add_func ("/schema/construction", test_schema_construction);
	g_test_add_func ("/schema/parsing/simple", test_schema_parsing_simple);
	g_test_add_func ("/schema/parsing/complex", test_schema_parsing_complex);
	g_test_add_func ("/schema/parsing/schema", test_schema_parsing_schema);
	g_test_add_func ("/schema/parsing/hyper-schema",
	                 test_schema_parsing_hyper_schema);

	for (i = 0; i < G_N_ELEMENTS (google_schemas); i++) {
		gchar *test_name = NULL;

		test_name = g_strdup_printf ("/schema/parsing/google/%s",
		                             google_schemas[i]);
		g_test_add_data_func (test_name, google_schemas[i],
		                      test_schema_parsing_google);
		g_free (test_name);
	}

	for (i = 0; i < G_N_ELEMENTS (varied_schemas); i++) {
		gchar *test_name = NULL;

		test_name = g_strdup_printf ("/schema/parsing/varied/%u", i);
		g_test_add_data_func (test_name, varied_schemas[i],
		                      test_schema_parsing_varied);
		g_free (test_name);
	}

	g_test_add_func ("/schema/application", test_schema_application);
	g_test_add_func ("/schema/instance-generation/simple",
	                 test_schema_instance_generation_simple);
	g_test_add_func ("/schema/instance-generation/complex",
	                 test_schema_instance_generation_complex);
	g_test_add_func ("/schema/instance-generation/schema",
	                 test_schema_instance_generation_schema);
	g_test_add_func ("/schema/instance-generation/hyper-schema",
	                 test_schema_instance_generation_hyper_schema);
	g_test_add_func ("/schema/node/refs", test_schema_node_refs);
	g_test_add_func ("/schema/node/title", test_schema_node_title);
	g_test_add_func ("/schema/node/description",
	                 test_schema_node_description);
	g_test_add_func ("/schema/node/default", test_schema_node_default);

	return g_test_run ();
}
