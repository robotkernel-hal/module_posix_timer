ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libmodule_posix_timer.la

libmodule_posix_timer_la_SOURCES  = posix_timer.cpp
libmodule_posix_timer_la_CPPFLAGS = -Wall -I$(srcdir) -I$(builddir) \
                                    @ROBOTKERNEL_CFLAGS@
libmodule_posix_timer_la_LDFLAGS  = -Wall -release $(PACKAGE_VERSION)

install-exec-hook:
	mkdir -p $(prefix)/share/ln
	-cp -rf $(top_srcdir)/share/ln/* $(prefix)/share/ln/
	-cp -rf $(top_builddir)/share/ln/* $(prefix)/share/ln/
