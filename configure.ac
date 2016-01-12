AC_PREREQ(2.63)
AC_INIT([zanata-glib], [0.1])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.12 dist-xz no-dist-gzip foreign tar-ustar no-define serial-tests -Wno-portability])

AC_PROG_CC
AM_PROG_CC_C_O
LT_INIT([disable-static])

AM_PATH_GLIB_2_0
GOBJECT_INTROSPECTION_REQUIRE([1.35.9])
GOBJECT_INTROSPECTION_CHECK([0.9.0])

PKG_CHECK_MODULES([DEPS], [gio-2.0 json-glib-1.0 libsoup-2.4 rest-0.7], ,
  [AC_MSG_ERROR([can't find dependency libraries])])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  tests/Makefile
])
AC_CONFIG_FILES([tests/run-test.sh], [chmod +x tests/run-test.sh])
AC_OUTPUT
