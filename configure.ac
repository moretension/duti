
# Process with autoconf to create configure script

AC_INIT(dh, INTERNAL, nosupport@all)
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright released into the public domain by Andrew Mortensen, December 2007.])
AC_CONFIG_SRCDIR([duti.c])

AC_CANONICAL_HOST
case $host_os in
    *darwin9*)
	;;
    *darwin8*)
	;;
    *)
	AC_ERROR([this software requires Mac OS X 10.4 or newer])
	;;
esac

AC_ARG_WITH(macosx_sdk, AC_HELP_STRING([--with-macosx-sdk=SDK_DIR], [path to Mac OS X SDK directory]), macosx_sdk="$withval", macosx_sdk="/Developer/SDKs/MacOSX10.4u.sdk")
AC_SUBST(macosx_sdk)

build_date=`date "+%B %d, %Y"`
AC_SUBST(build_date)

# program checks
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL

# function checks
AC_CHECK_FUNC(strlcpy,have_strlcpy=yes,)
if test x_$have_strlcpy = x_; then
    AC_ERROR([required function strlcpy not found.])
fi

if test x_$OPTOPTS = x_; then
    if test x_$GCC = x_yes; then
	OPTOPTS="$OPTOPTS -Wall -Wmissing-prototypes"
    fi
fi
AC_SUBST(OPTOPTS)

AC_CONFIG_FILES([Makefile version.c])
AC_OUTPUT
