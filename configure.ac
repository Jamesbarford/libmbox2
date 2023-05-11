AC_PREREQ([2.69])
AC_INIT([libmbox2], [1.0], [your-email@domain.com])
AC_CONFIG_SRCDIR(src/mbox.c)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AC_PREFIX_DEFAULT([/usr/local])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
dnl Try and get a full POSIX environment on obscure systems
AC_USE_SYSTEM_EXTENSIONS

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library not found])])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h pthread.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T


# Checks for library functions.
AC_CHECK_FUNCS([memset])

# Options for the library build
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [enable debugging information (default is no)])],
  [case "${enableval}" in    yes) CFLAGS="$CFLAGS -g" ;;    no) ;;    *) AC_MSG_ERROR([invalid value for --enable-debug]) ;;
  esac], [])

LT_PREREQ([2.4.2])
LT_INIT

AM_CONDITIONAL([CARES_LT_SHLIB_USE_VERSION_INFO],
  [test "x$xc_lt_shlib_use_version_info" = 'xyes'])
AM_CONDITIONAL([CARES_LT_SHLIB_USE_NO_UNDEFINED],
  [test "x$xc_lt_shlib_use_no_undefined" = 'xyes'])
AM_CONDITIONAL([CARES_LT_SHLIB_USE_MIMPURE_TEXT],
  [test "x$xc_lt_shlib_use_mimpure_text" = 'xyes'])


BUILD_SUBDIRS="include src"
AC_SUBST(BUILD_SUBDIRS)

# Generate Makefile
AC_CONFIG_FILES([Makefile \
                 include/Makefile \
                 src/Makefile \
                 ])

AC_OUTPUT
