## Process this file with automake to produce Makefile.in

ALL_SUBDIRS = 	 \
    include	 \
    po           \
    src	         \
    backend      \
    plugin       \
    metadata

SUBDIRS = $(ALL_SUBDIRS)

EXTRA_DIST =            \
    VERSION             \
    COPYING			    \
    bsettings.pc.in         \
    intltool-extract.in \
    intltool-merge.in   \
    intltool-update.in

DISTCLEANFILES =     \
    intltool-extract \
    intltool-merge   \
    intltool-update

clean:
	for sdir in $(SUBDIRS) ; do \
		cd $$sdir; make clean; cd .. ; \
	done

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libccs.pc
