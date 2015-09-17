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
