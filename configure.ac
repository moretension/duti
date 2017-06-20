
# Process with autoconf to create configure script

AC_INIT(dh, 1.5.2, nosupport@all)
AC_PREREQ(2.52)
AC_COPYRIGHT([Copyright released into the public domain by Andrew Mortensen, January 2008.])
AC_CONFIG_SRCDIR([duti.c])

build_date=`date "+%B %d, %Y"`
AC_SUBST(build_date)

# program checks
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL

AC_CANONICAL_SYSTEM
DUTI_CHECK_SDK
DUTI_CHECK_DEPLOYMENT_TARGET

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
