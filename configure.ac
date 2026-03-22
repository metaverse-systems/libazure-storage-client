AC_INIT([azure-storage-client], [0.0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT
AC_PROG_CXX
AC_CONFIG_MACRO_DIRS([m4])
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)
PKG_CHECK_MODULES([LIBCURL], [libcurl])
PKG_CHECK_MODULES([OPENSSL], [openssl])
AC_CONFIG_FILES([Makefile src/Makefile azure-storage-client.pc])
AC_OUTPUT
