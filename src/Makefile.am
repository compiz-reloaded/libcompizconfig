INCLUDES =			     \
	@COMPIZ_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	-I$(top_srcdir)/include	     \
	-I$(top_srcdir)/src \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-DMETADATADIR=\"$(METADATADIR)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DGLOBALMETADATA=\"$(globalmetadata)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\"


libcompizconfig_la_SOURCES = \
	main.c 		\
	lists.c 	\
	compiz.c 	\
	config.c 	\
	iniparser.c 	\
	ini.c 		\
	bindings.c 	\
	filewatch.c 	\
	ccs-private.h	\
	iniparser.h

libcompizconfig_la_LIBADD = @LIBXML2_LIBS@

lib_LTLIBRARIES=libcompizconfig.la

