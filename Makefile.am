SUBDIRS = . libwalbottle/tests libwalbottle/docs

ACLOCAL_AMFLAGS = -I m4

DISTCHECK_CONFIGURE_FLAGS =
CLEANFILES =
MAINTAINERCLEANFILES =
EXTRA_DIST =
bin_PROGRAMS =
man8_MANS =

# libwalbottle library
lib_LTLIBRARIES = libwalbottle/libwalbottle-@WBL_API_VERSION@.la

wblincludedir = $(includedir)/libwalbottle-@WBL_API_VERSION@/walbottle
wbl_headers = \
	libwalbottle/wbl-schema.h \
	libwalbottle/wbl-meta-schema.h \
	libwalbottle/wbl-version.h \
	$(NULL)

# The following headers are private, and shouldn't be installed:
private_headers = \
	$(NULL)
wblinclude_HEADERS = \
	$(main_header) \
	$(wbl_headers) \
	$(NULL)

wbl_sources = \
	libwalbottle/wbl-schema.c \
	libwalbottle/wbl-meta-schema.c \
	$(NULL)

meta_schema_sources = \
	libwalbottle/wbl-meta-schemas.c \
	$(NULL)

main_header = libwalbottle/walbottle.h
public_headers = $(wblinclude_HEADERS)

libwalbottle_libwalbottle_@WBL_API_VERSION@_la_SOURCES = \
	$(private_headers) \
	$(wbl_sources) \
	$(meta_schema_sources) \
	$(NULL)

libwalbottle_libwalbottle_@WBL_API_VERSION@_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libwalbottle \
	-I$(top_builddir)/libwalbottle \
	-DG_LOG_DOMAIN=\"libwalbottle\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

libwalbottle_libwalbottle_@WBL_API_VERSION@_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libwalbottle_libwalbottle_@WBL_API_VERSION@_la_LIBADD = \
	$(top_builddir)/libwalbottle/libwalbottle-utils.la \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(CODE_COVERAGE_LDFLAGS) \
	$(WARN_LDFLAGS) \
	$(AM_LIBADD) \
	$(NULL)

libwalbottle_libwalbottle_@WBL_API_VERSION@_la_LDFLAGS = \
	-version-info $(WBL_LT_VERSION) \
	-export-symbols $(srcdir)/libwalbottle/libwalbottle.symbols \
	-no-undefined \
	$(AM_LDFLAGS) \
	$(NULL)

EXTRA_DIST += libwalbottle/libwalbottle.symbols

meta_schema_json = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/libwalbottle --generate-dependencies $(top_srcdir)/libwalbottle/wbl-meta-schemas.gresource.xml)
$(meta_schema_sources): libwalbottle/wbl-meta-schemas.gresource.xml $(meta_schema_json)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) \
		--c-name=wbl_meta_schemas --generate-source \
		--sourcedir=$(top_srcdir)/libwalbottle --target=$@ $<

CLEANFILES += $(meta_schema_sources)
EXTRA_DIST += \
	libwalbottle/wbl-meta-schemas.gresource.xml \
	$(meta_schema_json) \
	$(NULL)

# Check if walbottle.h includes all the public headers
check-local: check-headers
check-headers:
	@any_missing=0; for header_file in $(public_headers); do \
		if test "x$$header_file" != "x$(main_header)"; then \
			if ! grep "#include <$${header_file#lib}>" $(top_srcdir)/$(main_header) >/dev/null; then \
				echo "$(main_header) doesn't appear to include \"$$header_file\""; \
				any_missing=1; \
			fi; \
		fi; \
	done; exit "$$any_missing"

# Internal utility helper library
noinst_LTLIBRARIES = libwalbottle/libwalbottle-utils.la

libwalbottle_libwalbottle_utils_la_SOURCES = \
	libwalbottle/wbl-json-node.h \
	libwalbottle/wbl-json-node.c \
	libwalbottle/wbl-string-set.h \
	libwalbottle/wbl-string-set.c \
	$(NULL)

libwalbottle_libwalbottle_utils_la_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libwalbottle \
	-I$(top_builddir)/libwalbottle \
	-DG_LOG_DOMAIN=\"libwalbottle\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)

libwalbottle_libwalbottle_utils_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(CODE_COVERAGE_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libwalbottle_libwalbottle_utils_la_LIBADD = \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(CODE_COVERAGE_LDFLAGS) \
	$(WARN_LDFLAGS) \
	$(AM_LIBADD) \
	$(NULL)

libwalbottle_libwalbottle_utils_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(NULL)

# json-validate utility
bin_PROGRAMS += utilities/json-validate-@WBL_API_VERSION@

utilities_json_validate_@WBL_API_VERSION@_SOURCES = \
	utilities/json-validate.c \
	$(NULL)
utilities_json_validate_@WBL_API_VERSION@_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libwalbottle \
	-I$(top_builddir)/libwalbottle \
	-DG_LOG_DOMAIN=\"json-validate\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)
utilities_json_validate_@WBL_API_VERSION@_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)
utilities_json_validate_@WBL_API_VERSION@_LDADD = \
	libwalbottle/libwalbottle-@WBL_API_VERSION@.la \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(WARN_LDFLAGS) \
	$(AM_LIBADD) \
	$(NULL)

EXTRA_DIST += utilities/docs/json-validate.8
man8_MANS += $(top_builddir)/utilities/docs/json-validate-@WBL_API_VERSION@.8
CLEANFILES += utilities/docs/json-validate-@WBL_API_VERSION@.8

# json-schema-validate utility
bin_PROGRAMS += utilities/json-schema-validate-@WBL_API_VERSION@

utilities_json_schema_validate_@WBL_API_VERSION@_SOURCES = \
	utilities/json-schema-validate.c \
	$(NULL)
utilities_json_schema_validate_@WBL_API_VERSION@_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libwalbottle \
	-I$(top_builddir)/libwalbottle \
	-DG_LOG_DOMAIN=\"json-schema-validate\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)
utilities_json_schema_validate_@WBL_API_VERSION@_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)
utilities_json_schema_validate_@WBL_API_VERSION@_LDADD = \
	libwalbottle/libwalbottle-@WBL_API_VERSION@.la \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(WARN_LDFLAGS) \
	$(AM_LIBADD) \
	$(NULL)

EXTRA_DIST += utilities/docs/json-schema-validate.8
man8_MANS += $(top_builddir)/utilities/docs/json-schema-validate-@WBL_API_VERSION@.8
CLEANFILES += utilities/docs/json-schema-validate-@WBL_API_VERSION@.8

# json-schema-generate utility
bin_PROGRAMS += utilities/json-schema-generate-@WBL_API_VERSION@

utilities_json_schema_generate_@WBL_API_VERSION@_SOURCES = \
	utilities/json-schema-generate.c \
	$(NULL)
utilities_json_schema_generate_@WBL_API_VERSION@_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libwalbottle \
	-I$(top_builddir)/libwalbottle \
	-DG_LOG_DOMAIN=\"json-schema-generate\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS) \
	$(NULL)
utilities_json_schema_generate_@WBL_API_VERSION@_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)
utilities_json_schema_generate_@WBL_API_VERSION@_LDADD = \
	libwalbottle/libwalbottle-@WBL_API_VERSION@.la \
	$(GLIB_LIBS) \
	$(JSON_GLIB_LIBS) \
	$(WARN_LDFLAGS) \
	$(AM_LIBADD) \
	$(NULL)

EXTRA_DIST += utilities/docs/json-schema-generate.8
man8_MANS += $(top_builddir)/utilities/docs/json-schema-generate-@WBL_API_VERSION@.8
CLEANFILES += utilities/docs/json-schema-generate-@WBL_API_VERSION@.8

# man file versioning rule
%-@WBL_API_VERSION@.8: %.8
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && cp $^ $@

# Non-versioned symlinks for bin_PROGRAMS utilities
utilities = json-validate json-schema-validate json-schema-generate
install-exec-hook:
	for i in $(utilities); do \
		(cd $(DESTDIR)$(bindir) && \
		 $(LN_S) $$i-@WBL_API_VERSION@$(EXEEXT) $$i$(EXEEXT)); \
	done

uninstall-hook:
	for i in $(utilities); do \
		(cd $(DESTDIR)$(bindir) && \
		 rm -f $$i$(EXEEXT)); \
	done

# Macros
EXTRA_DIST += \
	m4/ax_append_compile_flags.m4 \
	m4/ax_append_flag.m4 \
	m4/ax_check_compile_flag.m4 \
	m4/ax_check_enable_debug.m4 \
	m4/ax_code_coverage.m4 \
	m4/ax_compiler_flags.m4 \
	m4/ax_pkg_check_modules.m4 \
	m4/ax_require_defined.m4 \
	m4/ax_valgrind_check.m4 \
	$(NULL)

# Introspection
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all
INTROSPECTION_COMPILER_ARGS =
DISTCHECK_CONFIGURE_FLAGS += --enable-introspection
EXTRA_DIST += m4/introspection.m4

if HAVE_INTROSPECTION
libwalbottle/Walbottle-@WBL_API_VERSION@.gir: libwalbottle/libwalbottle-@WBL_API_VERSION@.la
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_INCLUDES = GObject-2.0 Gio-2.0 Json-1.0
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(libwalbottle_libwalbottle_@WBL_API_VERSION@_la_CPPFLAGS) \
	$(NULL)
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_LIBS = libwalbottle/libwalbottle-@WBL_API_VERSION@.la
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_FILES = $(wbl_sources) $(wbl_headers)
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_NAMESPACE = Walbottle
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_EXPORT_PACKAGES = libwalbottle
libwalbottle_Walbottle_@WBL_API_VERSION@_gir_SCANNERFLAGS = \
	--nsversion=@WBL_API_VERSION@ \
	--identifier-prefix=Wbl \
	--symbol-prefix=wbl \
	--c-include="libwalbottle/walbottle.h" \
	--warn-all \
	--warn-error \
	$(NULL)

INTROSPECTION_GIRS += libwalbottle/Walbottle-@WBL_API_VERSION@.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

if ENABLE_VAPIGEN
-include $(VAPIGEN_MAKEFILE)

# Note: The VAPI file name has to match the pkg-config file name.
libwalbottle/libwalbottle-@WBL_API_VERSION@.vapi: libwalbottle/Walbottle-@WBL_API_VERSION@.gir

VAPIGEN_VAPIS = libwalbottle/libwalbottle-@WBL_API_VERSION@.vapi

libwalbottle_libwalbottle_@WBL_API_VERSION@_vapi_DEPS = json-glib-1.0
libwalbottle_libwalbottle_@WBL_API_VERSION@_vapi_METADATADIRS = $(srcdir)/libwalbottle
libwalbottle_libwalbottle_@WBL_API_VERSION@_vapi_FILES = libwalbottle/Walbottle-@WBL_API_VERSION@.gir

libwalbottle/libwalbottle-@WBL_API_VERSION@.deps:
	$(AM_V_GEN) for pkg in $(libwalbottle_libwalbottle_@WBL_API_VERSION@_vapi_DEPS); do \
		echo $$pkg >> $@; \
	done

vapidir = $(datadir)/vala/vapi
vapi_DATA = $(VAPIGEN_VAPIS) $(VAPIGEN_VAPIS:.vapi=.deps)

CLEANFILES += $(vapi_DATA)
endif
endif

# Code coverage
@CODE_COVERAGE_RULES@
CODE_COVERAGE_DIRECTORY = $(top_builddir)/libwalbottle
CODE_COVERAGE_LCOV_OPTIONS = --base-directory $(abs_top_srcdir)

# Cleaning
EXTRA_DIST += \
	autogen.sh \
	HACKING \
	gtk-doc.make \
	walbottle.doap \
	$(NULL)

CLEANFILES += \
	`find "$(srcdir)" -type f -name .dirstamp -print` \
	$(NULL)

DISTCLEANFILES = \
	gtk-doc.make \
	$(NULL)

MAINTAINERCLEANFILES += \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	build-aux/ \
	INSTALL \
	omf.make \
	gtk-doc.make \
	config.rpath \
	m4/gtk-doc.m4 \
	$(NULL)

GITIGNOREFILES = .be/id-cache

DISTCHECK_CONFIGURE_FLAGS += --enable-gtk-doc

# pkg-config data
# Note that the template file is called libwalbottle.pc.in, but generates a
# versioned .pc file using some magic in AC_CONFIG_FILES.
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libwalbottle/libwalbottle-$(WBL_API_VERSION).pc

DISTCLEANFILES += $(pkgconfig_DATA)
EXTRA_DIST += \
	libwalbottle/libwalbottle.pc.in \
	libwalbottle/wbl-version.h.in \
	$(NULL)

# ChangeLog
ChangeLog: $(srcdir)/ChangeLog
$(srcdir)/ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run \
	   git log --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi
.PHONY: $(srcdir)/ChangeLog

MAINTAINERCLEANFILES += ChangeLog

-include $(top_srcdir)/git.mk
