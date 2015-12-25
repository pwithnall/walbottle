include $(top_srcdir)/gtester.mk

# Helper library.
noinst_LTLIBRARIES = libwalbottle-test-utils.la

libwalbottle_test_utils_la_SOURCES = \
	utils.c \
	utils.h \
	$(NULL)
libwalbottle_test_utils_la_CPPFLAGS = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/libwalbottle \
	-DTEST_FILE_DIR="\"$(top_srcdir)/libwalbottle/tests/\"" \
	-DG_LOG_DOMAIN=\"libwalbottle-tests\" \
	-DLIBWALBOTTLE_DISABLE_DEPRECATED \
	$(DISABLE_DEPRECATED) \
	$(NULL)
libwalbottle_test_utils_la_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(NULL)
libwalbottle_test_utils_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(NULL)
libwalbottle_test_utils_la_LIBADD = \
	$(top_builddir)/libwalbottle/libwalbottle-utils.la \
	$(top_builddir)/libwalbottle/libwalbottle-@WBL_API_VERSION@.la \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(NULL)

# Test environment.
TESTS_ENVIRONMENT = \
	G_TEST_SRCDIR="$(abs_srcdir)" \
	G_TEST_BUILDDIR="$(abs_builddir)" \
	$(NULL)

# Test programs.
test_cppflags = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/libwalbottle \
	-DTEST_FILE_DIR="\"$(top_srcdir)/libwalbottle/tests/\"" \
	-DG_LOG_DOMAIN=\"libwalbottle-tests\" \
	-DLIBWALBOTTLE_DISABLE_DEPRECATED \
	$(DISABLE_DEPRECATED) \
	$(NULL)
test_cflags = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(NULL)
test_ldflags = \
	$(WARN_LDFLAGS) \
	$(NULL)
test_ldadd = \
	libwalbottle-test-utils.la \
	$(top_builddir)/libwalbottle/libwalbottle-utils.la \
	$(top_builddir)/libwalbottle/libwalbottle-@WBL_API_VERSION@.la \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(NULL)

@VALGRIND_CHECK_RULES@

# TODO: installed-tests
EXTRA_DIST += \
	schema-instance-generation-complex.json \
	schema-instance-generation-hyper-schema.json \
	schema-instance-generation-schema.json \
	schema-instance-generation-simple.json \
	json-api.schema.json \
	example2.schema.json \
	google-adexchangebuyer-v1.2.json \
	google-adexchangebuyer-v1.3.json \
	google-adexchangebuyer-v1.4.json \
	google-adexchangeseller-v1.1.json \
	google-adexchangeseller-v1.json \
	google-adexchangeseller-v2.0.json \
	google-admin-datatransfer_v1.json \
	google-admin-directory_v1.json \
	google-admin-email_migration_v2.json \
	google-admin-reports_v1.json \
	google-adsensehost-v4.1.json \
	google-adsense-v1.2.json \
	google-adsense-v1.3.json \
	google-adsense-v1.4.json \
	google-analytics-v2.4.json \
	google-analytics-v3.json \
	google-androidenterprise-v1.json \
	google-androidpublisher-v1.1.json \
	google-androidpublisher-v1.json \
	google-androidpublisher-v2.json \
	google-appengine-v1beta4.json \
	google-appsactivity-v1.json \
	google-appstate-v1.json \
	google-autoscaler-v1beta2.json \
	google-bigquery-v2.json \
	google-blogger-v2.json \
	google-blogger-v3.json \
	google-books-v1.json \
	google-calendar-v3.json \
	google-civicinfo-v2.json \
	google-classroom-v1.json \
	google-cloudbilling-v1.json \
	google-clouddebugger-v2.json \
	google-cloudlatencytest-v2.json \
	google-cloudmonitoring-v2beta2.json \
	google-cloudresourcemanager-v1beta1.json \
	google-cloudtrace-v1.json \
	google-clouduseraccounts-alpha.json \
	google-clouduseraccounts-beta.json \
	google-clouduseraccounts-vm_alpha.json \
	google-clouduseraccounts-vm_beta.json \
	google-compute-v1.json \
	google-container-v1.json \
	google-content-v2sandbox.json \
	google-content-v2.json \
	google-coordinate-v1.json \
	google-customsearch-v1.json \
	google-dataflow-v1b3.json \
	google-dataproc-v1alpha1.json \
	google-dataproc-v1beta1.json \
	google-dataproc-v1.json \
	google-datastore-v1beta1.json \
	google-datastore-v1beta2.json \
	google-deploymentmanager-v2beta1.json \
	google-deploymentmanager-v2beta2.json \
	google-deploymentmanager-v2.json \
	google-dfareporting-v1.1.json \
	google-dfareporting-v1.2.json \
	google-dfareporting-v1.3.json \
	google-dfareporting-v1.json \
	google-dfareporting-v2.0.json \
	google-dfareporting-v2.1.json \
	google-dfareporting-v2.2.json \
	google-dfareporting-v2.3.json \
	google-discovery-v1.json \
	google-dns-v1.json \
	google-doubleclickbidmanager-v1.json \
	google-doubleclicksearch-v2.json \
	google-drive-v1.json \
	google-drive-v2.json \
	google-drive-v3.json \
	google-fitness-v1.json \
	google-freebase-v1.json \
	google-fusiontables-v1.json \
	google-fusiontables-v2.json \
	google-gamesConfiguration-v1configuration.json \
	google-gamesManagement-v1management.json \
	google-games-v1.json \
	google-gan-v1beta1.json \
	google-genomics-v1beta2.json \
	google-genomics-v1.json \
	google-gmail-v1.json \
	google-groupsmigration-v1.json \
	google-groupssettings-v1.json \
	google-identitytoolkit-v3.json \
	google-kgsearch-v1.json \
	google-licensing-v1.json \
	google-logging-v2beta1.json \
	google-manager-v1beta2.json \
	google-mapsengine-exp2.json \
	google-mapsengine-v1.json \
	google-mirror-v1.json \
	google-oauth2-v1.json \
	google-oauth2-v2.json \
	google-pagespeedonline-v1.json \
	google-pagespeedonline-v2.json \
	google-partners-v2.json \
	google-playmoviespartner-v1.json \
	google-plusDomains-v1.json \
	google-plus-v1.json \
	google-prediction-v1.2.json \
	google-prediction-v1.3.json \
	google-prediction-v1.4.json \
	google-prediction-v1.5.json \
	google-prediction-v1.6.json \
	google-proximitybeacon-v1beta1.json \
	google-pubsub-v1beta1a.json \
	google-pubsub-v1beta2.json \
	google-pubsub-v1.json \
	google-qpxExpress-v1.json \
	google-replicapoolupdater-v1beta1.json \
	google-replicapool-v1beta1.json \
	google-replicapool-v1beta2.json \
	google-reseller-v1sandbox.json \
	google-reseller-v1.json \
	google-resourceviews-v1beta1.json \
	google-resourceviews-v1beta2.json \
	google-script-v1.json \
	google-siteVerification-v1.json \
	google-spectrum-v1explorer.json \
	google-sqladmin-v1beta3.json \
	google-sqladmin-v1beta4.json \
	google-storagetransfer-v1.json \
	google-storage-v1beta1.json \
	google-storage-v1beta2.json \
	google-storage-v1.json \
	google-tagmanager-v1.json \
	google-taskqueue-v1beta1.json \
	google-taskqueue-v1beta2.json \
	google-tasks-v1.json \
	google-translate-v2.json \
	google-urlshortener-v1.json \
	google-webfonts-v1.json \
	google-webmasters-v3.json \
	google-youtubeAnalytics-v1beta1.json \
	google-youtubeAnalytics-v1.json \
	google-youtubereporting-v1.json \
	google-youtube-v3.json \
	$(NULL)

TESTS = \
	schema \
	schema-keywords \
	self-hosting \
	string-set \
	$(NULL)
noinst_PROGRAMS = $(TESTS)

schema_CPPFLAGS = $(test_cppflags)
schema_CFLAGS = $(test_cflags)
schema_LDFLAGS = $(test_ldflags)
schema_LDADD = $(test_ldadd)

schema_keywords_CPPFLAGS = $(test_cppflags)
schema_keywords_CFLAGS = $(test_cflags)
schema_keywords_LDFLAGS = $(test_ldflags)
schema_keywords_LDADD = $(test_ldadd)

self_hosting_CPPFLAGS = $(test_cppflags)
self_hosting_CFLAGS = $(test_cflags)
self_hosting_LDFLAGS = $(test_ldflags)
self_hosting_LDADD = $(test_ldadd)

string_set_CPPFLAGS = $(test_cppflags)
string_set_CFLAGS = $(test_cflags)
string_set_LDFLAGS = $(test_ldflags)
string_set_LDADD = $(test_ldadd)

-include $(top_srcdir)/git.mk
