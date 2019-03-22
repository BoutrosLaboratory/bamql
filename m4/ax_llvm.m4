# ===========================================================================
#          http://www.gnu.org/software/autoconf-archive/ax_llvm.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_LLVM([prefix], [llvm-libs])
#
# DESCRIPTION
#
#   Test for the existance of llvm, and make sure that it can be linked with
#   the llvm-libs argument that is passed on to llvm-config i.e.:
#
#     llvm --libs <llvm-libs>
#
#   llvm-config will also include any libraries that are depended apon.
#
# LICENSE
#
#   Copyright (c) 2008 Andy Kitchen <agimbleinthewabe@gmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved. This file is offered as-is, without any
#   warranty.

#serial 12

AC_DEFUN([AX_LLVM],
[
AC_ARG_ENABLE([static-llvm], AS_HELP_STRING([--enable-static-llvm], [compiled against the static LLVM libraries, instead of the shared library.]), [enable_static_llvm=yes])
AC_ARG_WITH([llvm],
	AS_HELP_STRING([--with-llvm@<:@=FILE@:>@], [use llvm (default is yes) - it is possible to specify the llvm-config script for llvm (optional)]),
	[
		if test "$withval" = "no"; then
		want_llvm="no"
		elif test "$withval" = "yes"; then
				want_llvm="yes"
				ac_llvm_config_path=`which llvm-config`
		else
			want_llvm="yes"
				ac_llvm_config_path="$withval"
	fi
		],
		[want_llvm="yes"])

	succeeded=no
	if test -z "$ac_llvm_config_path"; then
		ac_llvm_config_path=`which llvm-config llvm-config-8 llvm-config-7 llvm-config-6.0 llvm-config-5.0 llvm-config-4.0 llvm-config-3.9 llvm-config-3.8 llvm-config-3.7 llvm-config-3.6 llvm-config-3.5 llvm-config-3.4 | head -n 1`
	fi

	if test "x$want_llvm" = "xyes"; then
		if test -e "$ac_llvm_config_path"; then
			[$1]_CPPFLAGS="$($ac_llvm_config_path --cxxflags | sed -e 's/-fno-exceptions//g')"
			[$1]_LDFLAGS="$($ac_llvm_config_path --ldflags)"
			[$1]_BARELIBS="$($ac_llvm_config_path --libs)"
			[$1]_INCLUDEDIR="$($ac_llvm_config_path --includedir)"
			[$1]_LIBDIR="$($ac_llvm_config_path --libdir)"
			LLVM_VERSION="$($ac_llvm_config_path --version | cut -f 1-2 -d .)"
			LLVM_COMPONENTS="$2"
			if test "x$enable_static_llvm" != "xyes" ; then
				if test "x${LLVM_VERSION}" = "x3.4"; then
					AX_LLVM_SYSLIBS=""
				else
					AX_LLVM_SYSLIBS="--system-libs"
				fi
				if test "x${LLVM_VERSION}" = "x3.4" -o "x${LLVM_VERSION}" = "x3.5" && echo "${LLVM_COMPONENTS}" | grep mcjit > /dev/null ; then
					LLVM_COMPONENTS="${LLVM_COMPONENTS} jit"
				fi
				[$1]_LIBS="$($ac_llvm_config_path --libs ${AX_LLVM_SYSLIBS} $LLVM_COMPONENTS | tr '\n' ' ')"
			else
				[$1]_LIBS="-lLLVM-${LLVM_VERSION}"
			fi

			AC_REQUIRE([AC_PROG_CXX])
			CPPFLAGS_SAVED="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $[$1]_CPPFLAGS"

			LDFLAGS_SAVED="$LDFLAGS"
			LDFLAGS="$LDFLAGS $[$1]_LDFLAGS"

			LIBS_SAVED="$LIBS"
			LIBS="$[$1]_LIBS $LIBS"

			AC_CACHE_CHECK(can compile with and link with llvm([$2]),
				ax_cv_llvm, [
					AC_LANG_PUSH([C++])
					AC_LINK_IFELSE(
						[AC_LANG_PROGRAM([[@%:@include <llvm/IR/LLVMContext.h>]], [[llvm::LLVMContext C; return 0;]])],
						ax_cv_llvm=yes, ax_cv_llvm=no)
					AC_LANG_POP([C++])
				])

			if test "x$ax_cv_llvm" = "xyes"; then
				succeeded=yes
			fi

			CPPFLAGS="$CPPFLAGS_SAVED"
			LDFLAGS="$LDFLAGS_SAVED"
			LIBS="$LIBS_SAVED"
		else
			succeeded=no
		fi
	fi

		if test "$succeeded" != "yes" ; then
			AC_MSG_ERROR([[We could not detect the llvm libraries make sure that llvm-config is on your path or specified by --with-llvm.]])
		else
			AC_SUBST([$1][_CPPFLAGS])
			AC_SUBST([$1][_LDFLAGS])
			AC_SUBST([$1][_LIBS])
			AC_SUBST(LLVM_VERSION)
			AC_DEFINE(HAVE_LLVM,,[define if the llvm library is available])
		fi
])
