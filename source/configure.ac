# Process this file with autoconf to produce a configure script.

AC_INIT
dnl Using config.h would conflict with qtcsh, at least
AC_CONFIG_HEADERS([sge_config.h:common/sge_config.h.in])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_MEMBERS([struct dirent.d_type], [] ,[] ,[[#include <dirent.h>]])

# Checks for library functions.
AC_CHECK_FUNCS([getgrouplist])

# Compatibility

AC_OUTPUT
