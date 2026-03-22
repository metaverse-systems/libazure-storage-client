ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src
if HAVE_CATCH2
SUBDIRS += tests
endif
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = azure-storage-client.pc

libazurestorageincludedir = $(includedir)/libazure-storage-client
libazurestorageinclude_HEADERS = \
    include/libazure-storage-client/azure-storage-client.hpp \
    include/libazure-storage-client/json.hpp

EXTRA_DIST = m4/ax_cxx_compile_stdcxx.m4
