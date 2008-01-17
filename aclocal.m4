AC_DEFUN([CHECK_SDK],
[
    AC_MSG_CHECKING(which SDK to use)
    macosx_sdks="/Developer/SDKs/MacOSX10.4u.sdk \
		 /Developer/SDKs/MacOSX10.5.sdk"
    AC_ARG_WITH(macosx_sdk,
	    AC_HELP_STRING([--with-macosx_sdk=DIR], [path to SDK]),
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
