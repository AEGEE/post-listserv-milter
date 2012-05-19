#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.69)
AC_INIT(post-listserv-milter, 1.0, dpa@aegee.org)
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([src/post-listserv-milter.c])
AC_CONFIG_HEADER([config.h])
AM_SILENT_RULES([yes])
# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
# Checks for libraries.
# FIXME: Replace `main' with a function in `-lmilter':
AC_CHECK_LIB([milter], [smfi_main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_HEADER_STDC
AC_CHECK_HEADERS([netdb.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h])
AC_CHECK_FUNCS([gethostbyname memset socket strchr strdup])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
