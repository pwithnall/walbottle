AC_PREREQ(2.65)

# Release version
m4_define([wbl_version_major],[0])
m4_define([wbl_version_minor],[2])
m4_define([wbl_version_micro],[0])

# API version
m4_define([wbl_api_version],[0])

AC_INIT([walbottle],[wbl_version_major.wbl_version_minor.wbl_version_micro],
        [mailto:bugzilla@tecnocode.co.uk],[walbottle],
        [https://gitlab.com/groups/walbottle])

AX_CHECK_ENABLE_DEBUG
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([libwalbottle/walbottle.h])
AC_CONFIG_HEADERS([config.h])
AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([1.9 dist-xz no-dist-gzip check-news subdir-objects])

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CXX
AM_PROG_CC_C_O
LT_INIT
PKG_PROG_PKG_CONFIG
AC_PROG_LN_S

# Requirements
GLIB_REQS=2.31.0
JSON_GLIB_REQS=1.1.1

# Before making a release, the WBL_LT_VERSION string should be modified. The
# string is of the form c:r:a. Follow these instructions sequentially:
#
#  1. If the library source code has changed at all since the last update, then
#     increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
#  2. If any interfaces have been added, removed, or changed since the last
#     update, increment current, and set revision to 0.
#  3. If any interfaces have been added since the last public release, then
#     increment age.
#  4. If any interfaces have been removed or changed since the last public
#     release, then set age to 0.
AC_SUBST([WBL_LT_VERSION],[1:0:1])

AC_SUBST([WBL_VERSION_MAJOR],wbl_version_major)
AC_SUBST([WBL_VERSION_MINOR],wbl_version_minor)
AC_SUBST([WBL_VERSION_MICRO],wbl_version_micro)
AC_SUBST([WBL_API_VERSION],wbl_api_version)

# Dependencies
AX_PKG_CHECK_MODULES([GLIB],[glib-2.0 >= $GLIB_REQS gio-2.0 gobject-2.0],[])
AX_PKG_CHECK_MODULES([JSON_GLIB],[json-glib-1.0 >= $JSON_GLIB_REQS],[])

# Code coverage
AX_CODE_COVERAGE

# General macros
AX_COMPILER_FLAGS
AX_VALGRIND_CHECK

GOBJECT_INTROSPECTION_CHECK([0.9.7])

VAPIGEN_CHECK()

AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources])
AS_IF([test "$GLIB_COMPILE_RESOURCES" == ""],[
	AC_MSG_ERROR([glib-compile-resources not found])
])

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

GTK_DOC_CHECK([1.14],[--flavour no-tmpl])

AC_CONFIG_FILES([
Makefile
libwalbottle/libwalbottle-$WBL_API_VERSION.pc:libwalbottle/libwalbottle.pc.in
libwalbottle/wbl-version.h
libwalbottle/tests/Makefile
libwalbottle/docs/Makefile
libwalbottle/docs/version.xml
],[],
[WBL_API_VERSION='$WBL_API_VERSION'])
AC_OUTPUT
