/* dh: set default handlers for document types based on a settings file. */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "handler.h"
#include "plist.h"
#include "util.h"

extern char		*dh_version;
extern int		nroles;
int			verbose = 0;

struct roles		rtm[] = {
    { "none",   kLSRolesNone },
    { "viewer", kLSRolesViewer },
    { "editor", kLSRolesEditor },
    { "shell",  kLSRolesShell },
    { "all",    kLSRolesAll },
};

    int
main( int ac, char *av[] )
{
    int			c, err = 0;
    char		*path = NULL;
    extern int		optind;

    /* by default, read from a FILE stream */
    handler_f = fsethandler;

    while (( c = getopt( ac, av, "d:hVvp:" )) != -1 ) {
	switch ( c ) {
	case 'd':	/* process settings files from directory */
	    handler_f = dirsethandler;
	    path = optarg;
	    break;

	case 'h':	/* help */
	default:
	    err++;
	    break;

	case 'p':	/* read from plist */
	    handler_f = psethandler;
	    path = optarg;
	    break;

	case 'V':	/* version */
	    printf( "%s\n", dh_version );
	    exit( 0 );

	case 'v':	/* verbose */
	    verbose = 1;
	    break;
	}
    }

    if ( ac - optind == 1 ) {
	path = av[ optind ];
    } else if ( path == NULL && err ) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: %s [ -hvV ] [ -d settings_directory ] "
		"[ -p settings_plist ] [ settings_file ]\n", av[ 0 ] );
	exit( 1 );
    }

    nroles = sizeof( rtm ) / sizeof( rtm[ 0 ] );

    return( handler_f( path ));
}
