/* dh: set default handlers for document types based on a settings file. */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "handler.h"

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
    struct stat		st;
    int			c, err = 0;
    char		*path = NULL;
    char		*p;
    extern int		optind;

    /* by default, read from a FILE stream */
    handler_f = fsethandler;

    while (( c = getopt( ac, av, "hVv" )) != -1 ) {
	switch ( c ) {
	case 'h':	/* help */
	default:
	    err++;
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
	fprintf( stderr, "usage: %s [ -hvV ] [ settings_path ]\n", av[ 0 ] );
	exit( 1 );
    }

    if ( path ) {
	if ( stat( path, &st ) != 0 ) {
	    fprintf( stderr, "stat %s: %s\n", path, strerror( errno ));
	    exit( 2 );
	}
	switch ( st.st_mode & S_IFMT ) {
	case S_IFDIR:	/* directory of settings files */
	    handler_f = dirsethandler;
	    break;

	case S_IFREG:	/* settings file or plist */
	    if (( p = strrchr( path, '.' )) != NULL ) {
		p++;
		if ( strcmp( p, "plist" ) == 0 ) {
		    handler_f = psethandler;
		}
	    }
	    break;

	default:
	    fprintf( stderr, "%s: not a supported settings path\n", path );
	    exit( 1 );
	}
    }

    nroles = sizeof( rtm ) / sizeof( rtm[ 0 ] );

    return( handler_f( path ));
}
