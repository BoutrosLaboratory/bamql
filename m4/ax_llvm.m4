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
AC_ARG_ENABLE([static-llvm], [compiled against the static LLVM libraries, instead of the shared library.], [enable_static_llvm=yes])
AC_ARG_WITH([llvm],
	AS_HELP_STRING([--with-llvm@<:@=DIR@:>@], [use llvm (default is yes) - it is possible to specify the root directory for llvm (optional)]),
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
		ac_llvm_config_path=`which llvm-config`
	fi

	if test "x$want_llvm" = "xyes"; then
		if test -e "$ac_llvm_config_path"; then
			[$1]_CPPFLAGS="$($ac_llvm_config_path --cxxflags)"
			[$1]_LDFLAGS="$($ac_llvm_config_path --ldflags)"
			if test "x$enable_static_llvm" != "xyes" ; then
				[$1]_LIBS="$($ac_llvm_config_path --libs $2) $($ac_llvm_config_path --ldflags)"
			else
				[$1]_LIBS="-lLLVM-$($ac_llvm_config_path --version)"
			fi

			AC_REQUIRE([AC_PROG_CXX])
			CPPFLAGS_SAVED="$CPPFLAGS"
			CPPFLAGS="$CPPFLAGS $[$1]_CPPFLAGS"

			LDFLAGS_SAVED="$LDFLAGS"
			LDFLAGS="$LDFLAGS $[$1]_LDFLAGS"

			LIBS_SAVED="$LIBS"
			LIBS="$LIBS $[$1]_LIBS"

			AC_CACHE_CHECK(can compile with and link with llvm([$2]),
						   ax_cv_llvm,
		[AC_LANG_PUSH([C++])
				 AC_LINK_IFELSE([AC_LANG_PROGRAM([[@%:@include <llvm/IR/LLVMContext.h>
													]],
					   [[llvm::LLVMContext& C = llvm::getGlobalContext(); return 0;]])],
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
			AC_DEFINE(HAVE_LLVM,,[define if the llvm library is available])
		fi
])
