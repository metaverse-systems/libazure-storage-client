lib_LTLIBRARIES = libazure-storage-client.la
libazure_storage_client_la_SOURCES = azure-storage-client.cpp
libazure_storage_client_la_CXXFLAGS = $(LIBCURL_CFLAGS) $(OPENSSL_CFLAGS) -I$(top_srcdir)/include
libazure_storage_client_la_LIBADD = $(LIBCURL_LIBS) $(OPENSSL_LIBS)
libazure_storage_client_la_LDFLAGS = -no-undefined -version-info 0:0:0
