AC_INIT([bamql], [1.0], [paul.boutros@oicr.on.ca])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR
AC_PROG_CXX_C_O
AC_PROG_LIBTOOL

AX_LLVM(LLVM_CORE, [core])
AX_LLVM(LLVM_WRITE, [core nativecodegen])
AX_LLVM(LLVM_RUN, [core executionengine jit native mcjit])
AC_CHECK_PROGS(CLANG, [clang clang-${LLVM_VERSION} clang-${LLVM_VERSION%.*}])
if test "x$CLANG" = x
then
	AC_MSG_ERROR([*** clang is required, install clang compiler])
fi
AC_CHECK_PROGS(LLC, [llc llc-${LLVM_VERSION} llc-${LLVM_VERSION%.*}])
if test "x$LLC" = x
then
	AC_MSG_ERROR([*** llc is required, install LLVM utiltities])
fi
PKG_CHECK_MODULES(Z, [ zlib ])
PKG_CHECK_MODULES(UUID, [ uuid ])
PKG_CHECK_MODULES(PCRE, [ libpcre ])
PKG_CHECK_MODULES(HTS, [ htslib ], [], [
	ORIGINAL_CFLAGS="$CPPFLAGS"
	ORIGINAL_LIBS="$LIBS"
	# This is here because libhts does not correctly link against libm and pthread
	AC_CHECK_LIB([m],[pow])
	ACX_PTHREAD

	AC_CHECK_HEADER([htslib/sam.h], [], [AC_MSG_ERROR([*** htslib is required, install htslib header files])])
	AC_CHECK_LIB([hts], [hts_open], [], [AC_MSG_ERROR([*** htslib is required, install htslib library files])], [$PTHREAD_CFLAGS])
	HTS_CFLAGS="$CFLAGS $PTHREAD_CPPFLAGS"
	HTS_LIBS="$LIBS $PTHREAD_LIBS"
	AC_SUBST(HTS_CFLAGS)
	AC_SUBST(HTS_LIBS)
	CPPFLAGS="$ORIGINAL_CFLAGS"
	LIBS="$ORIGINAL_LIBS"
])

AC_CONFIG_FILES([Makefile bamql.pc bamql-jit.pc])
AC_OUTPUT
