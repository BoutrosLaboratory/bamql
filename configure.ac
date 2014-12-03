AC_INIT([barf], [1.0], [pul.boutros@oicr.on.ca])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
AM_PROG_AR
AC_PROG_CXX_C_O
AC_PROG_LIBTOOL

AX_LLVM([core executionengine])
PKG_CHECK_MODULES(HTS, [ htslib ], [], [
	ORIGINAL_CFLAGS="$CPPFLAGS"
	ORIGINAL_LIBS="$LIBS"
	# This is here because libhts does not correctly link against libm and pthread
	AC_CHECK_LIB([m],[pow])
	if test "$enable_threads" = no; then
		ACX_PTHREAD
	fi
	if test x$acx_pthread_ok = xno; then
		AC_MSG_ERROR([*** pthreads are needed by htslib])
	fi

	AC_CHECK_HEADER([htslib/sam.h], [], [AC_MSG_ERROR([*** htslib is required, install htslib header files])])
	AC_CHECK_LIB([hts], [hts_open], [], [AC_MSG_ERROR([*** htslib is required, install htslib library files])], [$PTHREAD_CFLAGS])
	HTS_CFLAGS="$CFLAGS $PTHREAD_CPPFLAGS"
	HTS_LIBS="$LIBS $PTHREAD_LIBS"
	AC_SUBST(HTS_CFLAGS)
	AC_SUBST(HTS_LIBS)
	CPPFLAGS="$ORIGINAL_CFLAGS"
	LIBS="$ORIGINAL_LIBS"
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
