lib_LTLIBRARIES = libzanata-glib.la

libzanata_glib_la_headers =			\
	zanata-authorizer.h			\
	zanata-enums.h				\
	zanata-enumtypes.h			\
	zanata-iteration.h			\
	zanata-key-file-authorizer.h		\
	zanata-project.h			\
	zanata-session.h			\
	zanata-suggestion.h

libzanata_glib_la_SOURCES =			\
	zanata-authorizer.c			\
	zanata-enumtypes.c			\
	zanata-iteration.c			\
	zanata-key-file-authorizer.c		\
	zanata-project.c			\
	zanata-session.c			\
	zanata-suggestion.c

BUILT_SOURCES = zanata-enumtypes.h zanata-enumtypes.c

zanata-enumtypes.h: zanata-enums.h zanata-enumtypes.h.template
	$(AM_V_GEN) $(GLIB_MKENUMS) --identifier-prefix Zanata --symbol-prefix zanata --template zanata-enumtypes.h.template zanata-enums.h > $@-t && mv $@-t $@ || rm $@-t
zanata-enumtypes.c: zanata-enums.h zanata-enumtypes.c.template
	$(AM_V_GEN) $(GLIB_MKENUMS) --identifier-prefix Zanata --symbol-prefix zanata --template zanata-enumtypes.c.template zanata-enums.h > $@-t && mv $@-t $@ || rm $@-t

libzanata_glib_la_CFLAGS = $(DEPS_CFLAGS)
libzanata_glib_la_LIBADD = $(DEPS_LIBS)

zanataglibincludedir = $(includedir)/zanata-glib-0.0
zanataglibsubincludedir = $(zanataglibincludedir)/zanata

zanataglibsubinclude_HEADERS =			\
	zanata.h				\
	$(libzanata_glib_la_headers)

CLEANFILES =

if HAVE_INTROSPECTION
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = Zanata-1.0.gir
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(builddir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

Zanata-1.0.gir: libzanata-glib.la
Zanata_1_0_gir_SCANNERFLAGS = --warn-all --pkg-export=zanata-glib --namespace=Zanata --identifier-prefix=Zanata --symbol-prefix=zanata $(ZANATA_GLIB_STATIC_DEPENDENCIES)
Zanata_1_0_gir_INCLUDES = GLib-2.0 GObject-2.0 Gio-2.0 Json-1.0 Rest-0.7 Soup-2.4
Zanata_1_0_gir_CFLAGS = $(libzanata_glib_la_CFLAGS)
Zanata_1_0_gir_LIBS = libzanata-glib.la
Zanata_1_0_gir_FILES = $(libzanata_glib_la_SOURCES) $(libzanata_glib_la_headers)

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

-include $(top_srcdir)/git.mk
