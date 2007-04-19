INCLUDES =			     \
	@COMPIZ_CFLAGS@ \
	@LIBXML2_CFLAGS@ \
	-I$(top_srcdir)/include	     \
	-I$(top_srcdir)/src \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-DLIBDIR=\"$(libdir)\"


libbsettings_la_SOURCES=main.c lists.c compiz.c config.c iniparser.c ini.c bindings.c filewatch.c
libbsettings_la_LIBADD = @LIBXML2_LIBS@

lib_LTLIBRARIES=libbsettings.la

