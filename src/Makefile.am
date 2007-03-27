INCLUDES =			     \
	@COMPIZ_CFLAGS@ \
	-I$(top_srcdir)/include	     \
	-I$(top_srcdir)/src \
	-DPLUGINDIR=\"$(PLUGINDIR)\" \
	-DLIBDIR=\"$(libdir)\"


libbsettings_la_SOURCES=main.c lists.c compiz.c match.c config.c option.c iniparser.c ini.c

lib_LTLIBRARIES=libbsettings.la

