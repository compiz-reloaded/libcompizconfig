ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

ALL_SUBDIRS =		\
	include		 \
	src		 \
	backend		 \
	plugin		 \
	metadata	 \
	config

SUBDIRS = $(ALL_SUBDIRS)

EXTRA_DIST =			\
	AUTHORS			 \
	COPYING			 \
	LICENSE.GPL		 \
	LICENSE.LGPL		 \
	NEWS			 \
	README.md		 \
	autogen.sh		 \
	libcompizconfig.pc.in

clean:
	for sdir in $(SUBDIRS) ; do \
		cd $$sdir; make clean; cd .. ; \
	done

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcompizconfig.pc

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
