include $(top_srcdir)/gtester.mk

AM_CPPFLAGS = \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/libwalbottle \
	-DTEST_FILE_DIR="\"$(top_srcdir)/libwalbottle/tests/\"" \
	-DG_LOG_DOMAIN=\"libwalbottle-tests\" \
	-DLIBWALBOTTLE_DISABLE_DEPRECATED \
	$(DISABLE_DEPRECATED) \
	$(NULL)
AM_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(NULL)

LIBS = \
	$(top_builddir)/libwalbottle/libwalbottle-@WBL_API_VERSION@.la \
	$(WARN_LDFLAGS) \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(NULL)

EXTRA_DIST = \
	schema.json \
	hyper-schema.json \
	$(NULL)

@VALGRIND_CHECK_RULES@

TESTS = \
	schema \
	$(NULL)
noinst_PROGRAMS = $(TESTS)

-include $(top_srcdir)/git.mk
