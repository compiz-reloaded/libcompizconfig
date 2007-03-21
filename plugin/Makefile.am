INCLUDES =			     \
	@COMPIZ_CFLAGS@ \
	-I$(top_srcdir)/include	     \
	-I$(top_srcdir)/src \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-DLIBDIR=\"$(libdir)\"

libbset_la_LDFLAGS = -module -avoid-version -no-undefined
libbset_la_LIBADD = $(top_builddir)/src/libbsettings.la
libbset_la_SOURCES = bset.c

moduledir = $(PLUGINDIR)

module_LTLIBRARIES = libbset.la

