INCLUDES =			     \
	@COMPIZ_CFLAGS@ \
	-I$(top_srcdir)/include	     \
	-I$(top_srcdir)/src \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-DLIBDIR=\"$(libdir)\"

libccp_la_LDFLAGS = -module -avoid-version -no-undefined
libccp_la_LIBADD = $(top_builddir)/src/libccs.la
libccp_la_SOURCES = ccp.c

moduledir = $(PLUGINDIR)

module_LTLIBRARIES = libccp.la

