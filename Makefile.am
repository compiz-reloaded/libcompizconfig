## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

ALL_SUBDIRS = 	 \
    include	 \
    po           \
    src	         \
    backend      \
    plugin       \
    metadata	 \
    config

SUBDIRS = $(ALL_SUBDIRS)

EXTRA_DIST =            \
    VERSION             \
    COPYING		\
    libcompizconfig.pc.in

clean:
	for sdir in $(SUBDIRS) ; do \
		cd $$sdir; make clean; cd .. ; \
	done

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcompizconfig.pc
