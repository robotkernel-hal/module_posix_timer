ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libmodule_posix_timer.la

BUILT_SOURCES = $(top_builddir)/src/gen_definitions.h
CLEANFILES = $(top_builddir)/src/gen_definitions.h

libmodule_posix_timer_la_SOURCES  = posix_timer.cpp
libmodule_posix_timer_la_CPPFLAGS = -Wall -I$(srcdir) -I$(builddir) \
                                    @ROBOTKERNEL_CFLAGS@ @SERVICE_PROVIDER_PROCESS_DATA_INSPECTION_CFLAGS@
libmodule_posix_timer_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

VPATH = $(srcdir):$(top_srcdir)/share

GEN_DEFINITIONS = pd_definitions/module_posix_timer/inputs

$(top_builddir)/src/gen_definitions.h: Makefile $(GEN_DEFINITIONS)
	@echo creating $@
	rk_generate --indir $(top_srcdir)/share --out $@ $(GEN_DEFINITIONS)
	touch $@

