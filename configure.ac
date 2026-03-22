AC_INIT([azure-storage-client], [0.0.1])
AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT
AC_PROG_CXX
AC_CONFIG_MACRO_DIRS([m4])
AX_CXX_COMPILE_STDCXX(20, noext, mandatory)
PKG_CHECK_MODULES([LIBCURL], [libcurl])
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([CATCH2], [catch2-with-main], [have_catch2=yes], [have_catch2=no])
AM_CONDITIONAL([HAVE_CATCH2], [test "x$have_catch2" = "xyes"])
AC_CONFIG_FILES([Makefile src/Makefile azure-storage-client.pc])
if test "x$have_catch2" = "xyes"; then
  AC_CONFIG_FILES([tests/Makefile])
fi
AC_OUTPUT
