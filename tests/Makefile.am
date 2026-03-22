check_PROGRAMS = test_azure_table_client
test_azure_table_client_SOURCES = test_azure_table_client.cpp
test_azure_table_client_CXXFLAGS = $(CATCH2_CFLAGS) $(LIBCURL_CFLAGS) $(OPENSSL_CFLAGS) -I$(top_srcdir)/include -pthread
test_azure_table_client_LDADD = $(top_builddir)/src/libazure-storage-client.la $(CATCH2_LIBS) $(LIBCURL_LIBS) $(OPENSSL_LIBS)
test_azure_table_client_LDFLAGS = -pthread

TESTS = test_azure_table_client
