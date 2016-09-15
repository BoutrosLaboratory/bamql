AC_INIT([bamql], [1.2], [paul.boutros@oicr.on.ca])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)
m4_pattern_allow([AM_PROG_AR])
AM_PROG_AR
AC_PROG_CXX_C_O
AC_PROG_LIBTOOL

AX_LLVM(LLVM_CORE, [core])
AX_LLVM(LLVM_WRITE, [core nativecodegen])
AX_LLVM(LLVM_RUN, [core executionengine native mcjit])
PKG_CHECK_MODULES(UUID, [ uuid ])
PKG_CHECK_MODULES(PCRE, [ libpcre ])
PKG_CHECK_MODULES(HTS, [ htslib ])

AC_CONFIG_FILES([Makefile compiler/bamql-cpl.pc iterator/bamql-itr.pc jit/bamql-jit.pc runtime/bamql-rt.pc])
AC_OUTPUT
