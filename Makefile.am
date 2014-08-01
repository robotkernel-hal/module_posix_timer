libdir = $(prefix)/lib/robotkernel/modules

lib_LTLIBRARIES = libmodule_posix_timer.la

libmodule_posix_timer_la_SOURCES  = module_posix_timer.cpp
libmodule_posix_timer_la_CPPFLAGS = -Wall -I$(top_srcdir)/src -I$(top_builddir) @YAMLCPP_CFLAGS@ @LN_CFLAGS@
libmodule_posix_timer_la_LDFLAGS  = -Wall  @RT_LIBS@ @PTHREAD_LIBS@  -release $(PACKAGE_VERSION)

include $(top_srcdir)/Makefile.defs
