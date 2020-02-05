AC_DEFUN([DUTI_CHECK_SDK],
[
    AC_MSG_CHECKING(which SDK to use)
    AC_ARG_WITH(macosx-sdk,
	    AC_HELP_STRING([--with-macosx-sdk=DIR], [path to SDK]),
	    macosx_sdk="$withval")

    sdk_path="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs"
    macosx_arches="-arch i386 -arch x86_64"

    case "${host_os}" in
	darwin8*)
	    sdk_path="/Developer/SDKs/MacOSX10.4u.sdk"
	    macosx_arches=""
	    ;;

	darwin9*)
	    sdk_path="/Developer/SDKs/MacOSX10.5.sdk"
	    ;;

	darwin10*)
	    sdk_path="/Developer/SDKs/MacOSX10.6.sdk"
	    ;;

	darwin11*)
	    sdk_path="${sdk_path}/MacOSX10.7.sdk"
	    ;;

	darwin12*)
	    sdk_path="${sdk_path}/MacOSX10.8.sdk"
	    ;;

	darwin13*)
	    sdk_path="${sdk_path}/MacOSX10.9.sdk"
	    macosx_arches=""
	    ;;

	darwin14*)
	    sdk_path="${sdk_path}/MacOSX10.10.sdk"
	    macosx_arches=""
	    ;;

	darwin15*|darwin16*|darwin17*|darwin18*)
	    sdk_path="${sdk_path}/MacOSX.sdk"
	    macosx_arches=""
	    ;;
	    
	darwin19*)
	    sdk_path="${sdk_path}/MacOSX.sdk"
	    macosx_arches="-arch x86_64"
	    ;;

	*)
	    AC_MSG_ERROR([${host_os} is not a supported system])
    esac

    if test -z "$macosx_sdk"; then
	macosx_sdk=$sdk_path
    fi

    AC_SUBST(macosx_arches)
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

	darwin10*)
	    dep_target="10.6"
	    ;;

	darwin11*)
	    dep_target="10.7"
	    ;;

	darwin12*)
	    dep_target="10.8"
	    ;;

	darwin13*)
	    dep_target="10.9"
	    ;;

	darwin14*)
	    dep_target="10.10"
	    ;;

	darwin15*)
	    dep_target="10.11"
	    ;;

	darwin16*)
	    dep_target="10.12"
	    ;;

	darwin17*)
	    dep_target="10.13"
	    ;;

	darwin18*)
	    dep_target="10.14"
	    ;;
	    
	darwin19*)
	    dep_target="10.15"
	    ;;
    esac

    if test -z "$macosx_dep_target"; then
	macosx_dep_target=$dep_target
    fi

    AC_SUBST(macosx_dep_target)
    AC_MSG_RESULT($macosx_dep_target)
])
