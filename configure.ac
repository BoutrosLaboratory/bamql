AC_INIT([bamql],[1.6],[paul.boutros@oicr.on.ca])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR
AC_PROG_CXX_C_O
LT_INIT
AC_LANG_PUSH([C++])

# Parameter pack bug in GCC 4.8
SAVED_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${SAVED_CPPFLAGS} -std=c++11"
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM([[struct S { int foo() const{ return 0; } }; template<class... Args> void evaluate(Args...){} template <class... Args> void bar(Args... args) { auto lmb = [=](){ evaluate(args.foo()...); }; } ]], [[S s{}; bar(s); ]])],
	[], [AC_MSG_ERROR([There is a bug in your C++ compiler. You probably have an older GCC.])])
AC_LANG_POP([C++])
CPPFLAGS="${SAVED_CPPFLAGS}"

AX_LLVM(LLVM_CORE, [core])
if test ${LLVM_VERSION} -lt 14 ; then
	AC_MSG_ERROR([[LLVM 14 or newer is required, but detected ${LLVM_VERSION}.]])
fi
AX_LLVM(LLVM_WRITE, [core nativecodegen passes])
AX_LLVM(LLVM_RUN, [core executionengine native orcjit])
PKG_CHECK_MODULES(UUID, [ uuid ], [], [PKG_CHECK_MODULES(UUID, [ ossp-uuid ])])
PKG_CHECK_MODULES(PCRE, [ libpcre ])
PKG_CHECK_MODULES(HTS, [ htslib ], [], [
  # This is only to support the very old HTSLib on Trusty. Delete in April 2019
	ORIGINAL_CFLAGS="$CPPFLAGS"
	ORIGINAL_LIBS="$LIBS"
	# This is here because libhts does not correctly link against libm, zlib and pthread
	AC_CHECK_LIB([m],[pow])
	PKG_CHECK_MODULES(Z, [ zlib ])
	ACX_PTHREAD

	AC_CHECK_HEADER([htslib/sam.h], [], [AC_MSG_ERROR([*** htslib is required, install htslib header files])])
	AC_CHECK_LIB([hts], [hts_open], [], [AC_MSG_ERROR([*** htslib is required, install htslib library files])], [$PTHREAD_CFLAGS $Z_CFLAGS])
	HTS_CFLAGS="$CFLAGS $PTHREAD_CPPFLAGS $Z_CFLAGS"
	HTS_LIBS="$LIBS $PTHREAD_LIBS $Z_LIBS"
	AC_SUBST(HTS_CFLAGS)
	AC_SUBST(HTS_LIBS)
	CPPFLAGS="$ORIGINAL_CFLAGS"
	LIBS="$ORIGINAL_LIBS"
])

AC_CONFIG_FILES([Makefile compiler/bamql-cpl.pc iterator/bamql-itr.pc jit/bamql-jit.pc runtime/bamql-rt.pc])
AC_OUTPUT
