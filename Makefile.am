ACLOCAL_AMFLAGS = -I m4 

libdir = $(prefix)/lib

lib_LTLIBRARIES = libmodule_posix_timer.la

libmodule_posix_timer_la_SOURCES  = module_posix_timer.cpp
libmodule_posix_timer_la_CPPFLAGS = -Wall -I$(top_srcdir)/src -I$(top_builddir) @ROBOTKERNEL_CFLAGS@
libmodule_posix_timer_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(srcdir)/share/ln/* $(prefix)/share/ln/