AC_DEFUN([DUTI_CHECK_SDK],
[
    AC_MSG_CHECKING(which SDK to use)
    AC_ARG_WITH(macosx-sdk,
	    AC_HELP_STRING([--with-macosx-sdk=DIR], [path to SDK]),
	    macosx_sdk="$withval")

    case "${host_os}" in
	darwin8*)
	    sdk="/Developer/SDKs/MacOSX10.4u.sdk"
	    ;;

	darwin9*)
	    sdk="/Developer/SDKs/MacOSX10.5.sdk"
	    ;;

	*)
	    AC_MSG_ERROR([${host_os} is not a supported system])
    esac

    if test -z "$macosx_sdk"; then
	macosx_sdk=$sdk
    fi

    AC_SUBST(macosx_sdk)
    AC_MSG_RESULT($macosx_sdk)
])

AC_DEFUN([DUTI_CHECK_DEPLOYMENT_TARGET],
[
    AC_MSG_CHECKING(Mac OS X deployment target)

    AC_ARG_WITH(macosx-deployment-target,
	    AC_HELP_STRING([--with-macosx-deployment-target=VERSION],
	    [OS version]), macosx_dep_target="$withval")

    case "${host_os}" in
	darwin8*)
	    dep_target="10.4"
	    ;;

	darwin9*)
	    dep_target="10.5"
	    ;;
    esac

    if test -z "$macosx_dep_target"; then
	macosx_dep_target=$dep_target
    fi

    AC_SUBST(macosx_dep_target)
    AC_MSG_RESULT($macosx_dep_target)
])
